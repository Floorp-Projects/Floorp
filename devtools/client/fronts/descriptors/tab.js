/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");

loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/client/fronts/targets/browsing-context",
  true
);
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  DescriptorMixin,
} = require("devtools/client/fronts/descriptors/descriptor-mixin");

/**
 * DescriptorFront for tab targets.
 *
 * @fires remoteness-change
 *        Fired only for target switching, when the debugged tab is a local tab.
 *        TODO: This event could move to the server in order to support
 *        remoteness change for remote debugging.
 */
class TabDescriptorFront extends DescriptorMixin(
  FrontClassWithSpec(tabDescriptorSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // The tab descriptor can be configured to create either local tab targets
    // (eg, regular tab toolbox) or browsing context targets (eg tab remote
    // debugging).
    this._localTab = null;

    // Flipped when creating dedicated tab targets for DevTools WebExtensions.
    // See toolkit/components/extensions/ExtensionParent.jsm .
    this.isDevToolsExtensionContext = false;

    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
    this._handleTabEvent = this._handleTabEvent.bind(this);
  }

  form(json) {
    this.actorID = json.actor;
    this._form = json;
    this.traits = json.traits || {};
  }

  destroy() {
    if (this.isLocalTab) {
      this._teardownLocalTabListeners();
    }
    super.destroy();
  }

  setLocalTab(localTab) {
    this._localTab = localTab;
    this._setupLocalTabListeners();
  }

  get isLocalTab() {
    return !!this._localTab;
  }

  get localTab() {
    return this._localTab;
  }

  _setupLocalTabListeners() {
    this.localTab.addEventListener("TabClose", this._handleTabEvent);
    this.localTab.addEventListener("TabRemotenessChange", this._handleTabEvent);
  }

  _teardownLocalTabListeners() {
    this.localTab.removeEventListener("TabClose", this._handleTabEvent);
    this.localTab.removeEventListener(
      "TabRemotenessChange",
      this._handleTabEvent
    );
  }

  get isZombieTab() {
    return this._form.isZombieTab;
  }

  get outerWindowID() {
    return this._form.outerWindowID;
  }

  get selected() {
    return this._form.selected;
  }

  get title() {
    return this._form.title;
  }

  get url() {
    return this._form.url;
  }

  get favicon() {
    // Note: the favicon is not part of the default form() payload, it will be
    // added in `retrieveFavicon`.
    return this._form.favicon;
  }

  _createTabTarget(form) {
    const front = new BrowsingContextTargetFront(this._client, null, this);

    if (this.isLocalTab) {
      front.shouldCloseClient = true;
    }

    // As these fronts aren't instantiated by protocol.js, we have to set their actor ID
    // manually like that:
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
    front.on("target-destroyed", this._onTargetDestroyed);
    return front;
  }

  _onTargetDestroyed() {
    // Clear the cached targetFront when the target is destroyed.
    // Note that we are also checking that _targetFront has a valid actorID
    // in getTarget, this acts as an additional security to avoid races.
    this._targetFront = null;
  }

  /**
   * Safely retrieves the favicon via getFavicon() and populates this._form.favicon.
   *
   * We could let callers explicitly retrieve the favicon instead of inserting it in the
   * form dynamically.
   */
  async retrieveFavicon() {
    try {
      this._form.favicon = await this.getFavicon();
    } catch (e) {
      // We might request the data for a tab which is going to be destroyed.
      // In this case the TargetFront will be destroyed. Otherwise log an error.
      if (!this.isDestroyed()) {
        console.error("Failed to retrieve the favicon for " + this.url, e);
      }
    }
  }

  async getTarget() {
    if (this._targetFront && !this._targetFront.isDestroyed()) {
      return this._targetFront;
    }

    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }

    this._targetFrontPromise = (async () => {
      let targetFront = null;
      try {
        const targetForm = await super.getTarget();
        targetFront = this._createTabTarget(targetForm);
        await targetFront.attach();
      } catch (e) {
        console.log(
          `Request to connect to TabDescriptor "${this.id}" failed: ${e}`
        );
      }
      this._targetFront = targetFront;
      this._targetFrontPromise = null;
      return targetFront;
    })();
    return this._targetFrontPromise;
  }

  /**
   * Handle tabs events.
   */
  async _handleTabEvent(event) {
    switch (event.type) {
      case "TabClose":
        // Always destroy the toolbox opened for this local tab target.
        // Toolboxes are no longer destroyed on target destruction.
        // When the toolbox is in a Window Host, it won't be removed from the
        // DOM when the tab is closed.
        const toolbox = gDevTools.getToolbox(this._targetFront);
        // A few tests are using TabTargetFactory.forTab, but aren't spawning any
        // toolbox. In this case, the toobox won't destroy the target, so we
        // do it from here. But ultimately, the target should destroy itself
        // from the actor side anyway.
        if (toolbox) {
          // Toolbox.destroy will call target.destroy eventually.
          await toolbox.destroy();
        }
        break;
      case "TabRemotenessChange":
        this._onRemotenessChange();
        break;
    }
  }

  /**
   * Automatically respawn the toolbox when the tab changes between being
   * loaded within the parent process and loaded from a content process.
   * Process change can go in both ways.
   */
  async _onRemotenessChange() {
    // The front that was created for DevTools page extension does not have corresponding toolbox.
    if (this.isDevToolsExtensionContext) {
      return;
    }

    // In a near future, this client side code should be replaced by actor code,
    // notifying about new tab targets.
    this.emit("remoteness-change", this._targetFront);
  }
}

exports.TabDescriptorFront = TabDescriptorFront;
registerFront(TabDescriptorFront);
