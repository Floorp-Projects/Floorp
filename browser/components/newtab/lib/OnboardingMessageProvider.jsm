/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
ChromeUtils.import("resource://gre/modules/Localization.jsm");
ChromeUtils.import("resource://gre/modules/FxAccountsConfig.jsm");
ChromeUtils.import("resource:///modules/AttributionCode.jsm");
ChromeUtils.import("resource://gre/modules/addons/AddonRepository.jsm");

async function getAddonName() {
  try {
    const {content} = await AttributionCode.getAttrDataAsync();
    if (!content) {
      return null;
    }
    const addons = await AddonRepository.getAddonsByIDs([content]);
    return addons[0].name;
  } catch (e) {
    return null;
  }
}

const L10N = new Localization([
  "branding/brand.ftl",
  "browser/branding/sync-brand.ftl",
  "browser/newtab/onboarding.ftl",
]);

const ONBOARDING_MESSAGES = async () => ([
  {
    id: "ONBOARDING_1",
    template: "onboarding",
    bundled: 3,
    order: 2,
    content: {
      title: {string_id: "onboarding-private-browsing-title"},
      text: {string_id: "onboarding-private-browsing-text"},
      icon: "privatebrowsing",
      primary_button: {
        label: {string_id: "onboarding-button-label-try-now"},
        action: {type: "OPEN_PRIVATE_BROWSER_WINDOW"},
      },
    },
    trigger: {id: "showOnboarding"},
  },
  {
    id: "ONBOARDING_2",
    template: "onboarding",
    bundled: 3,
    order: 3,
    content: {
      title: {string_id: "onboarding-screenshots-title"},
      text: {string_id: "onboarding-screenshots-text"},
      icon: "screenshots",
      primary_button: {
        label: {string_id: "onboarding-button-label-try-now"},
        action: {
          type: "OPEN_URL",
          data: {args: "https://screenshots.firefox.com/#tour", where: "tabshifted"},
        },
      },
    },
    trigger: {id: "showOnboarding"},
  },
  {
    id: "ONBOARDING_3",
    template: "onboarding",
    bundled: 3,
    order: 1,
    content: {
      title: {string_id: "onboarding-addons-title"},
      text: {string_id: "onboarding-addons-text"},
      icon: "addons",
      primary_button: {
        label: {string_id: "onboarding-button-label-try-now"},
        action: {
          type: "OPEN_ABOUT_PAGE",
          data: {args: "addons"},
        },
      },
    },
    targeting: "attributionData.campaign != 'non-fx-button' && attributionData.source != 'addons.mozilla.org'",
    trigger: {id: "showOnboarding"},
  },
  {
    id: "ONBOARDING_4",
    template: "onboarding",
    bundled: 3,
    order: 1,
    content: {
      title: {string_id: "onboarding-ghostery-title"},
      text: {string_id: "onboarding-ghostery-text"},
      icon: "gift",
      primary_button: {
        label: {string_id: "onboarding-button-label-try-now"},
        action: {
          type: "OPEN_URL",
          data: {args: "https://addons.mozilla.org/en-US/firefox/addon/ghostery/", where: "tabshifted"},
        },
      },
    },
    targeting: "providerCohorts.onboarding == 'ghostery'",
    trigger: {id: "showOnboarding"},
  },
  {
    id: "ONBOARDING_5",
    template: "onboarding",
    bundled: 3,
    order: 4,
    content: {
      title: {string_id: "onboarding-fxa-title"},
      text: {string_id: "onboarding-fxa-text"},
      icon: "sync",
      primary_button: {
        label: {string_id: "onboarding-button-label-get-started"},
        action: {
          type: "OPEN_URL",
          data: {args: await FxAccountsConfig.promiseEmailFirstURI("onboarding"), where: "tabshifted"},
        },
      },
    },
    targeting: "attributionData.campaign == 'non-fx-button' && attributionData.source == 'addons.mozilla.org'",
    trigger: {id: "showOnboarding"},
  },
  {
    id: "FXA_1",
    template: "fxa_overlay",
    trigger: {id: "firstRun"},
  },
  {
    id: "RETURN_TO_AMO_1",
    template: "return_to_amo_overlay",
    content: {
      header: {string_id: "onboarding-welcome-header"},
      title: {string_id: "return-to-amo-sub-header"},
      addon_icon: null, // to be dynamically filled in, in ASRouter.jsm
      icon: "gift-extension",
      text: {string_id: "return-to-amo-addon-header", args: {"addon-name": await getAddonName()}},
      primary_button: {
        label: {string_id: "return-to-amo-extension-button"},
        action: {
          type: "INSTALL_ADDON_FROM_URL",
          data: {url: null}, // to be dynamically filled in, in ASRouter.jsm
        },
      },
      secondary_button: {
        label: {string_id: "return-to-amo-get-started-button"},
      },
    },
    targeting: "attributionData.campaign == 'non-fx-button' && attributionData.source == 'addons.mozilla.org'",
    trigger: {id: "firstRun"},
  },
]);

const OnboardingMessageProvider = {
  async getExtraAttributes() {
    const [header, button_label] = await L10N.formatMessages([
      {id: "onboarding-welcome-header"},
      {id: "onboarding-start-browsing-button-label"},
    ]);
    return {header: header.value, button_label: button_label.value};
  },
  async getMessages() {
    const messages = await this.translateMessages(await ONBOARDING_MESSAGES());
    return messages;
  },
  async getUntranslatedMessages() {
    // This is helpful for jsonSchema testing - since we are localizing in the provider
    const messages = await ONBOARDING_MESSAGES();
    return messages;
  },
  async translateMessages(messages) {
    let translatedMessages = [];
    for (const msg of messages) {
      let translatedMessage = {...msg};

      // If the message has no content, do not attempt to translate it
      if (!translatedMessage.content) {
        translatedMessages.push(translatedMessage);
        continue;
      }

      const [primary_button_string, title_string, text_string] = await L10N.formatMessages([
        {id: msg.content.primary_button.label.string_id},
        {id: msg.content.title.string_id},
        {id: msg.content.text.string_id, args: msg.content.text.args},
      ]);
      translatedMessage.content.primary_button.label = primary_button_string.value;
      translatedMessage.content.title = title_string.value;
      translatedMessage.content.text = text_string.value;

      // Translate any secondary buttons separately
      if (msg.content.secondary_button) {
        const [secondary_button_string] = await L10N.formatMessages([{id: msg.content.secondary_button.label.string_id}]);
        translatedMessage.content.secondary_button.label = secondary_button_string.value;
      }
      if (msg.content.header) {
        const [header_string] = await L10N.formatMessages([{id: msg.content.header.string_id}]);
        translatedMessage.content.header = header_string.value;
      }
      translatedMessages.push(translatedMessage);
    }
    return translatedMessages;
  },
};
this.OnboardingMessageProvider = OnboardingMessageProvider;

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
