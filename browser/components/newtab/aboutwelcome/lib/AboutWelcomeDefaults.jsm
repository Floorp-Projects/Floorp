/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeDefaults"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ShellService: "resource:///modules/ShellService.jsm",
  AttributionCode: "resource:///modules/AttributionCode.jsm",
  AddonRepository: "resource://gre/modules/addons/AddonRepository.jsm",
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
  background_url:
    "chrome://activity-stream/content/data/content/assets/proton-bkg.avif",
  screens: [
    {
      id: "AW_SET_DEFAULT",
      order: 0,
      content: {
        title: "Welcome to Firefox",
        subtitle: "An inspiring headline goes here",
        help_text: {
          text: "Placeholder Name - Metal drummer, Firefox aficianado",
        },
        primary_button: {
          label: "Always use Firefox",
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
      },
    },
    {
      id: "AW_IMPORT_SETTINGS",
      order: 1,
      content: {
        title: "Dive right in",
        subtitle: "Import your passwords, bookmarks, and more",
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
        title: "Get a fresh look",
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
              label: "Automatic",
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
              label: "Alpenglow",
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

// Helper function to determine if Windows platform supports
// automated pinning to taskbar.
// See https://searchfox.org/mozilla-central/rev/002023eb262be9db3479142355e1675645d52d52/browser/components/shell/nsIWindowsShellService.idl#17
function canPinCurrentAppToTaskbar() {
  try {
    ShellService.QueryInterface(
      Ci.nsIWindowsShellService
    ).checkPinCurrentAppToTaskbar();
    return true;
  } catch (e) {}
  return false;
}

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
    return await getAddonFromRepository(content);
  } catch (e) {
    Cu.reportError("Failed to get the latest add-on version for Return to AMO");
    return null;
  }
}

function hasAMOAttribution(attributionData) {
  return (
    attributionData &&
    attributionData.campaign === "non-fx-button" &&
    attributionData.source === "addons.mozilla.org"
  );
}

async function formatAttributionData(attribution) {
  if (hasAMOAttribution(attribution)) {
    let extraProps = await getAddonInfo(attribution);
    if (extraProps) {
      return extraProps;
    }
  }
  return null;
}

async function getAttributionContent() {
  let attributionContent = await formatAttributionData(
    await AttributionCode.getAttrDataAsync()
  );

  if (attributionContent) {
    return { ...attributionContent, template: "return_to_amo" };
  }

  return null;
}

const RULES = [
  {
    description: "Proton Default AW content",
    getDefaults(featureConfig) {
      if (featureConfig?.design === "proton") {
        return { ...DEFAULT_PROTON_WELCOME_CONTENT };
      }

      return null;
    },
  },
  {
    description: "Windows pin to task bar screen",
    getDefaults() {
      if (canPinCurrentAppToTaskbar()) {
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
      return { ...DEFAULT_WELCOME_CONTENT };
    },
  },
];

function getDefaults(featureConfig) {
  for (const rule of RULES) {
    const result = rule.getDefaults(featureConfig);
    if (result) {
      return result;
    }
  }
  return null;
}

const AboutWelcomeDefaults = {
  getDefaults,
  getAttributionContent,
};
