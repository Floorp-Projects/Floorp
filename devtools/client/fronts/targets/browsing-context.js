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

    // For targets which support the Watcher and configuration actor, the status
    // for the `javascriptEnabled` setting will be available on the configuration
    // front, and the target will only be used to read the initial value.
    // For other targets, _javascriptEnabled will be updated everytime
    // `reconfigure` is called.
    // Note: this property is marked as private but is accessed by the
    // TargetCommand to provide the "isJavascriptEnabled" wrapper. It should NOT be
    // used anywhere else.
    this._javascriptEnabled = null;

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

  /**
   * Event listener for `frameUpdate` event.
   */
  _onFrameUpdate(packet) {
    this.emit("frame-update", packet);
  }

  async reload({ options } = {}) {
    try {
      await super.reload({ options });
    } catch (e) {
      dump(" reload exception: " + e + " >>> " + e.message + " <<<\n");
      // If the target follows the window global lifecycle, the reload request
      // will fail, and we should swallow the error. Re-throw it otherwise.
      // @backward-compat { version 88 } The trait check can be removed after
      // version 88 hits the release channel.
      const shouldSwallowReloadError =
        this.getTrait("supportsFollowWindowGlobalLifeCycleFlag") &&
        this.targetForm.followWindowGlobalLifeCycle;

      if (!shouldSwallowReloadError) {
        throw e;
      }
    }
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
      this._javascriptEnabled = response.javascriptEnabled;
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
      this._javascriptEnabled = options.javascriptEnabled;
    }

    return response;
  }

  async detach() {
    // When calling this.destroy() at the end of this method,
    // we will end up calling detach again from TargetMixin.destroy.
    // Avoid invalid loops and do not try to resolve only once the previous call to detach
    // is done as it would do async infinite loop that never resolves.
    if (this._isDetaching) {
      return;
    }
    this._isDetaching = true;

    // Remove listeners set in attach
    this.off("tabNavigated", this._onTabNavigated);
    this.off("frameUpdate", this._onFrameUpdate);

    try {
      await super.detach();
    } catch (e) {
      this.logDetachError(e, "browsing context");
    }

    // Detach will destroy the target actor, but the server won't emit any
    // target-destroyed-form in such manual, client side destruction.
    // So that we have to manually destroy the associated front on the client
    //
    // If detach was called by TargetFrontMixin.destroy, avoid recalling it from it
    // as it would do an async infinite loop which would never resolve.
    if (!this.isDestroyedOrBeingDestroyed()) {
      this.destroy();
    }
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
