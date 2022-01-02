/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class InspectorCommand {
  constructor({ commands }) {
    this.commands = commands;
  }

  /**
   * Return the list of all current target's inspector fronts
   *
   * @return {Promise<Array<InspectorFront>>}
   */
  async getAllInspectorFronts() {
    return this.commands.targetCommand.getAllFronts(
      [this.commands.targetCommand.TYPES.FRAME],
      "inspector"
    );
  }

  /**
   * Search the document for the given string and return all the results.
   *
   * @param {Object} walkerFront
   * @param {String} query
   *        The string to search for.
   * @param {Object} options
   *        {Boolean} options.reverse - search backwards
   * @returns {Array} The list of search results
   */
  async walkerSearch(walkerFront, query, options = {}) {
    const result = await walkerFront.search(query, options);
    return result.list.items();
  }

  /**
   * Incrementally search the top-level document and sub frames for a given string.
   * Only one result is sent back at a time. Calling the
   * method again with the same query will send the next result.
   * If a new query which does not match the current one all is reset and new search
   * is kicked off.
   *
   * @param {String} query
   *         The string / selector searched for
   * @param {Object} options
   *        {Boolean} reverse - determines if the search is done backwards
   * @returns {Object} res
   *          {String} res.type
   *          {String} res.query - The string / selector searched for
   *          {Object} res.node - the current node
   *          {Number} res.resultsIndex - The index of the current node
   *          {Number} res.resultsLength - The total number of results found.
   */
  async findNextNode(query, { reverse } = {}) {
    const inspectors = await this.getAllInspectorFronts();
    const nodes = await Promise.all(
      inspectors.map(({ walker }) =>
        this.walkerSearch(walker, query, { reverse })
      )
    );
    const results = nodes.flat();

    // If the search query changes
    if (this._searchQuery !== query) {
      this._searchQuery = query;
      this._currentIndex = -1;
    }

    if (!results.length) {
      return null;
    }

    this._currentIndex = reverse
      ? this._currentIndex - 1
      : this._currentIndex + 1;

    if (this._currentIndex >= results.length) {
      this._currentIndex = 0;
    }
    if (this._currentIndex < 0) {
      this._currentIndex = results.length - 1;
    }

    return {
      node: results[this._currentIndex],
      resultsIndex: this._currentIndex,
      resultsLength: results.length,
    };
  }

  /**
   * Returns a list of matching results for CSS selector autocompletion.
   *
   * @param {String} query
   *        The selector query being completed
   * @param {String} firstPart
   *        The exact token being completed out of the query
   * @param {String} state
   *        One of "pseudo", "id", "tag", "class", "null"
   * @return {Array<string>} suggestions
   *        The list of suggested CSS selectors
   */
  async getSuggestionsForQuery(query, firstPart, state) {
    // Get all inspectors where we want suggestions from.
    const inspectors = await this.getAllInspectorFronts();

    const mergedSuggestions = [];
    // Get all of the suggestions.
    await Promise.all(
      inspectors.map(async ({ walker }) => {
        const { suggestions } = await walker.getSuggestionsForQuery(
          query,
          firstPart,
          state
        );
        for (const [suggestion, count, type] of suggestions) {
          // Merge any already existing suggestion with the new one, by incrementing the count
          // which is the second element of the array.
          const existing = mergedSuggestions.find(
            ([s, , t]) => s == suggestion && t == type
          );
          if (existing) {
            existing[1] += count;
          } else {
            mergedSuggestions.push([suggestion, count, type]);
          }
        }
      })
    );

    // Descending sort the list by count, i.e. second element of the arrays
    return sortSuggestions(mergedSuggestions);
  }

  /**
   * Find a nodeFront from an array of selectors. The last item of the array is the selector
   * for the element in its owner document, and the previous items are selectors to iframes
   * that lead to the frame where the searched node lives in.
   *
   * For example, with the following markup
   * <html>
   *  <iframe id="level-1" src="…">
   *    <iframe id="level-2" src="…">
   *      <h1>Waldo</h1>
   *    </iframe>
   *  </iframe>
   *
   * If you want to retrieve the `<h1>` nodeFront, `selectors` would be:
   * [
   *   "#level-1",
   *   "#level-2",
   *   "h1",
   * ]
   *
   * @param {Array} selectors
   *        An array of CSS selectors to find the target accessible object.
   *        Several selectors can be needed if the element is nested in frames
   *        and not directly in the root document.
   * @param {Integer} timeoutInMs
   *        The maximum number of ms the function should run (defaults to 5000).
   *        If it exceeds this, the returned promise will resolve with `null`.
   * @return {Promise<NodeFront|null>} a promise that resolves when the node front is found
   *        for selection using inspector tools. It resolves with the deepest frame document
   *        that could be retrieved when the "final" nodeFront couldn't be found in the page.
   *        It resolves with `null` when the function runs for more than timeoutInMs.
   */
  async findNodeFrontFromSelectors(nodeSelectors, timeoutInMs = 5000) {
    if (
      !nodeSelectors ||
      !Array.isArray(nodeSelectors) ||
      nodeSelectors.length === 0
    ) {
      console.warn(
        "findNodeFrontFromSelectors expect a non-empty array but got",
        nodeSelectors
      );
      return null;
    }

    const { walker } = await this.commands.targetCommand.targetFront.getFront(
      "inspector"
    );
    const querySelectors = async nodeFront => {
      const selector = nodeSelectors.shift();
      if (!selector) {
        return nodeFront;
      }
      nodeFront = await nodeFront.walkerFront.querySelector(
        nodeFront,
        selector
      );
      // It's possible the containing iframe isn't available by the time
      // walkerFront.querySelector is called, which causes the re-selected node to be
      // unavailable. There also isn't a way for us to know when all iframes on the page
      // have been created after a reload. Because of this, we should should bail here.
      if (!nodeFront) {
        return null;
      }

      if (nodeSelectors.length > 0) {
        if (!nodeFront.isShadowHost) {
          await this.#waitForFrameLoad(nodeFront);
        }

        const { nodes } = await walker.children(nodeFront);

        // If there are remaining selectors to process, they will target a document or a
        // document-fragment under the current node. Whether the element is a frame or
        // a web component, it can only contain one document/document-fragment, so just
        // select the first one available.
        nodeFront = nodes.find(node => {
          const { nodeType } = node;
          return (
            nodeType === Node.DOCUMENT_FRAGMENT_NODE ||
            nodeType === Node.DOCUMENT_NODE
          );
        });

        // The iframe selector might have matched an element which is not an
        // iframe in the new page (or an iframe with no document?). In this
        // case, bail out and fallback to the root body element.
        if (!nodeFront) {
          return null;
        }
      }
      const childrenNodeFront = await querySelectors(nodeFront);
      return childrenNodeFront || nodeFront;
    };
    const rootNodeFront = await walker.getRootNode();

    // Since this is only used for re-setting a selection after a page reloads, we can
    // put a timeout, in case there's an iframe that would take too much time to load,
    // and prevent the markup view to be populated.
    const onTimeout = new Promise(res => setTimeout(res, timeoutInMs)).then(
      () => null
    );
    const onQuerySelectors = querySelectors(rootNodeFront);
    return Promise.race([onTimeout, onQuerySelectors]);
  }

  /**
   * Wait for the given NodeFront child document to be loaded.
   *
   * @param {NodeFront} A nodeFront representing a frame
   */
  async #waitForFrameLoad(nodeFront) {
    const domLoadingPromises = [];

    // if the flag isn't true, we don't know for sure if the iframe will be remote
    // or not; when the nodeFront was created, the iframe might still have been loading
    // and in such case, its associated window can be an initial document.
    // Luckily, once EFT is enabled everywhere we can remove this call and only wait
    // for the associated target.
    if (!nodeFront.useChildTargetToFetchChildren) {
      domLoadingPromises.push(nodeFront.waitForFrameLoad());
    }

    const {
      onResource: onDomInteractiveResource,
    } = await this.commands.resourceCommand.waitForNextResource(
      this.commands.resourceCommand.TYPES.DOCUMENT_EVENT,
      {
        // We might be in a case where the children document is already loaded (i.e. we
        // would already have received the dom-interactive resource), so it's important
        // to _not_ ignore existing resource.
        predicate: resource =>
          resource.name == "dom-interactive" &&
          resource.targetFront !== nodeFront.targetFront &&
          resource.targetFront.browsingContextID == nodeFront.browsingContextID,
      }
    );
    const newTargetResolveValue = Symbol();
    domLoadingPromises.push(
      onDomInteractiveResource.then(() => newTargetResolveValue)
    );

    // Here we wait for any promise to resolve first. `waitForFrameLoad` might throw
    // (if the iframe does end up being remote), so we don't want to use `Promise.race`.
    const loadResult = await Promise.any(domLoadingPromises);

    // The Node may have `useChildTargetToFetchChildren` set to false because the
    // child document was still loading when fetching its form. But it may happen that
    // the Node ends up being a remote iframe.
    // When this happen we will try to call `waitForFrameLoad` which will throw, but
    // we will be notified about the new target.
    // This is the special edge case we are trying to handle here.
    // We want WalkerFront.children to consider this as an iframe with a dedicated target.
    if (loadResult == newTargetResolveValue) {
      nodeFront._form.useChildTargetToFetchChildren = true;
    }
  }

  /**
   * Get the full array of selectors from the topmost document, going through
   * iframes.
   * For example, given the following markup:
   *
   * <html>
   *   <body>
   *     <iframe src="...">
   *       <html>
   *         <body>
   *           <h1 id="sub-document-title">Title of sub document</h1>
   *         </body>
   *       </html>
   *     </iframe>
   *   </body>
   * </html>
   *
   * If this function is called with the NodeFront for the h1#sub-document-title element,
   * it will return something like: ["body > iframe", "#sub-document-title"]
   *
   * @param {NodeFront} nodeFront: The nodefront to get the selectors for
   * @returns {Promise<Array<String>>} A promise that resolves with an array of selectors (strings)
   */
  async getNodeFrontSelectorsFromTopDocument(nodeFront) {
    const selectors = [];

    let currentNode = nodeFront;
    while (currentNode) {
      // Get the selector for the node inside its document
      const selector = await currentNode.getUniqueSelector();
      selectors.unshift(selector);

      // Retrieve the node's document/shadowRoot nodeFront so we can get its parent
      // (so if we're in an iframe, we'll get the <iframe> node front, and if we're in a
      // shadow dom document, we'll get the host).
      const rootNode = currentNode.getOwnerRootNodeFront();
      currentNode = rootNode?.parentOrHost();
    }

    return selectors;
  }
}

// This is a fork of the server sort:
// https://searchfox.org/mozilla-central/rev/46a67b8656ac12b5c180e47bc4055f713d73983b/devtools/server/actors/inspector/walker.js#1447
function sortSuggestions(suggestions) {
  const sorted = suggestions.sort((a, b) => {
    // Computed a sortable string with first the inverted count, then the name
    let sortA = 10000 - a[1] + a[0];
    let sortB = 10000 - b[1] + b[0];

    // Prefixing ids, classes and tags, to group results
    const firstA = a[0].substring(0, 1);
    const firstB = b[0].substring(0, 1);

    const getSortKeyPrefix = firstLetter => {
      if (firstLetter === "#") {
        return "2";
      }
      if (firstLetter === ".") {
        return "1";
      }
      return "0";
    };

    sortA = getSortKeyPrefix(firstA) + sortA;
    sortB = getSortKeyPrefix(firstB) + sortB;

    // String compare
    return sortA.localeCompare(sortB);
  });
  return sorted.slice(0, 25);
}

module.exports = InspectorCommand;
