# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

function getPluginInfo(pluginElement)
{
  var tagMimetype;
  var pluginsPage;
  if (pluginElement instanceof HTMLAppletElement) {
    tagMimetype = "application/x-java-vm";
  } else {
    if (pluginElement instanceof HTMLObjectElement) {
      pluginsPage = pluginElement.getAttribute("codebase");
    } else {
      pluginsPage = pluginElement.getAttribute("pluginspage");
    }

    // only attempt if a pluginsPage is defined.
    if (pluginsPage) {
      var doc = pluginElement.ownerDocument;
      var docShell = findChildShell(doc, gBrowser.docShell, null);
      try {
        pluginsPage = makeURI(pluginsPage, doc.characterSet, docShell.currentURI).spec;
      } catch (ex) {
        pluginsPage = "";
      }
    }

    tagMimetype = pluginElement.QueryInterface(Components.interfaces.nsIObjectLoadingContent)
                               .actualType;

    if (tagMimetype == "") {
      tagMimetype = pluginElement.type;
    }
  }

  return {mimetype: tagMimetype, pluginsPage: pluginsPage};
}

var gPluginHandler = {

#ifdef MOZ_CRASHREPORTER
  get CrashSubmit() {
    delete this.CrashSubmit;
    Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
    return this.CrashSubmit;
  },
#endif

  // Map the plugin's name to a filtered version more suitable for user UI.
  makeNicePluginName : function (aName, aFilename) {
    if (aName == "Shockwave Flash")
      return "Adobe Flash";

    // Clean up the plugin name by stripping off any trailing version numbers
    // or "plugin". EG, "Foo Bar Plugin 1.23_02" --> "Foo Bar"
    let newName = aName.replace(/\bplug-?in\b/i, "").replace(/[\s\d\.\-\_\(\)]+$/, "");
    return newName;
  },

  isTooSmall : function (plugin, overlay) {
    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginRect.width) ||
                    (overlay.scrollHeight - 5 > pluginRect.height);
    return overflows;
  },

  addLinkClickCallback: function (linkNode, callbackName /*callbackArgs...*/) {
    // XXX just doing (callback)(arg) was giving a same-origin error. bug?
    let self = this;
    let callbackArgs = Array.prototype.slice.call(arguments).slice(2);
    linkNode.addEventListener("click",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                evt.preventDefault();
                                if (callbackArgs.length == 0)
                                  callbackArgs = [ evt ];
                                (self[callbackName]).apply(self, callbackArgs);
                              },
                              true);

    linkNode.addEventListener("keydown",
                              function(evt) {
                                if (!evt.isTrusted)
                                  return;
                                if (evt.keyCode == evt.DOM_VK_RETURN) {
                                  evt.preventDefault();
                                  if (callbackArgs.length == 0)
                                    callbackArgs = [ evt ];
                                  evt.preventDefault();
                                  (self[callbackName]).apply(self, callbackArgs);
                                }
                              },
                              true);
  },

  handleEvent : function(event) {
    let self = gPluginHandler;
    let plugin = event.target;
    let doc = plugin.ownerDocument;

    // We're expecting the target to be a plugin.
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return;

    // Force a style flush, so that we ensure our binding is attached.
    plugin.clientTop;

    switch (event.type) {
      case "PluginCrashed":
        self.pluginInstanceCrashed(plugin, event);
        break;

      case "PluginNotFound":
        // For non-object plugin tags, register a click handler to install the
        // plugin. Object tags can, and often do, deal with that themselves,
        // so don't stomp on the page developers toes.
        if (!(plugin instanceof HTMLObjectElement)) {
          // We don't yet check to see if there's actually an installer available.
          let installStatus = doc.getAnonymousElementByAttribute(plugin, "class", "installStatus");
          installStatus.setAttribute("status", "ready");
          let iconStatus = doc.getAnonymousElementByAttribute(plugin, "class", "icon");
          iconStatus.setAttribute("status", "ready");

          let installLink = doc.getAnonymousElementByAttribute(plugin, "class", "installPluginLink");
          self.addLinkClickCallback(installLink, "installSinglePlugin", plugin);
        }
        /* FALLTHRU */

      case "PluginBlocklisted":
      case "PluginOutdated":
#ifdef XP_MACOSX
      case "npapi-carbon-event-model-failure":
#endif
        self.pluginUnavailable(plugin, event.type);
        break;

      case "PluginClickToPlay":
        self._handleClickToPlayEvent(plugin);
        break;

      case "PluginDisabled":
        let manageLink = doc.getAnonymousElementByAttribute(plugin, "class", "managePluginsLink");
        self.addLinkClickCallback(manageLink, "managePlugins");
        break;
    }

    // Hide the in-content UI if it's too big. The crashed plugin handler already did this.
    if (event.type != "PluginCrashed" && event.type != "PluginClickToPlay") {
      let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
      if (self.isTooSmall(plugin, overlay))
          overlay.style.visibility = "hidden";
    }
  },

  activatePlugins: function PH_activatePlugins(aContentWindow) {
    let browser = gBrowser.getBrowserForDocument(aContentWindow.document);
    browser._clickToPlayPluginsActivated = true;
    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    for (let plugin of plugins) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (!objLoadingContent.activated)
        objLoadingContent.playPlugin();
    }
    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);
    if (notification)
      notification.remove();
  },

  activateSinglePlugin: function PH_activateSinglePlugin(aContentWindow, aPlugin) {
    let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    if (!objLoadingContent.activated)
      objLoadingContent.playPlugin();

    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let haveUnplayedPlugins = cwu.plugins.some(function(plugin) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      return (plugin != aPlugin && !objLoadingContent.activated);
    });
    let browser = gBrowser.getBrowserForDocument(aContentWindow.document);
    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);
    if (notification && !haveUnplayedPlugins) {
      browser._clickToPlayDoorhangerShown = false;
      notification.remove();
    }
  },

  newPluginInstalled : function(event) {
    // browser elements are anonymous so we can't just use target.
    var browser = event.originalTarget;
    // clear the plugin list, now that at least one plugin has been installed
    browser.missingPlugins = null;

    var notificationBox = gBrowser.getNotificationBox(browser);
    var notification = notificationBox.getNotificationWithValue("missing-plugins");
    if (notification)
      notificationBox.removeNotification(notification);

    // reload the browser to make the new plugin show.
    browser.reload();
  },

  // Callback for user clicking on a missing (unsupported) plugin.
  installSinglePlugin: function (plugin) {
    var missingPluginsArray = {};

    var pluginInfo = getPluginInfo(plugin);
    missingPluginsArray[pluginInfo.mimetype] = pluginInfo;

    openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
               "PFSWindow", "chrome,centerscreen,resizable=yes",
               {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
  },

  // Callback for user clicking on a disabled plugin
  managePlugins: function (aEvent) {
    BrowserOpenAddonsMgr("addons://list/plugin");
  },

#ifdef MOZ_CRASHREPORTER
  // Callback for user clicking "submit a report" link
  submitReport : function(pluginDumpID, browserDumpID) {
    // The crash reporter wants a DOM element it can append an IFRAME to,
    // which it uses to submit a form. Let's just give it gBrowser.
    this.CrashSubmit.submit(pluginDumpID);
    if (browserDumpID)
      this.CrashSubmit.submit(browserDumpID);
  },
#endif

  // Callback for user clicking a "reload page" link
  reloadPage: function (browser) {
    browser.reload();
  },

  // Callback for user clicking the help icon
  openHelpPage: function () {
    openHelpLink("plugin-crashed", false);
  },

  // Event listener for click-to-play plugins.
  _handleClickToPlayEvent: function PH_handleClickToPlayEvent(aPlugin) {
    let doc = aPlugin.ownerDocument;
    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
    let pluginsPermission = Services.perms.testPermission(browser.currentURI, "plugins");
    let overlay = doc.getAnonymousElementByAttribute(aPlugin, "class", "mainBox");

    if (browser._clickToPlayPluginsActivated) {
      let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
      objLoadingContent.playPlugin();
      return;
    } else if (pluginsPermission == Ci.nsIPermissionManager.DENY_ACTION) {
      overlay.style.visibility = "hidden";
      return;
    }

    let overlay = doc.getAnonymousElementByAttribute(aPlugin, "class", "mainBox");
    // The overlay is null if the XBL binding is not attached (element is display:none).
    if (overlay) {
      overlay.addEventListener("click", function(aEvent) {
        if (aEvent.button == 0 && aEvent.isTrusted)
          gPluginHandler.activateSinglePlugin(aEvent.target.ownerDocument.defaultView.top, aPlugin);
      }, true);
    }

    if (!browser._clickToPlayDoorhangerShown)
      gPluginHandler._showClickToPlayNotification(browser);
  },

  reshowClickToPlayNotification: function PH_reshowClickToPlayNotification() {
    if (!Services.prefs.getBoolPref("plugins.click_to_play"))
      return;

    let browser = gBrowser.selectedBrowser;

    let pluginsPermission = Services.perms.testPermission(browser.currentURI, "plugins");
    if (pluginsPermission == Ci.nsIPermissionManager.DENY_ACTION)
      return;

    let contentWindow = browser.contentWindow;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let pluginNeedsActivation = cwu.plugins.some(function(plugin) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      return !objLoadingContent.activated;
    });
    if (pluginNeedsActivation)
      gPluginHandler._showClickToPlayNotification(browser);
  },

  _showClickToPlayNotification: function PH_showClickToPlayNotification(aBrowser) {
    aBrowser._clickToPlayDoorhangerShown = true;
    let contentWindow = aBrowser.contentWindow;

    let messageString = gNavigatorBundle.getString("activatePluginsMessage.message");
    let mainAction = {
      label: gNavigatorBundle.getString("activatePluginsMessage.label"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.accesskey"),
      callback: function() { gPluginHandler.activatePlugins(contentWindow); }
    };
    let secondaryActions = [{
      label: gNavigatorBundle.getString("activatePluginsMessage.always"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.always.accesskey"),
      callback: function () {
        Services.perms.add(aBrowser.currentURI, "plugins", Ci.nsIPermissionManager.ALLOW_ACTION);
        gPluginHandler.activatePlugins(contentWindow);
      }
    },{
      label: gNavigatorBundle.getString("activatePluginsMessage.never"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.never.accesskey"),
      callback: function () {
        Services.perms.add(aBrowser.currentURI, "plugins", Ci.nsIPermissionManager.DENY_ACTION);
        let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
        if (notification)
          notification.remove();
        gPluginHandler._removeClickToPlayOverlays(contentWindow);
      }
    }];
    let options = { dismissed: true };
    PopupNotifications.show(aBrowser, "click-to-play-plugins",
                            messageString, "plugins-notification-icon",
                            mainAction, secondaryActions, options);
  },

  _removeClickToPlayOverlays: function PH_removeClickToPlayOverlays(aContentWindow) {
    let doc = aContentWindow.document;
    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    for (let plugin of cwu.plugins) {
      let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
      overlay.style.visibility = "hidden";
    }
  },

  // event listener for missing/blocklisted/outdated/carbonFailure plugins.
  pluginUnavailable: function (plugin, eventType) {
    let browser = gBrowser.getBrowserForDocument(plugin.ownerDocument
                                                       .defaultView.top.document);
    if (!browser.missingPlugins)
      browser.missingPlugins = {};

    var pluginInfo = getPluginInfo(plugin);
    browser.missingPlugins[pluginInfo.mimetype] = pluginInfo;

    var notificationBox = gBrowser.getNotificationBox(browser);

    // Should only display one of these warnings per page.
    // In order of priority, they are: outdated > missing > blocklisted
    let outdatedNotification = notificationBox.getNotificationWithValue("outdated-plugins");
    let blockedNotification  = notificationBox.getNotificationWithValue("blocked-plugins");
    let missingNotification  = notificationBox.getNotificationWithValue("missing-plugins");


    function showBlocklistInfo() {
      var url = formatURL("extensions.blocklist.detailsURL", true);
      gBrowser.loadOneTab(url, {inBackground: false});
      return true;
    }

    function showOutdatedPluginsInfo() {
      gPrefService.setBoolPref("plugins.update.notifyUser", false);
      var url = formatURL("plugins.update.url", true);
      gBrowser.loadOneTab(url, {inBackground: false});
      return true;
    }

    function showPluginsMissing() {
      // get the urls of missing plugins
      var missingPluginsArray = gBrowser.selectedBrowser.missingPlugins;
      if (missingPluginsArray) {
        openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                   "PFSWindow", "chrome,centerscreen,resizable=yes",
                   {plugins: missingPluginsArray, browser: gBrowser.selectedBrowser});
      }
    }

#ifdef XP_MACOSX
    function carbonFailurePluginsRestartBrowser()
    {
      // Notify all windows that an application quit has been requested.
      let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].
                         createInstance(Ci.nsISupportsPRBool);
      Services.obs.notifyObservers(cancelQuit, "quit-application-requested", null);

      // Something aborted the quit process.
      if (cancelQuit.data)
        return;

      let as = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      as.quit(Ci.nsIAppStartup.eRestarti386 | Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
    }
#endif

    let notifications = {
      PluginBlocklisted : {
                            barID   : "blocked-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginBlocked.png",
                            message : gNavigatorBundle.getString("blockedpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("blockedpluginsMessage.infoButton.label"),
                                         accessKey : gNavigatorBundle.getString("blockedpluginsMessage.infoButton.accesskey"),
                                         popup     : null,
                                         callback  : showBlocklistInfo
                                       },
                                       {
                                         label     : gNavigatorBundle.getString("blockedpluginsMessage.searchButton.label"),
                                         accessKey : gNavigatorBundle.getString("blockedpluginsMessage.searchButton.accesskey"),
                                         popup     : null,
                                         callback  : showOutdatedPluginsInfo
                                      }],
                          },
      PluginOutdated    : {
                            barID   : "outdated-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginOutdated.png",
                            message : gNavigatorBundle.getString("outdatedpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("outdatedpluginsMessage.updateButton.label"),
                                         accessKey : gNavigatorBundle.getString("outdatedpluginsMessage.updateButton.accesskey"),
                                         popup     : null,
                                         callback  : showOutdatedPluginsInfo
                                      }],
                          },
      PluginNotFound    : {
                            barID   : "missing-plugins",
                            iconURL : "chrome://mozapps/skin/plugins/notifyPluginGeneric.png",
                            message : gNavigatorBundle.getString("missingpluginsMessage.title"),
                            buttons : [{
                                         label     : gNavigatorBundle.getString("missingpluginsMessage.button.label"),
                                         accessKey : gNavigatorBundle.getString("missingpluginsMessage.button.accesskey"),
                                         popup     : null,
                                         callback  : showPluginsMissing
                                      }],
                            },
#ifdef XP_MACOSX
      "npapi-carbon-event-model-failure" : {
                                             barID    : "carbon-failure-plugins",
                                             iconURL  : "chrome://mozapps/skin/plugins/notifyPluginGeneric.png",
                                             message  : gNavigatorBundle.getString("carbonFailurePluginsMessage.message"),
                                             buttons: [{
                                                         label     : gNavigatorBundle.getString("carbonFailurePluginsMessage.restartButton.label"),
                                                         accessKey : gNavigatorBundle.getString("carbonFailurePluginsMessage.restartButton.accesskey"),
                                                         popup     : null,
                                                         callback  : carbonFailurePluginsRestartBrowser
                                                      }],
                            }
#endif
    };

    // If there is already an outdated plugin notification then do nothing
    if (outdatedNotification)
      return;

#ifdef XP_MACOSX
    if (eventType == "npapi-carbon-event-model-failure") {
      if (gPrefService.getBoolPref("plugins.hide_infobar_for_carbon_failure_plugin"))
        return;

      let carbonFailureNotification =
        notificationBox.getNotificationWithValue("carbon-failure-plugins");

      if (carbonFailureNotification)
         carbonFailureNotification.close();

      let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].getService(Ci.nsIMacUtils);
      // if this is not a Universal build, just follow PluginNotFound path
      if (!macutils.isUniversalBinary)
        eventType = "PluginNotFound";
    }
#endif

    if (eventType == "PluginBlocklisted") {
      if (gPrefService.getBoolPref("plugins.hide_infobar_for_missing_plugin")) // XXX add a new pref?
        return;

      if (blockedNotification || missingNotification)
        return;
    }
    else if (eventType == "PluginOutdated") {
      if (gPrefService.getBoolPref("plugins.hide_infobar_for_outdated_plugin"))
        return;

      // Cancel any notification about blocklisting/missing plugins
      if (blockedNotification)
        blockedNotification.close();
      if (missingNotification)
        missingNotification.close();
    }
    else if (eventType == "PluginNotFound") {
      if (gPrefService.getBoolPref("plugins.hide_infobar_for_missing_plugin"))
        return;

      if (missingNotification)
        return;

      // Cancel any notification about blocklisting plugins
      if (blockedNotification)
        blockedNotification.close();
    }

    let notify = notifications[eventType];
    notificationBox.appendNotification(notify.message, notify.barID, notify.iconURL,
                                       notificationBox.PRIORITY_WARNING_MEDIUM,
                                       notify.buttons);
  },

  // Crashed-plugin observer. Notified once per plugin crash, before events
  // are dispatched to individual plugin instances.
  pluginCrashed : function(subject, topic, data) {
    let propertyBag = subject;
    if (!(propertyBag instanceof Ci.nsIPropertyBag2) ||
        !(propertyBag instanceof Ci.nsIWritablePropertyBag2))
     return;

#ifdef MOZ_CRASHREPORTER
    let pluginDumpID = propertyBag.getPropertyAsAString("pluginDumpID");
    let browserDumpID= propertyBag.getPropertyAsAString("browserDumpID");
    let shouldSubmit = gCrashReporter.submitReports;
    let doPrompt     = true; // XXX followup to get via gCrashReporter

    // Submit automatically when appropriate.
    if (pluginDumpID && shouldSubmit && !doPrompt) {
      this.submitReport(pluginDumpID, browserDumpID);
      // Submission is async, so we can't easily show failure UI.
      propertyBag.setPropertyAsBool("submittedCrashReport", true);
    }
#endif
  },

  // Crashed-plugin event listener. Called for every instance of a
  // plugin in content.
  pluginInstanceCrashed: function (plugin, aEvent) {
    // Ensure the plugin and event are of the right type.
    if (!(aEvent instanceof Ci.nsIDOMDataContainerEvent))
      return;

    let submittedReport = aEvent.getData("submittedCrashReport");
    let doPrompt        = true; // XXX followup for .getData("doPrompt");
    let submitReports   = true; // XXX followup for .getData("submitReports");
    let pluginName      = aEvent.getData("pluginName");
    let pluginFilename  = aEvent.getData("pluginFilename");
    let pluginDumpID    = aEvent.getData("pluginDumpID");
    let browserDumpID   = aEvent.getData("browserDumpID");

    // Remap the plugin name to a more user-presentable form.
    pluginName = this.makeNicePluginName(pluginName, pluginFilename);

    let messageString = gNavigatorBundle.getFormattedString("crashedpluginsMessage.title", [pluginName]);

    //
    // Configure the crashed-plugin placeholder.
    //
    let doc = plugin.ownerDocument;
    let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
    let statusDiv = doc.getAnonymousElementByAttribute(plugin, "class", "submitStatus");
#ifdef MOZ_CRASHREPORTER
    let status;

    // Determine which message to show regarding crash reports.
    if (submittedReport) { // submitReports && !doPrompt, handled in observer
      status = "submitted";
    }
    else if (!submitReports && !doPrompt) {
      status = "noSubmit";
    }
    else { // doPrompt
      status = "please";
      // XXX can we make the link target actually be blank?
      let pleaseLink = doc.getAnonymousElementByAttribute(
                            plugin, "class", "pleaseSubmitLink");
      this.addLinkClickCallback(pleaseLink, "submitReport",
                                pluginDumpID, browserDumpID);
    }

    // If we don't have a minidumpID, we can't (or didn't) submit anything.
    // This can happen if the plugin is killed from the task manager.
    if (!pluginDumpID) {
        status = "noReport";
    }

    statusDiv.setAttribute("status", status);

    let bottomLinks = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgBottomLinks");
    bottomLinks.style.display = "block";
    let helpIcon = doc.getAnonymousElementByAttribute(plugin, "class", "helpIcon");
    this.addLinkClickCallback(helpIcon, "openHelpPage");

    // If we're showing the link to manually trigger report submission, we'll
    // want to be able to update all the instances of the UI for this crash to
    // show an updated message when a report is submitted.
    if (doPrompt) {
      let observer = {
        QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                               Ci.nsISupportsWeakReference]),
        observe : function(subject, topic, data) {
          let propertyBag = subject;
          if (!(propertyBag instanceof Ci.nsIPropertyBag2))
            return;
          // Ignore notifications for other crashes.
          if (propertyBag.get("minidumpID") != pluginDumpID)
            return;
          statusDiv.setAttribute("status", data);
        },

        handleEvent : function(event) {
            // Not expected to be called, just here for the closure.
        }
      }

      // Use a weak reference, so we don't have to remove it...
      Services.obs.addObserver(observer, "crash-report-status", true);
      // ...alas, now we need something to hold a strong reference to prevent
      // it from being GC. But I don't want to manually manage the reference's
      // lifetime (which should be no greater than the page).
      // Clever solution? Use a closue with an event listener on the document.
      // When the doc goes away, so do the listener references and the closure.
      doc.addEventListener("mozCleverClosureHack", observer, false);
    }
#endif

    let crashText = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgCrashed");
    crashText.textContent = messageString;

    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);

    let link = doc.getAnonymousElementByAttribute(plugin, "class", "reloadLink");
    this.addLinkClickCallback(link, "reloadPage", browser);

    let notificationBox = gBrowser.getNotificationBox(browser);

    // Is the <object>'s size too small to hold what we want to show?
    if (this.isTooSmall(plugin, overlay)) {
        // Hide the overlay's contents. Use visibility style, so that it
        // doesn't collapse down to 0x0.
        overlay.style.visibility = "hidden";
        // If another plugin on the page was large enough to show our UI, we
        // don't want to show a notification bar.
        if (!doc.mozNoPluginCrashedNotification)
          showNotificationBar(pluginDumpID, browserDumpID);
    } else {
        // If a previous plugin on the page was too small and resulted in
        // adding a notification bar, then remove it because this plugin
        // instance it big enough to serve as in-content notification.
        hideNotificationBar();
        doc.mozNoPluginCrashedNotification = true;
    }

    function hideNotificationBar() {
      let notification = notificationBox.getNotificationWithValue("plugin-crashed");
      if (notification)
        notificationBox.removeNotification(notification, true);
    }

    function showNotificationBar(pluginDumpID, browserDumpID) {
      // If there's already an existing notification bar, don't do anything.
      let notification = notificationBox.getNotificationWithValue("plugin-crashed");
      if (notification)
        return;

      // Configure the notification bar
      let priority = notificationBox.PRIORITY_WARNING_MEDIUM;
      let iconURL = "chrome://mozapps/skin/plugins/notifyPluginCrashed.png";
      let reloadLabel = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.label");
      let reloadKey   = gNavigatorBundle.getString("crashedpluginsMessage.reloadButton.accesskey");
      let submitLabel = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.label");
      let submitKey   = gNavigatorBundle.getString("crashedpluginsMessage.submitButton.accesskey");

      let buttons = [{
        label: reloadLabel,
        accessKey: reloadKey,
        popup: null,
        callback: function() { browser.reload(); },
      }];
#ifdef MOZ_CRASHREPORTER
      let submitButton = {
        label: submitLabel,
        accessKey: submitKey,
        popup: null,
          callback: function() { gPluginHandler.submitReport(pluginDumpID, browserDumpID); },
      };
      if (pluginDumpID)
        buttons.push(submitButton);
#endif

      let notification = notificationBox.appendNotification(messageString, "plugin-crashed",
                                                            iconURL, priority, buttons);

      // Add the "learn more" link.
      let XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      let link = notification.ownerDocument.createElementNS(XULNS, "label");
      link.className = "text-link";
      link.setAttribute("value", gNavigatorBundle.getString("crashedpluginsMessage.learnMore"));
      let crashurl = formatURL("app.support.baseURL", true);
      crashurl += "plugin-crashed-notificationbar";
      link.href = crashurl;

      let description = notification.ownerDocument.getAnonymousElementByAttribute(notification, "anonid", "messageText");
      description.appendChild(link);

      // Remove the notfication when the page is reloaded.
      doc.defaultView.top.addEventListener("unload", function() {
        notificationBox.removeNotification(notification);
      }, false);
    }

  }
};
