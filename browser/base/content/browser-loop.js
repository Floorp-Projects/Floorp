// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// the "exported" symbols
let LoopUI;

XPCOMUtils.defineLazyModuleGetter(this, "injectLoopAPI", "resource:///modules/loop/MozLoopAPI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService", "resource:///modules/loop/MozLoopService.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PanelFrame", "resource:///modules/PanelFrame.jsm");


(function() {

  LoopUI = {
    get toolbarButton() {
      delete this.toolbarButton;
      return this.toolbarButton = CustomizableUI.getWidget("loop-call-button").forWindow(window);
    },

    /**
     * Opens the panel for Loop and sizes it appropriately.
     *
     * @param {event} event The event opening the panel, used to anchor
     *                      the panel to the button which triggers it.
     */
    openCallPanel: function(event) {
      let callback = iframe => {
        iframe.addEventListener("DOMContentLoaded", function documentDOMLoaded() {
          iframe.removeEventListener("DOMContentLoaded", documentDOMLoaded, true);
          injectLoopAPI(iframe.contentWindow);
        }, true);
      };

      PanelFrame.showPopup(window, event.target, "loop", null,
                           "about:looppanel", null, callback);
    },

    /**
     * Triggers the initialization of the loop service.  Called by
     * delayedStartup.
     */
    init: function() {
      if (!Services.prefs.getBoolPref("loop.enabled")) {
        this.toolbarButton.node.hidden = true;
        return;
      }

      // Add observer notifications before the service is initialized
      Services.obs.addObserver(this, "loop-status-changed", false);

      // If we're throttled, check to see if it's our turn to be unthrottled
      if (Services.prefs.getBoolPref("loop.throttled")) {
        this.toolbarButton.node.hidden = true;
        MozLoopService.checkSoftStart(this.toolbarButton.node);
        return;
      }

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
      this.updateToolbarState();
    },

    updateToolbarState: function() {
      let state = "";
      if (MozLoopService.errors.size) {
        state = "error";
      } else if (MozLoopService.doNotDisturb) {
        state = "disabled";
      }
      this.toolbarButton.node.setAttribute("state", state);
    },
  };
})();
