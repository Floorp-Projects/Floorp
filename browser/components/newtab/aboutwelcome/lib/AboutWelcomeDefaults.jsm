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
  template: "multistage",
  screens: [
    {
      id: "AW_SET_DEFAULT",
      order: 0,
      content: {
        zap: true,
        title: {
          string_id: "onboarding-multistage-set-default-header",
        },
        subtitle: {
          string_id: "onboarding-multistage-set-default-subtitle",
        },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-set-default-primary-button-label",
          },
          action: {
            navigate: true,
            type: "SET_DEFAULT_BROWSER",
          },
        },
        secondary_button: {
          label: {
            string_id:
              "onboarding-multistage-set-default-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button_top: {
          text: {
            string_id: "onboarding-multistage-welcome-secondary-button-text",
          },
          label: {
            string_id: "onboarding-multistage-welcome-secondary-button-label",
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
      id: "AW_IMPORT_SETTINGS",
      order: 1,
      content: {
        zap: true,
        help_text: {
          text: {
            string_id: "onboarding-import-sites-disclaimer",
          },
        },
        title: {
          string_id: "onboarding-multistage-import-header",
        },
        subtitle: {
          string_id: "onboarding-multistage-import-subtitle",
        },
        tiles: {
          type: "topsites",
          showTitles: true,
        },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-import-primary-button-label",
          },
          action: {
            type: "SHOW_MIGRATION_WIZARD",
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "onboarding-multistage-import-secondary-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_CHOOSE_THEME",
      order: 2,
      content: {
        zap: true,
        title: {
          string_id: "onboarding-multistage-theme-header",
        },
        subtitle: {
          string_id: "onboarding-multistage-theme-subtitle",
        },
        tiles: {
          type: "theme",
          action: {
            theme: "<event>",
          },
          data: [
            {
              theme: "automatic",
              label: {
                string_id: "onboarding-multistage-theme-label-automatic",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-automatic-2",
              },
              description: {
                string_id:
                  "onboarding-multistage-theme-description-automatic-2",
              },
            },
            {
              theme: "light",
              label: {
                string_id: "onboarding-multistage-theme-label-light",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-light-2",
              },
              description: {
                string_id: "onboarding-multistage-theme-description-light",
              },
            },
            {
              theme: "dark",
              label: {
                string_id: "onboarding-multistage-theme-label-dark",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-dark-2",
              },
              description: {
                string_id: "onboarding-multistage-theme-description-dark",
              },
            },
            {
              theme: "alpenglow",
              label: {
                string_id: "onboarding-multistage-theme-label-alpenglow",
              },
              tooltip: {
                string_id: "onboarding-multistage-theme-tooltip-alpenglow-2",
              },
              description: {
                string_id: "onboarding-multistage-theme-description-alpenglow",
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "onboarding-multistage-theme-primary-button-label2",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "onboarding-multistage-theme-secondary-button-label",
          },
          action: {
            theme: "automatic",
            navigate: true,
          },
        },
      },
    },
  ],
};

const DEFAULT_PROTON_WELCOME_CONTENT = {
  id: "DEFAULT_ABOUTWELCOME_PROTON",
  template: "multistage",
  transitions: true,
  background_url:
    "chrome://activity-stream/content/data/content/assets/proton-bkg.jpg",
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
      id: "AW_CHOOSE_THEME",
      order: 3,
      content: {
        title: {
          string_id: "mr1-onboarding-theme-header",
        },
        subtitle: {
          string_id: "mr1-onboarding-theme-subtitle",
        },
        tiles: {
          type: "theme",
          action: {
            theme: "<event>",
          },
          data: [
            {
              theme: "automatic",
              label: {
                string_id: "mr1-onboarding-theme-label-system",
              },
              tooltip: {
                string_id: "mr1-onboarding-theme-tooltip-system",
              },
              description: {
                string_id: "mr1-onboarding-theme-description-system",
              },
            },
            {
              theme: "light",
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
              theme: "dark",
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
            {
              theme: "alpenglow",
              label: {
                string_id: "mr1-onboarding-theme-label-alpenglow",
              },
              tooltip: {
                string_id: "mr1-onboarding-theme-tooltip-alpenglow",
              },
              description: {
                string_id: "mr1-onboarding-theme-description-alpenglow",
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr1-onboarding-theme-primary-button-label",
          },
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr1-onboarding-theme-secondary-button-label",
          },
          action: {
            theme: "automatic",
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

const RULES = [
  {
    description: "Proton Default AW content",
    getDefaults(featureConfig) {
      if (featureConfig?.isProton) {
        return DEFAULT_PROTON_WELCOME_CONTENT;
      }
      return null;
    },
  },
  {
    description: "Windows pin to task bar screen",
    getDefaults(featureConfig) {
      if (featureConfig.needPin) {
        return {
          template: "multistage",
          screens: [
            {
              id: "AW_PIN_AND_DEFAULT",
              order: 0,
              content: {
                ...DEFAULT_WELCOME_CONTENT.screens[0].content,
                title: {
                  string_id: "onboarding-multistage-pin-default-header",
                },
                subtitle: {
                  string_id: "onboarding-multistage-pin-default-subtitle",
                },
                help_text: {
                  position: "default",
                  text: {
                    string_id: "onboarding-multistage-pin-default-help-text",
                  },
                },
                primary_button: {
                  label: {
                    string_id:
                      "onboarding-multistage-pin-default-primary-button-label",
                  },
                  action: {
                    navigate: true,
                    type: "PIN_AND_DEFAULT",
                    waitForDefault: true,
                  },
                },
                waiting_for_default: {
                  subtitle: {
                    string_id:
                      "onboarding-multistage-pin-default-waiting-subtitle",
                  },
                  help_text: null,
                  primary_button: null,
                  tiles: {
                    media_type: "tiles-delayed",
                    type: "image",
                    source: {
                      default:
                        "chrome://activity-stream/content/data/content/assets/remote/windows-default-browser.gif",
                    },
                  },
                },
              },
            },
            ...DEFAULT_WELCOME_CONTENT.screens.slice(1),
          ],
        };
      }

      return null;
    },
  },
  {
    description: "Default AW content",
    getDefaults() {
      return DEFAULT_WELCOME_CONTENT;
    },
  },
];

function getDefaults(featureConfig) {
  for (const rule of RULES) {
    const result = rule.getDefaults(featureConfig);
    if (result) {
      // Make a deep copy of the object to avoid editing the original default.
      return Cu.cloneInto(result, {});
    }
  }
  return null;
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

  if (content.isProton) {
    content.design = "proton";
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
    removeScreens(screen => screen.content?.tiles?.type === "theme");
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
