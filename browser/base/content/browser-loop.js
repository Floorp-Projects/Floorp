// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let LoopUI;

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI", "resource:///modules/loop/MozLoopAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "LoopRooms", "resource:///modules/loop/LoopRooms.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService", "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PanelFrame", "resource:///modules/PanelFrame.jsm");


(function() {
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

          iframe.addEventListener("DOMContentLoaded", function documentDOMLoaded() {
            iframe.removeEventListener("DOMContentLoaded", documentDOMLoaded, true);
            injectLoopAPI(iframe.contentWindow);
            iframe.contentWindow.addEventListener("loopPanelInitialized", function loopPanelInitialized() {
              iframe.contentWindow.removeEventListener("loopPanelInitialized",
                                                       loopPanelInitialized);
              showTab();
            });
          }, true);
        };

        // Used to clear the temporary "login" state from the button.
        Services.obs.notifyObservers(null, "loop-status-changed", null);

        this.shouldResumeTour().then((resume) => {
          if (resume) {
            // Assume the conversation with the visitor wasn't open since we would
            // have resumed the tour as soon as the visitor joined if it was (and
            // the pref would have been set to false already.
            MozLoopService.resumeTour("waiting");
            resolve();
            return;
          }

          PanelFrame.showPopup(window, event ? event.target : this.toolbarButton.node,
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

      if (!LoopRooms.participantsCount) {
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
        LoopRooms.getAll((error, rooms) => {
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

      MozLoopService.initialize();
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
      if (MozLoopService.errors.size) {
        state = "error";
      } else if (MozLoopService.screenShareActive) {
        state = "action";
      } else if (aReason == "login" && MozLoopService.userProfile) {
        state = "active";
      } else if (MozLoopService.doNotDisturb) {
        state = "disabled";
      } else if (MozLoopService.roomsParticipantsCount > 0) {
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
      if (MozLoopService.doNotDisturb) {
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
      if (this.ActiveSound || MozLoopService.doNotDisturb) {
        return;
      }

      this.activeSound = new window.Audio();
      this.activeSound.src = `chrome://browser/content/loop/shared/sounds/${name}.ogg`;
      this.activeSound.load();
      this.activeSound.play();

      this.activeSound.addEventListener("ended", () => this.activeSound = undefined, false);
    },
  };
})();
