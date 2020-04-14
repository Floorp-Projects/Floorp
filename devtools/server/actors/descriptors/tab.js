/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Descriptor Actor that represents a Tab in the parent process. It
 * launches a FrameTargetActor in the content process to do the real work and tunnels the
 * data.
 *
 * See devtools/docs/backend/actor-hierarchy.md for more details.
 */

const Services = require("Services");
const {
  connectToFrame,
} = require("devtools/server/connectors/frame-connector");
loader.lazyImporter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);
const { ActorClassWithSpec, Actor } = require("devtools/shared/protocol");
const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");
const { AppConstants } = require("resource://gre/modules/AppConstants.jsm");

/**
 * Creates a target actor proxy for handling requests to a single browser frame.
 * Both <xul:browser> and <iframe mozbrowser> are supported.
 * This actor is a shim that connects to a FrameTargetActor in a remote browser process.
 * All RDP packets get forwarded using the message manager.
 *
 * @param connection The main RDP connection.
 * @param browser <xul:browser> or <iframe mozbrowser> element to connect to.
 */
const TabDescriptorActor = ActorClassWithSpec(tabDescriptorSpec, {
  initialize(connection, browser) {
    Actor.prototype.initialize.call(this, connection);
    this._conn = connection;
    this._browser = browser;
    this._form = null;
    this.exited = false;

    // The update request could timeout if the descriptor is destroyed while an
    // update is pending. This property will hold a reject callback that can be
    // used to reject the current update promise and avoid blocking the client.
    this._formUpdateReject = null;
  },

  form() {
    return {
      actor: this.actorID,
      selected: this.selected,
      traits: {
        // Backward compatibility for FF75 or older.
        // Remove when FF76 is on the release channel.
        getFavicon: true,
      },
    };
  },

  get selected() {
    // getMostRecentBrowserWindow will find the appropriate window on Firefox
    // Desktop and on GeckoView.
    const topAppWindow = Services.wm.getMostRecentBrowserWindow();
    if (!topAppWindow) {
      return false;
    }
    const selectedBrowser = topAppWindow.gBrowser.selectedBrowser;
    return this._browser === selectedBrowser;
  },

  async getTarget() {
    if (!this._conn) {
      return {
        error: "tabDestroyed",
        message: "Tab destroyed while performing a TabDescriptorActor update",
      };
    }
    if (this._form) {
      return this._form;
    }
    /* eslint-disable-next-line no-async-promise-executor */
    return new Promise(async (resolve, reject) => {
      const onDestroy = () => {
        // Reject the update promise if the tab was destroyed while requesting an update
        reject({
          error: "tabDestroyed",
          message: "Tab destroyed while performing a TabDescriptorActor update",
        });
      };

      try {
        // Check if the browser is still connected before calling connectToFrame
        if (!this._browser.isConnected) {
          onDestroy();
          return;
        }

        const connectForm = await connectToFrame(
          this._conn,
          this._browser,
          onDestroy
        );

        const form = this._createTargetForm(connectForm);
        this._form = form;
        resolve(form);
      } catch (e) {
        reject({
          error: "tabDestroyed",
          message: "Tab destroyed while connecting to the frame",
        });
      }
    });
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

  async getFavicon() {
    if (!AppConstants.MOZ_PLACES) {
      // PlacesUtils is not supported
      return null;
    }

    try {
      const { data } = await PlacesUtils.promiseFaviconData(this._form.url);
      return data;
    } catch (e) {
      // Favicon unavailable for this url.
      return null;
    }
  },

  async update() {
    // If the child happens to be crashed/close/detach, it won't have _form set,
    // so only request form update if some code is still listening on the other
    // side.
    if (!this._form) {
      return;
    }

    const form = await new Promise((resolve, reject) => {
      this._formUpdateReject = reject;
      const onFormUpdate = msg => {
        // There may be more than one FrameTargetActor up and running
        if (this._form.actor != msg.json.actor) {
          return;
        }
        this._mm.removeMessageListener("debug:form", onFormUpdate);

        this._formUpdateReject = null;
        resolve(msg.json);
      };

      this._mm.addMessageListener("debug:form", onFormUpdate);
      this._mm.sendAsyncMessage("debug:form");
    });

    this._form = form;
  },

  _isZombieTab() {
    // Note: GeckoView doesn't support zombie tabs
    const tabbrowser = this._tabbrowser;
    const tab = tabbrowser ? tabbrowser.getTabForBrowser(this._browser) : null;
    return tab?.hasAttribute && tab.hasAttribute("pending");
  },

  /**
   * If we don't have a title from the content side because it's a zombie tab, try to find
   * it on the chrome side.
   */
  _getZombieTabTitle() {
    // If contentTitle is empty (e.g. on a not-yet-restored tab), but there is a
    // tabbrowser (i.e. desktop Firefox, but not GeckoView), we can use the label
    // as the title.
    if (this._tabbrowser) {
      const tab = this._tabbrowser.getTabForBrowser(this._browser);
      if (tab) {
        return tab.label;
      }
    }

    return null;
  },

  _createTargetForm(connectedForm) {
    const form = Object.assign({}, connectedForm);
    // In case of Zombie tabs (not yet restored), look up title from other.
    if (this._isZombieTab()) {
      form.title = this._getZombieTabTitle() || form.title;
    }

    return form;
  },

  destroy() {
    if (this._formUpdateReject) {
      this._formUpdateReject({
        error: "tabDestroyed",
        message: "Tab destroyed while performing a TabDescriptorActor update",
      });
      this._formUpdateReject = null;
    }
    this._browser = null;
    this._form = null;
    this.exited = true;
    this.emit("exited");

    Actor.prototype.destroy.call(this);
  },
});

exports.TabDescriptorActor = TabDescriptorActor;
