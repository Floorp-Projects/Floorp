# -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

const PLUGIN_SCRIPTED_STATE_NONE = 0;
const PLUGIN_SCRIPTED_STATE_FIRED = 1;
const PLUGIN_SCRIPTED_STATE_DONE = 2;

function getPluginInfo(pluginElement)
{
  var tagMimetype;
  var pluginsPage;
  var pluginName = gNavigatorBundle.getString("pluginInfo.unknownPlugin");
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

  if (tagMimetype) {
    let navMimeType = navigator.mimeTypes.namedItem(tagMimetype);
    if (navMimeType && navMimeType.enabledPlugin) {
      pluginName = navMimeType.enabledPlugin.name;
      pluginName = gPluginHandler.makeNicePluginName(pluginName);
    }
  }

  return { mimetype: tagMimetype,
           pluginsPage: pluginsPage,
           pluginName: pluginName };
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
  makeNicePluginName : function (aName) {
    if (aName == "Shockwave Flash")
      return "Adobe Flash";

    // Clean up the plugin name by stripping off any trailing version numbers
    // or "plugin". EG, "Foo Bar Plugin 1.23_02" --> "Foo Bar"
    // Do this by first stripping the numbers, etc. off the end, and then
    // removing "Plugin" (and then trimming to get rid of any whitespace).
    // (Otherwise, something like "Java(TM) Plug-in 1.7.0_07" gets mangled)
    let newName = aName.replace(/[\s\d\.\-\_\(\)]+$/, "").replace(/\bplug-?in\b/i, "").trim();
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

  handleEvent : function(event) {
    let self = gPluginHandler;
    let plugin = event.target;
    let doc = plugin.ownerDocument;

    // We're expecting the target to be a plugin.
    if (!(plugin instanceof Ci.nsIObjectLoadingContent))
      return;

    let eventType = event.type;
    if (eventType == "PluginBindingAttached") {
      // The plugin binding fires this event when it is created.
      // As an untrusted event, ensure that this object actually has a binding
      // and make sure we don't handle it twice
      let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
      if (!overlay || overlay._bindingHandled) {
        return;
      }
      overlay._bindingHandled = true;

      // Lookup the handler for this binding
      eventType = self._getBindingType(plugin);
      if (!eventType) {
        // Not all bindings have handlers
        return;
      }
    }

    switch (eventType) {
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
        self.pluginUnavailable(plugin, eventType);
        break;

      case "PluginVulnerableUpdatable":
        let updateLink = doc.getAnonymousElementByAttribute(plugin, "class", "checkForUpdatesLink");
        self.addLinkClickCallback(updateLink, "openPluginUpdatePage");
        /* FALLTHRU */

      case "PluginVulnerableNoUpdate":
      case "PluginClickToPlay":
        self._handleClickToPlayEvent(plugin);
        let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
        let pluginName = getPluginInfo(plugin).pluginName;
        let messageString = gNavigatorBundle.getFormattedString("PluginClickToPlay", [pluginName]);
        let overlayText = doc.getAnonymousElementByAttribute(plugin, "class", "msg msgClickToPlay");
        overlayText.textContent = messageString;
        if (eventType == "PluginVulnerableUpdatable" ||
            eventType == "PluginVulnerableNoUpdate") {
          let vulnerabilityString = gNavigatorBundle.getString(eventType);
          let vulnerabilityText = doc.getAnonymousElementByAttribute(plugin, "anonid", "vulnerabilityStatus");
          vulnerabilityText.textContent = vulnerabilityString;
        }
        break;

      case "PluginPlayPreview":
        self._handlePlayPreviewEvent(plugin);
        break;

      case "PluginDisabled":
        let manageLink = doc.getAnonymousElementByAttribute(plugin, "class", "managePluginsLink");
        self.addLinkClickCallback(manageLink, "managePlugins");
        break;

      case "PluginScripted":
        let browser = gBrowser.getBrowserForDocument(doc.defaultView.top.document);
        if (browser._pluginScriptedState == PLUGIN_SCRIPTED_STATE_NONE) {
          browser._pluginScriptedState = PLUGIN_SCRIPTED_STATE_FIRED;
          setTimeout(function() {
            gPluginHandler.handlePluginScripted(this);
          }.bind(browser), 500);
        }
        break;
    }

    // Hide the in-content UI if it's too big. The crashed plugin handler already did this.
    if (eventType != "PluginCrashed") {
      let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
      if (overlay != null && self.isTooSmall(plugin, overlay))
          overlay.style.visibility = "hidden";
    }
  },

  handlePluginScripted: function PH_handlePluginScripted(aBrowser) {
    let contentWindow = aBrowser.contentWindow;
    if (!contentWindow)
      return;

    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let haveVisibleCTPPlugin = cwu.plugins.some(function(plugin) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      let doc = plugin.ownerDocument;
      let overlay = doc.getAnonymousElementByAttribute(plugin, "class", "mainBox");
      if (!overlay)
        return false;

      // if the plugin's style is 240x200, it's a good bet we set that in
      // toolkit/mozapps/plugins/content/pluginProblemContent.css
      // (meaning this plugin was never actually given a size, so it's really
      // not part of visible content)
      let computedStyle = contentWindow.getComputedStyle(plugin);
      let isInvisible = ((computedStyle.width == "240px" &&
                          computedStyle.height == "200px") ||
                         gPluginHandler.isTooSmall(plugin, overlay));
      return (!isInvisible &&
              gPluginHandler.canActivatePlugin(objLoadingContent));
    });

    let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
    if (notification && !haveVisibleCTPPlugin) {
      notification.dismissed = false;
      PopupNotifications._update(notification.anchorElement);
    }

    aBrowser._pluginScriptedState = PLUGIN_SCRIPTED_STATE_DONE;
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
    let browser = gBrowser.getBrowserForDocument(objLoadingContent.ownerDocument.defaultView.top.document);
    let pluginPermission = Services.perms.testPermission(browser.currentURI, permissionString);

    return !objLoadingContent.activated &&
           pluginPermission != Ci.nsIPermissionManager.DENY_ACTION &&
           objLoadingContent.pluginFallbackType >= Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
           objLoadingContent.pluginFallbackType <= Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;
  },

  activatePlugins: function PH_activatePlugins(aContentWindow) {
    let browser = gBrowser.getBrowserForDocument(aContentWindow.document);
    browser._clickToPlayPluginsActivated = true;
    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    for (let plugin of plugins) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (gPluginHandler.canActivatePlugin(objLoadingContent))
        objLoadingContent.playPlugin();
    }
    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);
    if (notification)
      notification.remove();
  },

  activateSinglePlugin: function PH_activateSinglePlugin(aContentWindow, aPlugin) {
    let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    if (gPluginHandler.canActivatePlugin(objLoadingContent))
      objLoadingContent.playPlugin();

    let cwu = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindowUtils);
    let pluginNeedsActivation = gPluginHandler._pluginNeedsActivationExceptThese([aPlugin]);
    let browser = gBrowser.getBrowserForDocument(aContentWindow.document);
    let notification = PopupNotifications.getNotification("click-to-play-plugins", browser);
    if (notification) {
      browser._clickToPlayDoorhangerShown = false;
      notification.remove();
    }
    if (pluginNeedsActivation) {
      gPluginHandler._showClickToPlayNotification(browser);
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

    var pluginInfo = getPluginInfo(plugin);
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
    openURL(Services.urlFormatter.formatURLPref("plugins.update.url"));
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
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let objLoadingContent = aPlugin.QueryInterface(Ci.nsIObjectLoadingContent);
    // guard against giving pluginHost.getPermissionStringForType a type
    // not associated with any known plugin
    if (!gPluginHandler.isKnownPlugin(objLoadingContent))
      return;
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let pluginPermission = Services.perms.testPermission(browser.currentURI, permissionString);

    let overlay = doc.getAnonymousElementByAttribute(aPlugin, "class", "mainBox");

    if (pluginPermission == Ci.nsIPermissionManager.DENY_ACTION) {
      if (overlay)
        overlay.style.visibility = "hidden";
      return;
    }

    if (browser._clickToPlayPluginsActivated) {
      objLoadingContent.playPlugin();
      return;
    }

    if (overlay) {
      overlay.addEventListener("click", function(aEvent) {
        // Have to check that the target is not the link to update the plugin
        if (!(aEvent.originalTarget instanceof HTMLAnchorElement) &&
            aEvent.button == 0 && aEvent.isTrusted) {
          gPluginHandler.activateSinglePlugin(aEvent.target.ownerDocument.defaultView.top, aPlugin);
          aEvent.stopPropagation();
          aEvent.preventDefault();
        }
      }, true);
    }

    if (!browser._clickToPlayDoorhangerShown)
      gPluginHandler._showClickToPlayNotification(browser);
  },

  _handlePlayPreviewEvent: function PH_handlePlayPreviewEvent(aPlugin) {
    let doc = aPlugin.ownerDocument;
    let previewContent = doc.getAnonymousElementByAttribute(aPlugin, "class", "previewPluginContent");
    let iframe = previewContent.getElementsByClassName("previewPluginContentFrame")[0];
    if (!iframe) {
      // lazy initialization of the iframe
      iframe = doc.createElementNS("http://www.w3.org/1999/xhtml", "iframe");
      iframe.className = "previewPluginContentFrame";
      previewContent.appendChild(iframe);

      // Force a style flush, so that we ensure our binding is attached.
      aPlugin.clientTop;
    }
    let pluginInfo = getPluginInfo(aPlugin);
    let playPreviewUri = "data:application/x-moz-playpreview;," + pluginInfo.mimetype;
    iframe.src = playPreviewUri;

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
  },

  reshowClickToPlayNotification: function PH_reshowClickToPlayNotification() {
    let browser = gBrowser.selectedBrowser;
    if (gPluginHandler._pluginNeedsActivationExceptThese([]))
      gPluginHandler._showClickToPlayNotification(browser);
  },

  // returns true if there is a plugin on this page that needs activation
  // and isn't in the "except these" list
  _pluginNeedsActivationExceptThese: function PH_pluginNeedsActivationExceptThese(aExceptThese) {
    let contentWindow = gBrowser.selectedBrowser.contentWindow;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let pluginNeedsActivation = cwu.plugins.some(function(plugin) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      return (gPluginHandler.canActivatePlugin(objLoadingContent) &&
              aExceptThese.indexOf(plugin) < 0);
    });

    return pluginNeedsActivation;
  },

  /* Gets all plugins currently in the page of the given name */
  _getPluginsByName: function PH_getPluginsByName(aDOMWindowUtils, aName) {
    let plugins = [];
    for (let plugin of aDOMWindowUtils.plugins) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (gPluginHandler.canActivatePlugin(objLoadingContent)) {
        let pluginName = getPluginInfo(plugin).pluginName;
        if (aName == pluginName) {
          plugins.push(objLoadingContent);
        }
      }
    }
    return plugins;
  },

  _makeCenterActions: function PH_makeCenterActions(aBrowser) {
    let contentWindow = aBrowser.contentWindow;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let pluginsDictionary = new Map();
    for (let plugin of cwu.plugins) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (gPluginHandler.canActivatePlugin(objLoadingContent)) {
        let pluginName = getPluginInfo(plugin).pluginName;
        if (!pluginsDictionary.has(pluginName))
          pluginsDictionary.set(pluginName, []);
        pluginsDictionary.get(pluginName).push(objLoadingContent);
      }
    }

    let centerActions = [];
    for (let [pluginName, namedPluginArray] of pluginsDictionary) {
      let plugin = namedPluginArray[0];
      let warn = false;
      let warningText = "";
      let updateLink = Services.urlFormatter.formatURLPref("plugins.update.url");
      if (plugin.pluginFallbackType) {
        if (plugin.pluginFallbackType ==
              Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE) {
          warn = true;
          warningText = gNavigatorBundle.getString("vulnerableUpdatablePluginWarning");
        }
        else if (plugin.pluginFallbackType ==
                   Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE) {
          warn = true;
          warningText = gNavigatorBundle.getString("vulnerableNoUpdatePluginWarning");
          updateLink = "";
        }
      }

      let action = {
        message: pluginName,
        warn: warn,
        warningText: warningText,
        updateLink: updateLink,
        label: gNavigatorBundle.getString("activateSinglePlugin"),
        callback: function() {
          let plugins = gPluginHandler._getPluginsByName(cwu, this.message);
          for (let objLoadingContent of plugins) {
            objLoadingContent.playPlugin();
          }

          let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
          if (notification &&
              !gPluginHandler._pluginNeedsActivationExceptThese(plugins)) {
            notification.remove();
          }
        }
      };
      centerActions.push(action);
    }

    return centerActions;
  },

  _setPermissionForPlugins: function PH_setPermissionForPlugins(aBrowser, aPermission, aPluginList) {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    for (let plugin of aPluginList) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      // canActivatePlugin will return false if this isn't a known plugin type,
      // so the pluginHost.getPermissionStringForType call is protected
      if (gPluginHandler.canActivatePlugin(objLoadingContent)) {
        let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
        Services.perms.add(aBrowser.currentURI, permissionString, aPermission);
      }
    }
  },

  _showClickToPlayNotification: function PH_showClickToPlayNotification(aBrowser) {
    aBrowser._clickToPlayDoorhangerShown = true;
    let contentWindow = aBrowser.contentWindow;

    let messageString = gNavigatorBundle.getString("activatePluginsMessage.message");
    let mainAction = {
      label: gNavigatorBundle.getString("activateAllPluginsMessage.label"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.accesskey"),
      callback: function() { gPluginHandler.activatePlugins(contentWindow); }
    };
    let centerActions = gPluginHandler._makeCenterActions(aBrowser);
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let haveVulnerablePlugin = cwu.plugins.some(function(plugin) {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      return (gPluginHandler.canActivatePlugin(objLoadingContent) &&
              (objLoadingContent.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE ||
               objLoadingContent.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE));
    });
    if (haveVulnerablePlugin) {
      messageString = gNavigatorBundle.getString("vulnerablePluginsMessage");
    }
    let secondaryActions = [{
      label: gNavigatorBundle.getString("activatePluginsMessage.always"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.always.accesskey"),
      callback: function () {
        gPluginHandler._setPermissionForPlugins(aBrowser, Ci.nsIPermissionManager.ALLOW_ACTION, cwu.plugins);
        gPluginHandler.activatePlugins(contentWindow);
      }
    },{
      label: gNavigatorBundle.getString("activatePluginsMessage.never"),
      accessKey: gNavigatorBundle.getString("activatePluginsMessage.never.accesskey"),
      callback: function () {
        gPluginHandler._setPermissionForPlugins(aBrowser, Ci.nsIPermissionManager.DENY_ACTION, cwu.plugins);
        let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
        if (notification)
          notification.remove();
        gPluginHandler._removeClickToPlayOverlays(contentWindow);
      }
    }];
    let notification = PopupNotifications.getNotification("click-to-play-plugins", aBrowser);
    let dismissed = notification ? notification.dismissed : true;
    let options = { dismissed: dismissed, centerActions: centerActions };
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
      // for already activated plugins, there will be no overlay
      if (overlay)
        overlay.style.visibility = "hidden";
    }
  },

  // event listener for missing/blocklisted/outdated/carbonFailure plugins.
  pluginUnavailable: function (plugin, eventType) {
    let browser = gBrowser.getBrowserForDocument(plugin.ownerDocument
                                                       .defaultView.top.document);
    if (!browser.missingPlugins)
      browser.missingPlugins = new Map();

    var pluginInfo = getPluginInfo(plugin);
    browser.missingPlugins.set(pluginInfo.mimetype, pluginInfo);

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
      var missingPlugins = gBrowser.selectedBrowser.missingPlugins;
      if (missingPlugins) {
        openDialog("chrome://mozapps/content/plugins/pluginInstallerWizard.xul",
                   "PFSWindow", "chrome,centerscreen,resizable=yes",
                   {plugins: missingPlugins, browser: gBrowser.selectedBrowser});
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
