/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");
const DESCRIPTOR_TYPES = require("devtools/client/fronts/descriptors/descriptor-types");

loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "WindowGlobalTargetFront",
  "devtools/client/fronts/targets/window-global",
  true
);
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  DescriptorMixin,
} = require("devtools/client/fronts/descriptors/descriptor-mixin");

const SERVER_TARGET_SWITCHING_ENABLED_PREF =
  "devtools.target-switching.server.enabled";
const POPUP_DEBUG_PREF = "devtools.popups.debug";

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

    // Flag to prevent the server from trying to spawn targets by the watcher actor.
    this._disableTargetSwitching = false;

    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
    this._handleTabEvent = this._handleTabEvent.bind(this);

    // When the target is created from the server side,
    // it is not created via TabDescriptor.getTarget.
    // Instead, it is retrieved by the TargetCommand which
    // will call TabDescriptor.setTarget from TargetCommand.onTargetAvailable
    if (this.isServerTargetSwitchingEnabled()) {
      this._targetFrontPromise = new Promise(
        r => (this._resolveTargetFrontPromise = r)
      );
    }
  }

  descriptorType = DESCRIPTOR_TYPES.TAB;

  form(json) {
    this.actorID = json.actor;
    this._form = json;
    this.traits = json.traits || {};
  }

  /**
   * Destroy the front.
   *
   * @param Boolean If true, it means that we destroy the front when receiving the descriptor-destroyed
   *                event from the server.
   */
  destroy({ isServerDestroyEvent = false } = {}) {
    if (this.isDestroyed()) {
      return;
    }

    // The descriptor may be destroyed first by the frontend.
    // When closing the tab, the toolbox document is almost immediately removed from the DOM.
    // The `unload` event fires and toolbox destroys itself, as well as its related client.
    //
    // In such case, we emit the descriptor-destroyed event
    if (!isServerDestroyEvent) {
      this.emit("descriptor-destroyed");
    }
    if (this.isLocalTab) {
      this._teardownLocalTabListeners();
    }
    super.destroy();
  }

  getWatcher() {
    const isPopupDebuggingEnabled = Services.prefs.getBoolPref(
      POPUP_DEBUG_PREF,
      false
    );
    return super.getWatcher({
      isServerTargetSwitchingEnabled: this.isServerTargetSwitchingEnabled(),
      isPopupDebuggingEnabled,
    });
  }

  setLocalTab(localTab) {
    this._localTab = localTab;
    this._setupLocalTabListeners();

    // This is pure legacy. We always assumed closing the DevToolsClient
    // when the tab was closed. It is mostly important for tests,
    // but also ensure cleaning up the client and everything on tab closing.
    // (this flag is handled by DescriptorMixin)
    this.shouldCloseClient = true;
  }

  get isTabDescriptor() {
    return true;
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

  isServerTargetSwitchingEnabled() {
    const isEnabled = Services.prefs.getBoolPref(
      SERVER_TARGET_SWITCHING_ENABLED_PREF,
      false
    );
    const enabled = isEnabled && !this._disableTargetSwitching;
    return enabled;
  }

  /**
   * Called by CommandsFactory, when the WebExtension codebase instantiates
   * a commands. We have to flag the TabDescriptor for them as they don't support
   * target switching and gets severely broken when enabling server target which
   * introduce target switching for all navigations and reloads
   */
  setIsForWebExtension() {
    this.disableTargetSwitching();
  }

  /**
   * Method used by the WebExtension which still need to disable server side targets,
   * and also a few xpcshell tests which are using legacy API and don't support watcher actor.
   */
  disableTargetSwitching() {
    this._disableTargetSwitching = true;
    // Delete these two attributes which have to be set early from the constructor,
    // but we don't know yet if target switch should be disabled.
    delete this._targetFrontPromise;
    delete this._resolveTargetFrontPromise;
  }

  get isZombieTab() {
    return this._form.isZombieTab;
  }

  get browserId() {
    return this._form.browserId;
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
    const front = new WindowGlobalTargetFront(this._client, null, this);

    // As these fronts aren't instantiated by protocol.js, we have to set their actor ID
    // manually like that:
    front.actorID = form.actor;
    front.form(form);
    this.manage(front);
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

  /**
   * Top-level targets created on the server will not be created and managed
   * by a descriptor front. Instead they are created by the Watcher actor.
   * On the client side we manually re-establish a link between the descriptor
   * and the new top-level target.
   */
  setTarget(targetFront) {
    // Completely ignore the previous target.
    // We might nullify the _targetFront unexpectely due to previous target
    // being destroyed after the new is created
    if (this._targetFront) {
      this._targetFront.off("target-destroyed", this._onTargetDestroyed);
    }
    this._targetFront = targetFront;
    targetFront.setDescriptor(this);

    targetFront.on("target-destroyed", this._onTargetDestroyed);

    if (this.isServerTargetSwitchingEnabled()) {
      this._resolveTargetFrontPromise(targetFront);

      // Set a new promise in order to:
      // 1) Avoid leaking the targetFront we just resolved into the previous promise.
      // 2) Never return an empty target from `getTarget`
      //
      // About the second point:
      // There is a race condition where we call `onTargetDestroyed` (which clears `this.targetFront`)
      // a bit before calling `setTarget`. So that `this.targetFront` could be null,
      // while we now a new target will eventually come when calling `setTarget`.
      // Setting a new promise will help wait for the next target while `_targetFront` is null.
      // Note that `getTarget` first look into `_targetFront` before checking for `_targetFrontPromise`.
      this._targetFrontPromise = new Promise(
        r => (this._resolveTargetFrontPromise = r)
      );
    }
  }
  getCachedTarget() {
    return this._targetFront;
  }
  async getTarget() {
    if (this._targetFront && !this._targetFront.isDestroyed()) {
      return this._targetFront;
    }

    if (this._targetFrontPromise) {
      return this._targetFrontPromise;
    }

    this._targetFrontPromise = (async () => {
      let newTargetFront = null;
      try {
        const targetForm = await super.getTarget();
        newTargetFront = this._createTabTarget(targetForm);
        this.setTarget(newTargetFront);
      } catch (e) {
        console.log(
          `Request to connect to TabDescriptor "${this.id}" failed: ${e}`
        );
      }

      this._targetFrontPromise = null;
      return newTargetFront;
    })();
    return this._targetFrontPromise;
  }

  /**
   * Handle tabs events.
   */
  async _handleTabEvent(event) {
    switch (event.type) {
      case "TabClose":
        // Always destroy the toolbox opened for this local tab descriptor.
        // When the toolbox is in a Window Host, it won't be removed from the
        // DOM when the tab is closed.
        const toolbox = gDevTools.getToolboxForDescriptor(this);
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
    // In a near future, this client side code should be replaced by actor code,
    // notifying about new tab targets.
    this.emit("remoteness-change", this._targetFront);
  }
}

exports.TabDescriptorFront = TabDescriptorFront;
registerFront(TabDescriptorFront);
