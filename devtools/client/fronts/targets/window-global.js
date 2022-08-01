/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  windowGlobalTargetSpec,
} = require("devtools/shared/specs/targets/window-global");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const { TargetMixin } = require("devtools/client/fronts/targets/target-mixin");

class WindowGlobalTargetFront extends TargetMixin(
  FrontClassWithSpec(windowGlobalTargetSpec)
) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    // For targets which support the Watcher and configuration actor, the status
    // for the `javascriptEnabled` setting will be available on the configuration
    // front, and the target will only be used to read the initial value from older
    // servers.
    // Note: this property is marked as private but is accessed by the
    // TargetCommand to provide the "isJavascriptEnabled" wrapper. It should NOT be
    // used anywhere else.
    this._javascriptEnabled = null;

    // If this target was retrieved via NodeFront connectToFrame, keep a
    // reference to the parent NodeFront.
    this._parentNodeFront = null;

    this._onTabNavigated = this._onTabNavigated.bind(this);
    this._onFrameUpdate = this._onFrameUpdate.bind(this);

    this.on("tabNavigated", this._onTabNavigated);
    this.on("frameUpdate", this._onFrameUpdate);
  }

  form(json) {
    this.actorID = json.actor;
    this.browsingContextID = json.browsingContextID;
    this.innerWindowId = json.innerWindowId;
    this.processID = json.processID;

    // Save the full form for Target class usage.
    // Do not use `form` name to avoid colliding with protocol.js's `form` method
    this.targetForm = json;

    this.outerWindowID = json.outerWindowID;
    this.favicon = json.favicon;

    // Initial value for the page title and url. Since the WindowGlobalTargetActor can
    // be created very early, those might not represent the actual value we'd want to
    // display for the user (e.g. the <title> might not have been parsed yet, and the
    // url could still be about:blank, which is what the platform uses at the very start
    // of a navigation to a new location).
    // Those values are set again  from the targetCommand when receiving DOCUMENT_EVENT
    // resource, at which point both values should be in their expected form.
    this.setTitle(json.title);
    this.setUrl(json.url);
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
    event.isFrameSwitching = packet.isFrameSwitching;

    // Keep the title unmodified when a developer toolbox switches frame
    // for a tab (Bug 1261687), but always update the title when the target
    // is a WebExtension (where the addon name is always included in the title
    // and the url is supposed to be updated every time the selected frame changes).
    if (!packet.isFrameSwitching || this.isWebExtension) {
      this.setTitle(packet.title);
      this.setUrl(packet.url);
    }

    // Send any stored event payload (DOMWindow or nsIRequest) for backwards
    // compatibility with non-remotable tools.
    if (packet.state == "start") {
      this.emit("will-navigate", event);
    } else {
      this.emit("navigate", event);
    }
  }

  getParentNodeFront() {
    return this._parentNodeFront;
  }

  setParentNodeFront(nodeFront) {
    this._parentNodeFront = nodeFront;
  }

  /**
   * Set the targetFront url.
   *
   * @param {string} url
   */
  setUrl(url) {
    this._url = url;
  }

  /**
   * Set the targetFront title.
   *
   * @param {string} title
   */
  setTitle(title) {
    this._title = title;
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

    // Remove listeners set in constructor
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
    this._parentNodeFront = null;

    // As detach isn't necessarily called on target's destroy
    // (it isn't for local tabs), ensure removing listeners set in constructor.
    this.off("tabNavigated", this._onTabNavigated);
    this.off("frameUpdate", this._onFrameUpdate);

    return promise;
  }
}

exports.WindowGlobalTargetFront = WindowGlobalTargetFront;
registerFront(exports.WindowGlobalTargetFront);
