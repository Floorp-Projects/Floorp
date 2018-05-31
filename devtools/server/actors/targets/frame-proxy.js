/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Target actor proxy that represents a frame / docShell in the parent process. It
 * launches a FrameTargetActor in the content process to do the real work and tunnels the
 * data.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const { DebuggerServer } = require("devtools/server/main");
loader.lazyImporter(this, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");

/**
 * Creates a target actor proxy for handling requests to a single browser frame.
 * Both <xul:browser> and <iframe mozbrowser> are supported.
 * This actor is a shim that connects to a FrameTargetActor in a remote browser process.
 * All RDP packets get forwarded using the message manager.
 *
 * @param connection The main RDP connection.
 * @param browser <xul:browser> or <iframe mozbrowser> element to connect to.
 * @param options
 *        - {Boolean} favicons: true if the form should include the favicon for the tab.
 */
function FrameTargetActorProxy(connection, browser, options = {}) {
  this._conn = connection;
  this._browser = browser;
  this._form = null;
  this.exited = false;
  this.options = options;
}

FrameTargetActorProxy.prototype = {
  async connect() {
    const onDestroy = () => {
      if (this._deferredUpdate) {
        // Reject the update promise if the tab was destroyed while requesting an update
        this._deferredUpdate.reject({
          error: "tabDestroyed",
          message: "Tab destroyed while performing a FrameTargetActorProxy update"
        });
      }
      this.exit();
    };
    const connect = DebuggerServer.connectToFrame(this._conn, this._browser, onDestroy);
    const form = await connect;

    this._form = form;
    if (this.options.favicons) {
      this._form.favicon = await this.getFaviconData();
    }

    return this;
  },

  get _tabbrowser() {
    if (this._browser && typeof this._browser.getTabBrowser == "function") {
      return this._browser.getTabBrowser();
    }
    return null;
  },

  get _mm() {
    // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
    // or else fallback to asking the frameLoader itself.
    return this._browser.messageManager ||
           this._browser.frameLoader.messageManager;
  },

  async getFaviconData() {
    try {
      const { data } = await PlacesUtils.promiseFaviconData(this._form.url);
      return data;
    } catch (e) {
      // Favicon unavailable for this url.
      return null;
    }
  },

  /**
   * @param {Object} options
   *        See FrameTargetActorProxy constructor.
   */
  async update(options = {}) {
    // Update the FrameTargetActorProxy options.
    this.options = options;

    // If the child happens to be crashed/close/detach, it won't have _form set,
    // so only request form update if some code is still listening on the other
    // side.
    if (this.exited) {
      return this.connect();
    }

    const form = await new Promise(resolve => {
      const onFormUpdate = msg => {
        // There may be more than one FrameTargetActor up and running
        if (this._form.actor != msg.json.actor) {
          return;
        }
        this._mm.removeMessageListener("debug:form", onFormUpdate);

        resolve(msg.json);
      };

      this._mm.addMessageListener("debug:form", onFormUpdate);
      this._mm.sendAsyncMessage("debug:form");
    });

    this._form = form;
    if (this.options.favicons) {
      this._form.favicon = await this.getFaviconData();
    }

    return this;
  },

  /**
   * If we don't have a title from the content side because it's a zombie tab, try to find
   * it on the chrome side.
   */
  get title() {
    // On Fennec, we can check the session store data for zombie tabs
    if (this._browser && this._browser.__SS_restore) {
      const sessionStore = this._browser.__SS_data;
      // Get the last selected entry
      const entry = sessionStore.entries[sessionStore.index - 1];
      return entry.title;
    }
    // If contentTitle is empty (e.g. on a not-yet-restored tab), but there is a
    // tabbrowser (i.e. desktop Firefox, but not Fennec), we can use the label
    // as the title.
    if (this._tabbrowser) {
      const tab = this._tabbrowser.getTabForBrowser(this._browser);
      if (tab) {
        return tab.label;
      }
    }
    return "";
  },

  /**
   * If we don't have a url from the content side because it's a zombie tab, try to find
   * it on the chrome side.
   */
  get url() {
    // On Fennec, we can check the session store data for zombie tabs
    if (this._browser && this._browser.__SS_restore) {
      const sessionStore = this._browser.__SS_data;
      // Get the last selected entry
      const entry = sessionStore.entries[sessionStore.index - 1];
      return entry.url;
    }
    return null;
  },

  form() {
    const form = Object.assign({}, this._form);
    // In some cases, the title and url fields might be empty.  Zombie tabs (not yet
    // restored) are a good example.  In such cases, try to look up values for these
    // fields using other data in the parent process.
    if (!form.title) {
      form.title = this.title;
    }
    if (!form.url) {
      form.url = this.url;
    }

    return form;
  },

  exit() {
    this._browser = null;
    this._form = null;
    this.exited = true;
  },
};

exports.FrameTargetActorProxy = FrameTargetActorProxy;
