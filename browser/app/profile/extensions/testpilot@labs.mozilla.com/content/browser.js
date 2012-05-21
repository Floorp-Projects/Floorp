/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TestPilotMenuUtils;

(function() {
  var Cc = Components.classes;
  var Cu = Components.utils;
  var Ci = Components.interfaces;

  Cu.import("resource://testpilot/modules/setup.js");

  TestPilotMenuUtils = {
    updateSubmenu: function() {
      let ntfyMenuFinished =
        document.getElementById("pilot-menu-notify-finished");
      let ntfyMenuNew = document.getElementById("pilot-menu-notify-new");
      let ntfyMenuResults = document.getElementById("pilot-menu-notify-results");
      let alwaysSubmitData =
        document.getElementById("pilot-menu-always-submit-data");
      let Application = Cc["@mozilla.org/fuel/application;1"]
                      .getService(Ci.fuelIApplication);
      ntfyMenuFinished.setAttribute("checked", Application.prefs.getValue(
                                    POPUP_SHOW_ON_FINISH, false));
      ntfyMenuNew.setAttribute("checked", Application.prefs.getValue(
                                POPUP_SHOW_ON_NEW, false));
      ntfyMenuResults.setAttribute("checked", Application.prefs.getValue(
                                    POPUP_SHOW_ON_RESULTS, false));
      alwaysSubmitData.setAttribute("checked", Application.prefs.getValue(
                                     ALWAYS_SUBMIT_DATA, false));
    },

    togglePref: function(id) {
      let prefName = "extensions.testpilot." + id;
      let oldVal = Application.prefs.getValue(prefName, false);
      Application.prefs.setValue( prefName, !oldVal);

      // If you turn on or off the global pref, startup or shutdown test pilot
      // accordingly:
      if (prefName == RUN_AT_ALL_PREF) {
        if (oldVal == true) {
          TestPilotSetup.globalShutdown();
        }
        if (oldVal == false) {
          TestPilotSetup.globalStartup();
        }
      }
    },

    onPopupShowing: function(event) {
      this._setMenuLabels();
    },

    onPopupHiding: function(event) {
      let target = event.target;
      if (target.id == "pilot-menu-popup") {
        let menu = document.getElementById("pilot-menu");
        if (target.parentNode != menu) {
          menu.appendChild(target);
        }
      }
    },

    _setMenuLabels: function() {
      // Make the enable/disable User Studies menu item show the right label
      // for the current status...
      let runStudiesToggle = document.getElementById("feedback-menu-enable-studies");
      if (runStudiesToggle) {
        let currSetting = Application.prefs.getValue("extensions.testpilot.runStudies",
                                                     true);

        let stringBundle = Cc["@mozilla.org/intl/stringbundle;1"].
          getService(Ci.nsIStringBundleService).
          createBundle("chrome://testpilot/locale/main.properties");

        if (currSetting) {
          runStudiesToggle.setAttribute("label",
            stringBundle.GetStringFromName("testpilot.turnOff"));
        } else {
          runStudiesToggle.setAttribute("label",
            stringBundle.GetStringFromName("testpilot.turnOn"));
        }
      }

      let studiesMenuItem = document.getElementById("feedback-menu-show-studies");
      studiesMenuItem.setAttribute("disabled",
                                   !Application.prefs.getValue(RUN_AT_ALL_PREF, true));
    },

    onMenuButtonMouseDown: function(attachPointId) {
      if (!attachPointId) {
        attachPointId = "pilot-notifications-button";
      }
      let menuPopup = document.getElementById("pilot-menu-popup");
      let menuButton = document.getElementById(attachPointId);

      // TODO failing here with "menuPopup is null" for Tracy
      if (menuPopup.parentNode != menuButton)
        menuButton.appendChild(menuPopup);

      let alignment;
      // Menu should appear above status bar icon, but below Feedback button
      if (attachPointId == "pilot-notifications-button") {
        alignment = "before_start";
      } else {
        alignment = "after_end";
      }

      menuPopup.openPopup(menuButton, alignment, 0, 0, true);
    }
  };


  var TestPilotWindowHandlers = {
    onWindowLoad: function() {
      // Customize the interface of the newly opened window.
      Cu.import("resource://testpilot/modules/interface.js");
      TestPilotUIBuilder.buildCorrectInterface(window);

      /* "Hold" window load events for TestPilotSetup, passing them along only
       * after startup is complete.  It's hacky, but the benefit is that
       * TestPilotSetup.onWindowLoad can treat all windows the same no matter
       * whether they opened with Firefox on startup or were opened later. */
      if (TestPilotSetup && TestPilotSetup.startupComplete) {
        TestPilotSetup.onWindowLoad(window);
      } else {
        let observerSvc = Cc["@mozilla.org/observer-service;1"]
                             .getService(Ci.nsIObserverService);
        let observer = {
          observe: function(subject, topic, data) {
            observerSvc.removeObserver(this, "testpilot:startup:complete");
            TestPilotSetup.onWindowLoad(window);
          }
        };
        observerSvc.addObserver(observer, "testpilot:startup:complete", false);
      }
    },

    onWindowUnload: function() {
      TestPilotSetup.onWindowUnload(window);
    }
  };

  window.addEventListener("load", TestPilotWindowHandlers.onWindowLoad, false);
  window.addEventListener("unload", TestPilotWindowHandlers.onWindowUnload, false);
}());

