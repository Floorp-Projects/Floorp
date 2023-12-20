/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXPORTED_SYMBOLS = ["AboutWelcomeDefaults"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonRepository: "resource://gre/modules/addons/AddonRepository.sys.mjs",
  AttributionCode: "resource:///modules/AttributionCode.sys.mjs",
  BrowserUtils: "resource://gre/modules/BrowserUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  AWScreenUtils: "resource:///modules/aboutwelcome/AWScreenUtils.jsm",
});

// Message to be updated based on finalized MR designs
const MR_ABOUT_WELCOME_DEFAULT = {
  id: "MR_WELCOME_DEFAULT",
  template: "multistage",
  // Allow tests to easily disable transitions.
  transitions: Services.prefs.getBoolPref(
    "browser.aboutwelcome.transitions",
    true
  ),
  backdrop:
    "var(--mr-welcome-background-color) var(--mr-welcome-background-gradient)",
  screens: [
    {
      id: "AW_WELCOME_BACK",
      targeting: "isDeviceMigration",
      content: {
        position: "split",
        split_narrow_bkg_position: "-100px",
        image_alt_text: {
          string_id: "onboarding-device-migration-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/device-migration.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-device-migration-title",
        },
        subtitle: {
          string_id: "onboarding-device-migration-subtitle2",
        },
        primary_button: {
          label: {
            string_id: "onboarding-device-migration-primary-button-label",
          },
          action: {
            type: "FXA_SIGNIN_FLOW",
            navigate: "actionResult",
            data: {
              entrypoint: "fx-device-migration-onboarding",
              extraParams: {
                utm_content: "migration-onboarding",
                utm_source: "fx-new-device-sync",
                utm_medium: "firefox-desktop",
                utm_campaign: "migration",
              },
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
      },
    },
    {
      id: "AW_EASY_SETUP_NEEDS_DEFAULT_AND_PIN",
      targeting:
        "doesAppNeedPin && 'browser.shell.checkDefaultBrowser'|preferenceValue && !isDefaultBrowser",
      content: {
        position: "split",
        split_narrow_bkg_position: "-60px",
        image_alt_text: {
          string_id: "mr2022-onboarding-default-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        hide_secondary_section: "responsive",
        logo: {},
        title: {
          string_id: "onboarding-easy-setup-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-easy-setup-security-and-privacy-subtitle",
        },
        tiles: {
          type: "multiselect",
          data: [
            {
              id: "checkbox-1",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-pin-primary-button-label",
              },
              action: {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
            },
            {
              id: "checkbox-2",
              defaultValue: true,
              label: {
                string_id:
                  "mr2022-onboarding-easy-setup-set-default-checkbox-label",
              },
              action: {
                type: "SET_DEFAULT_BROWSER",
              },
            },
            {
              id: "checkbox-3",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-easy-setup-import-checkbox-label",
              },
              uncheckedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                        },
                      },
                    },
                  ],
                },
              },
              checkedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                          value: true,
                        },
                      },
                    },
                  ],
                },
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr2022-onboarding-easy-setup-primary-button-label",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            navigate: true,
            data: {
              actions: [],
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
        secondary_button_top: {
          label: {
            string_id: "mr1-onboarding-sign-in-button-label",
          },
          action: {
            data: {
              entrypoint: "activity-stream-firstrun",
              where: "tab",
            },
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
          },
        },
      },
    },
    {
      id: "AW_EASY_SETUP_NEEDS_DEFAULT",
      targeting:
        "!doesAppNeedPin && 'browser.shell.checkDefaultBrowser'|preferenceValue && !isDefaultBrowser",
      content: {
        position: "split",
        split_narrow_bkg_position: "-60px",
        image_alt_text: {
          string_id: "mr2022-onboarding-default-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-easy-setup-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-easy-setup-security-and-privacy-subtitle",
        },
        tiles: {
          type: "multiselect",
          data: [
            {
              id: "checkbox-1",
              defaultValue: true,
              label: {
                string_id:
                  "mr2022-onboarding-easy-setup-set-default-checkbox-label",
              },
              action: {
                type: "SET_DEFAULT_BROWSER",
              },
            },
            {
              id: "checkbox-2",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-easy-setup-import-checkbox-label",
              },
              uncheckedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                        },
                      },
                    },
                  ],
                },
              },
              checkedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                          value: true,
                        },
                      },
                    },
                  ],
                },
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr2022-onboarding-easy-setup-primary-button-label",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            navigate: true,
            data: {
              actions: [],
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
        secondary_button_top: {
          label: {
            string_id: "mr1-onboarding-sign-in-button-label",
          },
          action: {
            data: {
              entrypoint: "activity-stream-firstrun",
              where: "tab",
            },
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
          },
        },
      },
    },
    {
      id: "AW_EASY_SETUP_NEEDS_PIN",
      targeting:
        "doesAppNeedPin && (!'browser.shell.checkDefaultBrowser'|preferenceValue || isDefaultBrowser)",
      content: {
        position: "split",
        split_narrow_bkg_position: "-60px",
        image_alt_text: {
          string_id: "mr2022-onboarding-default-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-easy-setup-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-easy-setup-security-and-privacy-subtitle",
        },
        tiles: {
          type: "multiselect",
          data: [
            {
              id: "checkbox-1",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-pin-primary-button-label",
              },
              action: {
                type: "PIN_FIREFOX_TO_TASKBAR",
              },
            },
            {
              id: "checkbox-2",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-easy-setup-import-checkbox-label",
              },
              uncheckedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                        },
                      },
                    },
                  ],
                },
              },
              checkedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                          value: true,
                        },
                      },
                    },
                  ],
                },
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr2022-onboarding-easy-setup-primary-button-label",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            navigate: true,
            data: {
              actions: [],
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
        secondary_button_top: {
          label: {
            string_id: "mr1-onboarding-sign-in-button-label",
          },
          action: {
            data: {
              entrypoint: "activity-stream-firstrun",
              where: "tab",
            },
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
          },
        },
      },
    },
    {
      id: "AW_EASY_SETUP_ONLY_IMPORT",
      targeting:
        "!doesAppNeedPin && (!'browser.shell.checkDefaultBrowser'|preferenceValue || isDefaultBrowser)",
      content: {
        position: "split",
        split_narrow_bkg_position: "-60px",
        image_alt_text: {
          string_id: "mr2022-onboarding-default-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-settodefault.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-easy-setup-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-easy-setup-security-and-privacy-subtitle",
        },
        tiles: {
          type: "multiselect",
          data: [
            {
              id: "checkbox-1",
              defaultValue: true,
              label: {
                string_id: "mr2022-onboarding-easy-setup-import-checkbox-label",
              },
              uncheckedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                        },
                      },
                    },
                  ],
                },
              },
              checkedAction: {
                type: "MULTI_ACTION",
                data: {
                  actions: [
                    {
                      type: "SET_PREF",
                      data: {
                        pref: {
                          name: "showEmbeddedImport",
                          value: true,
                        },
                      },
                    },
                  ],
                },
              },
            },
          ],
        },
        primary_button: {
          label: {
            string_id: "mr2022-onboarding-easy-setup-primary-button-label",
          },
          action: {
            type: "MULTI_ACTION",
            collectSelect: true,
            navigate: true,
            data: {
              actions: [],
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
        secondary_button_top: {
          label: {
            string_id: "mr1-onboarding-sign-in-button-label",
          },
          action: {
            data: {
              entrypoint: "activity-stream-firstrun",
              where: "tab",
            },
            type: "SHOW_FIREFOX_ACCOUNTS",
            addFlowParams: true,
          },
        },
      },
    },
    {
      id: "AW_LANGUAGE_MISMATCH",
      content: {
        position: "split",
        background: "var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "mr2022-onboarding-live-language-text",
        },
        subtitle: {
          string_id: "mr2022-language-mismatch-subtitle",
        },
        hero_text: {
          string_id: "mr2022-onboarding-live-language-text",
          useLangPack: true,
        },
        languageSwitcher: {
          downloading: {
            string_id: "onboarding-live-language-button-label-downloading",
          },
          cancel: {
            string_id: "onboarding-live-language-secondary-cancel-download",
          },
          waiting: { string_id: "onboarding-live-language-waiting-button" },
          skip: { string_id: "mr2022-onboarding-secondary-skip-button-label" },
          action: {
            navigate: true,
          },
          switch: {
            string_id: "mr2022-onboarding-live-language-switch-to",
            useLangPack: true,
          },
          continue: {
            string_id: "mr2022-onboarding-live-language-continue-in",
          },
        },
      },
    },
    {
      id: "AW_IMPORT_SETTINGS_EMBEDDED",
      targeting: `("messaging-system-action.showEmbeddedImport" |preferenceValue == true) && useEmbeddedMigrationWizard`,
      content: {
        tiles: { type: "migration-wizard" },
        position: "split",
        split_narrow_bkg_position: "-42px",
        image_alt_text: {
          string_id: "mr2022-onboarding-import-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-import.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        hide_secondary_section: "responsive",
        migrate_start: {
          action: {},
        },
        migrate_close: {
          action: {
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
      },
    },
    {
      id: "AW_MOBILE_DOWNLOAD",
      // The mobile download screen should only be shown to users who
      // are either not logged into FxA, or don't have any mobile devices syncing
      targeting: "!isFxASignedIn || sync.mobileDevices == 0",
      content: {
        position: "split",
        split_narrow_bkg_position: "-160px",
        image_alt_text: {
          string_id: "mr2022-onboarding-mobile-download-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-mobilecrosspromo.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-mobile-download-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-mobile-download-security-and-privacy-subtitle",
        },
        hero_image: {
          url: "chrome://activity-stream/content/data/content/assets/mobile-download-qr-new-user.svg",
        },
        cta_paragraph: {
          text: {
            string_id: "mr2022-onboarding-mobile-download-cta-text",
            string_name: "download-label",
          },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://www.mozilla.org/firefox/mobile/get-app/?utm_medium=firefox-desktop&utm_source=onboarding-modal&utm_campaign=mr2022&utm_content=new-global",
              where: "tab",
            },
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
          has_arrow_icon: true,
        },
      },
    },
    {
      id: "AW_AMO_INTRODUCE",
      // Show to en-* locales only
      targeting: "localeLanguageCode == 'en'",
      content: {
        position: "split",
        split_narrow_bkg_position: "-58px",
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-amo-collection.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: { string_id: "amo-screen-title" },
        subtitle: { string_id: "amo-screen-subtitle" },
        primary_button: {
          label: { string_id: "amo-screen-primary-cta" },
          action: {
            type: "OPEN_URL",
            data: {
              args: "https://addons.mozilla.org/en-US/firefox/collections/4757633/25c2b44583534b3fa8fea977c419cd/?page=1&collection_sort=-added",
              where: "tabshifted",
            },
            navigate: true,
          },
        },
        secondary_button: {
          label: {
            string_id: "mr2022-onboarding-secondary-skip-button-label",
          },
          action: {
            navigate: true,
          },
        },
      },
    },
    {
      id: "AW_GRATITUDE",
      content: {
        position: "split",
        split_narrow_bkg_position: "-228px",
        image_alt_text: {
          string_id: "mr2022-onboarding-gratitude-image-alt",
        },
        background:
          "url('chrome://activity-stream/content/data/content/assets/mr-gratitude.svg') var(--mr-secondary-position) no-repeat var(--mr-screen-background-color)",
        progress_bar: true,
        logo: {},
        title: {
          string_id: "onboarding-gratitude-security-and-privacy-title",
        },
        subtitle: {
          string_id: "onboarding-gratitude-security-and-privacy-subtitle",
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
  const [addonInfo] = await lazy.AddonRepository.getAddonsByIDs([data]);
  if (addonInfo.sourceURI.scheme !== "https") {
    return null;
  }

  return {
    name: addonInfo.name,
    url: addonInfo.sourceURI.spec,
    iconURL: addonInfo.icons["64"] || addonInfo.icons["32"],
    type: addonInfo.type,
    screenshots: addonInfo.screenshots,
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
    console.error("Failed to get the latest add-on version for Return to AMO");
  }
  return null;
}

async function getAttributionContent() {
  let attribution = await lazy.AttributionCode.getAttrDataAsync();
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
  return Cu.cloneInto(MR_ABOUT_WELCOME_DEFAULT, {});
}

let gSourceL10n = null;

// Localize Firefox download source from user agent attribution to show inside
// import primary button label such as 'Import from <localized browser name>'.
// no firefox as import wizard doesn't show it
const allowedUAs = ["chrome", "edge", "ie"];
function getLocalizedUA(ua) {
  if (!gSourceL10n) {
    gSourceL10n = new Localization(["browser/migrationWizard.ftl"]);
  }
  if (allowedUAs.includes(ua)) {
    return gSourceL10n.formatValue(`migration-source-name-${ua.toLowerCase()}`);
  }
  return null;
}

function prepareMobileDownload(content) {
  let mobileContent = content?.screens?.find(
    screen => screen.id === "AW_MOBILE_DOWNLOAD"
  )?.content;

  if (!mobileContent) {
    return content;
  }
  if (!lazy.BrowserUtils.sendToDeviceEmailsSupported()) {
    // If send to device emails are not supported for a user's locale,
    // remove the send to device link and update the screen text
    delete mobileContent.cta_paragraph.action;
    mobileContent.cta_paragraph.text = {
      string_id: "mr2022-onboarding-no-mobile-download-cta-text",
    };
  }
  // Update CN specific QRCode url
  if (AppConstants.isChinaRepack()) {
    mobileContent.hero_image.url = `${mobileContent.hero_image.url.slice(
      0,
      mobileContent.hero_image.url.indexOf(".svg")
    )}-cn.svg`;
  }

  return content;
}

async function prepareContentForReact(content) {
  const { screens } = content;

  if (content?.template === "return_to_amo") {
    return content;
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
        : "mr2022-onboarding-import-primary-button-label-no-attribution";

      label.args = browserStr ? { previous: browserStr } : {};
    }
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

  let shouldRemoveLanguageMismatchScreen = true;
  if (content.languageMismatchEnabled) {
    const screen = content?.screens?.find(s => s.id === "AW_LANGUAGE_MISMATCH");
    if (screen && content.appAndSystemLocaleInfo.canLiveReload) {
      // Add the display names for the OS and Firefox languages, like "American English".
      function addMessageArgs(obj) {
        for (const value of Object.values(obj)) {
          if (value?.string_id) {
            value.args = content.appAndSystemLocaleInfo.displayNames;
          }
        }
      }

      addMessageArgs(screen.content.languageSwitcher);
      addMessageArgs(screen.content);
      shouldRemoveLanguageMismatchScreen = false;
    }
  }

  if (shouldRemoveLanguageMismatchScreen) {
    await lazy.AWScreenUtils.removeScreens(
      screens,
      screen => screen.id === "AW_LANGUAGE_MISMATCH"
    );
  }

  return prepareMobileDownload(content);
}

const AboutWelcomeDefaults = {
  prepareContentForReact,
  getDefaults,
  getAttributionContent,
};
