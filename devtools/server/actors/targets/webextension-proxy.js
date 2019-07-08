/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DebuggerServer } = require("devtools/server/main");

loader.lazyImporter(
  this,
  "ExtensionParent",
  "resource://gre/modules/ExtensionParent.jsm"
);

function WebExtensionTargetActorProxy(connection, parentActor) {
  this._conn = connection;
  this._parentActor = parentActor;
  this.addonId = parentActor.addonId;

  this._onChildExit = this._onChildExit.bind(this);

  this._form = null;
  this._browser = null;
  this._childActorID = null;
}

WebExtensionTargetActorProxy.prototype = {
  /**
   * Connect the webextension child actor.
   */
  async connect() {
    if (this._browser) {
      throw new Error(
        "This actor is already connected to the extension process"
      );
    }

    // Called when the debug browser element has been destroyed
    // (no actor is using it anymore to connect the child extension process).
    const onDestroy = this.destroy.bind(this);

    this._browser = await ExtensionParent.DebugUtils.getExtensionProcessBrowser(
      this
    );

    this._form = await DebuggerServer.connectToFrame(
      this._conn,
      this._browser,
      onDestroy,
      { addonId: this.addonId }
    );

    this._childActorID = this._form.actor;

    // Exit the proxy child actor if the child actor has been destroyed.
    this._mm.addMessageListener("debug:webext_child_exit", this._onChildExit);

    return this._form;
  },

  get isOOP() {
    return this._browser ? this._browser.isRemoteBrowser : undefined;
  },

  get _mm() {
    return (
      this._browser &&
      (this._browser.messageManager || this._browser.frameLoader.messageManager)
    );
  },

  destroy() {
    if (this._mm) {
      this._mm.removeMessageListener(
        "debug:webext_child_exit",
        this._onChildExit
      );

      this._mm.sendAsyncMessage("debug:webext_parent_exit", {
        actor: this._childActorID,
      });

      ExtensionParent.DebugUtils.releaseExtensionProcessBrowser(this);
    }

    if (this._parentActor) {
      this._parentActor.onProxyDestroy();
    }

    this._parentActor = null;
    this._browser = null;
    this._childActorID = null;
    this._form = null;
  },

  /**
   * Handle the child actor exit.
   */
  _onChildExit(msg) {
    if (msg.json.actor !== this._childActorID) {
      return;
    }

    this.destroy();
  },
};

exports.WebExtensionTargetActorProxy = WebExtensionTargetActorProxy;
