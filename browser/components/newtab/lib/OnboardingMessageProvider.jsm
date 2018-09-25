/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
ChromeUtils.import("resource://gre/modules/Localization.jsm");
const L10N = new Localization([
  "branding/brand.ftl",
  "browser/newtab/onboarding.ftl",
]);

const ONBOARDING_MESSAGES = [
  {
    id: "ONBOARDING_1",
    template: "onboarding",
    bundled: 3,
    order: 2,
    content: {
      title: {string_id: "onboarding-private-browsing-title"},
      text: {string_id: "onboarding-private-browsing-text"},
      icon: "privatebrowsing",
      button_label: {string_id: "onboarding-button-label-try-now"},
      button_action: {type: "OPEN_PRIVATE_BROWSER_WINDOW"},
    },
    trigger: {id: "firstRun"},
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
      button_label: {string_id: "onboarding-button-label-try-now"},
      button_action: {
        type: "OPEN_URL",
        data: {url: "https://screenshots.firefox.com/#tour"},
      },
    },
    trigger: {id: "firstRun"},
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
      button_label: {string_id: "onboarding-button-label-try-now"},
      button_action: {
        type: "OPEN_ABOUT_PAGE",
        data: {page: "addons"},
      },
    },
    targeting: "isInExperimentCohort == 1",
    trigger: {id: "firstRun"},
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
      button_label: {string_id: "onboarding-button-label-try-now"},
      button_action: {
        type: "OPEN_URL",
        data: {url: "https://addons.mozilla.org/en-US/firefox/addon/ghostery/"},
      },
    },
    targeting: "isInExperimentCohort == 2",
    trigger: {id: "firstRun"},
  },
];

const OnboardingMessageProvider = {
  async getExtraAttributes() {
    const [header, button_label] = await L10N.formatMessages([
      {id: "onboarding-welcome-header"},
      {id: "onboarding-start-browsing-button-label"},
    ]);
    return {header: header.value, button_label: button_label.value};
  },
  async getMessages() {
    const messages = await this.translateMessages(ONBOARDING_MESSAGES);
    return messages;
  },
  async translateMessages(messages) {
    let translatedMessages = [];
    for (const msg of messages) {
      let translatedMessage = msg;
      const [button_string, title_string, text_string] = await L10N.formatMessages([
        {id: msg.content.button_label.string_id},
        {id: msg.content.title.string_id},
        {id: msg.content.text.string_id},
      ]);
      translatedMessage.content.button_label = button_string.value;
      translatedMessage.content.title = title_string.value;
      translatedMessage.content.text = text_string.value;
      translatedMessages.push(translatedMessage);
    }
    return translatedMessages;
  },
};
this.OnboardingMessageProvider = OnboardingMessageProvider;

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
