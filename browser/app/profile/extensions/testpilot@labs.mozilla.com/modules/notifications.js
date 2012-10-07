/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The TestPilotSetup object will choose one of these implementations to instantiate.
 * The interface for all NotificationManager implementations is:
     showNotification: function(window, features, choices) {},
     hideNotification: function(window) {}
 */

EXPORTED_SYMBOLS = ["CustomNotificationManager", "PopupNotificationManager"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

/* CustomNotificationManager: the one where notifications
 * come up from the Test Pilot icon in the addon bar.  For Firefox 3.6. */
function CustomNotificationManager(anchorId, tailIsUp) {
  this._anchorId = anchorId;
  this._tailIsUp = tailIsUp;
}
CustomNotificationManager.prototype = {
  showNotification: function TP_OldNotfn_showNotification(window, features, choices) {
    let doc = window.document;
    let popup = doc.getElementById("pilot-notification-popup");
    let textLabel = doc.getElementById("pilot-notification-text");
    let titleLabel = doc.getElementById("pilot-notification-title");
    let icon = doc.getElementById("pilot-notification-icon");
    let button = doc.getElementById("pilot-notification-submit");
    let closeBtn = doc.getElementById("pilot-notification-close");
    let link = doc.getElementById("pilot-notification-link");
    let checkbox = doc.getElementById("pilot-notification-always-submit-checkbox");
    let self = this;
    let buttonChoice = null;
    let linkChoice = null;
    let checkBoxChoice = null;

    if (this._tailIsUp) {
      popup.setAttribute("class", "tail-up");
    } else {
      popup.setAttribute("class", "tail-down");
    }

    popup.setAttribute("noautohide", !(features.fragile));
    if (features.title) {
      titleLabel.setAttribute("value", features.title);
    }
    while (textLabel.lastChild) {
      textLabel.removeChild(textLabel.lastChild);
    }
    if (features.text) {
      textLabel.appendChild(doc.createTextNode(features.text));
    }
    if (features.iconClass) {
      // css will set the image url based on the class.
      icon.setAttribute("class", features.iconClass);
    }

    /* Go through the specified choices and figure out which one to turn into a link, which one
     * (if any) to turn into a button, and which one (if any) to turn into a check box. */
    for (let i = 0; i < choices.length; i++) {
      switch(choices[i].customUiType) {
      case "button":
        buttonChoice = choices[i];
        break;
      case "link":
        linkChoice = choices[i];
        break;
      case "checkbox":
        checkBoxChoice = choices[i];
        break;
      }
    }
    // Create check box if specified:
    if (checkBoxChoice) {
      checkbox.removeAttribute("hidden");
      checkbox.setAttribute("label", checkBoxChoice.label);
    } else {
      checkbox.setAttribute("hidden", true);
    }

    // Create button if specified:
    if (buttonChoice) {
      button.setAttribute("label", buttonChoice.label);
      button.onclick = function(event) {
        if (event.button == 0) {
          if (checkbox.checked && checkBoxChoice) {
            checkBoxChoice.callback();
          }
          buttonChoice.callback();
          self.hideNotification(window);
          if (features.closeCallback) {
            features.closeCallback();
          }
        }
      };
      button.removeAttribute("hidden");
    } else {
      button.setAttribute("hidden", true);
    }

    // Create the link if specified:
    if (linkChoice) {
      link.setAttribute("value", linkChoice.label);
      link.setAttribute("class", "notification-link");
      link.onclick = function(event) {
        if (event.button == 0) {
          linkChoice.callback();
          self.hideNotification(window);
          if (features.closeCallback) {
            features.closeCallback();
          }
        }
      };
      link.removeAttribute("hidden");
    } else {
      link.setAttribute("hidden", true);
    }

    closeBtn.onclick = function() {
      self.hideNotification(window);
      if (features.closeCallback) {
        features.closeCallback();
      }
    };

    // Show the popup:
    popup.hidden = false;
    popup.setAttribute("open", "true");
    let anchorElement = window.document.getElementById(this._anchorId);
    popup.openPopup(anchorElement, "after_end");
  },

  hideNotification: function TP_OldNotfn_hideNotification(window) {
    let popup = window.document.getElementById("pilot-notification-popup");
    popup.removeAttribute("open");
    popup.hidePopup();
  }
};

// For Fx 4.0 + , uses the built-in doorhanger notification system (but with my own anchor icon)
function PopupNotificationManager() {
  /* In the future, we may want to anchor these to the Feedback button if present,
   * but for now that option is unimplemented. */
  this._popupModule = {};
  Components.utils.import("resource://gre/modules/PopupNotifications.jsm", this._popupModule);
  this._pn = null;
}
PopupNotificationManager.prototype = {
  showNotification: function TP_NewNotfn_showNotification(window, features, choices) {
    let self = this;
    let tabbrowser = window.getBrowser();
    let panel = window.document.getElementById("testpilot-notification-popup");
    let iconBox = window.document.getElementById("tp-notification-popup-box");
    let defaultChoice = null;
    let additionalChoices = [];

    // hide any existing notification so we don't get a weird stack
    this.hideNotification();

    // Create notifications object for window
    this._pn = new this._popupModule.PopupNotifications(tabbrowser, panel, iconBox);

    /* Add hideNotification() calls to the callbacks of each choice -- the client code shouldn't
     * have to worry about hiding the notification in its callbacks.*/
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let choiceWithHide = {
        label: choice.label,
        accessKey: choice.accessKey,
        callback: function() {
          self.hideNotification();
          choice.callback();
        }};
      // Take the first one to be the default choice:
      if (i == 0) {
        defaultChoice = choiceWithHide;
      } else {
        additionalChoices.push(choiceWithHide);
      }
    }

    this._notifRef = this._pn.show(tabbrowser.selectedBrowser,
                             "testpilot",
                             features.text,
                             "tp-notification-popup-icon", // All TP notifications use this icon
                             defaultChoice,
                             additionalChoices,
                             {persistWhileVisible: true,
                              removeOnDismissal: features.fragile,
                              title: features.title,
                              iconClass: features.iconClass,
                              closeButtonFunc: function() {
                                self.hideNotification();
                              },
                              eventCallback: function(stateChange){
                                /* Note - closeCallback() will be called AFTER the button handler,
                                 * and will be called no matter whether the notification is closed via
                                 * close button or a menu button item.
                                 */
                                if (stateChange == "removed" && features.closeCallback) {
                                  features.closeCallback();
                                }
                                self._notifRef = null;
                              }});
    // See http://mxr.mozilla.org/mozilla-central/source/toolkit/content/PopupNotifications.jsm
  },

  hideNotification: function TP_NewNotfn_hideNotification() {
    if (this._notifRef && this._pn) {
      this._pn.remove(this._notifRef);
      this._notifRef = null;
    }
  }
};
