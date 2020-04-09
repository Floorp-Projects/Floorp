/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  browsingContextTargetSpec,
} = require("devtools/shared/specs/targets/browsing-context");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { TargetMixin } = require("devtools/client/fronts/targets/target-mixin");

class BrowsingContextTargetFront extends TargetMixin(
  FrontClassWithSpec(browsingContextTargetSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // Cache the value of some target properties that are being returned by `attach`
    // request and then keep them up-to-date in `reconfigure` request.
    this.configureOptions = {
      javascriptEnabled: null,
    };

    // RootFront.listTabs is going to update this state via `setIsSelected`  method
    this._selected = false;

    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onFrameUpdate = this._onFrameUpdate.bind(this);
  }

  form(json) {
    this.actorID = json.actor;
    this.browsingContextID = json.browsingContextID;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;

    this.outerWindowID = json.outerWindowID;
    this.favicon = json.favicon;
    this._title = json.title;
    this._url = json.url;
  }

  // Reports if the related tab is selected. Only applies to BrowsingContextTarget
  // issued from RootFront.listTabs.
  get selected() {
    return this._selected;
  }

  // This is called by RootFront.listTabs, to update the currently selected tab.
  setIsSelected(selected) {
    this._selected = selected;
  }

  /**
   * Event listener for `frameUpdate` event.
   */
  _onFrameUpdate(packet) {
    this.emit("frame-update", packet);
  }

  /**
   * Event listener for `tabNavigated` event.
   */
  _onTabNavigated(packet) {
    const event = Object.create(null);
    event.url = packet.url;
    event.title = packet.title;
    event.nativeConsoleAPI = packet.nativeConsoleAPI;
    event.isFrameSwitching = packet.isFrameSwitching;

    // Keep the title unmodified when a developer toolbox switches frame
    // for a tab (Bug 1261687), but always update the title when the target
    // is a WebExtension (where the addon name is always included in the title
    // and the url is supposed to be updated every time the selected frame changes).
    if (!packet.isFrameSwitching || this.isWebExtension) {
      this._url = packet.url;
      this._title = packet.title;
    }

    // Send any stored event payload (DOMWindow or nsIRequest) for backwards
    // compatibility with non-remotable tools.
    if (packet.state == "start") {
      this.emit("will-navigate", event);
    } else {
      this.emit("navigate", event);
    }
  }

  async attach() {
    if (this._attach) {
      return this._attach;
    }
    this._attach = (async () => {
      // All Browsing context inherited target emit a few event that are being
      // translated on the target class. Listen for them before attaching as they
      // can start firing on attach call.
      this.on("tabNavigated", this._onTabNavigated);
      this.on("frameUpdate", this._onFrameUpdate);

      const response = await super.attach();

      this.targetForm.threadActor = response.threadActor;
      this.configureOptions.javascriptEnabled = response.javascriptEnabled;
      this.traits = response.traits || {};

      // xpcshell tests from devtools/server/tests/xpcshell/ are implementing
      // fake BrowsingContextTargetActor which do not expose any console actor.
      if (this.targetForm.consoleActor) {
        await this.attachConsole();
      }
    })();
    return this._attach;
  }

  async reconfigure({ options }) {
    const response = await super.reconfigure({ options });

    if (typeof options.javascriptEnabled != "undefined") {
      this.configureOptions.javascriptEnabled = options.javascriptEnabled;
    }

    return response;
  }

  listRemoteFrames() {
    return this.client.mainRoot.listRemoteFrames(this.browsingContextID);
  }

  async detach() {
    try {
      await super.detach();
    } catch (e) {
      this.logDetachError(e, "browsing context");
    }

    // Remove listeners set in attach
    this.off("tabNavigated", this._onTabNavigated);
    this.off("frameUpdate", this._onFrameUpdate);
  }

  destroy() {
    const promise = super.destroy();

    // As detach isn't necessarily called on target's destroy
    // (it isn't for local tabs), ensure removing listeners set in attach.
    this.off("tabNavigated", this._onTabNavigated);
    this.off("frameUpdate", this._onFrameUpdate);

    return promise;
  }
}

exports.BrowsingContextTargetFront = BrowsingContextTargetFront;
registerFront(exports.BrowsingContextTargetFront);
