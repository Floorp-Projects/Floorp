/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

loader.lazyRequireGetter(
  this,
  "gDevTools",
  "devtools/client/framework/devtools",
  true
);
loader.lazyRequireGetter(
  this,
  "TargetFactory",
  "devtools/client/framework/target",
  true
);
loader.lazyRequireGetter(
  this,
  "BrowsingContextTargetFront",
  "devtools/shared/fronts/targets/browsing-context",
  true
);

// This represent a Target front for a local tab and includes specialized
// implementation in order to:
// * distinguish local tabs from remote (see target.isLocalTab)
// * being able to hookup into Firefox UI (see Hosts and event listeners of
//   this class)
class LocalTabTargetFront extends BrowsingContextTargetFront {
  constructor(client, tab) {
    super(client);

    this._teardownTabListeners = this._teardownTabListeners.bind(this);
    this._handleTabEvent = this._handleTabEvent.bind(this);

    this._tab = tab;
    this._setupTabListeners();

    this.once("close", this._teardownTabListeners);
  }

  get isLocalTab() {
    return true;
  }
  get tab() {
    return this._tab;
  }
  get contentPrincipal() {
    return this.tab.linkedBrowser.contentPrincipal;
  }
  get csp() {
    return this.tab.linkedBrowser.csp;
  }
  toString() {
    return `Target:${this.tab}`;
  }

  /**
   * Listen to the different events.
   */
  _setupTabListeners() {
    this.tab.addEventListener("TabClose", this._handleTabEvent);
    this.tab.ownerDocument.defaultView.addEventListener(
      "unload",
      this._handleTabEvent
    );
    this.tab.addEventListener("TabRemotenessChange", this._handleTabEvent);
  }

  /**
   * Teardown event listeners.
   */
  _teardownTabListeners() {
    if (this.tab.ownerDocument.defaultView) {
      this.tab.ownerDocument.defaultView.removeEventListener(
        "unload",
        this._handleTabEvent
      );
    }
    this.tab.removeEventListener("TabClose", this._handleTabEvent);
    this.tab.removeEventListener("TabRemotenessChange", this._handleTabEvent);
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
        const toolbox = gDevTools.getToolbox(this);
        // A few tests are using TargetFactory.forTab, but aren't spawning any
        // toolbox. In this case, the toobox won't destroy the target, so we
        // do it from here. But ultimately, the target should destroy itself
        // from the actor side anyway.
        if (toolbox) {
          // Toolbox.destroy will call target.destroy eventually.
          await toolbox.destroy();
        }
        break;
      case "unload":
        this.destroy();
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
    // Responsive design does a crazy dance around tabs and triggers
    // remotenesschange events. But we should ignore them as at the end
    // the content doesn't change its remoteness.
    if (this.tab.isResponsiveDesignMode) {
      return;
    }

    const toolbox = gDevTools.getToolbox(this);

    // Force destroying the toolbox as the target will be destroyed,
    // but not the toolbox.
    await toolbox.destroy();

    // Recreate a fresh target instance as the current one is now destroyed
    const newTarget = await TargetFactory.forTab(this.tab);
    gDevTools.showToolbox(newTarget);
  }
}
exports.LocalTabTargetFront = LocalTabTargetFront;
