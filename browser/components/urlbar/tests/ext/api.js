/* global ExtensionAPI */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  AppMenuNotifications: "resource://gre/modules/AppMenuNotifications.jsm",
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
});

this.experiments_urlbar = class extends ExtensionAPI {
  getAPI() {
    return {
      experiments: {
        urlbar: {
          isBrowserShowingNotification() {
            let window = BrowserWindowTracker.getTopWindow();

            // urlbar view, app menu notification, notification box (info bar)
            if (
              window.gURLBar.view.isOpen ||
              AppMenuNotifications.activeNotification ||
              window.gBrowser.getNotificationBox().currentNotification
            ) {
              return true;
            }

            // tracking protection and identity box doorhangers
            if (
              ["tracking-protection-icon-container", "identity-box"].some(
                id =>
                  window.document.getElementById(id).getAttribute("open") ==
                  "true"
              )
            ) {
              return true;
            }

            // page action button panels
            let pageActions = window.document.getElementById(
              "page-action-buttons"
            );
            if (pageActions) {
              for (let child of pageActions.childNodes) {
                if (child.getAttribute("open") == "true") {
                  return true;
                }
              }
            }

            // toolbar button panels
            let navbar = window.document.getElementById(
              "nav-bar-customization-target"
            );
            for (let node of navbar.querySelectorAll("toolbarbutton")) {
              if (node.getAttribute("open") == "true") {
                return true;
              }
            }

            return false;
          },
        },
      },
    };
  }
};
