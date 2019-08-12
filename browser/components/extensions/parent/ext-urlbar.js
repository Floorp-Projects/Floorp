/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarContextualTip: "resource:///modules/UrlbarContextualTip.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
  UrlbarProviderExtension: "resource:///modules/UrlbarProviderExtension.jsm",
});

var { ExtensionPreferencesManager } = ChromeUtils.import(
  "resource://gre/modules/ExtensionPreferencesManager.jsm"
);
var { getSettingsAPI } = ExtensionPreferencesManager;

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);
var { IconDetails } = ExtensionParent;

ExtensionPreferencesManager.addSetting("openViewOnFocus", {
  prefNames: ["browser.urlbar.openViewOnFocus"],
  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },
});

ExtensionPreferencesManager.addSetting("engagementTelemetry", {
  prefNames: ["browser.urlbar.eventTelemetry.enabled"],
  setCallback(value) {
    return { [this.prefNames[0]]: value };
  },
});

// Keeps track of the id of the extension using the contextual tip.
// Only one extension is allowed to use the contextual tip.
// If another extension attempts to use the contextual tip,
// then an error is thrown.
let idOfExtUsingContextualTip = null;

this.urlbar = class extends ExtensionAPI {
  onShutdown() {
    if (idOfExtUsingContextualTip === this.extension.id) {
      for (let w of windowTracker.browserWindows()) {
        w.gURLBar.view.removeContextualTip();
      }
      idOfExtUsingContextualTip = null;
    }
  }

  /**
   * Produces the styles to add to the icon element.
   * These styles dictate the background images to use by default,
   * for light/dark themes, and for low/high resolution screens.
   *
   * @param {object} icons
   *   Object obtained from calling `IconDetails.normalize`.
   *   It's a map of the icon size (e.g. 16) to the actual path of the icon.
   *   The map can contain multiple size-to-path entries (one entry per icon).
   * @returns {string} iconStyle
   */
  _getStyleFromIconsObject(icons) {
    let getIcon = (icon, theme) => {
      if (typeof icon === "object") {
        return IconDetails.escapeUrl(icon[theme]);
      }
      return IconDetails.escapeUrl(icon);
    };

    let getStyle = (name, icon) => {
      return `
        --webextension-${name}: url("${getIcon(icon, "default")}");
        --webextension-${name}-light: url("${getIcon(icon, "light")}");
        --webextension-${name}-dark: url("${getIcon(icon, "dark")}");
      `;
    };

    let icon16 = IconDetails.getPreferredIcon(icons, this.extension, 16).icon;
    let icon32 = IconDetails.getPreferredIcon(icons, this.extension, 32).icon;
    return `
      ${getStyle("contextual-tip-icon", icon16)}
      ${getStyle("contextual-tip-icon-2x", icon32)}
    `;
  }

  /**
   * If no extension is using the contextual tip then the current extension
   * will own the contextual tip. If the contextual tip is already being used
   * by another extension then an error is thrown.
   */
  _registerExtensionToUseContextualTip() {
    if (idOfExtUsingContextualTip == null) {
      idOfExtUsingContextualTip = this.extension.id;
    }

    if (idOfExtUsingContextualTip !== this.extension.id) {
      throw new Error(
        "There was an attempt to use the contextual tip API but " +
          "another extension is already using the contextual tip API."
      );
    }
  }

  /**
   * Given either a "button" or "link" string, returns a function to
   * register click listeners on the contextual tip's button or link element.
   *
   * See the `urlbar.contextualTip.onButtonClicked` and
   * `urlbar.contextualTip.onLinkClicked` events for usage.
   *
   * @param {string} type
   *   Represents the type of element to register a click listener on.
   *   Must be either "button" or "link".
   * @returns {function} Returns a function to register on the EventManager.
   */
  _registerClickListener(type) {
    return fire => {
      this._registerExtensionToUseContextualTip();

      const listener = window => {
        const windowId = this.extension.windowManager.wrapWindow(window).id;
        fire.async(windowId);
      };

      UrlbarContextualTip.addClickListener(type, listener);

      return () => {
        UrlbarContextualTip.removeClickListener(type, listener);
      };
    };
  }

  getAPI(context) {
    return {
      urlbar: {
        onBehaviorRequested: new EventManager({
          context,
          name: "urlbar.onBehaviorRequested",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "behaviorRequested",
              async queryContext => {
                if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                  return "inactive";
                }
                return fire.async(queryContext).catch(error => {
                  throw context.normalizeError(error);
                });
              }
            );
            return () => provider.setEventListener("behaviorRequested", null);
          },
        }).api(),

        onQueryCanceled: new EventManager({
          context,
          name: "urlbar.onQueryCanceled",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener("queryCanceled", async queryContext => {
              if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                return;
              }
              await fire.async(queryContext).catch(error => {
                throw context.normalizeError(error);
              });
            });
            return () => provider.setEventListener("queryCanceled", null);
          },
        }).api(),

        onResultsRequested: new EventManager({
          context,
          name: "urlbar.onResultsRequested",
          register: (fire, providerName) => {
            let provider = UrlbarProviderExtension.getOrCreate(providerName);
            provider.setEventListener(
              "resultsRequested",
              async queryContext => {
                if (queryContext.isPrivate && !context.privateBrowsingAllowed) {
                  return [];
                }
                return fire.async(queryContext).catch(error => {
                  throw context.normalizeError(error);
                });
              }
            );
            return () => provider.setEventListener("resultsRequested", null);
          },
        }).api(),

        openViewOnFocus: getSettingsAPI(
          context.extension.id,
          "openViewOnFocus",
          () => UrlbarPrefs.get("openViewOnFocus")
        ),

        engagementTelemetry: getSettingsAPI(
          context.extension.id,
          "engagementTelemetry",
          () => UrlbarPrefs.get("eventTelemetry.enabled")
        ),

        contextualTip: {
          /**
           * Sets the contextual tip's icon,
           * title, button title, and link title.
           *
           * @param {object} details
           *   If null, then the contextual tip will be hidden.
           * @param {string} details.title
           *   Main title on the contextual tip.
           * @param {string} [details.buttonTitle]
           *   If not given, no button is shown.
           * @param {string} [details.linkTitle]
           *   If not given, no link is shown.
           * @param {object} [details.icon]
           * @param {string} [details.icon.defaultIcon]
           *   Path to an image file to be used as the default icon.
           * @param {object} [details.icon.themeIcons]
           * @param {string} [details.icon.themeIcons.light]
           *   Path to an image file to be used as the icon for light themes.
           * @param {string} [details.icon.themeIcons.dark]
           *   Path to an image file to be used as the icon for dark themes.
           * @param {integer} [details.icon.themeIcons.size]
           *   Indicates the size of the icons.
           */
          set: details => {
            this._registerExtensionToUseContextualTip();

            const mostRecentWindow = windowTracker.getTopNormalWindow(context);

            const iconPathFromDetails = details.icon
              ? details.icon.defaultIcon
              : null;
            const iconPathFromExtensionManifest =
              context.extension.manifest.icons;
            const icons = IconDetails.normalize(
              {
                path: iconPathFromDetails || iconPathFromExtensionManifest,
                iconType: "contextualTip",
                themeIcons: details.icon ? details.icon.themeIcons : null,
              },
              context.extension
            );

            const iconStyle = this._getStyleFromIconsObject(icons);

            mostRecentWindow.gURLBar.view.setContextualTip({
              iconStyle,
              title: details.title,
              buttonTitle: details.buttonTitle,
              linkTitle: details.linkTitle,
            });
          },

          /**
           * Hides the contextual tip.
           */
          remove: () => {
            this._registerExtensionToUseContextualTip();
            const mostRecentWindow = windowTracker.getTopNormalWindow(context);
            mostRecentWindow.gURLBar.view.hideContextualTip();
          },

          /*
           * Fired when the user clicks the contextual tip's button.
           * The listener is passed the id of the window where
           * the contextual tip was clicked.
           */
          onButtonClicked: new EventManager({
            context,
            name: "urlbar.contextualTip.onButtonClicked",
            register: this._registerClickListener("button"),
          }).api(),

          /**
           * Fired when the user clicks the contextual tip's link.
           * The listener is passed the id of the window where
           * the contextual tip was clicked.
           */
          onLinkClicked: new EventManager({
            context,
            name: "urlbar.contextualTip.onLinkClicked",
            register: this._registerClickListener("link"),
          }).api(),
        },
      },
    };
  }
};
