/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

customElements.define("addon-progress-notification", class MozAddonProgressNotification extends customElements.get("popupnotification") {
  show() {
    super.show();
    this.progressmeter = document.getElementById("addon-progress-notification-progressmeter");

    this.progresstext = document.getElementById("addon-progress-notification-progresstext");

    if (!this.notification)
      return;

    this.notification.options.installs.forEach(function(aInstall) {
      aInstall.addListener(this);
    }, this);

    // Calling updateProgress can sometimes cause this notification to be
    // removed in the middle of refreshing the notification panel which
    // makes the panel get refreshed again. Just initialise to the
    // undetermined state and then schedule a proper check at the next
    // opportunity
    this.setProgress(0, -1);
    this._updateProgressTimeout = setTimeout(this.updateProgress.bind(this), 0);
  }

  disconnectedCallback() {
    this.destroy();
  }

  destroy() {
    if (!this.notification)
      return;
    this.notification.options.installs.forEach(function(aInstall) {
      aInstall.removeListener(this);
    }, this);

    clearTimeout(this._updateProgressTimeout);
  }

  setProgress(aProgress, aMaxProgress) {
    if (aMaxProgress == -1) {
      this.progressmeter.removeAttribute("value");
    } else {
      this.progressmeter.setAttribute("value", (aProgress * 100) / aMaxProgress);
    }

    let now = Date.now();

    if (!this.notification.lastUpdate) {
      this.notification.lastUpdate = now;
      this.notification.lastProgress = aProgress;
      return;
    }

    let delta = now - this.notification.lastUpdate;
    if ((delta < 400) && (aProgress < aMaxProgress))
      return;

    delta /= 1000;

    // This algorithm is the same used by the downloads code.
    let speed = (aProgress - this.notification.lastProgress) / delta;
    if (this.notification.speed)
      speed = speed * 0.9 + this.notification.speed * 0.1;

    this.notification.lastUpdate = now;
    this.notification.lastProgress = aProgress;
    this.notification.speed = speed;

    let status = null;
    [status, this.notification.last] = DownloadUtils.getDownloadStatus(aProgress, aMaxProgress, speed, this.notification.last);
    this.progresstext.setAttribute("value", status);
    this.progresstext.setAttribute("tooltiptext", status);
  }

  cancel() {
    let installs = this.notification.options.installs;
    installs.forEach(function(aInstall) {
      try {
        aInstall.cancel();
      } catch (e) {
        // Cancel will throw if the download has already failed
      }
    }, this);

    PopupNotifications.remove(this.notification);
  }

  updateProgress() {
    if (!this.notification)
      return;

    let downloadingCount = 0;
    let progress = 0;
    let maxProgress = 0;

    this.notification.options.installs.forEach(function(aInstall) {
      if (aInstall.maxProgress == -1)
        maxProgress = -1;
      progress += aInstall.progress;
      if (maxProgress >= 0)
        maxProgress += aInstall.maxProgress;
      if (aInstall.state < AddonManager.STATE_DOWNLOADED)
        downloadingCount++;
    });

    if (downloadingCount == 0) {
      this.destroy();
      this.progressmeter.removeAttribute("value");
      let status = gNavigatorBundle.getString("addonDownloadVerifying");
      this.progresstext.setAttribute("value", status);
      this.progresstext.setAttribute("tooltiptext", status);
    } else {
      this.setProgress(progress, maxProgress);
    }
  }

  onDownloadProgress() {
    this.updateProgress();
  }

  onDownloadFailed() {
    this.updateProgress();
  }

  onDownloadCancelled() {
    this.updateProgress();
  }

  onDownloadEnded() {
    this.updateProgress();
  }
});

// Removes a doorhanger notification if all of the installs it was notifying
// about have ended in some way.
function removeNotificationOnEnd(notification, installs) {
  let count = installs.length;

  function maybeRemove(install) {
    install.removeListener(this);

    if (--count == 0) {
      // Check that the notification is still showing
      let current = PopupNotifications.getNotification(notification.id, notification.browser);
      if (current === notification)
        notification.remove();
    }
  }

  for (let install of installs) {
    install.addListener({
      onDownloadCancelled: maybeRemove,
      onDownloadFailed: maybeRemove,
      onInstallFailed: maybeRemove,
      onInstallEnded: maybeRemove,
    });
  }
}

var gXPInstallObserver = {
  _findChildShell(aDocShell, aSoughtShell) {
    if (aDocShell == aSoughtShell)
      return aDocShell;

    var node = aDocShell.QueryInterface(Ci.nsIDocShellTreeItem);
    for (var i = 0; i < node.childCount; ++i) {
      var docShell = node.getChildAt(i);
      docShell = this._findChildShell(docShell, aSoughtShell);
      if (docShell == aSoughtShell)
        return docShell;
    }
    return null;
  },

  _getBrowser(aDocShell) {
    for (let browser of gBrowser.browsers) {
      if (this._findChildShell(browser.docShell, aDocShell))
        return browser;
    }
    return null;
  },

  pendingInstalls: new WeakMap(),

  showInstallConfirmation(browser, installInfo, height = undefined) {
    // If the confirmation notification is already open cache the installInfo
    // and the new confirmation will be shown later
    if (PopupNotifications.getNotification("addon-install-confirmation", browser)) {
      let pending = this.pendingInstalls.get(browser);
      if (pending) {
        pending.push(installInfo);
      } else {
        this.pendingInstalls.set(browser, [installInfo]);
      }
      return;
    }

    let showNextConfirmation = () => {
      // Make sure the browser is still alive.
      if (!gBrowser.browsers.includes(browser))
        return;

      let pending = this.pendingInstalls.get(browser);
      if (pending && pending.length)
        this.showInstallConfirmation(browser, pending.shift());
    };

    // If all installs have already been cancelled in some way then just show
    // the next confirmation
    if (installInfo.installs.every(i => i.state != AddonManager.STATE_DOWNLOADED)) {
      showNextConfirmation();
      return;
    }

    const anchorID = "addons-notification-icon";

    // Make notifications persistent
    var options = {
      displayURI: installInfo.originatingURI,
      persistent: true,
      hideClose: true,
    };

    let acceptInstallation = () => {
      for (let install of installInfo.installs)
        install.install();
      installInfo = null;

      Services.telemetry
              .getHistogramById("SECURITY_UI")
              .add(Ci.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL_CLICK_THROUGH);
    };

    let cancelInstallation = () => {
      if (installInfo) {
        for (let install of installInfo.installs) {
          // The notification may have been closed because the add-ons got
          // cancelled elsewhere, only try to cancel those that are still
          // pending install.
          if (install.state != AddonManager.STATE_CANCELLED)
            install.cancel();
        }
      }

      showNextConfirmation();
    };

    let unsigned = installInfo.installs.filter(i => i.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING);
    let someUnsigned = unsigned.length > 0 && unsigned.length < installInfo.installs.length;

    options.eventCallback = (aEvent) => {
      switch (aEvent) {
        case "removed":
          cancelInstallation();
          break;
        case "shown":
          let addonList = document.getElementById("addon-install-confirmation-content");
          while (addonList.firstChild)
            addonList.firstChild.remove();

          for (let install of installInfo.installs) {
            let container = document.createXULElement("hbox");

            let name = document.createXULElement("label");
            name.setAttribute("value", install.addon.name);
            name.setAttribute("class", "addon-install-confirmation-name");
            container.appendChild(name);

            if (someUnsigned && install.addon.signedState <= AddonManager.SIGNEDSTATE_MISSING) {
              let unsignedLabel = document.createXULElement("label");
              unsignedLabel.setAttribute("value",
                gNavigatorBundle.getString("addonInstall.unsigned"));
              unsignedLabel.setAttribute("class",
                "addon-install-confirmation-unsigned");
              container.appendChild(unsignedLabel);
            }

            addonList.appendChild(container);
          }
          break;
      }
    };

    options.learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL");

    let messageString;
    let notification = document.getElementById("addon-install-confirmation-notification");
    if (unsigned.length == installInfo.installs.length) {
      // None of the add-ons are verified
      messageString = gNavigatorBundle.getString("addonConfirmInstallUnsigned.message");
      notification.setAttribute("warning", "true");
      options.learnMoreURL += "unsigned-addons";
    } else if (unsigned.length == 0) {
      // All add-ons are verified or don't need to be verified
      messageString = gNavigatorBundle.getString("addonConfirmInstall.message");
      notification.removeAttribute("warning");
      options.learnMoreURL += "find-and-install-add-ons";
    } else {
      // Some of the add-ons are unverified, the list of names will indicate
      // which
      messageString = gNavigatorBundle.getString("addonConfirmInstallSomeUnsigned.message");
      notification.setAttribute("warning", "true");
      options.learnMoreURL += "unsigned-addons";
    }

    let brandBundle = document.getElementById("bundle_brand");
    let brandShortName = brandBundle.getString("brandShortName");

    messageString = PluralForm.get(installInfo.installs.length, messageString);
    messageString = messageString.replace("#1", brandShortName);
    messageString = messageString.replace("#2", installInfo.installs.length);

    let action = {
      label: gNavigatorBundle.getString("addonInstall.acceptButton2.label"),
      accessKey: gNavigatorBundle.getString("addonInstall.acceptButton2.accesskey"),
      callback: acceptInstallation,
    };

    let secondaryAction = {
      label: gNavigatorBundle.getString("addonInstall.cancelButton.label"),
      accessKey: gNavigatorBundle.getString("addonInstall.cancelButton.accesskey"),
      callback: () => {},
    };

    if (height) {
      notification.style.minHeight = height + "px";
    }

    let tab = gBrowser.getTabForBrowser(browser);
    if (tab) {
      gBrowser.selectedTab = tab;
    }

    let popup = PopupNotifications.show(browser, "addon-install-confirmation",
                                        messageString, anchorID, action,
                                        [secondaryAction], options);

    removeNotificationOnEnd(popup, installInfo.installs);

    Services.telemetry
            .getHistogramById("SECURITY_UI")
            .add(Ci.nsISecurityUITelemetry.WARNING_CONFIRM_ADDON_INSTALL);
  },

  // IDs of addon install related notifications
  NOTIFICATION_IDS: [
    "addon-install-blocked",
    "addon-install-complete",
    "addon-install-confirmation",
    "addon-install-failed",
    "addon-install-origin-blocked",
    "addon-progress",
    "addon-webext-permissions",
    "xpinstall-disabled",
  ],

  // Remove all opened addon installation notifications
  removeAllNotifications(browser) {
    this.NOTIFICATION_IDS.forEach((id) => {
      let notification = PopupNotifications.getNotification(id, browser);
      if (notification) {
        PopupNotifications.remove(notification);
      }
    });
  },

  observe(aSubject, aTopic, aData) {
    var brandBundle = document.getElementById("bundle_brand");
    var installInfo = aSubject.wrappedJSObject;
    var browser = installInfo.browser;

    // Make sure the browser is still alive.
    if (!browser || !gBrowser.browsers.includes(browser))
      return;

    const anchorID = "addons-notification-icon";
    var messageString, action;
    var brandShortName = brandBundle.getString("brandShortName");

    var notificationID = aTopic;
    // Make notifications persistent
    var options = {
      displayURI: installInfo.originatingURI,
      persistent: true,
      hideClose: true,
      timeout: Date.now() + 30000,
    };

    switch (aTopic) {
    case "addon-install-disabled": {
      notificationID = "xpinstall-disabled";
      let secondaryActions = null;

      if (Services.prefs.prefIsLocked("xpinstall.enabled")) {
        messageString = gNavigatorBundle.getString("xpinstallDisabledMessageLocked");
      } else {
        messageString = gNavigatorBundle.getString("xpinstallDisabledMessage");

        action = {
          label: gNavigatorBundle.getString("xpinstallDisabledButton"),
          accessKey: gNavigatorBundle.getString("xpinstallDisabledButton.accesskey"),
          callback: function editPrefs() {
            Services.prefs.setBoolPref("xpinstall.enabled", true);
          },
        };

        secondaryActions = [{
          label: gNavigatorBundle.getString("addonInstall.cancelButton.label"),
          accessKey: gNavigatorBundle.getString("addonInstall.cancelButton.accesskey"),
          callback: () => {},
        }];
      }

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, secondaryActions, options);
      break; }
    case "addon-install-origin-blocked": {
      messageString = gNavigatorBundle.getFormattedString("xpinstallPromptMessage",
                        [brandShortName]);

      options.removeOnDismissal = true;
      options.persistent = false;

      let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
      secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED);
      let popup = PopupNotifications.show(browser, notificationID,
                                          messageString, anchorID,
                                          null, null, options);
      removeNotificationOnEnd(popup, installInfo.installs);
      break; }
    case "addon-install-blocked": {
      let hasHost = !!options.displayURI;
      if (hasHost) {
        messageString = gNavigatorBundle.getFormattedString("xpinstallPromptMessage.header", ["<>"]);
        options.name = options.displayURI.displayHost;
      } else {
        messageString = gNavigatorBundle.getString("xpinstallPromptMessage.header.unknown");
      }
      // displayURI becomes it's own label, so we unset it for this panel. It will become part of the
      // messageString above.
      options.displayURI = undefined;

      options.eventCallback = (topic) => {
        if (topic !== "showing") {
          return;
        }
        let doc = browser.ownerDocument;
        let message = doc.getElementById("addon-install-blocked-message");
        // We must remove any prior use of this panel message in this window.
        while (message.firstChild) {
          message.firstChild.remove();
        }
        if (hasHost) {
          let text = gNavigatorBundle.getString("xpinstallPromptMessage.message");
          let b = doc.createElementNS("http://www.w3.org/1999/xhtml", "b");
          b.textContent = options.name;
          let fragment = BrowserUtils.getLocalizedFragment(doc, text, b);
          message.appendChild(fragment);
        } else {
          message.textContent = gNavigatorBundle.getString("xpinstallPromptMessage.message.unknown");
        }
        let learnMore = doc.getElementById("addon-install-blocked-info");
        learnMore.textContent = gNavigatorBundle.getString("xpinstallPromptMessage.learnMore");
        learnMore.setAttribute("href", Services.urlFormatter.formatURLPref("app.support.baseURL") + "unlisted-extensions-risks");
      };

      let secHistogram = Services.telemetry.getHistogramById("SECURITY_UI");
      action = {
        label: gNavigatorBundle.getString("xpinstallPromptMessage.install"),
        accessKey: gNavigatorBundle.getString("xpinstallPromptMessage.install.accesskey"),
        callback() {
          secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED_CLICK_THROUGH);
          installInfo.install();
        },
      };
      let dontAllowAction = {
        label: gNavigatorBundle.getString("xpinstallPromptMessage.dontAllow"),
        accessKey: gNavigatorBundle.getString("xpinstallPromptMessage.dontAllow.accesskey"),
        callback: () => {
          for (let install of installInfo.installs) {
            if (install.state != AddonManager.STATE_CANCELLED) {
              install.cancel();
            }
          }
        },
      };
      let neverAllowAction = {
        label: gNavigatorBundle.getString("xpinstallPromptMessage.neverAllow"),
        accessKey: gNavigatorBundle.getString("xpinstallPromptMessage.neverAllow.accesskey"),
        callback: () => {
          SitePermissions.set(browser.currentURI, "install", SitePermissions.BLOCK);
          for (let install of installInfo.installs) {
            if (install.state != AddonManager.STATE_CANCELLED) {
              install.cancel();
            }
          }
        },
      };

      secHistogram.add(Ci.nsISecurityUITelemetry.WARNING_ADDON_ASKING_PREVENTED);
      let popup = PopupNotifications.show(browser, notificationID,
                                          messageString, anchorID,
                                          action, [dontAllowAction, neverAllowAction], options);
      removeNotificationOnEnd(popup, installInfo.installs);
      break; }
    case "addon-install-started": {
      let needsDownload = function needsDownload(aInstall) {
        return aInstall.state != AddonManager.STATE_DOWNLOADED;
      };
      // If all installs have already been downloaded then there is no need to
      // show the download progress
      if (!installInfo.installs.some(needsDownload))
        return;
      notificationID = "addon-progress";
      messageString = gNavigatorBundle.getString("addonDownloadingAndVerifying");
      messageString = PluralForm.get(installInfo.installs.length, messageString);
      messageString = messageString.replace("#1", installInfo.installs.length);
      options.installs = installInfo.installs;
      options.contentWindow = browser.contentWindow;
      options.sourceURI = browser.currentURI;
      options.eventCallback = function(aEvent) {
        switch (aEvent) {
          case "shown":
            let notificationElement = [...this.owner.panel.children]
                                      .find(n => n.notification == this);
            if (notificationElement) {
              notificationElement.setAttribute("mainactiondisabled", "true");
            }
            break;
          case "removed":
            options.contentWindow = null;
            options.sourceURI = null;
            break;
        }
      };
      action = {
        label: gNavigatorBundle.getString("addonInstall.acceptButton2.label"),
        accessKey: gNavigatorBundle.getString("addonInstall.acceptButton2.accesskey"),
        callback: () => {},
      };
      let secondaryAction = {
        label: gNavigatorBundle.getString("addonInstall.cancelButton.label"),
        accessKey: gNavigatorBundle.getString("addonInstall.cancelButton.accesskey"),
        callback: () => {
          for (let install of installInfo.installs) {
            if (install.state != AddonManager.STATE_CANCELLED) {
              install.cancel();
            }
          }
        },
      };
      let notification = PopupNotifications.show(browser, notificationID, messageString,
                                                 anchorID, action,
                                                 [secondaryAction], options);
      notification._startTime = Date.now();

      break; }
    case "addon-install-failed": {
      options.removeOnDismissal = true;
      options.persistent = false;

      // TODO This isn't terribly ideal for the multiple failure case
      for (let install of installInfo.installs) {
        let host;
        try {
          host  = options.displayURI.host;
        } catch (e) {
          // displayURI might be missing or 'host' might throw for non-nsStandardURL nsIURIs.
        }

        if (!host)
          host = (install.sourceURI instanceof Ci.nsIStandardURL) &&
                 install.sourceURI.host;

        let error = (host || install.error == 0) ? "addonInstallError" : "addonLocalInstallError";
        let args;
        if (install.error < 0) {
          error += install.error;
          args = [brandShortName, install.name];
        } else if (install.addon.blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED) {
          error += "Blocklisted";
          args = [install.name];
        } else {
          error += "Incompatible";
          args = [brandShortName, Services.appinfo.version, install.name];
        }

        if (install.addon && !Services.policies.mayInstallAddon(install.addon)) {
          error = "addonInstallBlockedByPolicy";
          let extensionSettings = Services.policies.getExtensionSettings(install.addon.id);
          let message = "";
          if (extensionSettings && "blocked_install_message" in extensionSettings) {
            message = " " + extensionSettings.blocked_install_message;
          }
          args = [install.name, install.addon.id, message];
        }

        // Add Learn More link when refusing to install an unsigned add-on
        if (install.error == AddonManager.ERROR_SIGNEDSTATE_REQUIRED) {
          options.learnMoreURL = Services.urlFormatter.formatURLPref("app.support.baseURL") + "unsigned-addons";
        }

        messageString = gNavigatorBundle.getFormattedString(error, args);

        PopupNotifications.show(browser, notificationID, messageString, anchorID,
                                action, null, options);

        // Can't have multiple notifications with the same ID, so stop here.
        break;
      }
      this._removeProgressNotification(browser);
      break; }
    case "addon-install-confirmation": {
      let showNotification = () => {
        let height = undefined;

        if (PopupNotifications.isPanelOpen) {
          let rect = document.getElementById("addon-progress-notification").getBoundingClientRect();
          height = rect.height;
        }

        this._removeProgressNotification(browser);
        this.showInstallConfirmation(browser, installInfo, height);
      };

      let progressNotification = PopupNotifications.getNotification("addon-progress", browser);
      if (progressNotification) {
        let downloadDuration = Date.now() - progressNotification._startTime;
        let securityDelay = Services.prefs.getIntPref("security.dialog_enable_delay") - downloadDuration;
        if (securityDelay > 0) {
          setTimeout(() => {
            // The download may have been cancelled during the security delay
            if (PopupNotifications.getNotification("addon-progress", browser))
              showNotification();
          }, securityDelay);
          break;
        }
      }
      showNotification();
      break; }
    case "addon-install-complete": {
      let secondaryActions = null;
      let numAddons = installInfo.installs.length;

      if (numAddons == 1) {
        messageString = gNavigatorBundle.getFormattedString("addonInstalled",
                                                            [installInfo.installs[0].name]);
      } else {
        messageString = gNavigatorBundle.getString("addonsGenericInstalled");
        messageString = PluralForm.get(numAddons, messageString);
        messageString = messageString.replace("#1", numAddons);
      }
      action = null;

      options.removeOnDismissal = true;
      options.persistent = false;

      PopupNotifications.show(browser, notificationID, messageString, anchorID,
                              action, secondaryActions, options);
      break; }
    }
  },
  _removeProgressNotification(aBrowser) {
    let notification = PopupNotifications.getNotification("addon-progress", aBrowser);
    if (notification)
      notification.remove();
  },
};

var gExtensionsNotifications = {
  initialized: false,
  init() {
    this.updateAlerts();
    this.boundUpdate = this.updateAlerts.bind(this);
    ExtensionsUI.on("change", this.boundUpdate);
    this.initialized = true;
  },

  uninit() {
    // uninit() can race ahead of init() in some cases, if that happens,
    // we have no handler to remove.
    if (!this.initialized) {
      return;
    }
    ExtensionsUI.off("change", this.boundUpdate);
  },

  _createAddonButton(text, icon, callback) {
    let button = document.createXULElement("toolbarbutton");
    button.setAttribute("label", text);
    button.setAttribute("tooltiptext", text);
    const DEFAULT_EXTENSION_ICON =
      "chrome://mozapps/skin/extensions/extensionGeneric.svg";
    button.setAttribute("image", icon || DEFAULT_EXTENSION_ICON);
    button.className = "addon-banner-item";

    button.addEventListener("command", callback);
    PanelUI.addonNotificationContainer.appendChild(button);
  },

  updateAlerts() {
    let sideloaded = ExtensionsUI.sideloaded;
    let updates = ExtensionsUI.updates;

    let container = PanelUI.addonNotificationContainer;

    while (container.firstChild) {
      container.firstChild.remove();
    }

    let items = 0;
    for (let update of updates) {
      if (++items > 4) {
        break;
      }
      let text = gNavigatorBundle.getFormattedString("webextPerms.updateMenuItem", [update.addon.name]);
      this._createAddonButton(text, update.addon.iconURL, evt => {
        ExtensionsUI.showUpdate(gBrowser, update);
      });
    }

    let appName;
    for (let addon of sideloaded) {
      if (++items > 4) {
        break;
      }
      if (!appName) {
        let brandBundle = document.getElementById("bundle_brand");
        appName = brandBundle.getString("brandShortName");
      }

      let text = gNavigatorBundle.getFormattedString("webextPerms.sideloadMenuItem", [addon.name, appName]);
      this._createAddonButton(text, addon.iconURL, evt => {
        // We need to hide the main menu manually because the toolbarbutton is
        // removed immediately while processing this event, and PanelUI is
        // unable to identify which panel should be closed automatically.
        PanelUI.hide();
        ExtensionsUI.showSideloaded(gBrowser, addon);
      });
    }
  },
};
