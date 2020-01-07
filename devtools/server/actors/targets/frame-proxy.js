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

const {
  connectToFrame,
} = require("devtools/server/connectors/frame-connector");
const protocol = require("devtools/shared/protocol");

loader.lazyImporter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

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
// As this proxy is returned by RootActor.listTabs, it has to be inheriting from Actor
// class in order to be correctly marshalled by protocol.js.
// This is related to this very particular check in protocol.js:
// https://searchfox.org/mozilla-central/rev/b243debf6235b050b42fd2eb615fdc729636ca6b/devtools/shared/protocol/types.js#354-367
// But this isn't a real Actor. Only `form()` method is being called when protocol.js
// marshall the proxy. We also register the proxy into Pools and so `typeName`
// as well as `actorID` attributes are being used in this process.
const proxySpec = protocol.generateActorSpec({
  typeName: "frameTargetProxy",
  methods: {},
  events: {},
});
exports.FrameTargetActorProxy = protocol.ActorClassWithSpec(proxySpec, {
  initialize: function(conn, browser, options = {}) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this._conn = conn;
    this._browser = browser;
    this._form = null;
    this.exited = false;
    this.options = options;
  },

  async connect() {
    const onDestroy = () => {
      if (this._deferredUpdate) {
        // Reject the update promise if the tab was destroyed while requesting an update
        this._deferredUpdate.reject({
          error: "tabDestroyed",
          message:
            "Tab destroyed while performing a FrameTargetActorProxy update",
        });
      }
      this.exit();
    };

    await this._unzombifyIfNeeded();

    const connect = connectToFrame(this._conn, this._browser, onDestroy);
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
    return (
      this._browser.messageManager || this._browser.frameLoader.messageManager
    );
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

    // This function may be called if we are inspecting tabs and the actor proxy
    // has already been generated. In that case we need to unzombify tabs.
    // If we are not inspecting tabs then this will be a no-op.
    await this._unzombifyIfNeeded();

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

  _isZombieTab() {
    // Check for Firefox on Android.
    if (this._browser.hasAttribute("pending")) {
      return true;
    }

    // Check for other.
    const tabbrowser = this._tabbrowser;
    const tab = tabbrowser ? tabbrowser.getTabForBrowser(this._browser) : null;
    return tab && tab.hasAttribute && tab.hasAttribute("pending");
  },

  /**
   * If we don't have a title from the content side because it's a zombie tab, try to find
   * it on the chrome side.
   */
  _getZombieTabTitle() {
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

    return null;
  },

  /**
   * If we don't have a url from the content side because it's a zombie tab, try to find
   * it on the chrome side.
   */
  _getZombieTabUrl() {
    // On Fennec, we can check the session store data for zombie tabs
    if (this._browser && this._browser.__SS_restore) {
      const sessionStore = this._browser.__SS_data;
      // Get the last selected entry
      const entry = sessionStore.entries[sessionStore.index - 1];
      return entry.url;
    }

    return null;
  },

  async _unzombifyIfNeeded() {
    if (!this.options.forceUnzombify || !this._isZombieTab()) {
      return;
    }

    // Unzombify if the browser is a zombie tab on Android.
    const browserApp = this._browser
      ? this._browser.ownerGlobal.BrowserApp
      : null;
    if (browserApp) {
      // Wait until the content is loaded so as to ensure that the inspector actor refers
      // to same document.
      const waitForUnzombify = new Promise(resolve => {
        this._browser.addEventListener("DOMContentLoaded", resolve, {
          capture: true,
          once: true,
        });
      });

      const tab = browserApp.getTabForBrowser(this._browser);
      tab.unzombify();

      await waitForUnzombify;
    }
  },

  form() {
    const form = Object.assign({}, this._form);
    // In case of Zombie tabs (not yet restored), look up title and url from other.
    if (this._isZombieTab()) {
      form.title = this._getZombieTabTitle() || form.title;
      form.url = this._getZombieTabUrl() || form.url;
    }

    return form;
  },

  exit() {
    this._browser = null;
    this._form = null;
    this.exited = true;
  },
});
