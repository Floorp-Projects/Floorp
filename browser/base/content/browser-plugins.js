# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var gPluginHandler = {
  PREF_NOTIFY_MISSING_FLASH: "plugins.notifyMissingFlash",
  PREF_SESSION_PERSIST_MINUTES: "plugin.sessionPermissionNow.intervalInMinutes",
  PREF_PERSISTENT_DAYS: "plugin.persistentPermissionAlways.intervalInDays",

  getPluginUI: function (plugin, anonid) {
    return plugin.ownerDocument.
           getAnonymousElementByAttribute(plugin, "anonid", anonid);
  },

#ifdef MOZ_CRASHREPORTER
  get CrashSubmit() {
    delete this.CrashSubmit;
    Cu.import("resource://gre/modules/CrashSubmit.jsm", this);
    return this.CrashSubmit;
  },
#endif

  _getPluginInfo: function (pluginElement) {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    pluginElement.QueryInterface(Ci.nsIObjectLoadingContent);

    let tagMimetype;
    let pluginName = gNavigatorBundle.getString("pluginInfo.unknownPlugin");
    let pluginTag = null;
    let permissionString = null;
    let fallbackType = null;
    let blocklistState = null;

    if (pluginElement instanceof HTMLAppletElement) {
      tagMimetype = "application/x-java-vm";
    } else {
      tagMimetype = pluginElement.actualType;

      if (tagMimetype == "") {
        tagMimetype = pluginElement.type;
      }
    }

    if (gPluginHandler.isKnownPlugin(pluginElement)) {
      pluginTag = pluginHost.getPluginTagForType(pluginElement.actualType);
      pluginName = gPluginHandler.makeNicePluginName(pluginTag.name);

      permissionString = pluginHost.getPermissionStringForType(pluginElement.actualType);
      fallbackType = pluginElement.defaultFallbackType;
      blocklistState = pluginHost.getBlocklistStateForType(pluginElement.actualType);
      // Make state-softblocked == state-notblocked for our purposes,
      // they have the same UI. STATE_OUTDATED should not exist for plugin
      // items, but let's alias it anyway, just in case.
      if (blocklistState == Ci.nsIBlocklistService.STATE_SOFTBLOCKED ||
          blocklistState == Ci.nsIBlocklistService.STATE_OUTDATED) {
        blocklistState = Ci.nsIBlocklistService.STATE_NOT_BLOCKED;
      }
    }

    return { mimetype: tagMimetype,
             pluginName: pluginName,
             pluginTag: pluginTag,
             permissionString: permissionString,
             fallbackType: fallbackType,
             blocklistState: blocklistState,
           };
  },

  // Map the plugin's name to a filtered version more suitable for user UI.
  makeNicePluginName : function (aName) {
    if (aName == "Shockwave Flash")
      return "Adobe Flash";

    // Clean up the plugin name by stripping off parenthetical clauses,
    // trailing version numbers or "plugin".
    // EG, "Foo Bar (Linux) Plugin 1.23_02" --> "Foo Bar"
    // Do this by first stripping the numbers, etc. off the end, and then
    // removing "Plugin" (and then trimming to get rid of any whitespace).
    // (Otherwise, something like "Java(TM) Plug-in 1.7.0_07" gets mangled)
    let newName = aName.replace(/\(.*?\)/g, "").
                        replace(/[\s\d\.\-\_\(\)]+$/, "").
                        replace(/\bplug-?in\b/i, "").trim();
    return newName;
  },

  /**
   * Update the visibility of the plugin overlay.
   */
  setVisibility : function (plugin, overlay, shouldShow) {
    overlay.classList.toggle("visible", shouldShow);
  },

  /**
   * Check whether the plugin should be visible on the page. A plugin should
   * not be visible if the overlay is too big, or if any other page content
   * overlays it.
   *
   * This function will handle showing or hiding the overlay.
   * @returns true if the plugin is invisible.
   */
  shouldShowOverlay : function (plugin, overlay) {
    // If the overlay size is 0, we haven't done layout yet. Presume that
    // plugins are visible until we know otherwise.
    if (overlay.scrollWidth == 0) {
      return true;
    }

    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > pluginRect.width) ||
                    (overlay.scrollHeight - 5 > pluginRect.height);
    if (overflows) {
      return false;
    }

    // Is the plugin covered up by other content so that it is not clickable?
    // Floating point can confuse .elementFromPoint, so inset just a bit
    let left = pluginRect.left + 2;
    let right = pluginRect.right - 2;
    let top = pluginRect.top + 2;
    let bottom = pluginRect.bottom - 2;
    let centerX = left + (right - left) / 2;
    let centerY = top + (bottom - top) / 2;
    let points = [[left, top],
                   [left, bottom],
                   [right, top],
                   [right, bottom],
                   [centerX, centerY]];

    if (right <= 0 || top <= 0) {
      return false;
    }

    for (let [x, y] of points) {
      let el = plugin.ownerDocument.elementFromPoint(x, y);
      if (el !== plugin) {
        return false;
      }
    }

    return true;
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

  // Helper to get the binding handler type from a plugin object
  _getBindingType : function(plugin) {
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return null;

    switch (plugin.pluginFallbackType) {
      case Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED:
        return "PluginNotFound";
      case Ci.nsIObjectLoadingContent.PLUGIN_DISABLED:
        return "PluginDisabled";
      case Ci.nsIObjectLoadingContent.PLUGIN_BLOCKLISTED:
        return "PluginBlocklisted";
      case Ci.nsIObjectLoadingContent.PLUGIN_OUTDATED:
        return "PluginOutdated";
      case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
        return "PluginClickToPlay";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
        return "PluginVulnerableUpdatable";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
        return "PluginVulnerableNoUpdate";
      case Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW:
        return "PluginPlayPreview";
      default:
        // Not all states map to a handler
        return null;
    }
  },

  supportedPlugins: {
    "mimetypes": {
      "application/x-shockwave-flash": "flash",
      "application/futuresplash": "flash",
      "application/x-java-.*": "java",
      "application/x-director": "shockwave",
      "application/(sdp|x-(mpeg|rtsp|sdp))": "quicktime",
      "audio/(3gpp(2)?|AMR|aiff|basic|mid(i)?|mp4|mpeg|vnd\.qcelp|wav|x-(aiff|m4(a|b|p)|midi|mpeg|wav))": "quicktime",
      "image/(pict|png|tiff|x-(macpaint|pict|png|quicktime|sgi|targa|tiff))": "quicktime",
      "video/(3gpp(2)?|flc|mp4|mpeg|quicktime|sd-video|x-mpeg)": "quicktime",
      "application/x-unknown": "test",
    },

    "plugins": {
      "flash": {
        "displayName": "Flash",
        "installWINNT": true,
        "installDarwin": true,
        "installLinux": true,
      },
      "java": {
        "displayName": "Java",
        "installWINNT": true,
        "installDarwin": true,
        "installLinux": true,
      },
      "shockwave": {
        "displayName": "Shockwave",
        "installWINNT": true,
        "installDarwin": true,
      },
      "quicktime": {
        "displayName": "QuickTime",
        "installWINNT": true,
      },
      "test": {
        "displayName": "Test plugin",
        "installWINNT": true,
        "installLinux": true,
        "installDarwin": true,
      }
    }
  },

  nameForSupportedPlugin: function (aMimeType) {
    for (let type in this.supportedPlugins.mimetypes) {
      let re = new RegExp(type);
      if (re.test(aMimeType)) {
        return this.supportedPlugins.mimetypes[type];
      }
    }
    return null;
  },

  canInstallThisMimeType: function (aMimeType) {
    let os = Services.appinfo.OS;
    let pluginName = this.nameForSupportedPlugin(aMimeType);
    if (pluginName && "install" + os in this.supportedPlugins.plugins[pluginName]) {
      return true;
    }
    return false;
  },

  handleEvent : function(event) {
    let eventType = event.type;

    if (eventType == "PluginRemoved") {
      let doc = event.target;
      let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
      if (browser)
        this._setPluginNotificationIcon(browser);
      return;
    }

    let plugin = event.target;
    let doc = plugin.ownerDocument;

    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return;

    if (eventType == "PluginBindingAttached") {
      // The plugin binding fires this event when it is created.
      // As an untrusted event, ensure that this object actually has a binding
      // and make sure we don't handle it twice
      let overlay = this.getPluginUI(plugin, "main");
      if (!overlay || overlay._bindingHandled) {
        return;
      }
      overlay._bindingHandled = true;

      // Lookup the handler for this binding
      eventType = this._getBindingType(plugin);
      if (!eventType) {
        // Not all bindings have handlers
        return;
      }
    }

    let shouldShowNotification = false;
    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
    if (!browser)
      return;

    switch (eventType) {
      case "PluginCrashed":
        this.pluginInstanceCrashed(plugin, event);
        break;

      case "PluginNotFound":
        let installable = this.showInstallNotification(plugin, eventType);
        // For non-object plugin tags, register a click handler to install the
        // plugin. Object tags can, and often do, deal with that themselves,
        // so don't stomp on the page developers toes.
        if (installable && !(plugin instanceof HTMLObjectElement)) {
          let installStatus = this.getPluginUI(plugin, "installStatus");
          installStatus.setAttribute("installable", "true");
          let iconStatus = this.getPluginUI(plugin, "icon");
          iconStatus.setAttribute("installable", "true");

          let installLink = this.getPluginUI(plugin, "installPluginLink");
          this.addLinkClickCallback(installLink, "installSinglePlugin", plugin);
        }
        break;

      case "PluginBlocklisted":
      case "PluginOutdated":
        shouldShowNotification = true;
        break;

      case "PluginVulnerableUpdatable":
        let updateLink = this.getPluginUI(plugin, "checkForUpdatesLink");
        this.addLinkClickCallback(updateLink, "openPluginUpdatePage");
        /* FALLTHRU */

      case "PluginVulnerableNoUpdate":
      case "PluginClickToPlay":
        this._handleClickToPlayEvent(plugin);
        let overlay = this.getPluginUI(plugin, "main");
        let pluginName = this._getPluginInfo(plugin).pluginName;
        let messageString = gNavigatorBundle.getFormattedString("PluginClickToActivate", [pluginName]);
        let overlayText = this.getPluginUI(plugin, "clickToPlay");
        overlayText.textContent = messageString;
        if (eventType == "PluginVulnerableUpdatable" ||
            eventType == "PluginVulnerableNoUpdate") {
          let vulnerabilityString = gNavigatorBundle.getString(eventType);
          let vulnerabilityText = this.getPluginUI(plugin, "vulnerabilityStatus");
          vulnerabilityText.textContent = vulnerabilityString;
        }
        shouldShowNotification = true;
        break;

      case "PluginPlayPreview":
        this._handlePlayPreviewEvent(plugin);
        break;

      case "PluginDisabled":
        let manageLink = this.getPluginUI(plugin, "managePluginsLink");
        this.addLinkClickCallback(manageLink, "managePlugins");
        shouldShowNotification = true;
        break;

      case "PluginInstantiated":
        shouldShowNotification = true;
        break;
    }

    // Show the in-content UI if it's not too big. The crashed plugin handler already did this.
    if (eventType != "PluginCrashed") {
      let overlay = this.getPluginUI(plugin, "main");
      if (overlay != null) {
        this.setVisibility(plugin, overlay,
                           this.shouldShowOverlay(plugin, overlay));
        let resizeListener = (event) => {
          this.setVisibility(plugin, overlay,
            this.shouldShowOverlay(plugin, overlay));
          this._setPluginNotificationIcon(browser);
        };
        plugin.addEventListener("overflow", resizeListener);
        plugin.addEventListener("underflow", resizeListener);
      }
    }

    if (shouldShowNotification) {
      this._showClickToPlayNotification(browser, plugin, false);
    }
  },

  isKnownPlugin: function PH_isKnownPlugin(objLoadingContent) {
    return (objLoadingContent.getContentTypeForMIMEType(objLoadingContent.actualType) ==
            Ci.nsIObjectLoadingContent.TYPE_PLUGIN);
  },

  canActivatePlugin: function PH_canActivatePlugin(objLoadingContent) {
    // if this isn't a known plugin, we can't activate it
    // (this also guards pluginHost.getPermissionStringForType against
    // unexpected input)
    if (!gPluginHandler.isKnownPlugin(objLoadingContent))
      return false;

    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let principal = objLoadingContent.ownerDocument.defaultView.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let isFallbackTypeValid =
      objLoadingContent.pluginFallbackType >= Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
      objLoadingContent.pluginFallbackType <= Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;

    if (objLoadingContent.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_PLAY_PREVIEW) {
      // checking if play preview is subject to CTP rules
      let playPreviewInfo = pluginHost.getPlayPreviewInfo(objLoadingContent.actualType);
      isFallbackTypeValid = !playPreviewInfo.ignoreCTP;
    }

    return !objLoadingContent.activated &&
           pluginPermission != Ci.nsIPermissionManager.DENY_ACTION &&
           isFallbackTypeValid;
  },

  hideClickToPlayOverlay: function(aPlugin) {
    let overlay = this.getPluginUI(aPlugin, "main");
    if (overlay) {
      overlay.classList.remove("visible");
    }
  },

  stopPlayPreview: function PH_stopPlayPreview(aPlugin, aPlayPlugin) {
    let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    if (objLoadingContent.activated)
      return;

    if (aPlayPlugin)
      objLoadingContent.playPlugin();
    else
      objLoadingContent.cancelPlayPreview();
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
    var missingPlugins = new Map();

    var pluginInfo = this._getPluginInfo(plugin);
    missingPlugins.set(pluginInfo.mimetype, pluginInfo);

    openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
               "PFSWindow", "chrome,centerscreen,resizable=yes",
               {plugins: missingPlugins, browser: gBrowser.selectedBrowser});
  },

  // Callback for user clicking on a disabled plugin
  managePlugins: function (aEvent) {
    BrowserOpenAddonsMgr("addons://list/plugin");
  },

  // Callback for user clicking on the link in a click-to-play plugin
  // (where the plugin has an update)
  openPluginUpdatePage: function (aEvent) {
    openUILinkIn(Services.urlFormatter.formatURLPref("plugins.update.url"), "tab");
  },

#ifdef MOZ_CRASHREPORTER
  submitReport: function submitReport(pluginDumpID, browserDumpID, plugin) {
    let keyVals = {};
    if (plugin) {
      let userComment = this.getPluginUI(plugin, "submitComment").value.trim();
      if (userComment)
        keyVals.PluginUserComment = userComment;
      if (this.getPluginUI(plugin, "submitURLOptIn").checked)
        keyVals.PluginContentURL = plugin.ownerDocument.URL;
    }
    this.CrashSubmit.submit(pluginDumpID, { extraExtraKeyVals: keyVals });
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

  showInstallNotification: function (aPlugin) {
    let browser = gBrowser.getBrowserForDocument(aPlugin.ownerDocument
                                                        .defaultView.top.document);
    if (!browser.missingPlugins)
      browser.missingPlugins = new Map();

    let pluginInfo = this._getPluginInfo(aPlugin);
    browser.missingPlugins.set(pluginInfo.mimetype, pluginInfo);

    // only show notification for small subset of plugins
    let mimetype = pluginInfo.mimetype.split(";")[0];
    if (!this.canInstallThisMimeType(mimetype))
      return false;

    let pluginIdentifier = this.nameForSupportedPlugin(mimetype);
    if (!pluginIdentifier)
      return false;

    let displayName = this.supportedPlugins.plugins[pluginIdentifier].displayName;

    // don't show several notifications
    let notification = PopupNotifications.getNotification("plugins-not-found", browser);
    if (notification)
      return true;

    let messageString = gNavigatorBundle.getString("installPlugin.message");
    let mainAction = {
      label: gNavigatorBundle.getFormattedString("installPlugin.button.label",
                                                 [displayName]),
      accessKey: gNavigatorBundle.getString("installPlugin.button.accesskey"),
      callback: function () {
        openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                   "PFSWindow", "chrome,centerscreen,resizable=yes",
                   {plugins: browser.missingPlugins, browser: browser});
      }
    };
    let secondaryActions = null;
    let options = { dismissed: true };

    let showForFlash = Services.prefs.getBoolPref(this.PREF_NOTIFY_MISSING_FLASH);
    if (pluginIdentifier == "flash" && showForFlash) {
      let prefNotifyMissingFlash = this.PREF_NOTIFY_MISSING_FLASH;
      secondaryActions = [{
        label: gNavigatorBundle.getString("installPlugin.ignoreButton.label"),
        accessKey: gNavigatorBundle.getString("installPlugin.ignoreButton.accesskey"),
        callback: function () {
          Services.prefs.setBoolPref(prefNotifyMissingFlash, false);
        }
      }];
      options.dismissed = false;
    }
    PopupNotifications.show(browser, "plugins-not-found",
                            messageString, "plugin-install-notification-icon",
                            mainAction, secondaryActions, options);
    return true;
  },
  // Event listener for click-to-play plugins.
  _handleClickToPlayEvent: function PH_handleClickToPlayEvent(aPlugin) {
    let doc = aPlugin.ownerDocument;
    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    // guard against giving pluginHost.getPermissionStringForType a type
    // not associated with any known plugin
    if (!gPluginHandler.isKnownPlugin(objLoadingContent))
      return;
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let principal = doc.defaultView.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let overlay = this.getPluginUI(aPlugin, "main");

    if (pluginPermission == Ci.nsIPermissionManager.DENY_ACTION) {
      if (overlay) {
        overlay.classList.remove("visible");
      }
      return;
    }

    if (overlay) {
      overlay.addEventListener("click", gPluginHandler._overlayClickListener, true);
      let closeIcon = this.getPluginUI(aPlugin, "closeIcon");
      closeIcon.addEventListener("click", function(aEvent) {
        if (aEvent.button == 0 && aEvent.isTrusted)
          gPluginHandler.hideClickToPlayOverlay(aPlugin);
      }, true);
    }
  },

  _overlayClickListener: {
    handleEvent: function PH_handleOverlayClick(aEvent) {
      let plugin = document.getBindingParent(aEvent.target);
      let contentWindow = plugin.ownerDocument.defaultView.top;
      // gBrowser.getBrowserForDocument does not exist in the case where we
      // drag-and-dropped a tab from a window containing only that tab. In
      // that case, the window gets destroyed.
      let browser = gBrowser.getBrowserForDocument ?
                      gBrowser.getBrowserForDocument(contentWindow.document) :
                      null;
      // If browser is null here, we've been drag-and-dropped from another
      // window, and this is the wrong click handler.
      if (!browser) {
        aEvent.target.removeEventListener("click", gPluginHandler._overlayClickListener, true);
        return;
      }
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      // Have to check that the target is not the link to update the plugin
      if (!(aEvent.originalTarget instanceof HTMLAnchorElement) &&
          (aEvent.originalTarget.getAttribute('anonid') != 'closeIcon') &&
          aEvent.button == 0 && aEvent.isTrusted) {
        gPluginHandler._showClickToPlayNotification(browser, plugin, true);
        aEvent.stopPropagation();
        aEvent.preventDefault();
      }
    }
  },

  _handlePlayPreviewEvent: function PH_handlePlayPreviewEvent(aPlugin) {
    let doc = aPlugin.ownerDocument;
    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let pluginInfo = this._getPluginInfo(aPlugin);
    let playPreviewInfo = pluginHost.getPlayPreviewInfo(pluginInfo.mimetype);

    let previewContent = this.getPluginUI(aPlugin, "previewPluginContent");
    let iframe = previewContent.getElementsByClassName("previewPluginContentFrame")[0];
    if (!iframe) {
      // lazy initialization of the iframe
      iframe = doc.createElementNS("http://www.w3.org/1999/xhtml", "iframe");
      iframe.className = "previewPluginContentFrame";
      previewContent.appendChild(iframe);

      // Force a style flush, so that we ensure our binding is attached.
      aPlugin.clientTop;
    }
    iframe.src = playPreviewInfo.redirectURL;

    // MozPlayPlugin event can be dispatched from the extension chrome
    // code to replace the preview content with the native plugin
    previewContent.addEventListener("MozPlayPlugin", function playPluginHandler(aEvent) {
      if (!aEvent.isTrusted)
        return;

      previewContent.removeEventListener("MozPlayPlugin", playPluginHandler, true);

      let playPlugin = !aEvent.detail;
      gPluginHandler.stopPlayPreview(aPlugin, playPlugin);

      // cleaning up: removes overlay iframe from the DOM
      let iframe = previewContent.getElementsByClassName("previewPluginContentFrame")[0];
      if (iframe)
        previewContent.removeChild(iframe);
    }, true);

    if (!playPreviewInfo.ignoreCTP) {
      gPluginHandler._showClickToPlayNotification(browser, aPlugin, false);
    }
  },

  reshowClickToPlayNotification: function PH_reshowClickToPlayNotification() {
    let browser = gBrowser.selectedBrowser;
    let contentWindow = browser.contentWindow;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    for (let plugin of plugins) {
      let overlay = this.getPluginUI(plugin, "main");
      if (overlay)
        overlay.removeEventListener("click", gPluginHandler._overlayClickListener, true);
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (gPluginHandler.canActivatePlugin(objLoadingContent))
        gPluginHandler._handleClickToPlayEvent(plugin);
    }
    gPluginHandler._showClickToPlayNotification(browser, null, false);
  },

  _clickToPlayNotificationEventCallback: function PH_ctpEventCallback(event) {
    if (event == "showing") {
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_SHOWN")
        .add(!this.options.primaryPlugin);
      // Histograms always start at 0, even though our data starts at 1
      let histogramCount = this.options.centerActions.size - 1;
      if (histogramCount > 4) {
        histogramCount = 4;
      }
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_PLUGIN_COUNT")
        .add(histogramCount);
    }
    else if (event == "dismissed") {
      // Once the popup is dismissed, clicking the icon should show the full
      // list again
      this.options.primaryPlugin = null;
    }
  },

  // Match the behaviour of nsPermissionManager
  _getHostFromPrincipal: function PH_getHostFromPrincipal(principal) {
    if (!principal.URI || principal.URI.schemeIs("moz-nullprincipal")) {
      return "(null)";
    }

    try {
      if (principal.URI.host)
        return principal.URI.host;
    } catch (e) {}

    return principal.origin;
  },

  /**
   * Called from the plugin doorhanger to set the new permissions for a plugin
   * and activate plugins if necessary.
   * aNewState should be either "allownow" "allowalways" or "block"
   */
  _updatePluginPermission: function PH_setPermissionForPlugins(aNotification, aPluginInfo, aNewState) {
    let permission;
    let expireType;
    let expireTime;
    let histogram =
      Services.telemetry.getHistogramById("PLUGINS_NOTIFICATION_USER_ACTION");

    // Update the permission manager.
    // Also update the current state of pluginInfo.fallbackType so that
    // subsequent opening of the notification shows the current state.
    switch (aNewState) {
      case "allownow":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_SESSION;
        expireTime = Date.now() + Services.prefs.getIntPref(this.PREF_SESSION_PERSIST_MINUTES) * 60 * 1000;
        histogram.add(0);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        break;

      case "allowalways":
        permission = Ci.nsIPermissionManager.ALLOW_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_TIME;
        expireTime = Date.now() +
          Services.prefs.getIntPref(this.PREF_PERSISTENT_DAYS) * 24 * 60 * 60 * 1000;
        histogram.add(1);
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        break;

      case "block":
        permission = Ci.nsIPermissionManager.PROMPT_ACTION;
        expireType = Ci.nsIPermissionManager.EXPIRE_NEVER;
        expireTime = 0;
        histogram.add(2);
        switch (aPluginInfo.blocklistState) {
          case Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE;
            break;
          case Ci.nsIBlocklistService.STATE_VULNERABLE_NO_UPDATE:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;
            break;
          default:
            aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY;
        }
        break;

      // In case a plugin has already been allowed in another tab, the "continue allowing" button
      // shouldn't change any permissions but should run the plugin-enablement code below.
      case "continue":
        aPluginInfo.fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE;
        break;
      default:
        Cu.reportError(Error("Unexpected plugin state: " + aNewState));
        return;
    }

    let browser = aNotification.browser;
    let contentWindow = browser.contentWindow;
    if (aNewState != "continue") {
      let principal = contentWindow.document.nodePrincipal;
      Services.perms.addFromPrincipal(principal, aPluginInfo.permissionString,
                                      permission, expireType, expireTime);
      aPluginInfo.pluginPermissionType = expireType;
    }

    // Manually activate the plugins that would have been automatically
    // activated.
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

    let pluginFound = false;
    for (let plugin of plugins) {
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (!gPluginHandler.isKnownPlugin(plugin)) {
        continue;
      }
      if (aPluginInfo.permissionString == pluginHost.getPermissionStringForType(plugin.actualType)) {
        pluginFound = true;
        if (aNewState == "block") {
          plugin.reload(true);
        } else {
          if (gPluginHandler.canActivatePlugin(plugin)) {
            let overlay = this.getPluginUI(plugin, "main");
            if (overlay) {
              overlay.removeEventListener("click", gPluginHandler._overlayClickListener, true);
            }
            plugin.playPlugin();
          }
        }
      }
    }

    // If there are no instances of the plugin on the page any more, what the
    // user probably needs is for us to allow and then refresh.
    if (aNewState != "block" && !pluginFound) {
      browser.reload();
    }

    this._setPluginNotificationIcon(browser);
  },

  _showClickToPlayNotification: function PH_showClickToPlayNotification(aBrowser, aPlugin, aShowNow) {
    let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
    let plugins = [];

    // if aPlugin is null, that means the user has navigated back to a page with plugins, and we need
    // to collect all the plugins
    if (aPlugin === null) {
      let contentWindow = aBrowser.contentWindow;
      let contentDoc = aBrowser.contentDocument;
      let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
      // cwu.plugins may contain non-plugin <object>s, filter them out
      plugins = cwu.plugins.filter((plugin) =>
        plugin.getContentTypeForMIMEType(plugin.actualType) == Ci.nsIObjectLoadingContent.TYPE_PLUGIN);

      if (plugins.length == 0) {
        if (notification) {
          PopupNotifications.remove(notification);
        }
        return;
      }
    } else {
      plugins = [aPlugin];
    }

    // If this is a new notification, create a centerActions map, otherwise append
    let centerActions;
    if (notification) {
      centerActions = notification.options.centerActions;
    } else {
      centerActions = new Map();
    }

    let principal = aBrowser.contentDocument.nodePrincipal;
    let principalHost = this._getHostFromPrincipal(principal);

    for (var plugin of plugins) {
      let pluginInfo = this._getPluginInfo(plugin);
      if (pluginInfo.permissionString === null) {
        Cu.reportError("No permission string for active plugin.");
        continue;
      }
      if (centerActions.has(pluginInfo.permissionString)) {
        continue;
      }

      let permissionObj = Services.perms.
        getPermissionObject(principal, pluginInfo.permissionString, false);
      if (permissionObj) {
        pluginInfo.pluginPermissionHost = permissionObj.host;
        pluginInfo.pluginPermissionType = permissionObj.expireType;
      }
      else {
        pluginInfo.pluginPermissionHost = principalHost;
        pluginInfo.pluginPermissionType = undefined;
      }

      let url;
      // TODO: allow the blocklist to specify a better link, bug 873093
      if (pluginInfo.blocklistState == Ci.nsIBlocklistService.STATE_VULNERABLE_UPDATE_AVAILABLE) {
        url = Services.urlFormatter.formatURLPref("plugins.update.url");
      }
      else if (pluginInfo.blocklistState != Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
        url = Services.blocklist.getPluginBlocklistURL(pluginInfo.pluginTag);
      }
      else {
        url = Services.urlFormatter.formatURLPref("app.support.baseURL") + "clicktoplay";
      }
      pluginInfo.detailsLink = url;
      
      centerActions.set(pluginInfo.permissionString, pluginInfo);
    }

    let primaryPluginPermission = null;
    if (aShowNow) {
      primaryPluginPermission = this._getPluginInfo(aPlugin).permissionString;
    }

    if (notification) {
      // Don't modify the notification UI while it's on the screen, that would be
      // jumpy and might allow clickjacking.
      if (aShowNow) {
        notification.options.primaryPlugin = primaryPluginPermission;
        notification.reshow();
        setTimeout(() => { this._setPluginNotificationIcon(aBrowser); }, 0);
      }
      return;
    }

    let options = {
      dismissed: !aShowNow,
      eventCallback: this._clickToPlayNotificationEventCallback,
      primaryPlugin: primaryPluginPermission,
      centerActions: centerActions
    };
    PopupNotifications.show(aBrowser, "click-to-play-plugins",
                            "", "plugins-notification-icon",
                            null, null, options);
    setTimeout(() => { this._setPluginNotificationIcon(aBrowser); }, 0);
  },

  _setPluginNotificationIcon : function PH_setPluginNotificationIcon(aBrowser) {
    // Because this is called on a timeout, sanity-check before continuing
    if (!aBrowser.docShell || !aBrowser.contentWindow) {
      return;
    }

    let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
    if (!notification)
      return;

    // Make a copy of the actions, removing active plugins and checking for
    // outdated plugins.
    let haveInsecure = false;
    let actions = new Map();
    for (let action of notification.options.centerActions.values()) {
      switch (action.fallbackType) {
        // haveInsecure will trigger the red flashing icon and the infobar
        // styling below
        case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
        case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
          haveInsecure = true;
          // fall through

        case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
          actions.set(action.permissionString, action);
          continue;
      }
    }

    // check for hidden plugins
    let contentWindow = aBrowser.contentWindow;
    let contentDoc = aBrowser.contentDocument;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    for (let plugin of cwu.plugins) {
      let info = this._getPluginInfo(plugin);
      if (!actions.has(info.permissionString)) {
        continue;
      }
      let fallbackType = info.fallbackType;
      if (fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_ACTIVE) {
        actions.delete(info.permissionString);
        if (actions.size == 0) {
          break;
        }
        continue;
      }
      if (fallbackType != Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
          fallbackType != Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE &&
          fallbackType != Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE) {
        continue;
      }
      let overlay = this.getPluginUI(plugin, "main");
      if (!overlay) {
        continue;
      }
      let shouldShow = this.shouldShowOverlay(plugin, overlay);
      this.setVisibility(plugin, overlay, shouldShow);
      if (shouldShow) {
        actions.delete(info.permissionString);
        if (actions.size == 0) {
          break;
        }
      }
    }

    // Set up the icon
    document.getElementById("plugins-notification-icon").classList.
      toggle("plugin-blocked", haveInsecure);

    // Now configure the notification bar

    let notificationBox = gBrowser.getNotificationBox(aBrowser);

    function hideNotification() {
      let n = notificationBox.getNotificationWithValue("plugin-hidden");
      if (n) {
        notificationBox.removeNotification(n, true);
      }
    }

    // There are three different cases when showing an infobar:
    // 1.  A single type of plugin is hidden on the page. Show the UI for that
    //     plugin.
    // 2a. Multiple types of plugins are hidden on the page. Show the multi-UI
    //     with the vulnerable styling.
    // 2b. Multiple types of plugins are hidden on the page, but none are
    //     vulnerable. Show the nonvulnerable multi-UI.
    function showNotification() {
      let n = notificationBox.getNotificationWithValue("plugin-hidden");
      if (n) {
        // If something is already shown, just keep it
        return;
      }

      Services.telemetry.getHistogramById("PLUGINS_INFOBAR_SHOWN").
        add(true);

      let message;
      // Icons set directly cannot be manipulated using moz-image-region, so
      // we use CSS classes instead.
      let host = gPluginHandler._getHostFromPrincipal(aBrowser.contentDocument.nodePrincipal);
      let brand = document.getElementById("bundle_brand").getString("brandShortName");

      if (actions.size == 1) {
        let pluginInfo = [...actions.values()][0];
        let pluginName = pluginInfo.pluginName;

        switch (pluginInfo.fallbackType) {
          case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivateNew.message",
              [pluginName, host]);
            break;
          case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivateOutdated.message",
              [pluginName, host, brand]);
            break;
          case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
            message = gNavigatorBundle.getFormattedString(
              "pluginActivateVulnerable.message",
              [pluginName, host, brand]);
        }
      } else {
        // Multi-plugin
        message = gNavigatorBundle.getFormattedString(
          "pluginActivateMultiple.message", [host]);

        for (let action of actions.values()) {
          if (action.fallbackType != Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY) {
            break;
          }
        }
      }

      let buttons = [
        {
          label: gNavigatorBundle.getString("pluginContinueBlocking.label"),
          accessKey: gNavigatorBundle.getString("pluginContinueBlocking.accesskey"),
          callback: function() {
            Services.telemetry.getHistogramById("PLUGINS_INFOBAR_BLOCK").
              add(true);

            Services.perms.addFromPrincipal(aBrowser.contentDocument.nodePrincipal,
                                            "plugin-hidden-notification",
                                            Services.perms.DENY_ACTION);
          }
        },
        {
          label: gNavigatorBundle.getString("pluginActivateTrigger.label"),
          accessKey: gNavigatorBundle.getString("pluginActivateTrigger.accesskey"),
          callback: function() {
            Services.telemetry.getHistogramById("PLUGINS_INFOBAR_ALLOW").
              add(true);

            let curNotification =
              PopupNotifications.getNotification("click-to-play-plugins",
                                                 aBrowser);
            if (curNotification) {
              curNotification.reshow();
            }
          }
        }
      ];
      n = notificationBox.
        appendNotification(message, "plugin-hidden", null,
                           notificationBox.PRIORITY_INFO_HIGH, buttons);
      if (haveInsecure) {
        n.classList.add('pluginVulnerable');
      }
    }

    if (actions.size == 0) {
      hideNotification();
    } else {
      let notificationPermission = Services.perms.testPermissionFromPrincipal(
        aBrowser.contentDocument.nodePrincipal, "plugin-hidden-notification");
      if (notificationPermission == Ci.nsIPermissionManager.DENY_ACTION) {
        hideNotification();
      } else {
        showNotification();
      }
    }
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
    let pluginDumpID    = aEvent.getData("pluginDumpID");
    let browserDumpID   = aEvent.getData("browserDumpID");

    // Remap the plugin name to a more user-presentable form.
    pluginName = this.makeNicePluginName(pluginName);

    let messageString = gNavigatorBundle.getFormattedString("crashedpluginsMessage.title", [pluginName]);

    //
    // Configure the crashed-plugin placeholder.
    //

    // Force a layout flush so the binding is attached.
    plugin.clientTop;
    let overlay = this.getPluginUI(plugin, "main");
    let statusDiv = this.getPluginUI(plugin, "submitStatus");
    let doc = plugin.ownerDocument;
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
      this.getPluginUI(plugin, "submitButton").addEventListener("click",
        function (event) {
          if (event.button != 0 || !event.isTrusted)
            return;
          this.submitReport(pluginDumpID, browserDumpID, plugin);
          pref.setBoolPref("", optInCB.checked);
        }.bind(this));
      let optInCB = this.getPluginUI(plugin, "submitURLOptIn");
      let pref = Services.prefs.getBranch("dom.ipc.plugins.reportCrashURL");
      optInCB.checked = pref.getBoolPref("");
    }

    // If we don't have a minidumpID, we can't (or didn't) submit anything.
    // This can happen if the plugin is killed from the task manager.
    if (!pluginDumpID) {
        status = "noReport";
    }

    statusDiv.setAttribute("status", status);

    let helpIcon = this.getPluginUI(plugin, "helpIcon");
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

    let crashText = this.getPluginUI(plugin, "crashedText");
    crashText.textContent = messageString;

    let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);

    let link = this.getPluginUI(plugin, "reloadLink");
    this.addLinkClickCallback(link, "reloadPage", browser);

    let notificationBox = gBrowser.getNotificationBox(browser);

    let isShowing = this.shouldShowOverlay(plugin, overlay);

    // Is the <object>'s size too small to hold what we want to show?
    if (!isShowing) {
      // First try hiding the crash report submission UI.
      statusDiv.removeAttribute("status");

      isShowing = this.shouldShowOverlay(plugin, overlay);
    }
    this.setVisibility(plugin, overlay, isShowing);

    if (isShowing) {
      // If a previous plugin on the page was too small and resulted in adding a
      // notification bar, then remove it because this plugin instance it big
      // enough to serve as in-content notification.
      hideNotificationBar();
      doc.mozNoPluginCrashedNotification = true;
    } else {
      // If another plugin on the page was large enough to show our UI, we don't
      // want to show a notification bar.
      if (!doc.mozNoPluginCrashedNotification)
        showNotificationBar(pluginDumpID, browserDumpID);
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
