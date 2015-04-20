// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let LoopUI;

(function() {
  const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
  const kBrowserSharingNotificationId = "loop-sharing-notification";
  const kPrefBrowserSharingInfoBar = "browserSharing.showInfoBar";

  LoopUI = {
    /**
     * @var {XULWidgetSingleWrapper} toolbarButton Getter for the Loop toolbarbutton
     *                                             instance for this window.
     */
    get toolbarButton() {
      delete this.toolbarButton;
      return this.toolbarButton = CustomizableUI.getWidget("loop-button").forWindow(window);
    },

    /**
     * @var {XULElement} panel Getter for the Loop panel element.
     */
    get panel() {
      delete this.panel;
      return this.panel = document.getElementById("loop-notification-panel");
    },

    /**
     * @var {XULElement|null} browser Getter for the Loop panel browser element.
     *                                Will be NULL if the panel hasn't loaded yet.
     */
    get browser() {
      let browser = document.querySelector("#loop-notification-panel > #loop-panel-iframe");
      if (browser) {
        delete this.browser;
        this.browser = browser;
      }
      return browser;
    },

    /**
     * @var {String|null} selectedTab Getter for the name of the currently selected
     *                                tab inside the Loop panel. Will be NULL if
     *                                the panel hasn't loaded yet.
     */
    get selectedTab() {
      if (!this.browser) {
        return null;
      }

      let selectedTab = this.browser.contentDocument.querySelector(".tab-view > .selected");
      return selectedTab && selectedTab.getAttribute("data-tab-name");
    },

    /**
     * @return {Promise}
     */
    promiseDocumentVisible(aDocument) {
      if (!aDocument.hidden) {
        return Promise.resolve();
      }

      return new Promise((resolve) => {
        aDocument.addEventListener("visibilitychange", function onVisibilityChanged() {
          aDocument.removeEventListener("visibilitychange", onVisibilityChanged);
          resolve();
        });
      });
    },

    /**
     * Toggle between opening or hiding the Loop panel.
     *
     * @param {DOMEvent} [event] Optional event that triggered the call to this
     *                           function.
     * @param {String}   [tabId] Optional name of the tab to select after the panel
     *                           has opened. Does nothing when the panel is hidden.
     * @return {Promise}
     */
    togglePanel: function(event, tabId = null) {
      if (this.panel.state == "open") {
        return new Promise(resolve => {
          this.panel.hidePopup();
          resolve();
        });
      }

      return this.openCallPanel(event, tabId);
    },

    /**
     * Opens the panel for Loop and sizes it appropriately.
     *
     * @param {event}  event   The event opening the panel, used to anchor
     *                         the panel to the button which triggers it.
     * @param {String} [tabId] Identifier of the tab to select when the panel is
     *                         opened. Example: 'rooms', 'contacts', etc.
     * @return {Promise}
     */
    openCallPanel: function(event, tabId = null) {
      return new Promise((resolve) => {
        let callback = iframe => {
          // Helper function to show a specific tab view in the panel.
          function showTab() {
            if (!tabId) {
              resolve(LoopUI.promiseDocumentVisible(iframe.contentDocument));
              return;
            }

            let win = iframe.contentWindow;
            let ev = new win.CustomEvent("UIAction", Cu.cloneInto({
              detail: {
                action: "selectTab",
                tab: tabId
              }
            }, win));
            win.dispatchEvent(ev);
            resolve(LoopUI.promiseDocumentVisible(iframe.contentDocument));
          }

          // If the panel has been opened and initialized before, we can skip waiting
          // for the content to load - because it's already there.
          if (("contentWindow" in iframe) && iframe.contentWindow.document.readyState == "complete") {
            showTab();
            return;
          }

          let documentDOMLoaded = () => {
            iframe.removeEventListener("DOMContentLoaded", documentDOMLoaded, true);
  	    this.injectLoopAPI(iframe.contentWindow);
  	    iframe.contentWindow.addEventListener("loopPanelInitialized", function loopPanelInitialized() {
              iframe.contentWindow.removeEventListener("loopPanelInitialized",
                                                       loopPanelInitialized);
              showTab();
	    });
	  };
	  iframe.addEventListener("DOMContentLoaded", documentDOMLoaded, true); 
        };

        // Used to clear the temporary "login" state from the button.
        Services.obs.notifyObservers(null, "loop-status-changed", null);

        this.shouldResumeTour().then((resume) => {
          if (resume) {
            // Assume the conversation with the visitor wasn't open since we would
            // have resumed the tour as soon as the visitor joined if it was (and
            // the pref would have been set to false already.
            this.MozLoopService.resumeTour("waiting");
            resolve();
            return;
          }

          this.PanelFrame.showPopup(window, event ? event.target : this.toolbarButton.node,
                               "loop", null, "about:looppanel", null, callback);
        });
      });
    },

    /**
     * Method to know whether actions to open the panel should instead resume the tour.
     *
     * We need the panel to be opened via UITour so that it gets @noautohide.
     *
     * @return {Promise} resolving with a {Boolean} of whether the tour should be resumed instead of
     *                   opening the panel.
     */
    shouldResumeTour: Task.async(function* () {
      // Resume the FTU tour if this is the first time a room was joined by
      // someone else since the tour.
      if (!Services.prefs.getBoolPref("loop.gettingStarted.resumeOnFirstJoin")) {
        return false;
      }

      if (!this.LoopRooms.participantsCount) {
        // Nobody is in the rooms
        return false;
      }

      let roomsWithNonOwners = yield this.roomsWithNonOwners();
      if (!roomsWithNonOwners.length) {
        // We were the only one in a room but we want to know about someone else joining.
        return false;
      }

      return true;
    }),

    /**
     * @return {Promise} resolved with an array of Rooms with participants (excluding owners)
     */
    roomsWithNonOwners: function() {
      return new Promise(resolve => {
        this.LoopRooms.getAll((error, rooms) => {
          let roomsWithNonOwners = [];
          for (let room of rooms) {
            if (!("participants" in room)) {
              continue;
            }
            let numNonOwners = room.participants.filter(participant => !participant.owner).length;
            if (!numNonOwners) {
              continue;
            }
            roomsWithNonOwners.push(room);
          }
          resolve(roomsWithNonOwners);
        });
      });
    },

    /**
     * Triggers the initialization of the loop service.  Called by
     * delayedStartup.
     */
    init: function() {
      // Add observer notifications before the service is initialized
      Services.obs.addObserver(this, "loop-status-changed", false);

      this.MozLoopService.initialize();
      this.updateToolbarState();
    },

    uninit: function() {
      Services.obs.removeObserver(this, "loop-status-changed");
    },

    // Implements nsIObserver
    observe: function(subject, topic, data) {
      if (topic != "loop-status-changed") {
        return;
      }
      this.updateToolbarState(data);
    },

    /**
     * Updates the toolbar/menu-button state to reflect Loop status.
     *
     * @param {string} [aReason] Some states are only shown if
     *                           a related reason is provided.
     *
     *                 aReason="login": Used after a login is completed
     *                   successfully. This is used so the state can be
     *                   temporarily shown until the next state change.
     */
    updateToolbarState: function(aReason = null) {
      if (!this.toolbarButton.node) {
        return;
      }
      let state = "";
      if (this.MozLoopService.errors.size) {
        state = "error";
      } else if (this.MozLoopService.screenShareActive) {
        state = "action";
      } else if (aReason == "login" && this.MozLoopService.userProfile) {
        state = "active";
      } else if (this.MozLoopService.doNotDisturb) {
        state = "disabled";
      } else if (this.MozLoopService.roomsParticipantsCount > 0) {
        state = "active";
      }
      this.toolbarButton.node.setAttribute("state", state);
    },

    /**
     * Show a desktop notification when 'do not disturb' isn't enabled.
     *
     * @param {Object} options Set of options that may tweak the appearance and
     *                         behavior of the notification.
     *                         Option params:
     *                         - {String}   title       Notification title message
     *                         - {String}   [message]   Notification body text
     *                         - {String}   [icon]      Notification icon
     *                         - {String}   [sound]     Sound to play
     *                         - {String}   [selectTab] Tab to select when the panel
     *                                                  opens
     *                         - {Function} [onclick]   Callback to invoke when
     *                                                  the notification is clicked.
     *                                                  Opens the panel by default.
     */
    showNotification: function(options) {
      if (this.MozLoopService.doNotDisturb) {
        return;
      }

      if (!options.title) {
        throw new Error("Missing title, can not display notification");
      }

      let notificationOptions = {
        body: options.message || ""
      };
      if (options.icon) {
        notificationOptions.icon = options.icon;
      }
      if (options.sound) {
        // This will not do anything, until bug bug 1105222 is resolved.
        notificationOptions.mozbehavior = {
          soundFile: ""
        };
        this.playSound(options.sound);
      }

      let notification = new window.Notification(options.title, notificationOptions);
      notification.addEventListener("click", e => {
        if (window.closed) {
          return;
        }

        try {
          window.focus();
        } catch (ex) {}

        // We need a setTimeout here, otherwise the panel won't show after the
        // window received focus.
        window.setTimeout(() => {
          if (typeof options.onclick == "function") {
            options.onclick();
          } else {
            // Open the Loop panel as a default action.
            this.openCallPanel(null, options.selectTab || null);
          }
        }, 0);
      });
    },

    /**
     * Play a sound in this window IF there's no sound playing yet.
     *
     * @param {String} name Name of the sound, like 'ringtone' or 'room-joined'
     */
    playSound: function(name) {
      if (this.ActiveSound || this.MozLoopService.doNotDisturb) {
        return;
      }

      this.activeSound = new window.Audio();
      this.activeSound.src = `chrome://browser/content/loop/shared/sounds/${name}.ogg`;
      this.activeSound.load();
      this.activeSound.play();

      this.activeSound.addEventListener("ended", () => this.activeSound = undefined, false);
    },

    /**
     * Adds a listener for browser sharing. It will inform the listener straight
     * away for the current windowId, and then on every tab change.
     *
     * Listener parameters:
     * - {Object}  err       If there is a error this will be defined, null otherwise.
     * - {Integer} windowId  The new windowId for the browser.
     *
     * @param {Function} listener The listener to receive information on when the
     *                            windowId changes.
     */
    addBrowserSharingListener: function(listener) {
      if (!this._tabChangeListeners) {
        this._tabChangeListeners = new Set();
        gBrowser.tabContainer.addEventListener("TabSelect", this);
      }

      this._tabChangeListeners.add(listener);
      this._maybeShowBrowserSharingInfoBar();

      // Get the first window Id for the listener.
      listener(null, gBrowser.selectedBrowser.outerWindowID);
    },

    /**
     * Removes a listener from browser sharing.
     *
     * @param {Function} listener The listener to remove from the list.
     */
    removeBrowserSharingListener: function(listener) {
      if (!this._tabChangeListeners) {
        return;
      }

      if (this._tabChangeListeners.has(listener)) {
        this._tabChangeListeners.delete(listener);
      }

      if (!this._tabChangeListeners.size) {
        this._hideBrowserSharingInfoBar();
        gBrowser.tabContainer.removeEventListener("TabSelect", this);
        delete this._tabChangeListeners;
      }
    },

    /**
     * Helper function to fetch a localized string via the MozLoopService API.
     * It's currently inconveniently wrapped inside a string of stringified JSON.
     *
     * @param  {String} key The element id to get strings for.
     * @return {String}
     */
    _getString: function(key) {
      let str = this.MozLoopService.getStrings(key);
      if (str) {
        str = JSON.parse(str).textContent;
      }
      return str;
    },

    /**
     * Shows an infobar notification at the top of the browser window that warns
     * the user that their browser tabs are being broadcasted through the current
     * conversation.
     */
    _maybeShowBrowserSharingInfoBar: function() {
      this._hideBrowserSharingInfoBar();

      // Don't show the infobar if it's been permanently disabled from the menu.
      if (!this.MozLoopService.getLoopPref(kPrefBrowserSharingInfoBar)) {
        return;
      }

      // Create the menu that is shown when the menu-button' dropmarker is clicked
      // inside the notification bar.
      let menuPopup = document.createElementNS(kNSXUL, "menupopup");
      let menuItem = menuPopup.appendChild(document.createElementNS(kNSXUL, "menuitem"));
      menuItem.setAttribute("label", this._getString("infobar_menuitem_dontshowagain_label"));
      menuItem.setAttribute("accesskey", this._getString("infobar_menuitem_dontshowagain_accesskey"));
      menuItem.addEventListener("command", () => {
        // We're being told to hide the bar permanently.
        this._hideBrowserSharingInfoBar(true);
      });

      let box = gBrowser.getNotificationBox();
      let bar = box.appendNotification(
        this._getString("infobar_screenshare_browser_message"),
        kBrowserSharingNotificationId,
        // Icon is defined in browser theme CSS.
        null,
        box.PRIORITY_WARNING_LOW,
        [{
          label: this._getString("infobar_button_gotit_label"),
          accessKey: this._getString("infobar_button_gotit_accesskey"),
          type: "menu-button",
          popup: menuPopup,
          anchor: "dropmarker",
          callback: () => {
            this._hideBrowserSharingInfoBar();
          }
        }]
      );

      // Keep showing the notification bar until the user explicitly closes it.
      bar.persistence = -1;
    },

    /**
     * Hides the infobar, permanantly if requested.
     *
     * @param {Boolean} permanently Flag that determines if the infobar will never
     *                              been shown again. Defaults to `false`.
     * @return {Boolean} |true| if the infobar was hidden here.
     */
    _hideBrowserSharingInfoBar: function(permanently = false, browser) {
      browser = browser || gBrowser.selectedBrowser;
      let box = gBrowser.getNotificationBox(browser);
      let notification = box.getNotificationWithValue(kBrowserSharingNotificationId);
      let removed = false;
      if (notification) {
        box.removeNotification(notification);
        removed = true;
      }

      if (permanently) {
        this.MozLoopService.setLoopPref(kPrefBrowserSharingInfoBar, false);
      }

      return removed;
    },

    /**
     * Handles events from gBrowser.
     */
    handleEvent: function(event) {
      // We only should get "select" events.
      if (event.type != "TabSelect") {
        return;
      }

      let wasVisible = false;
      // Hide the infobar from the previous tab.
      if (event.detail.previousTab) {
        wasVisible = this._hideBrowserSharingInfoBar(false, event.detail.previousTab.linkedBrowser);
      }

      // We've changed the tab, so get the new window id.
      for (let listener of this._tabChangeListeners) {
        try {
          listener(null, gBrowser.selectedBrowser.outerWindowID);
        } catch (ex) {
          Cu.reportError("Tab switch caused an error: " + ex.message);
        }
      };

      if (wasVisible) {
        // If the infobar was visible before, we should show it again after the
        // switch.
        this._maybeShowBrowserSharingInfoBar();
      }
    },
  };
})();

XPCOMUtils.defineLazyModuleGetter(LoopUI, "injectLoopAPI", "resource:///modules/loop/MozLoopAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(LoopUI, "LoopRooms", "resource:///modules/loop/LoopRooms.jsm");
XPCOMUtils.defineLazyModuleGetter(LoopUI, "MozLoopService", "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(LoopUI, "PanelFrame", "resource:///modules/PanelFrame.jsm");
