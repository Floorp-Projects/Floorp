/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

this.EXPORTED_SYMBOLS = [ "PluginContent" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/BrowserUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

this.PluginContent = function(global) {
  this.init(global);
}

const FLASH_MIME_TYPE = "application/x-shockwave-flash";
const REPLACEMENT_STYLE_SHEET = Services.io.newURI("chrome://pluginproblem/content/pluginReplaceBinding.css", null, null);

PluginContent.prototype = {
  init: function(global) {
    this.global = global;
    // Need to hold onto the content window or else it'll get destroyed
    this.content = this.global.content;
    // Cache of plugin actions for the current page.
    this.pluginData = new Map();
    // Cache of plugin crash information sent from the parent
    this.pluginCrashData = new Map();

    // Note that the XBL binding is untrusted
    global.addEventListener("PluginBindingAttached", this, true, true);
    global.addEventListener("PluginPlaceholderReplaced", this, true, true);
    global.addEventListener("PluginCrashed",         this, true);
    global.addEventListener("PluginOutdated",        this, true);
    global.addEventListener("PluginInstantiated",    this, true);
    global.addEventListener("PluginRemoved",         this, true);
    global.addEventListener("pagehide",              this, true);
    global.addEventListener("pageshow",              this, true);
    global.addEventListener("unload",                this);
    global.addEventListener("HiddenPlugin",          this, true);

    global.addMessageListener("BrowserPlugins:ActivatePlugins", this);
    global.addMessageListener("BrowserPlugins:NotificationShown", this);
    global.addMessageListener("BrowserPlugins:ContextMenuCommand", this);
    global.addMessageListener("BrowserPlugins:NPAPIPluginProcessCrashed", this);
    global.addMessageListener("BrowserPlugins:CrashReportSubmitted", this);
    global.addMessageListener("BrowserPlugins:Test:ClearCrashData", this);

    Services.obs.addObserver(this, "decoder-doctor-notification", false);
  },

  uninit: function() {
    let global = this.global;

    global.removeEventListener("PluginBindingAttached", this, true);
    global.removeEventListener("PluginPlaceholderReplaced", this, true, true);
    global.removeEventListener("PluginCrashed",         this, true);
    global.removeEventListener("PluginOutdated",        this, true);
    global.removeEventListener("PluginInstantiated",    this, true);
    global.removeEventListener("PluginRemoved",         this, true);
    global.removeEventListener("pagehide",              this, true);
    global.removeEventListener("pageshow",              this, true);
    global.removeEventListener("unload",                this);
    global.removeEventListener("HiddenPlugin",          this, true);

    global.removeMessageListener("BrowserPlugins:ActivatePlugins", this);
    global.removeMessageListener("BrowserPlugins:NotificationShown", this);
    global.removeMessageListener("BrowserPlugins:ContextMenuCommand", this);
    global.removeMessageListener("BrowserPlugins:NPAPIPluginProcessCrashed", this);
    global.removeMessageListener("BrowserPlugins:CrashReportSubmitted", this);
    global.removeMessageListener("BrowserPlugins:Test:ClearCrashData", this);

    Services.obs.removeObserver(this, "decoder-doctor-notification");

    delete this.global;
    delete this.content;
  },

  receiveMessage: function(msg) {
    switch (msg.name) {
      case "BrowserPlugins:ActivatePlugins":
        this.activatePlugins(msg.data.pluginInfo, msg.data.newState);
        break;
      case "BrowserPlugins:NotificationShown":
        setTimeout(() => this.updateNotificationUI(), 0);
        break;
      case "BrowserPlugins:ContextMenuCommand":
        switch (msg.data.command) {
          case "play":
            this._showClickToPlayNotification(msg.objects.plugin, true);
            break;
          case "hide":
            this.hideClickToPlayOverlay(msg.objects.plugin);
            break;
        }
        break;
      case "BrowserPlugins:NPAPIPluginProcessCrashed":
        this.NPAPIPluginProcessCrashed({
          pluginName: msg.data.pluginName,
          runID: msg.data.runID,
          state: msg.data.state,
        });
        break;
      case "BrowserPlugins:CrashReportSubmitted":
        this.NPAPIPluginCrashReportSubmitted({
          runID: msg.data.runID,
          state: msg.data.state,
        })
        break;
      case "BrowserPlugins:Test:ClearCrashData":
        // This message should ONLY ever be sent by automated tests.
        if (Services.prefs.getBoolPref("plugins.testmode")) {
          this.pluginCrashData.clear();
        }
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "decoder-doctor-notification":
        let data = JSON.parse(aData);
        if (this.haveShownNotification &&
            aSubject.top.document == this.content.document &&
            data.formats.toLowerCase().includes("application/x-mpegurl", 0)) {
          let principal = this.content.document.nodePrincipal;
          let location = this.content.document.location.href;
          this.global.content.pluginRequiresReload = true;
          this.global.sendAsyncMessage("PluginContent:ShowClickToPlayNotification",
                                       { plugins: [...this.pluginData.values()],
                                         showNow: true,
                                         location: location,
                                       }, null, principal);
        }
    }
  },

  onPageShow: function(event) {
    // Ignore events that aren't from the main document.
    if (!this.content || event.target != this.content.document) {
      return;
    }

    // The PluginClickToPlay events are not fired when navigating using the
    // BF cache. |persisted| is true when the page is loaded from the
    // BF cache, so this code reshows the notification if necessary.
    if (event.persisted) {
      this.reshowClickToPlayNotification();
    }
  },

  onPageHide: function(event) {
    // Ignore events that aren't from the main document.
    if (!this.content || event.target != this.content.document) {
      return;
    }

    this._finishRecordingFlashPluginTelemetry();
    this.clearPluginCaches();
    this.haveShownNotification = false;
  },

  getPluginUI: function(plugin, anonid) {
    return plugin.ownerDocument.
           getAnonymousElementByAttribute(plugin, "anonid", anonid);
  },

  _getPluginInfo: function(pluginElement) {
    if (pluginElement instanceof Ci.nsIDOMHTMLAnchorElement) {
      // Anchor elements are our place holders, and we only have them for Flash
      let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
      return {
        pluginName: "Shockwave Flash",
        mimetype: FLASH_MIME_TYPE,
        permissionString: pluginHost.getPermissionStringForType(FLASH_MIME_TYPE)
      };
    }
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    pluginElement.QueryInterface(Ci.nsIObjectLoadingContent);

    let tagMimetype;
    let pluginName = gNavigatorBundle.GetStringFromName("pluginInfo.unknownPlugin");
    let pluginTag = null;
    let permissionString = null;
    let fallbackType = null;
    let blocklistState = null;

    tagMimetype = pluginElement.actualType;
    if (tagMimetype == "") {
      tagMimetype = pluginElement.type;
    }

    if (this.isKnownPlugin(pluginElement)) {
      pluginTag = pluginHost.getPluginTagForType(pluginElement.actualType);
      pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);

      // Convert this from nsIPluginTag so it can be serialized.
      let properties = ["name", "description", "filename", "version", "enabledState", "niceName"];
      let pluginTagCopy = {};
      for (let prop of properties) {
        pluginTagCopy[prop] = pluginTag[prop];
      }
      pluginTag = pluginTagCopy;

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

  _getPluginInfoForTag: function(pluginTag, tagMimetype) {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

    let pluginName = gNavigatorBundle.GetStringFromName("pluginInfo.unknownPlugin");
    let permissionString = null;
    let blocklistState = null;

    if (pluginTag) {
      pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);

      permissionString = pluginHost.getPermissionStringForTag(pluginTag);
      blocklistState = pluginTag.blocklistState;

      // Convert this from nsIPluginTag so it can be serialized.
      let properties = ["name", "description", "filename", "version", "enabledState", "niceName"];
      let pluginTagCopy = {};
      for (let prop of properties) {
        pluginTagCopy[prop] = pluginTag[prop];
      }
      pluginTag = pluginTagCopy;

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
             fallbackType: null,
             blocklistState: blocklistState,
           };
  },

  /**
   * Update the visibility of the plugin overlay.
   */
  setVisibility : function(plugin, overlay, shouldShow) {
    overlay.classList.toggle("visible", shouldShow);
    if (shouldShow) {
      overlay.removeAttribute("dismissed");
    }
  },

  /**
   * Check whether the plugin should be visible on the page. A plugin should
   * not be visible if the overlay is too big, or if any other page content
   * overlays it.
   *
   * This function will handle showing or hiding the overlay.
   * @returns true if the plugin is invisible.
   */
  shouldShowOverlay : function(plugin, overlay) {
    // If the overlay size is 0, we haven't done layout yet. Presume that
    // plugins are visible until we know otherwise.
    if (overlay.scrollWidth == 0) {
      return true;
    }

    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = plugin.getBoundingClientRect();
    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows = (overlay.scrollWidth > Math.ceil(pluginRect.width)) ||
                    (overlay.scrollHeight - 5 > Math.ceil(pluginRect.height));
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

    let contentWindow = plugin.ownerGlobal;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);

    for (let [x, y] of points) {
      let el = cwu.elementFromPoint(x, y, true, true);
      if (el !== plugin) {
        return false;
      }
    }

    return true;
  },

  addLinkClickCallback: function(linkNode, callbackName /* callbackArgs...*/) {
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
      default:
        // Not all states map to a handler
        return null;
    }
  },

  handleEvent: function(event) {
    let eventType = event.type;

    if (eventType == "unload") {
      this.uninit();
      return;
    }

    if (eventType == "pagehide") {
      this.onPageHide(event);
      return;
    }

    if (eventType == "pageshow") {
      this.onPageShow(event);
      return;
    }

    if (eventType == "PluginRemoved") {
      this.updateNotificationUI(event.target);
      return;
    }

    if (eventType == "click") {
      this.onOverlayClick(event);
      return;
    }

    if (eventType == "PluginCrashed" &&
        !(event.target instanceof Ci.nsIObjectLoadingContent)) {
      // If the event target is not a plugin object (i.e., an <object> or
      // <embed> element), this call is for a window-global plugin.
      this.onPluginCrashed(event.target, event);
      return;
    }

    if (eventType == "HiddenPlugin") {
      let win = event.target.defaultView;
      if (!win.mozHiddenPluginTouched) {
        let pluginTag = event.tag.QueryInterface(Ci.nsIPluginTag);
        if (win.top.document != this.content.document) {
          return;
        }
        this._showClickToPlayNotification(pluginTag, false);
        let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
        try {
          winUtils.loadSheet(REPLACEMENT_STYLE_SHEET, win.AGENT_SHEET);
          win.mozHiddenPluginTouched = true;
        } catch (e) {
          Cu.reportError("Error adding plugin replacement style sheet: " + e);
        }
      }
    }

    let plugin = event.target;

    if (eventType == "PluginPlaceholderReplaced") {
      plugin.removeAttribute("href");
      let overlay = this.getPluginUI(plugin, "main");
      this.setVisibility(plugin, overlay, true);
      let inIDOMUtils = Cc["@mozilla.org/inspector/dom-utils;1"]
                          .getService(Ci.inIDOMUtils);
      // Add psuedo class so our styling will take effect
      inIDOMUtils.addPseudoClassLock(plugin, "-moz-handler-clicktoplay");
      overlay.addEventListener("click", this, true);
      return;
    }

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
    switch (eventType) {
      case "PluginCrashed":
        this.onPluginCrashed(plugin, event);
        break;

      case "PluginNotFound": {
        /* NOP */
        break;
      }

      case "PluginBlocklisted":
      case "PluginOutdated":
        shouldShowNotification = true;
        break;

      case "PluginVulnerableUpdatable":
        let updateLink = this.getPluginUI(plugin, "checkForUpdatesLink");
        let { pluginTag } = this._getPluginInfo(plugin);
        this.addLinkClickCallback(updateLink, "forwardCallback",
                                  "openPluginUpdatePage", pluginTag);
        /* FALLTHRU */

      case "PluginVulnerableNoUpdate":
      case "PluginClickToPlay":
        this._handleClickToPlayEvent(plugin);
        let pluginName = this._getPluginInfo(plugin).pluginName;
        let messageString = gNavigatorBundle.formatStringFromName("PluginClickToActivate", [pluginName], 1);
        let overlayText = this.getPluginUI(plugin, "clickToPlay");
        overlayText.textContent = messageString;
        if (eventType == "PluginVulnerableUpdatable" ||
            eventType == "PluginVulnerableNoUpdate") {
          let vulnerabilityString = gNavigatorBundle.GetStringFromName(eventType);
          let vulnerabilityText = this.getPluginUI(plugin, "vulnerabilityStatus");
          vulnerabilityText.textContent = vulnerabilityString;
        }
        shouldShowNotification = true;
        break;

      case "PluginDisabled":
        let manageLink = this.getPluginUI(plugin, "managePluginsLink");
        this.addLinkClickCallback(manageLink, "forwardCallback", "managePlugins");
        shouldShowNotification = true;
        break;

      case "PluginInstantiated":
        let key = this._getPluginInfo(plugin).pluginTag.niceName;
        Services.telemetry.getKeyedHistogramById('PLUGIN_ACTIVATION_COUNT').add(key);
        shouldShowNotification = true;
        let pluginRect = plugin.getBoundingClientRect();
        if (pluginRect.width <= 5 && pluginRect.height <= 5) {
          Services.telemetry.getHistogramById('PLUGIN_TINY_CONTENT').add(1);
        }
        break;
    }

    if (this._getPluginInfo(plugin).mimetype === FLASH_MIME_TYPE) {
      this._recordFlashPluginTelemetry(eventType, plugin);
    }

    // Show the in-content UI if it's not too big. The crashed plugin handler already did this.
    let overlay = this.getPluginUI(plugin, "main");
    if (eventType != "PluginCrashed") {
      if (overlay != null) {
        this.setVisibility(plugin, overlay,
                           this.shouldShowOverlay(plugin, overlay));
        let resizeListener = (event) => {
          this.setVisibility(plugin, overlay,
            this.shouldShowOverlay(plugin, overlay));
          this.updateNotificationUI();
        };
        plugin.addEventListener("overflow", resizeListener);
        plugin.addEventListener("underflow", resizeListener);
      }
    }

    let closeIcon = this.getPluginUI(plugin, "closeIcon");
    if (closeIcon) {
      closeIcon.addEventListener("click", event => {
        if (event.button == 0 && event.isTrusted) {
          this.hideClickToPlayOverlay(plugin);
          overlay.setAttribute("dismissed", "true");
        }
      }, true);
    }

    if (shouldShowNotification) {
      this._showClickToPlayNotification(plugin, false);
    }
  },

  _recordFlashPluginTelemetry: function(eventType, plugin) {
    if (!Services.telemetry.canRecordExtended) {
      return;
    }

    if (!this.flashPluginStats) {
      this.flashPluginStats = {
        instancesCount: 0,
        plugins: new WeakSet()
      };
    }

    if (!this.flashPluginStats.plugins.has(plugin)) {
      // Reporting plugin instance and its dimensions only once.
      this.flashPluginStats.plugins.add(plugin);

      this.flashPluginStats.instancesCount++;

      let pluginRect = plugin.getBoundingClientRect();
      Services.telemetry.getHistogramById('FLASH_PLUGIN_WIDTH')
                       .add(pluginRect.width);
      Services.telemetry.getHistogramById('FLASH_PLUGIN_HEIGHT')
                       .add(pluginRect.height);
      Services.telemetry.getHistogramById('FLASH_PLUGIN_AREA')
                       .add(pluginRect.width * pluginRect.height);

      let state = this._getPluginInfo(plugin).fallbackType;
      if (state === null) {
        state = Ci.nsIObjectLoadingContent.PLUGIN_UNSUPPORTED;
      }
      Services.telemetry.getHistogramById('FLASH_PLUGIN_STATES')
                       .add(state);
    }
  },

  _finishRecordingFlashPluginTelemetry: function() {
    if (this.flashPluginStats) {
      Services.telemetry.getHistogramById('FLASH_PLUGIN_INSTANCES_ON_PAGE')
                        .add(this.flashPluginStats.instancesCount);
    delete this.flashPluginStats;
    }
  },

  isKnownPlugin: function(objLoadingContent) {
    return (objLoadingContent.getContentTypeForMIMEType(objLoadingContent.actualType) ==
            Ci.nsIObjectLoadingContent.TYPE_PLUGIN);
  },

  canActivatePlugin: function(objLoadingContent) {
    // if this isn't a known plugin, we can't activate it
    // (this also guards pluginHost.getPermissionStringForType against
    // unexpected input)
    if (!this.isKnownPlugin(objLoadingContent))
      return false;

    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    let principal = objLoadingContent.ownerGlobal.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let isFallbackTypeValid =
      objLoadingContent.pluginFallbackType >= Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
      objLoadingContent.pluginFallbackType <= Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE;

    return !objLoadingContent.activated &&
           pluginPermission != Ci.nsIPermissionManager.DENY_ACTION &&
           isFallbackTypeValid;
  },

  hideClickToPlayOverlay: function(plugin) {
    let overlay = this.getPluginUI(plugin, "main");
    if (overlay) {
      overlay.classList.remove("visible");
    }
  },

  // Forward a link click callback to the chrome process.
  forwardCallback: function(name, pluginTag) {
    this.global.sendAsyncMessage("PluginContent:LinkClickCallback",
      { name, pluginTag });
  },

  submitReport: function submitReport(plugin) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    if (!plugin) {
      Cu.reportError("Attempted to submit crash report without an associated plugin.");
      return;
    }
    if (!(plugin instanceof Ci.nsIObjectLoadingContent)) {
      Cu.reportError("Attempted to submit crash report on plugin that does not" +
                     "implement nsIObjectLoadingContent.");
      return;
    }

    let runID = plugin.runID;
    let submitURLOptIn = this.getPluginUI(plugin, "submitURLOptIn").checked;
    let keyVals = {};
    let userComment = this.getPluginUI(plugin, "submitComment").value.trim();
    if (userComment)
      keyVals.PluginUserComment = userComment;
    if (submitURLOptIn)
      keyVals.PluginContentURL = plugin.ownerDocument.URL;

    this.global.sendAsyncMessage("PluginContent:SubmitReport",
                                 { runID, keyVals, submitURLOptIn });
  },

  reloadPage: function() {
    this.global.content.location.reload();
  },

  // Event listener for click-to-play plugins.
  _handleClickToPlayEvent: function(plugin) {
    let doc = plugin.ownerDocument;
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let permissionString;
    if (plugin instanceof Ci.nsIDOMHTMLAnchorElement) {
      // We only have replacement content for Flash installs
      permissionString = pluginHost.getPermissionStringForType(FLASH_MIME_TYPE);
    } else {
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      // guard against giving pluginHost.getPermissionStringForType a type
      // not associated with any known plugin
      if (!this.isKnownPlugin(objLoadingContent))
        return;
      permissionString = pluginHost.getPermissionStringForType(objLoadingContent.actualType);
    }

    let principal = doc.defaultView.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(principal, permissionString);

    let overlay = this.getPluginUI(plugin, "main");

    if (pluginPermission == Ci.nsIPermissionManager.DENY_ACTION) {
      if (overlay) {
        overlay.classList.remove("visible");
      }
      return;
    }

    if (overlay) {
      overlay.addEventListener("click", this, true);
    }
  },

  onOverlayClick: function(event) {
    let document = event.target.ownerDocument;
    let plugin = document.getBindingParent(event.target);
    let contentWindow = plugin.ownerGlobal.top;
    let overlay = this.getPluginUI(plugin, "main");
    // Have to check that the target is not the link to update the plugin
    if (!(event.originalTarget instanceof contentWindow.HTMLAnchorElement) &&
        (event.originalTarget.getAttribute('anonid') != 'closeIcon') &&
        !overlay.hasAttribute('dismissed') &&
        event.button == 0 &&
        event.isTrusted) {
      this._showClickToPlayNotification(plugin, true);
    event.stopPropagation();
    event.preventDefault();
    }
  },

  reshowClickToPlayNotification: function() {
    let contentWindow = this.global.content;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    for (let plugin of plugins) {
      let overlay = this.getPluginUI(plugin, "main");
      if (overlay)
        overlay.removeEventListener("click", this, true);
      let objLoadingContent = plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (this.canActivatePlugin(objLoadingContent))
        this._handleClickToPlayEvent(plugin);
    }
    this._showClickToPlayNotification(null, false);
  },

  /**
   * Activate the plugins that the user has specified.
   */
  activatePlugins: function(pluginInfo, newState) {
    let contentWindow = this.global.content;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);

    let pluginFound = false;
    let placeHolderFound = false;
    for (let plugin of plugins) {
      plugin.QueryInterface(Ci.nsIObjectLoadingContent);
      if (!this.isKnownPlugin(plugin)) {
        continue;
      }
      if (pluginInfo.permissionString == pluginHost.getPermissionStringForType(plugin.actualType)) {
        let overlay = this.getPluginUI(plugin, "main");
        if (plugin instanceof Ci.nsIDOMHTMLAnchorElement) {
          placeHolderFound = true;
        } else {
          pluginFound = true;
        }
        if (newState == "block") {
          if (overlay) {
            overlay.addEventListener("click", this, true);
          }
          plugin.reload(true);
        } else if (this.canActivatePlugin(plugin)) {
          if (overlay) {
            overlay.removeEventListener("click", this, true);
          }
          plugin.playPlugin();
        }
      }
    }

    // If there are no instances of the plugin on the page any more, what the
    // user probably needs is for us to allow and then refresh. Additionally, if
    // this is content that requires HLS or we replaced the placeholder the page
    // needs to be refreshed for it to insert its plugins
    if (newState != "block" &&
       (!pluginFound || placeHolderFound || contentWindow.pluginRequiresReload)) {
      this.reloadPage();
    }
    this.updateNotificationUI();
  },

  _showClickToPlayNotification: function(plugin, showNow) {
    let plugins = [];

    // If plugin is null, that means the user has navigated back to a page with
    // plugins, and we need to collect all the plugins.
    if (plugin === null) {
      let contentWindow = this.content;
      let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIDOMWindowUtils);
      // cwu.plugins may contain non-plugin <object>s, filter them out
      plugins = cwu.plugins.filter((plugin) =>
        plugin.getContentTypeForMIMEType(plugin.actualType) == Ci.nsIObjectLoadingContent.TYPE_PLUGIN);

      if (plugins.length == 0) {
        this.removeNotification("click-to-play-plugins");
        return;
      }
    } else {
      plugins = [plugin];
    }

    let pluginData = this.pluginData;

    let principal = this.content.document.nodePrincipal;
    let location = this.content.document.location.href;

    for (let p of plugins) {
      let pluginInfo;
      if (p instanceof Ci.nsIPluginTag) {
        let mimeType = p.getMimeTypes() > 0 ? p.getMimeTypes()[0] : null;
        pluginInfo = this._getPluginInfoForTag(p, mimeType);
      } else {
        pluginInfo = this._getPluginInfo(p);
      }
      if (pluginInfo.permissionString === null) {
        Cu.reportError("No permission string for active plugin.");
        continue;
      }
      if (pluginData.has(pluginInfo.permissionString)) {
        continue;
      }

      let permissionObj = Services.perms.
        getPermissionObject(principal, pluginInfo.permissionString, false);
      if (permissionObj) {
        pluginInfo.pluginPermissionPrePath = permissionObj.principal.originNoSuffix;
        pluginInfo.pluginPermissionType = permissionObj.expireType;
      }
      else {
        pluginInfo.pluginPermissionPrePath = principal.originNoSuffix;
        pluginInfo.pluginPermissionType = undefined;
      }

      this.pluginData.set(pluginInfo.permissionString, pluginInfo);
    }

    this.haveShownNotification = true;

    this.global.sendAsyncMessage("PluginContent:ShowClickToPlayNotification", {
      plugins: [...this.pluginData.values()],
      showNow: showNow,
      location: location,
    }, null, principal);
  },

  /**
   * Updates the "hidden plugin" notification bar UI.
   *
   * @param document (optional)
   *        Specify the document that is causing the update.
   *        This is useful when the document is possibly no longer
   *        the current loaded document (for example, if we're
   *        responding to a PluginRemoved event for an unloading
   *        document). If this parameter is omitted, it defaults
   *        to the current top-level document.
   */
  updateNotificationUI: function(document) {
    document = document || this.content.document;

    // We're only interested in the top-level document, since that's
    // the one that provides the Principal that we send back to the
    // parent.
    let principal = document.defaultView.top.document.nodePrincipal;
    let location = document.location.href;

    // Make a copy of the actions from the last popup notification.
    let haveInsecure = false;
    let actions = new Map();
    for (let action of this.pluginData.values()) {
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

    // Remove plugins that are already active, or large enough to show an overlay.
    let cwu = this.content.QueryInterface(Ci.nsIInterfaceRequestor)
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

    // If there are any items remaining in `actions` now, they are hidden
    // plugins that need a notification bar.
    this.global.sendAsyncMessage("PluginContent:UpdateHiddenPluginUI", {
      haveInsecure: haveInsecure,
      actions: [...actions.values()],
      location: location,
    }, null, principal);
  },

  removeNotification: function(name) {
    this.global.sendAsyncMessage("PluginContent:RemoveNotification", { name: name });
  },

  clearPluginCaches: function() {
    this.pluginData.clear();
    this.pluginCrashData.clear();
  },

  hideNotificationBar: function(name) {
    this.global.sendAsyncMessage("PluginContent:HideNotificationBar", { name: name });
  },

  /**
   * The PluginCrashed event handler. Note that the PluginCrashed event is
   * fired for both NPAPI and Gecko Media plugins. In the latter case, the
   * target of the event is the document that the GMP is being used in.
   */
  onPluginCrashed: function(target, aEvent) {
    if (!(aEvent instanceof this.content.PluginCrashedEvent))
      return;

    if (aEvent.gmpPlugin) {
      this.GMPCrashed(aEvent);
      return;
    }

    if (!(target instanceof Ci.nsIObjectLoadingContent))
      return;

    let crashData = this.pluginCrashData.get(target.runID);
    if (!crashData) {
      // We haven't received information from the parent yet about
      // this crash, so we should hold off showing the crash report
      // UI.
      return;
    }

    crashData.instances.delete(target);
    if (crashData.instances.length == 0) {
      this.pluginCrashData.delete(target.runID);
    }

    this.setCrashedNPAPIPluginState({
      plugin: target,
      state: crashData.state,
      message: crashData.message,
    });
  },

  NPAPIPluginProcessCrashed: function({pluginName, runID, state}) {
    let message =
      gNavigatorBundle.formatStringFromName("crashedpluginsMessage.title",
                                            [pluginName], 1);

    let contentWindow = this.global.content;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;

    for (let plugin of plugins) {
      if (plugin instanceof Ci.nsIObjectLoadingContent &&
          plugin.runID == runID) {
        // The parent has told us that the plugin process has died.
        // It's possible that this content process hasn't yet noticed,
        // in which case we need to stash this data around until the
        // PluginCrashed events get sent up.
        if (plugin.pluginFallbackType == Ci.nsIObjectLoadingContent.PLUGIN_CRASHED) {
          // This plugin has already been put into the crashed state by the
          // content process, so we can tweak its crash UI without delay.
          this.setCrashedNPAPIPluginState({plugin, state, message});
        } else {
          // The content process hasn't yet determined that the plugin has crashed.
          // Stash the data in our map, and throw the plugin into a WeakSet. When
          // the PluginCrashed event fires on the <object>/<embed>, we'll retrieve
          // the information we need from the Map and remove the instance from the
          // WeakSet. Once the WeakSet is empty, we can clear the map.
          if (!this.pluginCrashData.has(runID)) {
            this.pluginCrashData.set(runID, {
              state: state,
              message: message,
              instances: new WeakSet(),
            });
          }
          let crashData = this.pluginCrashData.get(runID);
          crashData.instances.add(plugin);
        }
      }
    }
  },

  setCrashedNPAPIPluginState: function({plugin, state, message}) {
    // Force a layout flush so the binding is attached.
    plugin.clientTop;
    let overlay = this.getPluginUI(plugin, "main");
    let statusDiv = this.getPluginUI(plugin, "submitStatus");
    let optInCB = this.getPluginUI(plugin, "submitURLOptIn");

    this.getPluginUI(plugin, "submitButton")
        .addEventListener("click", (event) => {
          if (event.button != 0 || !event.isTrusted)
            return;
          this.submitReport(plugin);
        });

    let pref = Services.prefs.getBranch("dom.ipc.plugins.reportCrashURL");
    optInCB.checked = pref.getBoolPref("");

    statusDiv.setAttribute("status", state);

    let helpIcon = this.getPluginUI(plugin, "helpIcon");
    this.addLinkClickCallback(helpIcon, "openHelpPage");

    let crashText = this.getPluginUI(plugin, "crashedText");
    crashText.textContent = message;

    let link = this.getPluginUI(plugin, "reloadLink");
    this.addLinkClickCallback(link, "reloadPage");

    let isShowing = this.shouldShowOverlay(plugin, overlay);

    // Is the <object>'s size too small to hold what we want to show?
    if (!isShowing) {
      // First try hiding the crash report submission UI.
      statusDiv.removeAttribute("status");

      isShowing = this.shouldShowOverlay(plugin, overlay);
    }
    this.setVisibility(plugin, overlay, isShowing);

    let doc = plugin.ownerDocument;
    let runID = plugin.runID;

    if (isShowing) {
      // If a previous plugin on the page was too small and resulted in adding a
      // notification bar, then remove it because this plugin instance it big
      // enough to serve as in-content notification.
      this.hideNotificationBar("plugin-crashed");
      doc.mozNoPluginCrashedNotification = true;

      // Notify others that the crash reporter UI is now ready.
      // Currently, this event is only used by tests.
      let winUtils = this.content.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIDOMWindowUtils);
      let event = new this.content.CustomEvent("PluginCrashReporterDisplayed", {bubbles: true});
      winUtils.dispatchEventToChromeOnly(plugin, event);
    } else if (!doc.mozNoPluginCrashedNotification) {
      // If another plugin on the page was large enough to show our UI, we don't
      // want to show a notification bar.
      this.global.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification",
                                   { messageString: message, pluginID: runID });
      // Remove the notification when the page is reloaded.
      doc.defaultView.top.addEventListener("unload", event => {
        this.hideNotificationBar("plugin-crashed");
      }, false);
    }
  },

  NPAPIPluginCrashReportSubmitted: function({ runID, state }) {
    this.pluginCrashData.delete(runID);
    let contentWindow = this.global.content;
    let cwu = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDOMWindowUtils);
    let plugins = cwu.plugins;

    for (let plugin of plugins) {
      if (plugin instanceof Ci.nsIObjectLoadingContent &&
          plugin.runID == runID) {
        let statusDiv = this.getPluginUI(plugin, "submitStatus");
        statusDiv.setAttribute("status", state);
      }
    }
  },

  GMPCrashed: function(aEvent) {
    let target          = aEvent.target;
    let pluginName      = aEvent.pluginName;
    let gmpPlugin       = aEvent.gmpPlugin;
    let pluginID        = aEvent.pluginID;
    let doc             = target.document;

    if (!gmpPlugin || !doc) {
      // TODO: Throw exception? How did we get here?
      return;
    }

    let messageString =
      gNavigatorBundle.formatStringFromName("crashedpluginsMessage.title",
                                            [pluginName], 1);

    this.global.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification",
                                 { messageString, pluginID });

    // Remove the notification when the page is reloaded.
    doc.defaultView.top.addEventListener("unload", event => {
      this.hideNotificationBar("plugin-crashed");
    }, false);
  },
};
