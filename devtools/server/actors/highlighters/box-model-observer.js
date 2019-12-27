/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerServer } = require("devtools/server/debugger-server");
const { AutoRefreshHighlighter } = require("./auto-refresh");
const {
  getBindingElementAndPseudo,
  hasPseudoClassLock,
  isNodeValid,
} = require("./utils/markup");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
const { getCurrentZoom } = require("devtools/shared/layout/utils");
const {
  getNodeDisplayName,
  getNodeGridFlexType,
} = require("devtools/server/actors/inspector/utils");
const nodeConstants = require("devtools/shared/dom-node-constants");
const { LocalizationHelper } = require("devtools/shared/l10n");
const STRINGS_URI = "devtools/shared/locales/highlighters.properties";
const L10N = new LocalizationHelper(STRINGS_URI);
const {
  BOX_MODEL_REGIONS,
  BoxModelHighlighterRenderer,
} = require("devtools/server/actors/highlighters/box-model-renderer");

/**
 * The BoxModelHighlighterObserver observes the coordinates of a node and communicates
 * with the BoxModelHighlighterRenderer which draws the box model regions on top the a
 * node.
 *
 * When in the context of the content toolbox, the observer lives in
 * the child process (aka content process) and the renderer is set up in the parent
 * process. They communicate via messages.
 *
 * When in the context of the browser toolbox, both observer and renderer live in the
 * parent process. They communicate by direct reference.
 */
class BoxModelHighlighterObserver extends AutoRefreshHighlighter {
  constructor(highlighterEnv, conn) {
    super(highlighterEnv);
    this.conn = conn;
    this._ignoreScroll = true;
    this.typeName = this.constructor.name.replace("Observer", "");

    if (DebuggerServer.isInChildProcess) {
      // eslint-disable-next-line no-restricted-properties
      this.conn.setupInParent({
        module: "devtools/server/actors/highlighters/box-model-renderer",
        setupParent: "setupParentProcess",
      });
    } else {
      this.renderer = new BoxModelHighlighterRenderer();
    }

    /**
     * Optionally customize each region's fill color by adding an entry to the
     * regionFill property: `highlighter.regionFill.margin = "red";
     */
    this.regionFill = {};

    this.onPageHide = this.onPageHide.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);

    this.highlighterEnv.on("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = highlighterEnv;
    pageListenerTarget.addEventListener("pagehide", this.onPageHide);
  }

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy() {
    this.highlighterEnv.off("will-navigate", this.onWillNavigate);

    const { pageListenerTarget } = this.highlighterEnv;
    if (pageListenerTarget) {
      pageListenerTarget.removeEventListener("pagehide", this.onPageHide);
    }

    if (DebuggerServer.isInChildProcess) {
      this.postMessage("destroy");
    } else {
      this.renderer.destroy();
      this.renderer = null;
    }

    AutoRefreshHighlighter.prototype.destroy.call(this);
  }

  get messageManager() {
    if (!DebuggerServer.isInChildProcess) {
      throw new Error(
        "Message manager should only be used when actor is in child process."
      );
    }

    return this.conn.parentMessageManager;
  }

  postMessage(topic, data = {}) {
    this._msgName = `debug:${this.conn.prefix}${this.typeName}`;
    this.messageManager.sendAsyncMessage(this._msgName, { topic, data });
  }

  /**
   * Tell the renderer to update the markup of the box model highlighter.
   *
   * @param {Object} data
   *        Object with data about the node position, type and its attributes.
   *        @see BoxModelHighlighterRenderer.render()
   */
  render(data) {
    if (DebuggerServer.isInChildProcess) {
      this.postMessage("render", data);
    } else {
      this.renderer.render(data);
    }
  }

  /**
   * Override the AutoRefreshHighlighter's _isNodeValid method to also return true for
   * text nodes since these can also be highlighted.
   * @param {DOMNode} node
   * @return {Boolean}
   */
  _isNodeValid(node) {
    return (
      node && (isNodeValid(node) || isNodeValid(node, nodeConstants.TEXT_NODE))
    );
  }

  /**
   * Show the highlighter on a given node
   */
  _show() {
    if (!BOX_MODEL_REGIONS.includes(this.options.region)) {
      this.options.region = "content";
    }

    const shown = this._update();
    this._trackMutations();
    return shown;
  }

  /**
   * Track the current node markup mutations so that the node info bar can be
   * updated to reflects the node's attributes
   */
  _trackMutations() {
    if (isNodeValid(this.currentNode)) {
      const win = this.currentNode.ownerGlobal;
      this.currentNodeObserver = new win.MutationObserver(this.update);
      this.currentNodeObserver.observe(this.currentNode, { attributes: true });
    }
  }

  _untrackMutations() {
    if (isNodeValid(this.currentNode) && this.currentNodeObserver) {
      this.currentNodeObserver.disconnect();
      this.currentNodeObserver = null;
    }
  }

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update() {
    const node = this.currentNode;
    let shown = false;

    if (this._nodeNeedsHighlighting()) {
      // Tell the renderer to update the highlighter markup and provide it
      // with options, metadata and coordinates of the target node.
      const data = {
        ...this.options,
        currentQuads: { ...this.currentQuads },
        regionFill: { ...this.regionFill },
        nodeData: this._getNodeData(),

        showBoxModel: true,
        showInfoBar:
          !this.options.hideInfoBar &&
          (node.nodeType === node.ELEMENT_NODE ||
            node.nodeType === node.TEXT_NODE),
      };
      this.render(data);
      shown = true;
    } else {
      // Nothing to highlight (0px rectangle like a <script> tag for instance)
      this._hide();
    }

    return shown;
  }

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide() {
    this._untrackMutations();

    // Tell the renderer to hide the highlighter markup.
    this.render({
      showBoxModel: false,
      showInfoBar: false,
    });
  }

  /**
   * Can the current node be highlighted? Does it have quads.
   * @return {Boolean}
   */
  _nodeNeedsHighlighting() {
    return (
      this.currentQuads.margin.length ||
      this.currentQuads.border.length ||
      this.currentQuads.padding.length ||
      this.currentQuads.content.length
    );
  }

  /**
   * Get data from the highlighted node to populate the infobar tooltip with
   * information such as the node's id, class names, grid or flex item type, etc.
   *
   * @return {Object|null} Information about the highlighted node
   */
  _getNodeData() {
    if (!this.currentNode) {
      return null;
    }

    const { bindingElement: node, pseudo } = getBindingElementAndPseudo(
      this.currentNode
    );

    // Update the tag, id, classes, pseudo-classes and dimensions
    const displayName = getNodeDisplayName(node);

    const id = node.id ? "#" + node.id : "";

    const classList = (node.classList || []).length
      ? "." + [...node.classList].join(".")
      : "";

    let pseudos = this._getPseudoClasses(node).join("");
    if (pseudo) {
      // Display :after as ::after
      pseudos += ":" + pseudo;
    }

    const zoom = getCurrentZoom(this.win);

    const { grid: gridType, flex: flexType } = getNodeGridFlexType(node);
    const gridLayoutTextType = this._getLayoutTextType("gridType", gridType);
    const flexLayoutTextType = this._getLayoutTextType("flexType", flexType);

    return {
      classList,
      displayName,
      flexLayoutTextType,
      gridLayoutTextType,
      id,
      pseudos,
      zoom,
    };
  }

  _getLayoutTextType(layoutTypeKey, { isContainer, isItem }) {
    if (!isContainer && !isItem) {
      return "";
    }
    if (isContainer && !isItem) {
      return L10N.getStr(`${layoutTypeKey}.container`);
    }
    if (!isContainer && isItem) {
      return L10N.getStr(`${layoutTypeKey}.item`);
    }
    return L10N.getStr(`${layoutTypeKey}.dual`);
  }

  _getPseudoClasses(node) {
    if (node.nodeType !== nodeConstants.ELEMENT_NODE) {
      // hasPseudoClassLock can only be used on Elements.
      return [];
    }

    return PSEUDO_CLASSES.filter(pseudo => hasPseudoClassLock(node, pseudo));
  }

  onPageHide({ target }) {
    // If a pagehide event is triggered for current window's highlighter, hide the
    // highlighter.
    if (target.defaultView === this.win) {
      this.hide();
    }
  }

  onWillNavigate({ isTopLevel }) {
    if (isTopLevel) {
      this.hide();
    }
  }
}

exports.BoxModelHighlighterObserver = BoxModelHighlighterObserver;
