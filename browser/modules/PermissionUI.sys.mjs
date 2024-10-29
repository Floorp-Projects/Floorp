/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * PermissionUI is responsible for exposing both a prototype
 * PermissionPrompt that can be used by arbitrary browser
 * components and add-ons, but also hosts the implementations of
 * built-in permission prompts.
 *
 * If you're developing a feature that requires web content to ask
 * for special permissions from the user, this module is for you.
 *
 * Suppose a system add-on wants to add a new prompt for a new request
 * for getting more low-level access to the user's sound card, and the
 * permission request is coming up from content by way of the
 * nsContentPermissionHelper. The system add-on could then do the following:
 *
 * const { Integration } = ChromeUtils.importESModule(
 *   "resource://gre/modules/Integration.sys.mjs"
 * );
 * const { PermissionUI } = ChromeUtils.importESModule(
 *   "resource:///modules/PermissionUI.sys.mjs"
 * );
 *
 * const SoundCardIntegration = base => {
 *   let soundCardObj = {
 *     createPermissionPrompt(type, request) {
 *       if (type != "sound-api") {
 *         return super.createPermissionPrompt(...arguments);
 *       }
 *
 *       let permissionPrompt = {
 *         get permissionKey() {
 *           return "sound-permission";
 *         }
 *         // etc - see the documentation for PermissionPrompt for
 *         // a better idea of what things one can and should override.
 *       };
 *       Object.setPrototypeOf(
 *         permissionPrompt,
 *         PermissionUI.PermissionPromptForRequest
 *       );
 *       return permissionPrompt;
 *     },
 *   };
 *   Object.setPrototypeOf(soundCardObj, base);
 *   return soundCardObj;
 * };
 *
 * // Add-on startup:
 * Integration.contentPermission.register(SoundCardIntegration);
 * // ...
 * // Add-on shutdown:
 * Integration.contentPermission.unregister(SoundCardIntegration);
 *
 * Note that PermissionPromptForRequest must be used as the
 * prototype, since the prompt is wrapping an nsIContentPermissionRequest,
 * and going through nsIContentPermissionPrompt.
 *
 * It is, however, possible to take advantage of PermissionPrompt without
 * having to go through nsIContentPermissionPrompt or with a
 * nsIContentPermissionRequest. The PermissionPrompt can be
 * imported, subclassed, and have prompt() called directly, without
 * the caller having called into createPermissionPrompt.
 */
import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  SitePermissions: "resource:///modules/SitePermissions.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "IDNService",
  "@mozilla.org/network/idn-service;1",
  "nsIIDNService"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ContentPrefService2",
  "@mozilla.org/content-pref/service;1",
  "nsIContentPrefService2"
);

ChromeUtils.defineLazyGetter(lazy, "gBrandBundle", function () {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

ChromeUtils.defineLazyGetter(lazy, "gBrowserBundle", function () {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

import { SITEPERMS_ADDON_PROVIDER_PREF } from "resource://gre/modules/addons/siteperms-addon-utils.sys.mjs";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "sitePermsAddonsProviderEnabled",
  SITEPERMS_ADDON_PROVIDER_PREF,
  false
);

/**
 * PermissionPrompt should be subclassed by callers that
 * want to display prompts to the user. See each method and property
 * below for guidance on what to override.
 *
 * Note that if you're creating a prompt for an
 * nsIContentPermissionRequest, you'll want to subclass
 * PermissionPromptForRequest instead.
 */
class PermissionPrompt {
  /**
   * Returns the associated <xul:browser> for the request. This should
   * work for the e10s and non-e10s case.
   *
   * Subclasses must override this.
   *
   * @return {<xul:browser>}
   */
  get browser() {
    throw new Error("Not implemented.");
  }

  /**
   * Returns the nsIPrincipal associated with the request.
   *
   * Subclasses must override this.
   *
   * @return {nsIPrincipal}
   */
  get principal() {
    throw new Error("Not implemented.");
  }

  /**
   * Indicates the type of the permission request from content. This type might
   * be different from the permission key used in the permissions database.
   */
  get type() {
    return undefined;
  }

  /**
   * If the nsIPermissionManager is being queried and written
   * to for this permission request, set this to the key to be
   * used. If this is undefined, no integration with temporary
   * permissions infrastructure will be provided.
   *
   * Note that if a permission is set, in any follow-up
   * prompting within the expiry window of that permission,
   * the prompt will be skipped and the allow or deny choice
   * will be selected automatically.
   */
  get permissionKey() {
    return undefined;
  }

  /**
   * If true, user permissions will be read from and written to.
   * When this is false, we still provide integration with
   * infrastructure such as temporary permissions. permissionKey should
   * still return a valid name in those cases for that integration to work.
   */
  get usePermissionManager() {
    return true;
  }

  /**
   * Indicates what URI should be used as the scope when using temporary
   * permissions. If undefined, it defaults to the browser.currentURI.
   */
  get temporaryPermissionURI() {
    return undefined;
  }

  /**
   * These are the options that will be passed to the PopupNotification when it
   * is shown. See the documentation of `PopupNotifications_show` in
   * PopupNotifications.sys.mjs for details.
   *
   * Note that prompt() will automatically set displayURI to
   * be the URI of the requesting pricipal, unless the displayURI is exactly
   * set to false.
   */
  get popupOptions() {
    return {};
  }

  /**
   * If true, automatically denied permission requests will
   * spawn a "post-prompt" that allows the user to correct the
   * automatic denial by giving permanent permission access to
   * the site.
   *
   * Note that if this function returns true, the permissionKey
   * and postPromptActions attributes must be implemented.
   */
  get postPromptEnabled() {
    return false;
  }

  /**
   * If true, the prompt will be cancelled automatically unless
   * request.hasValidTransientUserGestureActivation is true.
   */
  get requiresUserInput() {
    return false;
  }

  /**
   * PopupNotification requires a unique ID to open the notification.
   * You must return a unique ID string here, for which PopupNotification
   * will then create a <xul:popupnotification> node with the ID
   * "<notificationID>-notification".
   *
   * If there's a custom <xul:popupnotification> you're hoping to show,
   * then you need to make sure its ID has the "-notification" suffix,
   * and then return the prefix here.
   *
   * See PopupNotifications.sys.mjs for more details.
   *
   * @return {string}
   *         The unique ID that will be used to as the
   *         "<unique ID>-notification" ID for the <xul:popupnotification>
   *         to use or create.
   */
  get notificationID() {
    throw new Error("Not implemented.");
  }

  /**
   * The ID of the element to anchor the PopupNotification to.
   *
   * @return {string}
   */
  get anchorID() {
    return "default-notification-icon";
  }

  /**
   * The message to show to the user in the PopupNotification, see
   * `PopupNotifications_show` in PopupNotifications.sys.mjs.
   *
   * Subclasses must override this.
   *
   * @return {string}
   */
  get message() {
    throw new Error("Not implemented.");
  }

  /**
   * The hint text to show to the user in the PopupNotification, see
   * `PopupNotifications_show` in PopupNotifications.sys.mjs.
   * By default, no hint is shown.
   *
   * @return {string}
   */
  get hintText() {
    return undefined;
  }

  /**
   * Provides the preferred name to use in the permission popups,
   * based on the principal URI (the URI.hostPort for any URI scheme
   * besides the moz-extension one which should default to the
   * extension name).
   */
  getPrincipalName(principal = this.principal) {
    if (principal.addonPolicy) {
      return principal.addonPolicy.name;
    }

    return principal.hostPort;
  }

  /**
   * This will be called if the request is to be cancelled.
   *
   * Subclasses only need to override this if they provide a
   * permissionKey.
   */
  cancel() {
    throw new Error("Not implemented.");
  }

  /**
   * This will be called if the request is to be allowed.
   *
   * Subclasses only need to override this if they provide a
   * permissionKey.
   */
  allow() {
    throw new Error("Not implemented.");
  }

  /**
   * The actions that will be displayed in the PopupNotification
   * via a dropdown menu. The first item in this array will be
   * the default selection. Each action is an Object with the
   * following properties:
   *
   *  label (string):
   *    The label that will be displayed for this choice.
   *  accessKey (string):
   *    The access key character that will be used for this choice.
   *  action (SitePermissions state)
   *    The action that will be associated with this choice.
   *    This should be either SitePermissions.ALLOW or SitePermissions.BLOCK.
   *  scope (SitePermissions scope)
   *    The scope of the associated action (e.g. SitePermissions.SCOPE_PERSISTENT)
   *
   *  callback (function, optional)
   *    A callback function that will fire if the user makes this choice, with
   *    a single parameter, state. State is an Object that contains the property
   *    checkboxChecked, which identifies whether the checkbox to remember this
   *    decision was checked.
   */
  get promptActions() {
    return [];
  }

  /**
   * The actions that will be displayed in the PopupNotification
   * for post-prompt notifications via a dropdown menu.
   * The first item in this array will be the default selection.
   * Each action is an Object with the following properties:
   *
   *  label (string):
   *    The label that will be displayed for this choice.
   *  accessKey (string):
   *    The access key character that will be used for this choice.
   *  action (SitePermissions state)
   *    The action that will be associated with this choice.
   *    This should be either SitePermissions.ALLOW or SitePermissions.BLOCK.
   *    Note that the scope of this action will always be persistent.
   *
   *  callback (function, optional)
   *    A callback function that will fire if the user makes this choice.
   */
  get postPromptActions() {
    return null;
  }

  /**
   * If the prompt will be shown to the user, this callback will
   * be called just before. Subclasses may want to override this
   * in order to, for example, bump a counter Telemetry probe for
   * how often a particular permission request is seen.
   *
   * If this returns false, it cancels the process of showing the prompt.  In
   * that case, it is the responsibility of the onBeforeShow() implementation
   * to ensure that allow() or cancel() are called on the object appropriately.
   */
  onBeforeShow() {
    return true;
  }

  /**
   * If the prompt was shown to the user, this callback will be called just
   * after it's been shown.
   */
  onShown() {}

  /**
   * If the prompt was shown to the user, this callback will be called just
   * after it's been hidden.
   */
  onAfterShow() {}

  /**
   * Will determine if a prompt should be shown to the user, and if so,
   * will show it.
   *
   * If a permissionKey is defined prompt() might automatically
   * allow or cancel itself based on the user's current
   * permission settings without displaying the prompt.
   *
   * If the permission is not already set and the <xul:browser> that the request
   * is associated with does not belong to a browser window with the
   * PopupNotifications global set, the prompt request is ignored.
   */
  prompt() {
    // We ignore requests from non-nsIStandardURLs
    let requestingURI = this.principal.URI;
    if (!(requestingURI instanceof Ci.nsIStandardURL)) {
      return;
    }

    if (this.usePermissionManager && this.permissionKey) {
      // If we're reading and setting permissions, then we need
      // to check to see if we already have a permission setting
      // for this particular principal.
      let { state } = lazy.SitePermissions.getForPrincipal(
        this.principal,
        this.permissionKey,
        this.browser,
        this.temporaryPermissionURI
      );

      if (state == lazy.SitePermissions.BLOCK) {
        this.cancel();
        return;
      }

      if (
        state == lazy.SitePermissions.ALLOW &&
        !this.request.isRequestDelegatedToUnsafeThirdParty
      ) {
        this.allow();
        return;
      }
    } else if (this.permissionKey) {
      // If we're reading a permission which already has a temporary value,
      // see if we can use the temporary value.
      let { state } = lazy.SitePermissions.getForPrincipal(
        null,
        this.permissionKey,
        this.browser,
        this.temporaryPermissionURI
      );

      if (state == lazy.SitePermissions.BLOCK) {
        this.cancel();
        return;
      }
    }

    if (
      this.requiresUserInput &&
      !this.request.hasValidTransientUserGestureActivation
    ) {
      if (this.postPromptEnabled) {
        this.postPrompt();
      }
      this.cancel();
      return;
    }

    let chromeWin = this.browser.ownerGlobal;
    if (!chromeWin.PopupNotifications) {
      this.cancel();
      return;
    }

    // Transform the PermissionPrompt actions into PopupNotification actions.
    let popupNotificationActions = [];
    for (let promptAction of this.promptActions) {
      let action = {
        label: promptAction.label,
        accessKey: promptAction.accessKey,
        callback: state => {
          if (promptAction.callback) {
            promptAction.callback();
          }

          if (this.usePermissionManager && this.permissionKey) {
            if (
              (state && state.checkboxChecked && state.source != "esc-press") ||
              promptAction.scope == lazy.SitePermissions.SCOPE_PERSISTENT
            ) {
              // Permanently store permission.
              let scope = lazy.SitePermissions.SCOPE_PERSISTENT;
              // Only remember permission for session if in PB mode.
              if (lazy.PrivateBrowsingUtils.isBrowserPrivate(this.browser)) {
                scope = lazy.SitePermissions.SCOPE_SESSION;
              }
              lazy.SitePermissions.setForPrincipal(
                this.principal,
                this.permissionKey,
                promptAction.action,
                scope
              );
            } else if (promptAction.action == lazy.SitePermissions.BLOCK) {
              // Temporarily store BLOCK permissions only
              // SitePermissions does not consider subframes when storing temporary
              // permissions on a tab, thus storing ALLOW could be exploited.
              lazy.SitePermissions.setForPrincipal(
                this.principal,
                this.permissionKey,
                promptAction.action,
                lazy.SitePermissions.SCOPE_TEMPORARY,
                this.browser
              );
            }

            // Grant permission if action is ALLOW.
            if (promptAction.action == lazy.SitePermissions.ALLOW) {
              this.allow();
            } else {
              this.cancel();
            }
          } else if (this.permissionKey) {
            // TODO: Add support for permitTemporaryAllow
            if (promptAction.action == lazy.SitePermissions.BLOCK) {
              // Temporarily store BLOCK permissions.
              // We don't consider subframes when storing temporary
              // permissions on a tab, thus storing ALLOW could be exploited.
              lazy.SitePermissions.setForPrincipal(
                null,
                this.permissionKey,
                promptAction.action,
                lazy.SitePermissions.SCOPE_TEMPORARY,
                this.browser
              );
            }
          }
        },
      };
      if (promptAction.dismiss) {
        action.dismiss = promptAction.dismiss;
      }

      popupNotificationActions.push(action);
    }

    this.#showNotification(popupNotificationActions);
  }

  postPrompt() {
    let browser = this.browser;
    let principal = this.principal;
    let chromeWin = browser.ownerGlobal;
    if (!chromeWin.PopupNotifications) {
      return;
    }

    if (!this.permissionKey) {
      throw new Error("permissionKey is required to show a post-prompt");
    }

    if (!this.postPromptActions) {
      throw new Error("postPromptActions are required to show a post-prompt");
    }

    // Transform the PermissionPrompt actions into PopupNotification actions.
    let popupNotificationActions = [];
    for (let promptAction of this.postPromptActions) {
      let action = {
        label: promptAction.label,
        accessKey: promptAction.accessKey,
        callback: () => {
          if (promptAction.callback) {
            promptAction.callback();
          }

          // Post-prompt permissions are stored permanently by default.
          // Since we can not reply to the original permission request anymore,
          // the page will need to listen for permission changes which are triggered
          // by permanent entries in the permission manager.
          let scope = lazy.SitePermissions.SCOPE_PERSISTENT;
          // Only remember permission for session if in PB mode.
          if (lazy.PrivateBrowsingUtils.isBrowserPrivate(browser)) {
            scope = lazy.SitePermissions.SCOPE_SESSION;
          }
          lazy.SitePermissions.setForPrincipal(
            principal,
            this.permissionKey,
            promptAction.action,
            scope
          );
        },
      };
      popupNotificationActions.push(action);
    }

    // Post-prompt animation
    if (!chromeWin.gReduceMotion) {
      let anchor = chromeWin.document.getElementById(this.anchorID);
      // Only show the animation on the first request, not after e.g. tab switching.
      anchor.addEventListener(
        "animationend",
        () => anchor.removeAttribute("animate"),
        { once: true }
      );
      anchor.setAttribute("animate", "true");
    }

    this.#showNotification(popupNotificationActions, true);
  }

  #showNotification(actions, postPrompt = false) {
    let chromeWin = this.browser.ownerGlobal;
    let mainAction = actions.length ? actions[0] : null;
    let secondaryActions = actions.splice(1);

    let options = this.popupOptions;

    if (!options.hasOwnProperty("displayURI") || options.displayURI) {
      options.displayURI = this.principal.URI;
    }

    if (!postPrompt) {
      // Permission prompts are always persistent; the close button is controlled by a pref.
      options.persistent = true;
      options.hideClose = true;
    }

    options.eventCallback = (topic, nextRemovalReason, isCancel) => {
      // When the docshell of the browser is aboout to be swapped to another one,
      // the "swapping" event is called. Returning true causes the notification
      // to be moved to the new browser.
      if (topic == "swapping") {
        return true;
      }
      // The prompt has been shown, notify the PermissionUI.
      // onShown() is currently not called for post-prompts,
      // because there is no prompt that would make use of this.
      // You can remove this restriction if you need it, but be
      // mindful of other consumers.
      if (topic == "shown" && !postPrompt) {
        this.onShown();
      }
      // The prompt has been removed, notify the PermissionUI.
      // onAfterShow() is currently not called for post-prompts,
      // because there is no prompt that would make use of this.
      // You can remove this restriction if you need it, but be
      // mindful of other consumers.
      if (topic == "removed" && !postPrompt) {
        if (isCancel) {
          this.cancel();
        }
        this.onAfterShow();
      }
      return false;
    };

    options.hintText = this.hintText;
    // Post-prompts show up as dismissed.
    options.dismissed = postPrompt;

    // onBeforeShow() is currently not called for post-prompts,
    // because there is no prompt that would make use of this.
    // You can remove this restriction if you need it, but be
    // mindful of other consumers.
    if (postPrompt || this.onBeforeShow() !== false) {
      chromeWin.PopupNotifications.show(
        this.browser,
        this.notificationID,
        this.message,
        this.anchorID,
        mainAction,
        secondaryActions,
        options
      );
    }
  }
}

/**
 * A subclass of PermissionPrompt that assumes
 * that this.request is an nsIContentPermissionRequest
 * and fills in some of the required properties on the
 * PermissionPrompt. For callers that are wrapping an
 * nsIContentPermissionRequest, this should be subclassed
 * rather than PermissionPrompt.
 */
class PermissionPromptForRequest extends PermissionPrompt {
  get browser() {
    // In the e10s-case, the <xul:browser> will be at request.element.
    // In the single-process case, we have to use some XPCOM incantations
    // to resolve to the <xul:browser>.
    if (this.request.element) {
      return this.request.element;
    }
    return this.request.window.docShell.chromeEventHandler;
  }

  get principal() {
    let request = this.request.QueryInterface(Ci.nsIContentPermissionRequest);
    return request.getDelegatePrincipal(this.type);
  }

  cancel() {
    this.request.cancel();
  }

  allow(choices) {
    this.request.allow(choices);
  }
}

/**
 * A subclass of PermissionPromptForRequest that prompts
 * for a Synthetic SitePermsAddon addon type and starts a synthetic
 * addon install flow.
 */
class SitePermsAddonInstallRequest extends PermissionPromptForRequest {
  prompt() {
    // fallback to regular permission prompt for localhost,
    // or when the SitePermsAddonProvider is not enabled.
    if (this.principal.isLoopbackHost || !lazy.sitePermsAddonsProviderEnabled) {
      super.prompt();
      return;
    }

    // Otherwise, we'll use the addon install flow.
    lazy.AddonManager.installSitePermsAddonFromWebpage(
      this.browser,
      this.principal,
      this.permName
    ).then(
      () => {
        this.allow();
      },
      err => {
        this.cancel();

        // Print an error message in the console to give more information to the developer.
        let scriptErrorClass = Cc["@mozilla.org/scripterror;1"];
        let errorMessage =
          this.getInstallErrorMessage(err) ||
          `${this.permName} access was rejected: ${err.message}`;

        let scriptError = scriptErrorClass.createInstance(Ci.nsIScriptError);
        scriptError.initWithWindowID(
          errorMessage,
          null,
          null,
          0,
          0,
          0,
          "content javascript",
          this.browser.browsingContext.currentWindowGlobal.innerWindowId
        );
        Services.console.logMessage(scriptError);
      }
    );
  }

  /**
   * Returns an error message that will be printed to the console given a passed Component.Exception.
   * This should be overriden by children classes.
   *
   * @param {Components.Exception} err
   * @returns {String} The error message
   */
  getInstallErrorMessage() {
    return null;
  }
}

/**
 * Creates a PermissionPrompt for a nsIContentPermissionRequest for
 * the GeoLocation API.
 *
 * @param request (nsIContentPermissionRequest)
 *        The request for a permission from content.
 */
class GeolocationPermissionPrompt extends PermissionPromptForRequest {
  constructor(request) {
    super();
    this.request = request;
    let types = request.types.QueryInterface(Ci.nsIArray);
    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    if (perm.options.length) {
      this.systemPermissionMsg = perm.options.queryElementAt(
        0,
        Ci.nsISupportsString
      );
    }
  }

  get type() {
    return "geo";
  }

  get permissionKey() {
    return "geo";
  }

  get popupOptions() {
    let pref = "browser.geolocation.warning.infoURL";
    let options = {
      learnMoreURL: Services.urlFormatter.formatURLPref(pref),
      displayURI: false,
      name: this.getPrincipalName(),
    };

    // Don't offer "always remember" action in PB mode
    options.checkbox = {
      show: !lazy.PrivateBrowsingUtils.isWindowPrivate(
        this.browser.ownerGlobal
      ),
    };

    if (this.request.isRequestDelegatedToUnsafeThirdParty) {
      // Second name should be the third party origin
      options.secondName = this.getPrincipalName(this.request.principal);
      options.checkbox = { show: false };
    }

    if (options.checkbox.show) {
      options.checkbox.label = lazy.gBrowserBundle.GetStringFromName(
        "geolocation.remember"
      );
    }

    return options;
  }

  get notificationID() {
    return "geolocation";
  }

  get anchorID() {
    return "geo-notification-icon";
  }

  get message() {
    if (this.principal.schemeIs("file")) {
      return lazy.gBrowserBundle.GetStringFromName(
        "geolocation.shareWithFile4"
      );
    }

    if (this.request.isRequestDelegatedToUnsafeThirdParty) {
      return lazy.gBrowserBundle.formatStringFromName(
        "geolocation.shareWithSiteUnsafeDelegation2",
        ["<>", "{}"]
      );
    }

    return lazy.gBrowserBundle.formatStringFromName(
      "geolocation.shareWithSite4",
      ["<>"]
    );
  }

  get hintText() {
    let productName = lazy.gBrandBundle.GetStringFromName("brandShortName");

    if (this.systemPermissionMsg == "sysdlg") {
      return lazy.gBrowserBundle.formatStringFromName(
        "geolocation.systemWillRequestPermission",
        [productName]
      );
    }

    if (this.systemPermissionMsg == "syssetting") {
      return lazy.gBrowserBundle.formatStringFromName(
        "geolocation.needsSystemSetting",
        [productName]
      );
    }

    return undefined;
  }

  get promptActions() {
    return [
      {
        label: lazy.gBrowserBundle.GetStringFromName("geolocation.allow"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "geolocation.allow.accesskey"
        ),
        action: lazy.SitePermissions.ALLOW,
      },
      {
        label: lazy.gBrowserBundle.GetStringFromName("geolocation.block"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "geolocation.block.accesskey"
        ),
        action: lazy.SitePermissions.BLOCK,
      },
    ];
  }

  #updateGeoSharing(state) {
    let gBrowser = this.browser.ownerGlobal.gBrowser;
    if (gBrowser == null) {
      return;
    }
    gBrowser.updateBrowserSharing(this.browser, { geo: state });

    // Update last access timestamp
    let host;
    try {
      host = this.browser.currentURI.host;
    } catch (e) {
      return;
    }
    if (host == null || host == "") {
      return;
    }
    lazy.ContentPrefService2.set(
      this.browser.currentURI.host,
      "permissions.geoLocation.lastAccess",
      new Date().toString(),
      this.browser.loadContext
    );
  }

  allow(...args) {
    this.#updateGeoSharing(true);
    super.allow(...args);
  }

  cancel(...args) {
    this.#updateGeoSharing(false);
    super.cancel(...args);
  }
}

/**
 * Creates a PermissionPrompt for a nsIContentPermissionRequest for
 * the WebXR API.
 *
 * @param request (nsIContentPermissionRequest)
 *        The request for a permission from content.
 */
class XRPermissionPrompt extends PermissionPromptForRequest {
  constructor(request) {
    super();
    this.request = request;
  }

  get type() {
    return "xr";
  }

  get permissionKey() {
    return "xr";
  }

  get popupOptions() {
    let pref = "browser.xr.warning.infoURL";
    let options = {
      learnMoreURL: Services.urlFormatter.formatURLPref(pref),
      displayURI: false,
      name: this.getPrincipalName(),
    };

    // Don't offer "always remember" action in PB mode
    options.checkbox = {
      show: !lazy.PrivateBrowsingUtils.isWindowPrivate(
        this.browser.ownerGlobal
      ),
    };

    if (options.checkbox.show) {
      options.checkbox.label =
        lazy.gBrowserBundle.GetStringFromName("xr.remember");
    }

    return options;
  }

  get notificationID() {
    return "xr";
  }

  get anchorID() {
    return "xr-notification-icon";
  }

  get message() {
    if (this.principal.schemeIs("file")) {
      return lazy.gBrowserBundle.GetStringFromName("xr.shareWithFile4");
    }

    return lazy.gBrowserBundle.formatStringFromName("xr.shareWithSite4", [
      "<>",
    ]);
  }

  get promptActions() {
    return [
      {
        label: lazy.gBrowserBundle.GetStringFromName("xr.allow2"),
        accessKey: lazy.gBrowserBundle.GetStringFromName("xr.allow2.accesskey"),
        action: lazy.SitePermissions.ALLOW,
      },
      {
        label: lazy.gBrowserBundle.GetStringFromName("xr.block"),
        accessKey: lazy.gBrowserBundle.GetStringFromName("xr.block.accesskey"),
        action: lazy.SitePermissions.BLOCK,
      },
    ];
  }

  #updateXRSharing(state) {
    let gBrowser = this.browser.ownerGlobal.gBrowser;
    if (gBrowser == null) {
      return;
    }
    gBrowser.updateBrowserSharing(this.browser, { xr: state });

    let devicePermOrigins = this.browser.getDevicePermissionOrigins("xr");
    if (!state) {
      devicePermOrigins.delete(this.principal.origin);
      return;
    }
    devicePermOrigins.add(this.principal.origin);
  }

  allow(...args) {
    this.#updateXRSharing(true);
    super.allow(...args);
  }

  cancel(...args) {
    this.#updateXRSharing(false);
    super.cancel(...args);
  }
}

/**
 * Creates a PermissionPrompt for a nsIContentPermissionRequest for
 * the Desktop Notification API.
 *
 * @param request (nsIContentPermissionRequest)
 *        The request for a permission from content.
 * @return {PermissionPrompt} (see documentation in header)
 */
class DesktopNotificationPermissionPrompt extends PermissionPromptForRequest {
  constructor(request) {
    super();
    this.request = request;

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "requiresUserInput",
      "dom.webnotifications.requireuserinteraction"
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "postPromptEnabled",
      "permissions.desktop-notification.postPrompt.enabled"
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "notNowEnabled",
      "permissions.desktop-notification.notNow.enabled"
    );
  }

  get type() {
    return "desktop-notification";
  }

  get permissionKey() {
    return "desktop-notification";
  }

  get popupOptions() {
    let learnMoreURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") + "push";

    return {
      learnMoreURL,
      displayURI: false,
      name: this.getPrincipalName(),
    };
  }

  get notificationID() {
    return "web-notifications";
  }

  get anchorID() {
    return "web-notifications-notification-icon";
  }

  get message() {
    return lazy.gBrowserBundle.formatStringFromName(
      "webNotifications.receiveFromSite3",
      ["<>"]
    );
  }

  get promptActions() {
    let actions = [
      {
        label: lazy.gBrowserBundle.GetStringFromName("webNotifications.allow2"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "webNotifications.allow2.accesskey"
        ),
        action: lazy.SitePermissions.ALLOW,
        scope: lazy.SitePermissions.SCOPE_PERSISTENT,
      },
    ];
    if (this.notNowEnabled) {
      actions.push({
        label: lazy.gBrowserBundle.GetStringFromName("webNotifications.notNow"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "webNotifications.notNow.accesskey"
        ),
        action: lazy.SitePermissions.BLOCK,
      });
    }

    let isBrowserPrivate = lazy.PrivateBrowsingUtils.isBrowserPrivate(
      this.browser
    );
    actions.push({
      label: isBrowserPrivate
        ? lazy.gBrowserBundle.GetStringFromName("webNotifications.block")
        : lazy.gBrowserBundle.GetStringFromName("webNotifications.alwaysBlock"),
      accessKey: isBrowserPrivate
        ? lazy.gBrowserBundle.GetStringFromName(
            "webNotifications.block.accesskey"
          )
        : lazy.gBrowserBundle.GetStringFromName(
            "webNotifications.alwaysBlock.accesskey"
          ),
      action: lazy.SitePermissions.BLOCK,
      scope: isBrowserPrivate
        ? lazy.SitePermissions.SCOPE_SESSION
        : lazy.SitePermissions.SCOPE_PERSISTENT,
    });
    return actions;
  }

  get postPromptActions() {
    let actions = [
      {
        label: lazy.gBrowserBundle.GetStringFromName("webNotifications.allow2"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "webNotifications.allow2.accesskey"
        ),
        action: lazy.SitePermissions.ALLOW,
      },
    ];

    let isBrowserPrivate = lazy.PrivateBrowsingUtils.isBrowserPrivate(
      this.browser
    );
    actions.push({
      label: isBrowserPrivate
        ? lazy.gBrowserBundle.GetStringFromName("webNotifications.block")
        : lazy.gBrowserBundle.GetStringFromName("webNotifications.alwaysBlock"),
      accessKey: isBrowserPrivate
        ? lazy.gBrowserBundle.GetStringFromName(
            "webNotifications.block.accesskey"
          )
        : lazy.gBrowserBundle.GetStringFromName(
            "webNotifications.alwaysBlock.accesskey"
          ),
      action: lazy.SitePermissions.BLOCK,
    });
    return actions;
  }
}

/**
 * Creates a PermissionPrompt for a nsIContentPermissionRequest for
 * the persistent-storage API.
 *
 * @param request (nsIContentPermissionRequest)
 *        The request for a permission from content.
 */
class PersistentStoragePermissionPrompt extends PermissionPromptForRequest {
  constructor(request) {
    super();
    this.request = request;
  }

  get type() {
    return "persistent-storage";
  }

  get permissionKey() {
    return "persistent-storage";
  }

  get popupOptions() {
    let learnMoreURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "storage-permissions";
    return {
      learnMoreURL,
      displayURI: false,
      name: this.getPrincipalName(),
    };
  }

  get notificationID() {
    return "persistent-storage";
  }

  get anchorID() {
    return "persistent-storage-notification-icon";
  }

  get message() {
    return lazy.gBrowserBundle.formatStringFromName(
      "persistentStorage.allowWithSite2",
      ["<>"]
    );
  }

  get promptActions() {
    return [
      {
        label: lazy.gBrowserBundle.GetStringFromName("persistentStorage.allow"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "persistentStorage.allow.accesskey"
        ),
        action: Ci.nsIPermissionManager.ALLOW_ACTION,
        scope: lazy.SitePermissions.SCOPE_PERSISTENT,
      },
      {
        label: lazy.gBrowserBundle.GetStringFromName(
          "persistentStorage.block.label"
        ),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "persistentStorage.block.accesskey"
        ),
        action: lazy.SitePermissions.BLOCK,
      },
    ];
  }
}

/**
 * Creates a PermissionPrompt for a nsIContentPermissionRequest for
 * the WebMIDI API.
 *
 * @param request (nsIContentPermissionRequest)
 *        The request for a permission from content.
 */
class MIDIPermissionPrompt extends SitePermsAddonInstallRequest {
  constructor(request) {
    super();
    this.request = request;
    let types = request.types.QueryInterface(Ci.nsIArray);
    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    this.isSysexPerm =
      !!perm.options.length &&
      perm.options.queryElementAt(0, Ci.nsISupportsString) == "sysex";
    this.permName = "midi";
    if (this.isSysexPerm) {
      this.permName = "midi-sysex";
    }
  }

  get type() {
    return "midi";
  }

  get permissionKey() {
    return this.permName;
  }

  get popupOptions() {
    // TODO (bug 1433235) We need a security/permissions explanation URL for this
    let options = {
      displayURI: false,
      name: this.getPrincipalName(),
    };

    // Don't offer "always remember" action in PB mode
    options.checkbox = {
      show: !lazy.PrivateBrowsingUtils.isWindowPrivate(
        this.browser.ownerGlobal
      ),
    };

    if (options.checkbox.show) {
      options.checkbox.label =
        lazy.gBrowserBundle.GetStringFromName("midi.remember");
    }

    return options;
  }

  get notificationID() {
    return "midi";
  }

  get anchorID() {
    return "midi-notification-icon";
  }

  get message() {
    let message;
    if (this.principal.schemeIs("file")) {
      if (this.isSysexPerm) {
        message = lazy.gBrowserBundle.GetStringFromName(
          "midi.shareSysexWithFile"
        );
      } else {
        message = lazy.gBrowserBundle.GetStringFromName("midi.shareWithFile");
      }
    } else if (this.isSysexPerm) {
      message = lazy.gBrowserBundle.formatStringFromName(
        "midi.shareSysexWithSite",
        ["<>"]
      );
    } else {
      message = lazy.gBrowserBundle.formatStringFromName("midi.shareWithSite", [
        "<>",
      ]);
    }
    return message;
  }

  get promptActions() {
    return [
      {
        label: lazy.gBrowserBundle.GetStringFromName("midi.allow.label"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "midi.allow.accesskey"
        ),
        action: Ci.nsIPermissionManager.ALLOW_ACTION,
      },
      {
        label: lazy.gBrowserBundle.GetStringFromName("midi.block.label"),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "midi.block.accesskey"
        ),
        action: Ci.nsIPermissionManager.DENY_ACTION,
      },
    ];
  }

  /**
   * @override
   * @param {Components.Exception} err
   * @returns {String}
   */
  getInstallErrorMessage(err) {
    return `WebMIDI access request was denied: ❝${err.message}❞. See https://developer.mozilla.org/docs/Web/API/Navigator/requestMIDIAccess for more information`;
  }
}

class StorageAccessPermissionPrompt extends PermissionPromptForRequest {
  #permissionKey;

  constructor(request) {
    super();
    this.request = request;
    this.siteOption = null;
    this.#permissionKey = `3rdPartyStorage${lazy.SitePermissions.PERM_KEY_DELIMITER}${this.principal.origin}`;

    let types = this.request.types.QueryInterface(Ci.nsIArray);
    let perm = types.queryElementAt(0, Ci.nsIContentPermissionType);
    let options = perm.options.QueryInterface(Ci.nsIArray);
    // If we have an option, the permission request is different in some way.
    // We may be in a call from requestStorageAccessUnderSite or a frame-scoped
    // request, which means that the embedding principal is not the current top-level
    // or the permission key is different.
    if (options.length != 2) {
      return;
    }

    let topLevelOption = options.queryElementAt(0, Ci.nsISupportsString).data;
    if (topLevelOption) {
      this.siteOption = topLevelOption;
    }
    let frameOption = options.queryElementAt(1, Ci.nsISupportsString).data;
    if (frameOption) {
      // We replace the permission key with a frame-specific one that only has a site after the delimiter
      this.#permissionKey = `3rdPartyFrameStorage${lazy.SitePermissions.PERM_KEY_DELIMITER}${this.principal.siteOrigin}`;
    }
  }

  get usePermissionManager() {
    return false;
  }

  get type() {
    return "storage-access";
  }

  get permissionKey() {
    // Make sure this name is unique per each third-party tracker
    return this.#permissionKey;
  }

  get temporaryPermissionURI() {
    if (this.siteOption) {
      return Services.io.newURI(this.siteOption);
    }
    return undefined;
  }

  prettifyHostPort(hostport) {
    let [host, port] = hostport.split(":");
    host = lazy.IDNService.convertToDisplayIDN(host, {});
    if (port) {
      return `${host}:${port}`;
    }
    return host;
  }

  get popupOptions() {
    let learnMoreURL =
      Services.urlFormatter.formatURLPref("app.support.baseURL") +
      "third-party-cookies";
    let hostPort = this.prettifyHostPort(this.principal.hostPort);
    let hintText = lazy.gBrowserBundle.formatStringFromName(
      "storageAccess1.hintText",
      [hostPort]
    );
    return {
      learnMoreURL,
      displayURI: false,
      hintText,
      escAction: "secondarybuttoncommand",
    };
  }

  get notificationID() {
    return "storage-access";
  }

  get anchorID() {
    return "storage-access-notification-icon";
  }

  get message() {
    let embeddingHost = this.topLevelPrincipal.host;

    if (this.siteOption) {
      embeddingHost = this.siteOption.split("://").at(-1);
    }

    return lazy.gBrowserBundle.formatStringFromName("storageAccess4.message", [
      this.prettifyHostPort(this.principal.hostPort),
      this.prettifyHostPort(embeddingHost),
    ]);
  }

  get promptActions() {
    let self = this;

    return [
      {
        label: lazy.gBrowserBundle.GetStringFromName(
          "storageAccess1.Allow.label"
        ),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "storageAccess1.Allow.accesskey"
        ),
        action: Ci.nsIPermissionManager.ALLOW_ACTION,
        callback() {
          self.allow({ "storage-access": "allow" });
        },
      },
      {
        label: lazy.gBrowserBundle.GetStringFromName(
          "storageAccess1.DontAllow.label"
        ),
        accessKey: lazy.gBrowserBundle.GetStringFromName(
          "storageAccess1.DontAllow.accesskey"
        ),
        action: Ci.nsIPermissionManager.DENY_ACTION,
        callback() {
          self.cancel();
        },
      },
    ];
  }

  get topLevelPrincipal() {
    return this.request.topLevelPrincipal;
  }
}

export const PermissionUI = {
  PermissionPromptForRequest,
  GeolocationPermissionPrompt,
  XRPermissionPrompt,
  DesktopNotificationPermissionPrompt,
  PersistentStoragePermissionPrompt,
  MIDIPermissionPrompt,
  StorageAccessPermissionPrompt,
};
