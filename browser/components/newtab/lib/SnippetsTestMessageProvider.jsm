/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_ICON = "chrome://branding/content/icon64.png";

const MESSAGES = () => ([
  {
    "id": "SIMPLE_TEST_1",
    "template": "simple_snippet",
    "content": {
      "icon": TEST_ICON,
      "text": "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      "links": {"syncLink": {"url": "https://www.mozilla.org/en-US/firefox/accounts"}},
      "block_button_text": "Block",
    },
  },
  {
    "id": "SIMPLE_WITH_TITLE_TEST_1",
    "template": "simple_snippet",
    "content": {
      "icon": TEST_ICON,
      "title": "Ready to sync?",
      "text": "Get connected with a <syncLink>Firefox account</syncLink>.",
      "links": {"syncLink": {"url": "https://www.mozilla.org/en-US/firefox/accounts"}},
      "block_button_text": "Block",
    },
  },
  {
    "id": "NEWSLETTER_TEST_1",
    "template": "newsletter_snippet",
    "content": {
      "scene1_icon": TEST_ICON,
      "scene1_title": "Be a part of a movement.",
      "scene1_title_icon": "",
      "scene1_text": "Internet shutdowns, hackers, harassment &ndash; the health of the internet is on the line. Sign up and Mozilla will keep you updated on how you can help.",
      "scene1_button_label": "Continue",
       // TODO: Need to update schema
      // "scene1_button_color": "#712b00",
      // "scene1_button_background_color": "#ff9400",
      "scene2_dismiss_button_text": "Dismiss",
      "scene2_text": "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      "scene2_privacy_html": "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      "scene2_button_label": "Sign Me up",
      "scene2_email_placeholder_text": "Your email here",
      "form_action": "https://basket.mozilla.org/subscribe.json",
      "success_text": "Check your inbox for the confirmation!",
      "error_text": "Error!",
      "hidden_inputs": {
        "fmt": "H",
        "lang": "en-US",
        "newsletters": "mozilla-foundation",
      },
      "links": {"privacyLink": {"url": "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894"}},
    },
  },
  {
    "id": "FXA_SNIPPET_TEST_1",
    "template": "fxa_signup_snippet",
    "content": {
      "scene1_icon": TEST_ICON,
      "scene1_button_label": "Get connected with sync!",
      // TODO: Need to update schema
      // "scene1_button_color": "#712b00",
      // "scene1_button_background_color": "#ff9400",

      "scene1_text": "Connect to Firefox by securely syncing passwords, bookmarks, and open tabs.",
      "scene1_title": "Browser better.",
      "scene1_title_icon": "",

      "scene2_text": "Connect to your Firefox account to securely sync passwords, bookmarks, and open tabs.",
      // TODO: needs to be added
      // "scene2_title": "Title 123",
      "scene2_email_placeholder_text": "Your email",
      "scene2_button_label": "Continue",
      "scene2_dismiss_button_text": "Dismiss",
      "form_action": "https://basket.mozilla.org/subscribe.json",

      // TODO: This should not be required
      "success_text": "Check your inbox for the confirmation!",
      "error_text": "Error!",
      "hidden_inputs": {},
    },
  },
]);

const SnippetsTestMessageProvider = {
  getMessages() {
    return MESSAGES()
      // Ensures we never actually show test except when triggered by debug tools
      .map(message => ({...message, targeting: `providerCohorts.snippets_local_testing == "SHOW_TEST"`}));
  },
};
this.SnippetsTestMessageProvider = SnippetsTestMessageProvider;

const EXPORTED_SYMBOLS = ["SnippetsTestMessageProvider"];
