/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const ONBOARDING_MESSAGES = [
  {
    id: "ONBOARDING_1",
    template: "onboarding",
    bundled: 3,
    content: {
      title: "Private Browsing",
      text: "Browse by yourself. Private Browsing with Tracking Protection blocks online trackers that follow you around the web.",
      icon: "privatebrowsing",
      button_label: "Try It Now",
      button_action: "OPEN_PRIVATE_BROWSER_WINDOW"
    },
    trigger: "firstRun"
  },
  {
    id: "ONBOARDING_2",
    template: "onboarding",
    bundled: 3,
    content: {
      title: "Screenshots",
      text: "Take, save and share screenshots - without leaving Firefox. Capture a region or an entire page as you browse. Then save to the web for easy access and sharing.",
      icon: "screenshots",
      button_label: "Try It Now",
      button_action: "OPEN_URL",
      button_action_params: "https://screenshots.firefox.com/#tour"
    },
    trigger: "firstRun"
  },
  {
    id: "ONBOARDING_3",
    template: "onboarding",
    bundled: 3,
    content: {
      title: "Add-ons",
      text: "Add even more features that make Firefox work harder for you. Compare prices, check the weather or express your personality with a custom theme.",
      icon: "addons",
      button_label: "Try It Now",
      button_action: "OPEN_ABOUT_PAGE",
      button_action_params: "addons"
    },
    targeting: "isInExperimentCohort == 1",
    trigger: "firstRun"
  },
  {
    id: "ONBOARDING_4",
    template: "onboarding",
    bundled: 3,
    content: {
      title: "Extensions",
      text: "Make browsing faster, smarter, or safer with browser apps. Protect passwords, find deals, download videos, and much more. You can even block annoying ads with extensions like Ghostery.",
      icon: "gift",
      button_label: "Get Ghostery",
      button_action: "OPEN_URL",
      button_action_params: "https://addons.mozilla.org/en-US/firefox/addon/ghostery/"
    },
    targeting: "isInExperimentCohort == 2",
    trigger: "firstRun"
  }
];

const OnboardingMessageProvider = {
  getMessages() {
    return ONBOARDING_MESSAGES;
  }
};
this.OnboardingMessageProvider = OnboardingMessageProvider;

const EXPORTED_SYMBOLS = ["OnboardingMessageProvider"];
