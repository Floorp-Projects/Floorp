/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeDefaults", "DEFAULT_WELCOME_CONTENT"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AttributionCode: "resource:///modules/AttributionCode.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

const DEFAULT_WELCOME_CONTENT = {
  id: "DEFAULT_ABOUTWELCOME_PROTON",
  template: "multistage",
  transitions: true,
  background_url:
    "chrome://activity-stream/content/data/content/assets/proton-bkg.avif",
  screens: [
    {
      id: "AW_PIN_FIREFOX",
      order: 0,
      content: {
        title: {
          string_id: "mr1-onboarding-pin-header",
        },
        subtitle: {
          string_id: "mr1-welcome-screen-hero-text",
        },
        help_text: {
          text: {
            string_id: "mr1-onboarding-welcome-image-caption",
          },
        },
        primary_button: {
          label: {
            string_id: "mr1-onboarding-pin-primary-button-label",
          },
          action: {
            navigate: true,
            type: "PIN_FIREFOX_TO_TASKBAR",
          },
        },
        secondary_button: {
          label: {
            string_id: "mr1-onboarding-set-default-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button_top: {
          label: {
            string_id: "mr1-onboarding-sign-in-button-label",
          },
          action: {
            data: {
              entrypoint: "activity-stream-firstrun",
            },
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
          },
        },
      },
    },
    {
      id: "AW_SET_DEFAULT",
      order: 1,
      content: {
        title: {
          string_id: "mr1-onboarding-default-header",
        },
        subtitle: {
          string_id: "mr1-onboarding-default-subtitle",
        },
        primary_button: {
          label: {
            string_id: "mr1-onboarding-default-primary-button-label",
          },
          action: {
            navigate: true,
            type: "SET_DEFAULT_BROWSER",
          },
        },
        secondary_button: {
          label: {
            string_id: "mr1-onboarding-set-default-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_IMPORT_SETTINGS",
      order: 2,
      content: {
        title: {
          string_id: "mr1-onboarding-import-header",
        },
        subtitle: {
          string_id: "mr1-onboarding-import-subtitle",
        },
        primary_button: {
          label: {
            string_id:
              "mr1-onboarding-import-primary-button-label-no-attribution",
          },
          action: {
            type: "SHOW_MIGRATION_WIZARD",
            data: {},
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr1-onboarding-import-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_CHOOSE_COLORWAY",
      order: 3,
      content: {
        title: {
          string_id: "mr2-onboarding-colorway-header",
        },
        subtitle: {
          string_id: "mr2-onboarding-colorway-subtitle",
        },
        hideLogo: true,
        tiles: {
          type: "colorway",
          action: {
            theme: "<event>",
          },
          defaultVariationIndex: 1,
          systemVariations: [
            {
              id: "light",
              label: {
                string_id: "mr1-onboarding-theme-label-light",
              },
              tooltip: {
                string_id: "mr1-onboarding-theme-tooltip-light",
              },
              description: {
                string_id: "mr1-onboarding-theme-description-light",
              },
            },
            {
              id: "automatic",
              label: {
                string_id: "mr2-onboarding-theme-label-auto",
              },
              tooltip: {
                string_id: "mr1-onboarding-theme-tooltip-system",
              },
              description: {
                string_id: "mr1-onboarding-theme-description-system",
              },
            },
            {
              id: "dark",
              label: {
                string_id: "mr1-onboarding-theme-label-dark",
              },
              tooltip: {
                string_id: "mr1-onboarding-theme-tooltip-dark",
              },
              description: {
                string_id: "mr1-onboarding-theme-description-dark",
              },
            },
          ],
          variations: [
            {
              id: "soft",
              label: {
                string_id: "mr2-onboarding-colorway-label-soft",
              },
              tooltip: {
                string_id: "mr2-onboarding-theme-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-theme-description",
              },
            },
            {
              id: "balanced",
              label: {
                string_id: "mr2-onboarding-colorway-label-balanced",
              },
              tooltip: {
                string_id: "mr2-onboarding-theme-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-theme-description",
              },
            },
            {
              id: "bold",
              label: {
                string_id: "mr2-onboarding-colorway-label-bold",
              },
              tooltip: {
                string_id: "mr2-onboarding-theme-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-theme-description",
              },
            },
          ],
          colorways: [
            {
              id: "default",
              label: {
                string_id: "mr2-onboarding-theme-label-default",
              },
              tooltip: {
                string_id: "mr2-onboarding-default-theme-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-default-theme-label",
              },
            },
            {
              id: "abstract",
              label: "Abstract",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
            {
              id: "cheers",
              label: "Cheers",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
            {
              id: "foto",
              label: "Foto",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
            {
              id: "lush",
              label: "Lush",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
            {
              id: "graffiti",
              label: "Graffiti",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
            {
              id: "elemental",
              label: "Elemental",
              tooltip: {
                string_id: "mr2-onboarding-colorway-tooltip",
              },
              description: {
                string_id: "mr2-onboarding-colorway-label",
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr2-onboarding-colorway-primary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2-onboarding-colorway-secondary-button-label",
          },
          action: {
            theme: "automatic",
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_THANKS",
      order: 4,
      autoClose: true,
      content: {
        hasFancyTitle: true,
        title: { string_id: "mr2-onboarding-thank-you-header" },
        subtitle: {
          string_id: "mr2-onboarding-thank-you-text",
        },
        primary_button: {
          label: {
            string_id: "mr2-onboarding-start-browsing-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
  ],
};

async function getAddonFromRepository(data) {
  const [addonInfo] = await AddonRepository.getAddonsByIDs([data]);
  if (addonInfo.sourceURI.scheme !== "https") {
    return null;
  }
  return {
    name: addonInfo.name,
    url: addonInfo.sourceURI.spec,
    iconURL: addonInfo.icons["64"] || addonInfo.icons["32"],
  };
}

async function getAddonInfo(attrbObj) {
  let { content, source } = attrbObj;
  try {
    if (!content || source !== "addons.mozilla.org") {
      return null;
    }
    // Attribution data can be double encoded
    while (content.includes("%")) {
      try {
        const result = decodeURIComponent(content);
        if (result === content) {
          break;
        }
        content = result;
      } catch (e) {
        break;
      }
    }
    // return_to_amo embeds the addon id in the content
    // param, prefixed with "rta:".  Translating that
    // happens in AddonRepository, however we can avoid
    // an API call if we check up front here.
    if (content.startsWith("rta:")) {
      return await getAddonFromRepository(content);
    }
  } catch (e) {
    Cu.reportError("Failed to get the latest add-on version for Return to AMO");
  }
  return null;
}

async function getAttributionContent() {
  let attribution = await AttributionCode.getAttrDataAsync();
  if (attribution?.source === "addons.mozilla.org") {
    let addonInfo = await getAddonInfo(attribution);
    if (addonInfo) {
      return {
        ...addonInfo,
        template: "return_to_amo",
      };
    }
  }
  if (attribution?.ua) {
    return {
      ua: decodeURIComponent(attribution.ua),
    };
  }
  return null;
}

// Return default multistage welcome content
function getDefaults() {
  return Cu.cloneInto(DEFAULT_WELCOME_CONTENT, {});
}

let gSourceL10n = null;

// Localize Firefox download source from user agent attribution to show inside
// import primary button label such as 'Import from <localized browser name>'.
// no firefox as import wizard doesn't show it
const allowedUAs = ["chrome", "edge", "ie"];
function getLocalizedUA(ua) {
  if (!gSourceL10n) {
    gSourceL10n = new Localization(["browser/migration.ftl"]);
  }
  if (allowedUAs.includes(ua)) {
    return gSourceL10n.formatValue(`source-name-${ua.toLowerCase()}`);
  }
  return null;
}

async function prepareContentForReact(content) {
  if (content?.template === "return_to_amo") {
    return content;
  }

  // Helper to find screens to remove and adjust screen order.
  function removeScreens(check) {
    const { screens } = content;
    let removed = 0;
    for (let i = 0; i < screens?.length; i++) {
      if (check(screens[i])) {
        screens.splice(i--, 1);
        removed++;
      } else if (screens[i].order) {
        screens[i].order -= removed;
      }
    }
  }

  // Change content for Windows 7 because non-light themes aren't quite right.
  if (AppConstants.isPlatformAndVersionAtMost("win", "6.1")) {
    removeScreens(screen => ["theme"].includes(screen.content?.tiles?.type));
  }

  // Set the primary import button source based on attribution.
  if (content?.ua) {
    // If available, add the browser source to action data
    // and localized browser string args to primary button label
    const { label, action } =
      content?.screens?.find(
        screen =>
          screen?.content?.primary_button?.action?.type ===
          "SHOW_MIGRATION_WIZARD"
      )?.content?.primary_button ?? {};

    if (action) {
      action.data = { ...action.data, source: content.ua };
    }

    let browserStr = await getLocalizedUA(content.ua);

    if (label?.string_id) {
      label.string_id = browserStr
        ? "mr1-onboarding-import-primary-button-label-attribution"
        : "mr1-onboarding-import-primary-button-label-no-attribution";

      label.args = browserStr ? { previous: browserStr } : {};
    }
  }

  // If already pinned, convert "pin" screen to "welcome" with desired action.
  let removeDefault = !content.needDefault;
  if (!content.needPin) {
    const pinScreen = content.screens?.find(screen =>
      screen.id?.startsWith("AW_PIN_FIREFOX")
    );
    if (pinScreen?.content) {
      pinScreen.id = removeDefault ? "AW_GET_STARTED" : "AW_ONLY_DEFAULT";
      pinScreen.content.title = {
        string_id: "mr1-onboarding-welcome-header",
      };
      pinScreen.content.primary_button = {
        label: {
          string_id: removeDefault
            ? "mr1-onboarding-get-started-primary-button-label"
            : "mr1-onboarding-set-default-only-primary-button-label",
        },
        action: {
          navigate: true,
        },
      };

      // Get started content will navigate without action, so remove "Not now."
      if (removeDefault) {
        delete pinScreen.content.secondary_button;
      } else {
        // The "pin" screen will now handle "default" so remove other "default."
        pinScreen.content.primary_button.action.type = "SET_DEFAULT_BROWSER";
        removeDefault = true;
      }
    }
  }
  if (removeDefault) {
    removeScreens(screen => screen.id?.startsWith("AW_SET_DEFAULT"));
  }

  // Remove Firefox Accounts related UI and prevent related metrics.
  if (!Services.prefs.getBoolPref("identity.fxaccounts.enabled", false)) {
    delete content.screens?.find(
      screen =>
        screen.content?.secondary_button_top?.action?.type ===
        "SHOW_FIREFOX_ACCOUNTS"
    )?.content.secondary_button_top;
    content.skipFxA = true;
  }

  // Remove the English-only image caption.
  if (Services.locale.appLocaleAsBCP47.split("-")[0] !== "en") {
    delete content.screens?.find(
      screen => screen.content?.help_text?.deleteIfNotEn
    )?.content.help_text.text;
  }

  return content;
}

const AboutWelcomeDefaults = {
  prepareContentForReact,
  getDefaults,
  getAttributionContent,
};
