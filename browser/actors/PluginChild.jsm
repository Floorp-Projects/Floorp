/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["PluginChild"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { BrowserUtils } = ChromeUtils.import(
  "resource://gre/modules/BrowserUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "gNavigatorBundle", function() {
  const url = "chrome://browser/locale/browser.properties";
  return Services.strings.createBundle(url);
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gPluginHost",
  "@mozilla.org/plugin/host;1",
  "nsIPluginHost"
);

const OVERLAY_DISPLAY = {
  HIDDEN: 0, // The overlay will be transparent
  BLANK: 1, // The overlay will be just a grey box
  TINY: 2, // The overlay with a 16x16 plugin icon
  REDUCED: 3, // The overlay with a 32x32 plugin icon
  NOTEXT: 4, // The overlay with a 48x48 plugin icon and the close button
  FULL: 5, // The full overlay: 48x48 plugin icon, close button and label
};

// This gets sent through the content process message manager because the parent
// can't know exactly which child needs to hear about the progress of the
// submission, so we listen "manually" on the CPMM instead of through the actor
// definition.
const kSubmitMsg = "PluginParent:NPAPIPluginCrashReportSubmitted";

class PluginChild extends JSWindowActorChild {
  constructor() {
    super();

    // Cache of plugin crash information sent from the parent
    this.pluginCrashData = new Map();
    Services.cpmm.addMessageListener(kSubmitMsg, this);
  }

  willDestroy() {
    Services.cpmm.removeMessageListener(kSubmitMsg, this);
    if (this._addedListeners) {
      this.contentWindow.removeEventListener("pagehide", this, {
        capture: true,
        mozSystemGroup: true,
      });
      this.contentWindow.removeEventListener("pageshow", this, {
        capture: true,
        mozSystemGroup: true,
      });
    }
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "PluginParent:ActivatePlugins":
        this.activatePlugins(msg.data.activationInfo, msg.data.newState);
        break;
      case "PluginParent:NPAPIPluginCrashReportSubmitted":
        this.NPAPIPluginCrashReportSubmitted({
          runID: msg.data.runID,
          state: msg.data.state,
        });
        break;
      case "PluginParent:Test:ClearCrashData":
        // This message should ONLY ever be sent by automated tests.
        if (Services.prefs.getBoolPref("plugins.testmode")) {
          this.pluginCrashData.clear();
        }
    }
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "decoder-doctor-notification":
        let data = JSON.parse(aData);
        let type = data.type.toLowerCase();
        if (
          type == "cannot-play" &&
          this.haveShownNotification &&
          aSubject.top.document == this.document &&
          data.formats.toLowerCase().includes("application/x-mpegurl", 0)
        ) {
          this.contentWindow.pluginRequiresReload = true;
        }
    }
  }

  onPageShow(event) {
    // Ignore events that aren't from the main document.
    if (!this.contentWindow || event.target != this.document) {
      return;
    }

    // The PluginClickToPlay events are not fired when navigating using the
    // BF cache. |persisted| is true when the page is loaded from the
    // BF cache, so this code reshows the notification if necessary.
    if (event.persisted) {
      this.reshowClickToPlayNotification();
    }
  }

  onPageHide(event) {
    // Ignore events that aren't from the main document.
    if (!this.contentWindow || event.target != this.document) {
      return;
    }

    this.clearPluginCaches();
    this.haveShownNotification = false;
  }

  getPluginUI(pluginElement, anonid) {
    if (
      pluginElement.openOrClosedShadowRoot &&
      pluginElement.openOrClosedShadowRoot.isUAWidget()
    ) {
      return pluginElement.openOrClosedShadowRoot.getElementById(anonid);
    }
    return null;
  }

  _getPluginInfo(pluginElement) {
    if (this.isKnownPlugin(pluginElement)) {
      let pluginTag = gPluginHost.getPluginTagForType(pluginElement.actualType);
      let pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);
      let fallbackType = pluginElement.defaultFallbackType;
      let permissionString = gPluginHost.getPermissionStringForType(
        pluginElement.actualType
      );
      return { pluginTag, pluginName, fallbackType, permissionString };
    }
    return {
      fallbackType: null,
      permissionString: null,
      pluginName: gNavigatorBundle.GetStringFromName(
        "pluginInfo.unknownPlugin"
      ),
      pluginTag: null,
    };
  }

  /**
   * _getPluginInfoForTag is called when iterating the plugins for a document,
   * and what we get from nsIDOMWindowUtils is an nsIPluginTag, and not an
   * nsIObjectLoadingContent. This only should happen if the plugin is
   * click-to-play (see bug 1186948).
   */
  _getPluginInfoForTag(pluginTag) {
    // Since we should only have entered _getPluginInfoForTag when
    // examining a click-to-play plugin, we can safely hard-code
    // this fallback type, since we don't actually have an
    // nsIObjectLoadingContent to check.
    let fallbackType = Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY;
    if (pluginTag) {
      let pluginName = BrowserUtils.makeNicePluginName(pluginTag.name);
      let permissionString = gPluginHost.getPermissionStringForTag(pluginTag);
      return { pluginTag, pluginName, permissionString, fallbackType };
    }
    return {
      fallbackType,
      permissionString: null,
      pluginName: gNavigatorBundle.GetStringFromName(
        "pluginInfo.unknownPlugin"
      ),
      pluginTag: null,
    };
  }

  /**
   * Update the visibility of the plugin overlay.
   */
  setVisibility(plugin, overlay, overlayDisplayState) {
    overlay.classList.toggle(
      "visible",
      overlayDisplayState != OVERLAY_DISPLAY.HIDDEN
    );
    if (overlayDisplayState != OVERLAY_DISPLAY.HIDDEN) {
      overlay.removeAttribute("dismissed");
    }
  }

  /**
   * Adjust the style in which the overlay will be displayed. It might be adjusted
   * based on its size, or if there's some other element covering all corners of
   * the overlay.
   *
   * This function will handle adjusting the style of the overlay, but will
   * not handle hiding it. That is done by setVisibility with the return value
   * from this function.
   *
   * @param {Element} plugin  The plug-in element
   * @param {Element} overlay The overlay element inside the UA Shadow DOM of
   *                          the plug-in element
   * @param {boolean} flushLayout Allow flush layout during computation and
   *                              adjustment.
   * @returns A value from OVERLAY_DISPLAY.
   */
  computeAndAdjustOverlayDisplay(plugin, overlay, flushLayout) {
    let fallbackType = plugin.pluginFallbackType;
    if (plugin.pluginFallbackTypeOverride !== undefined) {
      fallbackType = plugin.pluginFallbackTypeOverride;
    }
    if (fallbackType == Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET) {
      return OVERLAY_DISPLAY.HIDDEN;
    }

    // If the overlay size is 0, we haven't done layout yet. Presume that
    // plugins are visible until we know otherwise.
    if (flushLayout && overlay.scrollWidth == 0) {
      return OVERLAY_DISPLAY.FULL;
    }

    let overlayDisplay = OVERLAY_DISPLAY.FULL;
    let cwu = plugin.ownerGlobal.windowUtils;

    // Is the <object>'s size too small to hold what we want to show?
    let pluginRect = flushLayout
      ? plugin.getBoundingClientRect()
      : cwu.getBoundsWithoutFlushing(plugin);
    let pluginWidth = Math.ceil(pluginRect.width);
    let pluginHeight = Math.ceil(pluginRect.height);

    let layoutNeedsFlush =
      !flushLayout &&
      cwu.needsFlush(cwu.FLUSH_STYLE) &&
      cwu.needsFlush(cwu.FLUSH_LAYOUT);

    // We must set the attributes while here inside this function in order
    // for a possible re-style to occur, which will make the scrollWidth/Height
    // checks below correct. Otherwise, we would be requesting e.g. a TINY
    // overlay here, but the default styling would be used, and that would make
    // it overflow, causing it to change to BLANK instead of remaining as TINY.

    if (layoutNeedsFlush) {
      // Set the content to be oversized when we the overlay size is 0,
      // so that we could receive an overflow event afterwards when there is
      // a layout.
      overlayDisplay = OVERLAY_DISPLAY.FULL;
      overlay.setAttribute("sizing", "oversized");
      overlay.removeAttribute("notext");
    } else if (pluginWidth <= 32 || pluginHeight <= 32) {
      overlay.setAttribute("sizing", "blank");
      overlayDisplay = OVERLAY_DISPLAY.BLANK;
    } else if (pluginWidth <= 80 || pluginHeight <= 60) {
      overlayDisplay = OVERLAY_DISPLAY.TINY;
      overlay.setAttribute("sizing", "tiny");
      overlay.setAttribute("notext", "notext");
    } else if (pluginWidth <= 120 || pluginHeight <= 80) {
      overlayDisplay = OVERLAY_DISPLAY.REDUCED;
      overlay.setAttribute("sizing", "reduced");
      overlay.setAttribute("notext", "notext");
    } else if (pluginWidth <= 240 || pluginHeight <= 160) {
      overlayDisplay = OVERLAY_DISPLAY.NOTEXT;
      overlay.removeAttribute("sizing");
      overlay.setAttribute("notext", "notext");
    } else {
      overlayDisplay = OVERLAY_DISPLAY.FULL;
      overlay.removeAttribute("sizing");
      overlay.removeAttribute("notext");
    }

    // The hit test below only works with correct layout information,
    // don't do it if layout needs flush.
    // We also don't want to access scrollWidth/scrollHeight if
    // the layout needs flush.
    if (layoutNeedsFlush) {
      return overlayDisplay;
    }

    // XXX bug 446693. The text-shadow on the submitted-report text at
    //     the bottom causes scrollHeight to be larger than it should be.
    let overflows =
      overlay.scrollWidth > pluginWidth ||
      overlay.scrollHeight - 5 > pluginHeight;
    if (overflows) {
      overlay.setAttribute("sizing", "blank");
      return OVERLAY_DISPLAY.BLANK;
    }

    // Is the plugin covered up by other content so that it is not clickable?
    // Floating point can confuse .elementFromPoint, so inset just a bit
    let left = pluginRect.left + 2;
    let right = pluginRect.right - 2;
    let top = pluginRect.top + 2;
    let bottom = pluginRect.bottom - 2;
    let centerX = left + (right - left) / 2;
    let centerY = top + (bottom - top) / 2;
    let points = [
      [left, top],
      [left, bottom],
      [right, top],
      [right, bottom],
      [centerX, centerY],
    ];

    for (let [x, y] of points) {
      if (x < 0 || y < 0) {
        continue;
      }
      let el = cwu.elementFromPoint(x, y, true, true);
      if (el === plugin) {
        return overlayDisplay;
      }
    }

    overlay.setAttribute("sizing", "blank");
    return OVERLAY_DISPLAY.BLANK;
  }

  addLinkClickCallback(linkNode, callbackName /* callbackArgs...*/) {
    // XXX just doing (callback)(arg) was giving a same-origin error. bug?
    let self = this;
    let callbackArgs = Array.prototype.slice.call(arguments).slice(2);
    linkNode.addEventListener(
      "click",
      function(evt) {
        if (!evt.isTrusted) {
          return;
        }
        evt.preventDefault();
        if (callbackArgs.length == 0) {
          callbackArgs = [evt];
        }
        self[callbackName].apply(self, callbackArgs);
      },
      true
    );

    linkNode.addEventListener(
      "keydown",
      function(evt) {
        if (!evt.isTrusted) {
          return;
        }
        if (evt.keyCode == evt.DOM_VK_RETURN) {
          evt.preventDefault();
          if (callbackArgs.length == 0) {
            callbackArgs = [evt];
          }
          evt.preventDefault();
          self[callbackName].apply(self, callbackArgs);
        }
      },
      true
    );
  }

  // Helper to get the binding handler type from a plugin object
  _getBindingType(plugin) {
    if (!(plugin instanceof Ci.nsIObjectLoadingContent)) {
      return null;
    }

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
      case Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET:
        return "PluginClickToPlay";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_UPDATABLE:
        return "PluginVulnerableUpdatable";
      case Ci.nsIObjectLoadingContent.PLUGIN_VULNERABLE_NO_UPDATE:
        return "PluginVulnerableNoUpdate";
      default:
        // Not all states map to a handler
        return null;
    }
  }

  handleEvent(event) {
    // Ignore events for other frames.
    let eventDoc = event.target.ownerDocument || event.target.document;
    if (eventDoc && eventDoc != this.document) {
      return;
    }
    if (!this._addedListeners) {
      // Only add pageshow/pagehide listeners here. We don't want this actor
      // to be instantiated for every frame, we only care if a plugin actually
      // gets used in a frame. So we don't add these listeners in the actor
      // specification, but only at runtime once one of our Plugin* events
      // fires.
      this.contentWindow.addEventListener("pagehide", this, {
        capture: true,
        mozSystemGroup: true,
      });
      this.contentWindow.addEventListener("pageshow", this, {
        capture: true,
        mozSystemGroup: true,
      });
      this._addedListeners = true;
    }

    let eventType = event.type;

    if (eventType == "pagehide") {
      this.onPageHide(event);
      return;
    }

    if (eventType == "pageshow") {
      this.onPageShow(event);
      return;
    }

    if (eventType == "click") {
      this.onOverlayClick(event);
      return;
    }

    if (
      eventType == "PluginCrashed" &&
      !(event.target instanceof Ci.nsIObjectLoadingContent)
    ) {
      // If the event target is not a plugin object (i.e., an <object> or
      // <embed> element), this call is for a window-global plugin.
      this.onPluginCrashed(event.target, event);
      return;
    }

    if (eventType == "HiddenPlugin") {
      let pluginTag = event.tag.QueryInterface(Ci.nsIPluginTag);
      this.showClickToPlayNotification(pluginTag, false);
    }

    let pluginElement = event.target;

    if (!(pluginElement instanceof Ci.nsIObjectLoadingContent)) {
      return;
    }

    if (eventType == "PluginBindingAttached") {
      // The plugin binding fires this event when it is created.
      // As an untrusted event, ensure that this object actually has a binding
      // and make sure we don't handle it twice
      let overlay = this.getPluginUI(pluginElement, "main");
      if (!overlay || overlay._bindingHandled) {
        return;
      }
      overlay._bindingHandled = true;

      // Lookup the handler for this binding
      eventType = this._getBindingType(pluginElement);
      if (!eventType) {
        // Not all bindings have handlers
        return;
      }
    }

    let shouldShowNotification = false;
    switch (eventType) {
      case "PluginCrashed":
        this.onPluginCrashed(pluginElement, event);
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
        let updateLink = this.getPluginUI(pluginElement, "checkForUpdatesLink");
        let { pluginTag } = this._getPluginInfo(pluginElement);
        this.addLinkClickCallback(
          updateLink,
          "forwardCallback",
          "openPluginUpdatePage",
          pluginTag.id
        );

      /* FALLTHRU */
      case "PluginVulnerableNoUpdate":
      case "PluginClickToPlay":
        this._handleClickToPlayEvent(pluginElement);
        let { pluginName } = this._getPluginInfo(pluginElement);
        let messageString = gNavigatorBundle.formatStringFromName(
          "PluginClickToActivate2",
          [pluginName]
        );
        let overlayText = this.getPluginUI(pluginElement, "clickToPlay");
        overlayText.textContent = messageString;
        if (
          eventType == "PluginVulnerableUpdatable" ||
          eventType == "PluginVulnerableNoUpdate"
        ) {
          let vulnerabilityString = gNavigatorBundle.GetStringFromName(
            eventType
          );
          let vulnerabilityText = this.getPluginUI(
            pluginElement,
            "vulnerabilityStatus"
          );
          vulnerabilityText.textContent = vulnerabilityString;
        }
        shouldShowNotification = true;
        break;

      case "PluginDisabled":
        let manageLink = this.getPluginUI(pluginElement, "managePluginsLink");
        this.addLinkClickCallback(
          manageLink,
          "forwardCallback",
          "managePlugins"
        );
        shouldShowNotification = true;
        break;

      case "PluginInstantiated":
        shouldShowNotification = true;
        break;
    }

    // Show the in-content UI if it's not too big. The crashed plugin handler already did this.
    let overlay = this.getPluginUI(pluginElement, "main");
    if (eventType != "PluginCrashed") {
      if (overlay != null) {
        this.setVisibility(
          pluginElement,
          overlay,
          this.computeAndAdjustOverlayDisplay(pluginElement, overlay, false)
        );

        let resizeListener = () => {
          this.setVisibility(
            pluginElement,
            overlay,
            this.computeAndAdjustOverlayDisplay(pluginElement, overlay, true)
          );
        };
        pluginElement.addEventListener("overflow", resizeListener);
        pluginElement.addEventListener("underflow", resizeListener);
      }
    }

    let closeIcon = this.getPluginUI(pluginElement, "closeIcon");
    if (closeIcon) {
      closeIcon.addEventListener(
        "click",
        clickEvent => {
          if (clickEvent.button == 0 && clickEvent.isTrusted) {
            this.hideClickToPlayOverlay(pluginElement);
            overlay.setAttribute("dismissed", "true");
          }
        },
        true
      );
    }

    if (shouldShowNotification) {
      this.showClickToPlayNotification(pluginElement, false);
    }
  }

  isKnownPlugin(objLoadingContent) {
    return (
      objLoadingContent.getContentTypeForMIMEType(
        objLoadingContent.actualType
      ) == Ci.nsIObjectLoadingContent.TYPE_PLUGIN
    );
  }

  canActivatePlugin(objLoadingContent) {
    // if this isn't a known plugin, we can't activate it
    // (this also guards pluginHost.getPermissionStringForType against
    // unexpected input)
    if (!this.isKnownPlugin(objLoadingContent)) {
      return false;
    }

    let permissionString = gPluginHost.getPermissionStringForType(
      objLoadingContent.actualType
    );
    let principal = objLoadingContent.ownerGlobal.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(
      principal,
      permissionString
    );

    let isFallbackTypeValid =
      objLoadingContent.pluginFallbackType >=
        Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY &&
      objLoadingContent.pluginFallbackType <=
        Ci.nsIObjectLoadingContent.PLUGIN_CLICK_TO_PLAY_QUIET;

    return (
      !objLoadingContent.activated &&
      pluginPermission != Ci.nsIPermissionManager.DENY_ACTION &&
      isFallbackTypeValid
    );
  }

  hideClickToPlayOverlay(pluginElement) {
    let overlay = this.getPluginUI(pluginElement, "main");
    if (overlay) {
      overlay.classList.remove("visible");
    }
  }

  // Forward a link click callback to the chrome process.
  forwardCallback(name, pluginId) {
    this.sendAsyncMessage("PluginContent:LinkClickCallback", {
      name,
      pluginId,
    });
  }

  submitReport(plugin) {
    if (!AppConstants.MOZ_CRASHREPORTER) {
      return;
    }
    if (!plugin) {
      Cu.reportError(
        "Attempted to submit crash report without an associated plugin."
      );
      return;
    }
    if (!(plugin instanceof Ci.nsIObjectLoadingContent)) {
      Cu.reportError(
        "Attempted to submit crash report on plugin that does not" +
          "implement nsIObjectLoadingContent."
      );
      return;
    }

    let runID = plugin.runID;
    let submitURLOptIn = this.getPluginUI(plugin, "submitURLOptIn").checked;
    let keyVals = {};
    let userComment = this.getPluginUI(plugin, "submitComment").value.trim();
    if (userComment) {
      keyVals.PluginUserComment = userComment;
    }
    if (submitURLOptIn) {
      keyVals.PluginContentURL = plugin.ownerDocument.URL;
    }

    this.sendAsyncMessage("PluginContent:SubmitReport", {
      runID,
      keyVals,
      submitURLOptIn,
    });
  }

  reloadPage() {
    this.contentWindow.location.reload();
  }

  // Event listener for click-to-play plugins.
  _handleClickToPlayEvent(plugin) {
    let doc = plugin.ownerDocument;
    // guard against giving pluginHost.getPermissionStringForType a type
    // not associated with any known plugin
    if (!this.isKnownPlugin(plugin)) {
      return;
    }
    let permissionString = gPluginHost.getPermissionStringForType(
      plugin.actualType
    );
    let principal = doc.defaultView.top.document.nodePrincipal;
    let pluginPermission = Services.perms.testPermissionFromPrincipal(
      principal,
      permissionString
    );

    let overlay = this.getPluginUI(plugin, "main");

    if (
      pluginPermission == Ci.nsIPermissionManager.DENY_ACTION ||
      pluginPermission ==
        Ci.nsIObjectLoadingContent.PLUGIN_PERMISSION_PROMPT_ACTION_QUIET
    ) {
      if (overlay) {
        overlay.classList.remove("visible");
      }
      return;
    }

    if (overlay) {
      overlay.addEventListener("click", this, true);
    }
  }

  onOverlayClick(event) {
    let document = event.target.ownerDocument;
    let plugin = document.getBindingParent(event.target);
    let overlay = this.getPluginUI(plugin, "main");
    // Have to check that the target is not the link to update the plugin
    if (
      !(
        ChromeUtils.getClassName(event.originalTarget) === "HTMLAnchorElement"
      ) &&
      event.originalTarget.getAttribute("anonid") != "closeIcon" &&
      event.originalTarget.id != "closeIcon" &&
      !overlay.hasAttribute("dismissed") &&
      event.button == 0 &&
      event.isTrusted
    ) {
      this.showClickToPlayNotification(plugin, true);
      event.stopPropagation();
      event.preventDefault();
    }
  }

  reshowClickToPlayNotification() {
    let { plugins } = this.contentWindow.windowUtils;
    for (let plugin of plugins) {
      let overlay = this.getPluginUI(plugin, "main");
      if (overlay) {
        overlay.removeEventListener("click", this, true);
      }
      if (this.canActivatePlugin(plugin)) {
        this._handleClickToPlayEvent(plugin);
      }
    }
    this.showClickToPlayNotification(null, false);
  }

  /**
   * Activate the plugins that the user has specified.
   */
  activatePlugins(activationInfo, newState) {
    let { plugins } = this.contentWindow.windowUtils;

    let pluginFound = false;
    for (let plugin of plugins) {
      if (!this.isKnownPlugin(plugin)) {
        continue;
      }
      if (
        activationInfo.permissionString ==
        gPluginHost.getPermissionStringForType(plugin.actualType)
      ) {
        let overlay = this.getPluginUI(plugin, "main");
        pluginFound = true;
        if (
          newState == "block" ||
          newState == "blockalways" ||
          newState == "continueblocking"
        ) {
          if (overlay) {
            overlay.addEventListener("click", this, true);
          }
          plugin.pluginFallbackTypeOverride = activationInfo.fallbackType;
          plugin.reload(true);
        } else if (this.canActivatePlugin(plugin)) {
          if (overlay) {
            overlay.removeEventListener("click", this, true);
          }
          plugin.playPlugin();
        }
      }
    }

    // If there are no instances of the plugin on the page any more or if we've
    // noted that the content needs to be reloaded due to replacing HLS, what the
    // user probably needs is for us to allow and then refresh.
    if (
      newState != "block" &&
      newState != "blockalways" &&
      newState != "continueblocking" &&
      (!pluginFound || this.contentWindow.pluginRequiresReload)
    ) {
      this.reloadPage();
    }
  }

  showClickToPlayNotification(pluginElOrTag, showNow) {
    let plugins = [];

    // If pluginElOrTag is null, that means the user has navigated back to a page with
    // plugins, and we need to collect all the plugins.
    if (pluginElOrTag === null) {
      // cwu.plugins may contain non-plugin <object>s, filter them out
      plugins = this.contentWindow.windowUtils.plugins.filter(
        p =>
          p.getContentTypeForMIMEType(p.actualType) ==
          Ci.nsIObjectLoadingContent.TYPE_PLUGIN
      );

      if (plugins.length == 0) {
        this.removeNotification();
        return;
      }
    } else {
      plugins = [pluginElOrTag];
    }

    // Iterate over the plugins and ensure we have one value for each
    // permission string - though in principle there should only be 1 anyway
    // (for flash), in practice there are still some automated tests where we
    // could encounter other ones.
    let permissionMap = new Map();
    for (let p of plugins) {
      let pluginInfo;
      if (p instanceof Ci.nsIPluginTag) {
        pluginInfo = this._getPluginInfoForTag(p);
      } else {
        pluginInfo = this._getPluginInfo(p);
      }
      if (pluginInfo.permissionString === null) {
        Cu.reportError("No permission string for active plugin.");
        continue;
      }
      if (!permissionMap.has(pluginInfo.permissionString)) {
        permissionMap.set(pluginInfo.permissionString, pluginInfo);
        continue;
      }
    }
    if (permissionMap.size > 1) {
      Cu.reportError(
        "Err, we're not meant to have more than 1 plugin anymore!"
      );
    }
    if (!permissionMap.size) {
      return;
    }

    this.haveShownNotification = true;

    let permissionItem = permissionMap.values().next().value;
    let plugin = {
      id: permissionItem.pluginTag.id,
      fallbackType: permissionItem.fallbackType,
    };

    let msg = "PluginContent:ShowClickToPlayNotification";
    this.sendAsyncMessage(msg, { plugin, showNow });
  }

  removeNotification() {
    this.sendAsyncMessage("PluginContent:RemoveNotification");
  }

  clearPluginCaches() {
    this.pluginCrashData.clear();
  }

  /**
   * Determines whether or not the crashed plugin is contained within current
   * full screen DOM element.
   * @param fullScreenElement (DOM element)
   *   The DOM element that is currently full screen, or null.
   * @param domElement
   *   The DOM element which contains the crashed plugin, or the crashed plugin
   *   itself.
   * @returns bool
   *   True if the plugin is a descendant of the full screen DOM element, false otherwise.
   **/
  isWithinFullScreenElement(fullScreenElement, domElement) {
    /**
     * Traverses down iframes until it find a non-iframe full screen DOM element.
     * @param fullScreenIframe
     *  Target iframe to begin searching from.
     * @returns DOM element
     *  The full screen DOM element contained within the iframe (could be inner iframe), or the original iframe if no inner DOM element is found.
     **/
    let getTrueFullScreenElement = fullScreenIframe => {
      if (
        typeof fullScreenIframe.contentDocument !== "undefined" &&
        fullScreenIframe.contentDocument.mozFullScreenElement
      ) {
        return getTrueFullScreenElement(
          fullScreenIframe.contentDocument.mozFullScreenElement
        );
      }
      return fullScreenIframe;
    };

    if (fullScreenElement.tagName === "IFRAME") {
      fullScreenElement = getTrueFullScreenElement(fullScreenElement);
    }

    if (fullScreenElement.contains(domElement)) {
      return true;
    }
    let parentIframe = domElement.ownerGlobal.frameElement;
    if (parentIframe) {
      return this.isWithinFullScreenElement(fullScreenElement, parentIframe);
    }
    return false;
  }

  /**
   * The PluginCrashed event handler. Note that the PluginCrashed event is
   * fired for both NPAPI and Gecko Media plugins. In the latter case, the
   * target of the event is the document that the GMP is being used in.
   */
  async onPluginCrashed(target, aEvent) {
    if (!(aEvent instanceof this.contentWindow.PluginCrashedEvent)) {
      return;
    }

    let fullScreenElement = this.contentWindow.top.document
      .mozFullScreenElement;
    if (fullScreenElement) {
      if (this.isWithinFullScreenElement(fullScreenElement, target)) {
        this.contentWindow.top.document.mozCancelFullScreen();
      }
    }

    if (aEvent.gmpPlugin) {
      this.GMPCrashed(aEvent);
      return;
    }

    if (!(target instanceof Ci.nsIObjectLoadingContent)) {
      return;
    }

    let crashData = this.pluginCrashData.get(target.runID);
    if (!crashData) {
      // We haven't received information from the parent yet about
      // this crash, so go get it:
      crashData = await this.sendQuery("PluginContent:GetCrashData", {
        runID: target.runID,
      });
      this.pluginCrashData.set(target.runID, crashData);
    }

    this.setCrashedNPAPIPluginState({
      pluginElement: target,
      state: crashData.state,
      pluginName: crashData.pluginName,
    });
  }

  setCrashedNPAPIPluginState({ pluginElement, state, pluginName }) {
    // Force a layout flush so the binding is attached.
    pluginElement.clientTop;
    let overlay = this.getPluginUI(pluginElement, "main");
    let statusDiv = this.getPluginUI(pluginElement, "submitStatus");
    let optInCB = this.getPluginUI(pluginElement, "submitURLOptIn");

    this.getPluginUI(pluginElement, "submitButton").addEventListener(
      "click",
      event => {
        if (event.button != 0 || !event.isTrusted) {
          return;
        }
        this.submitReport(pluginElement);
      }
    );

    optInCB.checked = Services.prefs.getBoolPref(
      "dom.ipc.plugins.reportCrashURL",
      true
    );

    statusDiv.setAttribute("status", state);

    let helpIcon = this.getPluginUI(pluginElement, "helpIcon");
    this.addLinkClickCallback(helpIcon, "openHelpPage");

    let crashText = this.getPluginUI(pluginElement, "crashedText");

    let message = gNavigatorBundle.formatStringFromName(
      "crashedpluginsMessage.title",
      [pluginName]
    );
    crashText.textContent = message;

    let link = this.getPluginUI(pluginElement, "reloadLink");
    this.addLinkClickCallback(link, "reloadPage");

    // This might trigger force reflow, but plug-in crashing code path shouldn't be hot.
    let overlayDisplayState = this.computeAndAdjustOverlayDisplay(
      pluginElement,
      overlay,
      true
    );

    // Is the <object>'s size too small to hold what we want to show?
    if (overlayDisplayState != OVERLAY_DISPLAY.FULL) {
      // First try hiding the crash report submission UI.
      statusDiv.removeAttribute("status");

      overlayDisplayState = this.computeAndAdjustOverlayDisplay(
        pluginElement,
        overlay,
        true
      );
    }
    this.setVisibility(pluginElement, overlay, overlayDisplayState);

    let doc = pluginElement.ownerDocument;
    let runID = pluginElement.runID;

    if (overlayDisplayState == OVERLAY_DISPLAY.FULL) {
      doc.mozNoPluginCrashedNotification = true;

      // Notify others that the crash reporter UI is now ready.
      // Currently, this event is only used by tests.
      let winUtils = this.contentWindow.windowUtils;
      let event = new this.contentWindow.CustomEvent(
        "PluginCrashReporterDisplayed",
        {
          bubbles: true,
        }
      );
      winUtils.dispatchEventToChromeOnly(pluginElement, event);
    } else if (!doc.mozNoPluginCrashedNotification) {
      // If another plugin on the page was large enough to show our UI, we don't
      // want to show a notification bar.
      this.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification", {
        pluginCrashID: { runID },
      });
    }
  }

  NPAPIPluginCrashReportSubmitted({ runID, state }) {
    this.pluginCrashData.delete(runID);
    let { plugins } = this.contentWindow.windowUtils;

    for (let pluginElement of plugins) {
      if (
        pluginElement instanceof Ci.nsIObjectLoadingContent &&
        pluginElement.runID == runID
      ) {
        let statusDiv = this.getPluginUI(pluginElement, "submitStatus");
        statusDiv.setAttribute("status", state);
      }
    }
  }

  GMPCrashed(aEvent) {
    let { target, gmpPlugin, pluginID } = aEvent;
    if (!gmpPlugin || !target.document) {
      // TODO: Throw exception? How did we get here?
      return;
    }

    this.sendAsyncMessage("PluginContent:ShowPluginCrashedNotification", {
      pluginCrashID: { pluginID },
    });
  }
}
