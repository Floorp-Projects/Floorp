/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const Telemetry = require("devtools/client/shared/telemetry");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol.js");
const { inspectorSpec } = require("devtools/shared/specs/inspector");

const TELEMETRY_EYEDROPPER_OPENED = "DEVTOOLS_EYEDROPPER_OPENED_COUNT";
const TELEMETRY_EYEDROPPER_OPENED_MENU =
  "DEVTOOLS_MENU_EYEDROPPER_OPENED_COUNT";
const SHOW_ALL_ANONYMOUS_CONTENT_PREF =
  "devtools.inspector.showAllAnonymousContent";

const telemetry = new Telemetry();

/**
 * Client side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
class InspectorFront extends FrontClassWithSpec(inspectorSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this._client = client;
    this._highlighters = new Map();

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "inspectorActor";

    // Map of highlighter types to unsettled promises to create a highlighter of that type
    this._pendingGetHighlighterMap = new Map();
  }

  // async initialization
  async initialize() {
    if (this.initialized) {
      return this.initialized;
    }

    this.initialized = await Promise.all([
      this._getWalker(),
      this._getPageStyle(),
    ]);

    return this.initialized;
  }

  async _getWalker() {
    const showAllAnonymousContent = Services.prefs.getBoolPref(
      SHOW_ALL_ANONYMOUS_CONTENT_PREF
    );
    this.walker = await this.getWalker({
      showAllAnonymousContent,
      // Backward compatibility for Firefox 74 or older.
      // getWalker() now uses a single boolean flag to drive the display of
      // both anonymous content and user-agent shadow roots.
      // Older servers used separate flags. See Bug 1613773.
      showUserAgentShadowRoots: showAllAnonymousContent,
    });

    // We need to reparent the RootNode of remote iframe Walkers
    // so that their parent is the NodeFront of the <iframe>
    // element, coming from another process/target/WalkerFront.
    await this.walker.reparentRemoteFrame();
  }

  hasHighlighter(type) {
    return this._highlighters.has(type);
  }

  async _getPageStyle() {
    this.pageStyle = await super.getPageStyle();
  }

  async getCompatibilityFront() {
    if (!this._compatibility) {
      this._compatibility = await super.getCompatibility();
    }

    return this._compatibility;
  }

  destroy() {
    this._compatibility = null;
    // CustomHighlighter fronts are managed by InspectorFront and so will be
    // automatically destroyed. But we have to clear the `_highlighters`
    // Map as well as explicitly call `finalize` request on all of them.
    this.destroyHighlighters();
    super.destroy();
  }

  destroyHighlighters() {
    for (const type of this._highlighters.keys()) {
      if (this._highlighters.has(type)) {
        const highlighter = this._highlighters.get(type);
        if (!highlighter.isDestroyed()) {
          highlighter.finalize();
        }
        this._highlighters.delete(type);
      }
    }
  }

  async getHighlighterByType(typeName) {
    let highlighter = null;
    try {
      highlighter = await super.getHighlighterByType(typeName);
    } catch (_) {
      throw new Error(
        "The target doesn't support " +
          `creating highlighters by types or ${typeName} is unknown`
      );
    }
    return highlighter;
  }

  getKnownHighlighter(type) {
    return this._highlighters.get(type);
  }

  /**
   * Return a highlighter instance of the given type.
   * If an instance was previously created, return it. Else, create and return a new one.
   *
   * Store a promise for the request to create a new highlighter. If another request
   * comes in before that promise is resolved, wait for it to resolve and return the
   * highlighter instance it resolved with instead of creating a new request.
   *
   * @param  {String} type
   *         Highlighter type
   * @return {Promise}
   *         Promise which resolves with a highlighter instance of the given type
   */
  async getOrCreateHighlighterByType(type) {
    let front = this._highlighters.get(type);
    let pendingGetHighlighter = this._pendingGetHighlighterMap.get(type);

    if (!front && !pendingGetHighlighter) {
      pendingGetHighlighter = (async () => {
        const highlighter = await this.getHighlighterByType(type);
        this._highlighters.set(type, highlighter);
        return highlighter;
      })();

      this._pendingGetHighlighterMap.set(type, pendingGetHighlighter);
    }

    if (pendingGetHighlighter) {
      front = await pendingGetHighlighter;
      this._pendingGetHighlighterMap.delete(type);
    }

    return front;
  }

  async pickColorFromPage(options) {
    await super.pickColorFromPage(options);
    if (options?.fromMenu) {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED_MENU).add(true);
    } else {
      telemetry.getHistogramById(TELEMETRY_EYEDROPPER_OPENED).add(true);
    }
  }

  /**
   * Given a node grip, return a NodeFront on the right context.
   *
   * @param {Object} grip: The node grip.
   * @returns {Promise<NodeFront|null>} A promise that resolves with  a NodeFront or null
   *                                    if the NodeFront couldn't be created/retrieved.
   */
  async getNodeFrontFromNodeGrip(grip) {
    const gripHasContentDomReference = "contentDomReference" in grip;

    if (!gripHasContentDomReference) {
      // Backward compatibility ( < Firefox 71):
      // If the grip does not have a contentDomReference, we can't know in which browsing
      // context id the node lives. We fall back on gripToNodeFront that might retrieve
      // the expected nodeFront.
      return this.walker.gripToNodeFront(grip);
    }

    return this.getNodeActorFromContentDomReference(grip.contentDomReference);
  }

  async getNodeActorFromContentDomReference(contentDomReference) {
    const { browsingContextId } = contentDomReference;
    // If the contentDomReference lives in the same browsing context id than the
    // current one, we can directly use the current walker.
    if (this.targetFront.browsingContextID === browsingContextId) {
      return this.walker.getNodeActorFromContentDomReference(
        contentDomReference
      );
    }

    // If the contentDomReference has a different browsing context than the current one,
    // we are either in Fission or in the Multiprocess Browser Toolbox, so we need to
    // retrieve the walker of the BrowsingContextTarget.
    // Get the target for this remote frame element
    const { descriptorFront } = this.targetFront;

    // Tab and Process Descriptors expose a Watcher, which should be used to
    // fetch the node's target.
    let target;
    if (descriptorFront && descriptorFront.traits.watcher) {
      const watcher = await descriptorFront.getWatcher();
      target = await watcher.getBrowsingContextTarget(browsingContextId);
    } else {
      // For descriptors which don't expose a watcher (e.g. WebExtension)
      // we used to call RootActor::getBrowsingContextDescriptor, but it was
      // removed in FF77.
      // Support for watcher in WebExtension descriptors is Bug 1644341.
      throw new Error(
        `Unable to call getNodeActorFromContentDomReference for ${this.targetFront.actorID}`
      );
    }
    const { walker } = await target.getFront("inspector");
    return walker.getNodeActorFromContentDomReference(contentDomReference);
  }
}

exports.InspectorFront = InspectorFront;
registerFront(InspectorFront);
