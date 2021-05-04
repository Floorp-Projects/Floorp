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

loader.lazyRequireGetter(
  this,
  "WatcherActor",
  "devtools/server/actors/watcher",
  true
);

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
  },

  form() {
    const form = {
      actor: this.actorID,
      browsingContextID:
        this._browser && this._browser.browsingContext
          ? this._browser.browsingContext.id
          : null,
      isZombieTab: this._isZombieTab(),
      outerWindowID: this._getOuterWindowId(),
      selected: this.selected,
      title: this._getTitle(),
      traits: {
        // Supports the Watcher actor. Can be removed as part of Bug 1680280.
        watcher: true,
      },
      url: this._getUrl(),
    };

    return form;
  },

  _getTitle() {
    // If the content already provides a title, use it.
    if (this._browser.contentTitle) {
      return this._browser.contentTitle;
    }

    // For zombie or lazy tabs (tab created, but content has not been loaded),
    // try to retrieve the title from the XUL Tab itself.
    // Note: this only works on Firefox desktop.
    if (this._tabbrowser) {
      const tab = this._tabbrowser.getTabForBrowser(this._browser);
      if (tab) {
        return tab.label;
      }
    }

    // No title available.
    return null;
  },

  _getUrl() {
    if (!this._browser || !this._browser.browsingContext) {
      return "";
    }

    const { browsingContext } = this._browser;
    return browsingContext.currentWindowGlobal.documentURI.spec;
  },

  _getOuterWindowId() {
    if (!this._browser || !this._browser.browsingContext) {
      return "";
    }

    const { browsingContext } = this._browser;
    return browsingContext.currentWindowGlobal.outerWindowId;
  },

  get selected() {
    // getMostRecentBrowserWindow will find the appropriate window on Firefox
    // Desktop and on GeckoView.
    const topAppWindow = Services.wm.getMostRecentBrowserWindow();

    const selectedBrowser = topAppWindow?.gBrowser?.selectedBrowser;
    if (!selectedBrowser) {
      // Note: gBrowser is not available on GeckoView.
      // We should find another way to know if this browser is the selected
      // browser. See Bug 1631020.
      return false;
    }

    return this._browser === selectedBrowser;
  },

  async getTarget() {
    if (!this._conn) {
      return {
        error: "tabDestroyed",
        message: "Tab destroyed while performing a TabDescriptorActor update",
      };
    }

    /* eslint-disable-next-line no-async-promise-executor */
    return new Promise(async (resolve, reject) => {
      const onDestroy = () => {
        // Reject the update promise if the tab was destroyed while requesting an update
        reject({
          error: "tabDestroyed",
          message: "Tab destroyed while performing a TabDescriptorActor update",
        });

        // Targets created from the TabDescriptor are not created via JSWindowActors and
        // we need to notify the watcher manually about their destruction.
        // TabDescriptor's targets are created via TabDescriptor.getTarget and are still using
        // message manager instead of JSWindowActors.
        if (this.watcher && this.targetActorForm) {
          this.watcher.notifyTargetDestroyed(this.targetActorForm);
        }
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
        this.targetActorForm = connectForm;
        resolve(connectForm);
      } catch (e) {
        reject({
          error: "tabDestroyed",
          message: "Tab destroyed while connecting to the frame",
        });
      }
    });
  },

  /**
   * Return a Watcher actor, allowing to keep track of targets which
   * already exists or will be created. It also helps knowing when they
   * are destroyed.
   */
  getWatcher() {
    if (!this.watcher) {
      this.watcher = new WatcherActor(this.conn, { browser: this._browser });
      this.manage(this.watcher);
    }
    return this.watcher;
  },

  get _tabbrowser() {
    if (this._browser && typeof this._browser.getTabBrowser == "function") {
      return this._browser.getTabBrowser();
    }
    return null;
  },

  async getFavicon() {
    if (!AppConstants.MOZ_PLACES) {
      // PlacesUtils is not supported
      return null;
    }

    try {
      const { data } = await PlacesUtils.promiseFaviconData(this._getUrl());
      return data;
    } catch (e) {
      // Favicon unavailable for this url.
      return null;
    }
  },

  _isZombieTab() {
    // Note: GeckoView doesn't support zombie tabs
    const tabbrowser = this._tabbrowser;
    const tab = tabbrowser ? tabbrowser.getTabForBrowser(this._browser) : null;
    return tab?.hasAttribute && tab.hasAttribute("pending");
  },

  destroy() {
    this.emit("descriptor-destroyed");
    this._browser = null;

    Actor.prototype.destroy.call(this);
  },
});

exports.TabDescriptorActor = TabDescriptorActor;
