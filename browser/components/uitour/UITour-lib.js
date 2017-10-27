/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint valid-jsdoc: ["error", { "requireReturn": false }] */

// create namespace
if (typeof Mozilla == "undefined") {
  var Mozilla = {};
}

(function($) {
  "use strict";

  // create namespace
  if (typeof Mozilla.UITour == "undefined") {
    /**
     * Library that exposes an event-based Web API for communicating with the
     * desktop browser chrome. It can be used for tasks such as opening menu
     * panels and highlighting the position of buttons in the toolbar.
     *
     * <p>For security/privacy reasons `Mozilla.UITour` will only work on a list of allowed
     * secure origins. The list of allowed origins can be found in
     * {@link https://dxr.mozilla.org/mozilla-central/source/browser/app/permissions|
     * browser/app/permissions}.</p>
     *
     * @since 29
     * @namespace
     */
    Mozilla.UITour = {};
  }

  var themeIntervalId = null;
  function _stopCyclingThemes() {
    if (themeIntervalId) {
      clearInterval(themeIntervalId);
      themeIntervalId = null;
    }
  }

  function _sendEvent(action, data) {
    var event = new CustomEvent("mozUITour", {
      bubbles: true,
      detail: {
        action,
        data: data || {}
      }
    });

    document.dispatchEvent(event);
  }

  function _generateCallbackID() {
    return Math.random().toString(36).replace(/[^a-z]+/g, "");
  }

  function _waitForCallback(callback) {
    var id = _generateCallbackID();

    function listener(event) {
      if (typeof event.detail != "object")
        return;
      if (event.detail.callbackID != id)
        return;

      document.removeEventListener("mozUITourResponse", listener);
      callback(event.detail.data);
    }
    document.addEventListener("mozUITourResponse", listener);

    return id;
  }

  var notificationListener = null;
  function _notificationListener(event) {
    if (typeof event.detail != "object")
      return;
    if (typeof notificationListener != "function")
      return;

    notificationListener(event.detail.event, event.detail.params);
  }

  Mozilla.UITour.DEFAULT_THEME_CYCLE_DELAY = 10 * 1000;

  Mozilla.UITour.CONFIGNAME_SYNC = "sync";
  Mozilla.UITour.CONFIGNAME_AVAILABLETARGETS = "availableTargets";

  /**
   * @typedef {String} Mozilla.UITour.Target
   *
   * @summary Not all targets are available at all times because they may not be visible
   * or UITour doesn't not how to automatically make them visible. Use
   * <code>`Mozilla.UITour.getConfiguration('availableTargets', callback)`</code> to determine
   * which ones are available at a given time.
   * @see Mozilla.UITour.getConfiguration
   * @see Mozilla.UITour.showHighlight
   * @see Mozilla.UITour.showInfo
   *
   * @description Valid values:<ul>
   * <li>accountStatus
   * <li>addons
   * <li>appMenu
   * <li>backForward
   * <li>bookmarks
   * <li>controlCenter-trackingUnblock
   * <li>controlCenter-trackingBlock
   * <li>customize
   * <li>devtools
   * <li>forget
   * <li>help
   * <li>home
   * <li>library
   * <li>pageActionButton
   * <li>pageAction-bookmark
   * <li>pageAction-copyURL
   * <li>pageAction-emailLink
   * <li>pageAction-sendToDevice
   * <li>pocket
   * <li>privateWindow
   * <li>quit
   * <li>readerMode-urlBar
   * <li>screenshots
   * <li>search
   * <li>searchIcon
   * <li>searchPrefsLink
   * <li>selectedTabIcon
   * <li>trackingProtection
   * <li>urlbar
   * <li>webide
   * </ul>
   *
   * Generate using the following in the Browser Console:
   * <code>`[...UITour.targets.keys()].join("\n* &lt;li&gt;")`</code>
   */

  /**
   * Ensure the browser is ready to handle this document as a tour.
   *
   * @param {Function} [callback] Callback to call if UITour is working for the document.
   * @since 35
   */
  Mozilla.UITour.ping = function(callback) {
    var data = {};
    if (callback) {
      data.callbackID = _waitForCallback(callback);
    }
    _sendEvent("ping", data);
  };

  /**
   * @summary Register a listener to observe all UITour notifications.
   *
   * @description There can only be one observer per tour tab so calling this a second time will
   * replace any previous `listener`.
   * To remove an observer, call the method with `null` as the first argument.
   *
   * @param {?Function} listener - Called when any UITour notification occurs.
   * @param {Function} [callback] - Called when the browser acknowledges the observer.
   */
  Mozilla.UITour.observe = function(listener, callback) {
    notificationListener = listener;

    if (listener) {
      document.addEventListener("mozUITourNotification",
                                _notificationListener);
      Mozilla.UITour.ping(callback);
    } else {
      document.removeEventListener("mozUITourNotification",
                                   _notificationListener);
    }
  };

  /**
   * Register an identifier to use in
   * {@link https://wiki.mozilla.org/Telemetry|Telemetry}. `pageID` must be a
   * string unique to the page/tour.
   *
   * @example
   * Mozilla.UITour.registerPageID('firstrun-page-firefox-29');
   *
   * @param {string} pageID Unique identifier for the page/tour.
   * @memberof Mozilla.UITour
   */
  Mozilla.UITour.registerPageID = function(pageID) {
    _sendEvent("registerPageID", {
      pageID
    });
  };

  /**
   * @typedef {String} Mozilla.UITour.HighlightEffect
   *
   * Specifies the effect/animation to use when highlighting UI elements.
   * @description Valid values:<ul>
   * <li>random
   * <li>wobble
   * <li>zoom
   * <li>color
   * </ul>
   *
   * Generate using the following in the Browser Console:
   * <code>[...UITour.highlightEffects].join("\n* &lt;li&gt;")</code>
   * @see Mozilla.UITour.showHighlight
   */

  /**
   * Visually highlight a UI widget.
   *
   * @see Mozilla.UITour.hideHighlight
   * @example Mozilla.UITour.showHighlight('appMenu', 'wobble');
   *
   * @param {Mozilla.UITour.Target} target - Identifier of the UI widget to show a highlight on.
   * @param {Mozilla.UITour.HighlightEffect} [effect="none"] - Name of the effect to use when highlighting.
   */
  Mozilla.UITour.showHighlight = function(target, effect) {
    _sendEvent("showHighlight", {
      target,
      effect
    });
  };

  /**
   * Hide any visible UI highlight.
   * @see Mozilla.UITour.showHighlight
   */
  Mozilla.UITour.hideHighlight = function() {
    _sendEvent("hideHighlight");
  };

  /**
   * Show an arrow panel with optional images and buttons anchored at a specific UI target.
   *
   * @see Mozilla.UITour.hideInfo
   *
   * @param {Mozilla.UITour.Target} target - Identifier of the UI widget to anchor the panel at.
   * @param {String} title - Title text to be shown as the heading of the panel.
   * @param {String} text - Body text of the panel.
   * @param {String} [icon=null] - URL of a 48x48px (96px @ 2dppx) image (which will be resolved
   *                               relative to the tab's URI) to display in the panel.
   * @param {Object[]} [buttons=[]] - Array of objects describing buttons.
   * @param {String} buttons[].label - Button label
   * @param {String} buttons[].icon - Button icon URL
   * @param {String} buttons[].style - Button style ("primary" or "link")
   * @param {Function} buttons[].callback - Called when the button is clicked
   * @param {Object} [options={}] - Advanced options
   * @param {Function} options.closeButtonCallback - Called when the panel's close button is clicked.
   *
   * @example
   * var buttons = [
   *   {
   *     label: 'Cancel',
   *     style: 'link',
   *     callback: cancelBtnCallback
   *   },
   *   {
   *     label: 'Confirm',
   *     style: 'primary',
   *     callback: confirmBtnCallback
   *   }
   * ];
   *
   * var icon = '//mozorg.cdn.mozilla.net/media/img/firefox/australis/logo.png';
   *
   * var options = {
   *   closeButtonCallback: closeBtnCallback
   * };
   *
   * Mozilla.UITour.showInfo('appMenu', 'my title', 'my text', icon, buttons, options);
   */
  Mozilla.UITour.showInfo = function(target, title, text, icon, buttons, options) {
    var buttonData = [];
    if (Array.isArray(buttons)) {
      for (var i = 0; i < buttons.length; i++) {
        buttonData.push({
          label: buttons[i].label,
          icon: buttons[i].icon,
          style: buttons[i].style,
          callbackID: _waitForCallback(buttons[i].callback)
        });
      }
    }

    var closeButtonCallbackID, targetCallbackID;
    if (options && options.closeButtonCallback)
      closeButtonCallbackID = _waitForCallback(options.closeButtonCallback);
    if (options && options.targetCallback)
      targetCallbackID = _waitForCallback(options.targetCallback);

    _sendEvent("showInfo", {
      target,
      title,
      text,
      icon,
      buttons: buttonData,
      closeButtonCallbackID,
      targetCallbackID
    });
  };

  /**
   * Hide any visible info panels.
   * @see Mozilla.UITour.showInfo
   */
  Mozilla.UITour.hideInfo = function() {
    _sendEvent("hideInfo");
  };

  /**
   * Preview a lightweight-theme applied to the browser UI.
   *
   * @see Mozilla.UITour.cycleThemes
   * @see Mozilla.UITour.resetTheme
   *
   * @param {Object} theme - Theme object format expected by `LightweightThemeManager.previewTheme`
   * @example
   * var theme = {
   *   …
   *   "iconURL":      "https://addons.mozilla.org/_files/…/preview_small.jpg",
   *   "headerURL":    "https://addons.mozilla.org/_files/….jpg",
   *   "name":         "My cool theme",
   *   "author":       "Mozilla",
   *   "footer":       "https://addons.mozilla.org/_files/….jpg",
   *   "previewURL":   "https://addons.mozilla.org/_files/…/preview.jpg",
   *   "updateURL":    "https://versioncheck.addons.mozilla.org/…",
   *   "accentcolor":  "#000000",
   *   "header":       "https://addons.mozilla.org/_files/….jpg",
   *   "version":      "1.0",
   *   "detailURL":    "https://addons.mozilla.org/firefox/addon/…",
   *   "textcolor":    "#ffffff",
   *   "id":           "18066",
   *   "description":  "My awesome theme.",
   *   …
   * };
   *
   * Mozilla.UITour.previewTheme(theme);
   */
  Mozilla.UITour.previewTheme = function(theme) {
    _stopCyclingThemes();

    _sendEvent("previewTheme", {
      theme: JSON.stringify(theme)
    });
  };

  /**
   * Stop previewing and cycling themes, returning to the user's previous theme.
   * @see Mozilla.UITour.cycleThemes
   * @see Mozilla.UITour.previewTheme
   */
  Mozilla.UITour.resetTheme = function() {
    _stopCyclingThemes();

    _sendEvent("resetTheme");
  };

  /**
   * Cycle between an array of themes using the given delay.
   *
   * @see Mozilla.UITour.previewTheme
   * @see Mozilla.UITour.resetTheme
   *
   * @param {Object[]} themes - Array of themes
   * @param {Number} [delay=Mozilla.UITour.DEFAULT_THEME_CYCLE_DELAY]
   *                 - Time in milliseconds between rotating themes
   * @param {Function} callback - Function called at each rotation
   */
  Mozilla.UITour.cycleThemes = function(themes, delay, callback) {
    _stopCyclingThemes();

    if (!delay) {
      delay = Mozilla.UITour.DEFAULT_THEME_CYCLE_DELAY;
    }

    function nextTheme() {
      var theme = themes.shift();
      themes.push(theme);

      _sendEvent("previewTheme", {
        theme: JSON.stringify(theme),
        state: true
      });

      callback(theme);
    }

    themeIntervalId = setInterval(nextTheme, delay);
    nextTheme();
  };

  /**
   * @typedef {String} Mozilla.UITour.MenuName
   * Valid values:<ul>
   * <li>appMenu
   * <li>bookmarks
   * <li>controlCenter
   * <li>pocket
   * </ul>
   *
   * @see Mozilla.UITour.showMenu
   * @see Mozilla.UITour.hideMenu
   * @see Mozilla.UITour.openSearchPanel
   */

  /**
   * Open the named application menu.
   *
   * @see Mozilla.UITour.hideMenu
   *
   * @param {Mozilla.UITour.MenuName} name - Menu name
   * @param {Function} [callback] - Callback to be called with no arguments when
   *                                the menu opens.
   *
   * @example
   * Mozilla.UITour.showMenu('appMenu', function() {
   *   console.log('menu was opened');
   * });
   */
  Mozilla.UITour.showMenu = function(name, callback) {
    var showCallbackID;
    if (callback)
      showCallbackID = _waitForCallback(callback);

    _sendEvent("showMenu", {
      name,
      showCallbackID,
    });
  };

  /**
   * Close the named application menu.
   *
   * @see Mozilla.UITour.showMenu
   *
   * @param {Mozilla.UITour.MenuName} name - Menu name
   */
  Mozilla.UITour.hideMenu = function(name) {
    _sendEvent("hideMenu", {
      name
    });
  };

  /**
   * Loads about:newtab in the tour tab.
   * @since 51
   */
  Mozilla.UITour.showNewTab = function() {
    _sendEvent("showNewTab");
  };


  /**
   * @typedef Mozilla.UITour.ConfigurationName
   * @description Valid values:<ul>
   * <li>{@link Mozilla.UITour.Configuration.AppInfo|appinfo}</li>
   * <li>{@link Mozilla.UITour.Configuration.CanReset|canReset}</li>
   * <li>{@link Mozilla.UITour.Configuration.AvailableTargets|availableTargets}</li>
   * <li>{@link Mozilla.UITour.Configuration.Search|search}</li>
   * <li>{@link Mozilla.UITour.Configuration.Search|selectedSearchEngine}
   * - DEPRECATED, use 'search'</li>
   * <li>{@link Mozilla.UITour.Configuration.Sync|sync}</li>
   * </ul>
   */

  /**
   * @namespace Mozilla.UITour.Configuration
   * @see Mozilla.UITour.getConfiguration
   * @see Mozilla.UITour.ConfigurationName
   */

  /**
   * Indicate whether a user can refresh their Firefox profile via {@link Mozilla.UITour.resetFirefox}.
   * @typedef {Boolean} Mozilla.UITour.Configuration.CanReset
   * @see Mozilla.UITour.resetFirefox
   * @since 48
   */

  /**
   * @typedef {Object} Mozilla.UITour.Configuration.AppInfo
   * @property {Boolean} canSetDefaultBrowserInBackground - Whether the application can be set as
   *                                                        the default browser in the background
   *                                                        without user interaction.
   * @property {Boolean} defaultBrowser - Whether the application is the default browser. Since Fx40.
   * @property {String} defaultUpdateChannel - Update channel e.g. 'release', 'beta', 'aurora',
   *                                           'nightly', 'default'
   *                                           (self-built or automated testing builds)
   * @property {String} distribution - Contains the distributionId property. This value will be
   *                                   "default" in most cases but can differ for repack or
   *                                   funnelcake builds. Since Fx48
   * @property {Number} profileCreatedWeeksAgo - The number of weeks since the profile was created,
   *                                             starting from 0 for profiles dating less than
   *                                             seven days old. Since Fx56.
   * @property {Number} profileResetWeeksAgo - The number of weeks since the profile was last reset,
   *                                           starting from 0 for profiles reset less than seven
   *                                           days ago. If the profile has never been reset it
   *                                           returns null. Since Fx56.
   * @property {String} version - Version string e.g. "48.0a2"
   * @since 35
   */

  /**
   * @summary Search service configuration.
   *
   * @description From version 34 through 42 inclusive, a string was returned for the 'selectedSearchEngine'
   * configuration instead of the object like 'search'.
   *
   * @typedef {String|Object} Mozilla.UITour.Configuration.Search
   * @property {String} searchEngineIdentifier - The default engine's identifier
   * @property {String[]} engines - Identifiers of visible engines
   * @since 43
   */

  /**
   * Sync status and device counts.
   * @typedef {Object} Mozilla.UITour.Configuration.Sync
   * @property {Boolean} setup - Whether sync is setup
   * @property {Number} desktopDevices - Number of desktop devices
   * @property {Number} mobileDevices - Number of mobile devices
   * @property {Number} totalDevices - Total number of connected devices
   * @since 50
   */

  /**
   * Array of UI {@link Mozilla.UITour.Target|Targets} currently available to be annotated.
   * @typedef {Mozilla.UITour.Target[]} Mozilla.UITour.Configuration.AvailableTargets
   */

  /**
   * Retrieve some information about the application/profile.
   *
   * @param {Mozilla.UITour.ConfigurationName} configName - Name of configuration to retrieve
   * @param {Function} callback - Called with one argument containing the value of the configuration.
   */
  Mozilla.UITour.getConfiguration = function(configName, callback) {
    _sendEvent("getConfiguration", {
      callbackID: _waitForCallback(callback),
      configuration: configName,
    });
  };

  /**
   * Set some value or take some action.
   *
   * <p><strong>Valid configuration names:</strong><dl>
   * <dt>defaultBrowser</dt>
   * <dd>Try to set the application as the default web browser. Since Fx40</dd>
   * </dl></p>
   *
   * @param {String} configName - Configuration name to set (e.g. "defaultBrowser")
   * @param {String|Number|Boolean} [configValue] - Not currently used
   *
   * @since 40
   * @example
   * Mozilla.UITour.setConfiguration('defaultBrowser');
   */
  Mozilla.UITour.setConfiguration = function(configName, configValue) {
    _sendEvent("setConfiguration", {
      configuration: configName,
      value: configValue,
    });
  };

  /**
   * Request the browser open the Firefox Accounts page.
   *
   * @param {Object} extraURLCampaignParams - An object containing additional
   * parameters for the URL opened by the browser for reasons of promotional
   * campaign tracking. Each attribute of the object must have a name that
   * is a string, begins with "utm_" and contains only only alphanumeric
   * characters, dashes or underscores. The values may be any string and will
   * automatically be encoded.
   * @param {String} email - A string containing the default email account
   * for the URL opened by the browser.
   * @since 31, 47 for `extraURLCampaignParams`
   * @example
   * // Will open https://accounts.firefox.com/signup?entrypoint=uitour
   * Mozilla.UITour.showFirefoxAccounts();
   * @example
   * // Will open:
   * // https://accounts.firefox.com/signup?entrypoint=uitour&utm_foo=bar&utm_bar=baz
   * Mozilla.UITour.showFirefoxAccounts({
   *   'utm_foo': 'bar',
   *   'utm_bar': 'baz'
   * });
   * @example
   * // Will open:
   * // https://accounts.firefox.com/?action=email&email=foo%40bar.com&entrypoint=uitour
   * Mozilla.UITour.showFirefoxAccounts(null, "foo@bar.com");
   */
  Mozilla.UITour.showFirefoxAccounts = function(extraURLCampaignParams, email) {
    _sendEvent("showFirefoxAccounts", {
      extraURLCampaignParams: JSON.stringify(extraURLCampaignParams),
      email
    });
  };

  /**
   * Show a profile refresh/reset dialog, allowing users to choose to reomve
   * add-ons and customizations as well as restore browser defaults, if possible.
   * `getConfiguration('canReset')` should first be used to determine whether
   * Refresh/Reset is possible for the user's build/profile.
   * @since 48
   * @see Mozilla.UITour.Configuration.CanReset
   */
  Mozilla.UITour.resetFirefox = function() {
    _sendEvent("resetFirefox");
  };

  /**
   * Add the specified customizable widget to the navigation toolbar.
   *
   * @param {Mozilla.UITour.Target} name - Identifier of the customizable widget.
   * @param {Function} callback - Called with no arguments once the icon was successfully added to
   *                              the toolbar. Not called if it doesn't succeed.
   * @since 33.1
   * @example
   * Mozilla.UITour.addNavBarWidget('forget', function () {
   *   console.log('forget button added to toolbar');
   * });
   */
  Mozilla.UITour.addNavBarWidget = function(name, callback) {
    _sendEvent("addNavBarWidget", {
      name,
      callbackID: _waitForCallback(callback),
    });
  };

  /**
   * Set the specified search engine as the user-set default.
   *
   * @see {@link https://dxr.mozilla.org/mozilla-release/source/browser/locales/search/list.json}
   *
   * @param {String} identifier - Identifier of the engine (e.g. 'yahoo').
   * @see Mozilla.UITour.Configuration.Search
   * @since 34
   */
  Mozilla.UITour.setDefaultSearchEngine = function(identifier) {
    _sendEvent("setDefaultSearchEngine", {
      identifier,
    });
  };

  /**
   * Sets a key+value pair as a treatment tag for recording in FHR.
   * @param {String} name - tag name for the treatment
   * @param {String} value - tag value for the treatment
   * @since 34
   * @see Mozilla.UITour.getTreatmentTag
   * @example
   * Mozilla.UITour.setTreatmentTag('srch-chg-action', 'Switch');
   */
  Mozilla.UITour.setTreatmentTag = function(name, value) {
    _sendEvent("setTreatmentTag", {
      name,
      value
    });
  };

  /**
   * Retrieved the value for a set FHR treatment tag.
   *
   * @param {String} name - Tag name to be retrieved
   * @param {Function} callback - Called once the data has been retrieved
   * @since 34
   * @see Mozilla.UITour.setTreatmentTag
   * @example
   * Mozilla.UITour.getTreatmentTag('srch-chg-action', function(value) {
   *   console.log(value);
   * });
   */
  Mozilla.UITour.getTreatmentTag = function(name, callback) {
    _sendEvent("getTreatmentTag", {
      name,
      callbackID: _waitForCallback(callback)
    });
  };

  /**
   * Set the search term in the search box.
   *
   * This should have been implemented via `setConfiguration("searchTerm", …)`.
   *
   * @param {String} term - Search string e.g. 'Firefox'
   * @since 34
   */
  Mozilla.UITour.setSearchTerm = function(term) {
    _sendEvent("setSearchTerm", {
      term
    });
  };

  /**
   * @summary Opens the search box's panel.
   *
   * @description This should have been implemented via `showMenu("search", …)`.
   *
   * @param {Function} callback - Called once the panel has opened.
   * @since 34
   */
  Mozilla.UITour.openSearchPanel = function(callback) {
    _sendEvent("openSearchPanel", {
      callbackID: _waitForCallback(callback)
    });
  };

  /**
   * @summary Force the reader mode icon to appear in the address bar regardless of whether
   * heuristics determine it's appropriate.
   *
   * @description This is useful if you want to target an annotation (panel/highlight) on it
   * but the tour page doesn't have much textual content.
   */
  Mozilla.UITour.forceShowReaderIcon = function() {
    _sendEvent("forceShowReaderIcon");
  };

  /**
   * Toggle into reader mode for the current tab. Once the user enters reader
   * mode, the UITour document will not be active and therefore cannot call other
   * UITour APIs.
   */
  Mozilla.UITour.toggleReaderMode = function() {
    _sendEvent("toggleReaderMode");
  };

  /**
   * @param {String} pane - Pane to open/switch the preferences to.
   * Valid values match fragments on about:preferences and are subject to change e.g.:
   *
   * <ul>
   * For the Preferences
   * <li>general
   * <li>applications
   * <li>sync
   * <li>privacy
   * <li>advanced
   * </ul>
   *
   * To open to the options of sending telemetry, health report, crach reports,
   * that is, the privcacy pane > reports on the preferences.
   * Please call `Mozilla.UITour.openPreferences("privacy-reports")`.
   * UITour would do route mapping automatically.
   *
   * @since 42
   */
  Mozilla.UITour.openPreferences = function(pane) {
    _sendEvent("openPreferences", {
      pane
    });
  };

  /**
   * @summary Closes the tab where this code is running. As usual, if the tab is in the
   * foreground, the tab that was displayed before is selected.
   *
   * @description The last tab in the current window will never be closed, in which case
   * this call will have no effect. The calling code is expected to take an
   * action after a small timeout in order to handle this case, for example by
   * displaying a goodbye message or a button to restart the tour.
   * @since 46
   */
  Mozilla.UITour.closeTab = function() {
    _sendEvent("closeTab");
  };
})();

// Make this library Require-able.
/* eslint-env commonjs */
if (typeof module !== "undefined" && module.exports) {
  module.exports = Mozilla.UITour;
}
