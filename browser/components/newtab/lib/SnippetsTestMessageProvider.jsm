/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_ICON = "chrome://branding/content/icon64.png";

const MESSAGES = () => ([
  {
    "id": "SIMPLE_TEST_1",
    "template": "simple_snippet",
    "campaign": "test_campaign_blocking",
    "content": {
      "icon": TEST_ICON,
      "text": "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      "links": {"syncLink": {"url": "https://www.mozilla.org/en-US/firefox/accounts"}},
      "block_button_text": "Block",
    },
  },
  {
    "id": "SIMPLE_TEST_1_SAME_CAMPAIGN",
    "template": "simple_snippet",
    "campaign": "test_campaign_blocking",
    "content": {
      "icon": TEST_ICON,
      "text": "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      "links": {"syncLink": {"url": "https://www.mozilla.org/en-US/firefox/accounts"}},
      "block_button_text": "Block",
    },
  },
  {
    "id": "SIMPLE_TEST_TALL",
    "template": "simple_snippet",
    "content": {
      "icon": TEST_ICON,
      "text": "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      "links": {"syncLink": {"url": "https://www.mozilla.org/en-US/firefox/accounts"}},
      "button_label": "Get one now!",
      "button_url": "https://www.mozilla.org/en-US/firefox/accounts",
      "block_button_text": "Block",
      "tall": true,
    },
  },
  {
    "id": "SIMPLE_TEST_BUTTON_URL_1",
    "template": "simple_snippet",
    "content": {
      "icon": TEST_ICON,
      "button_label": "Get one now!",
      "button_url": "https://www.mozilla.org/en-US/firefox/accounts",
      "text": "Sync it, link it, take it with you. All this and more with a Firefox Account.",
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
    "id": "NEWSLETTER_TEST_DEFAULTS",
    "template": "newsletter_snippet",
    "content": {
      "scene1_icon": TEST_ICON,
      "scene1_title": "Be a part of a movement.",
      "scene1_title_icon": "",
      "scene1_text": "Internet shutdowns, hackers, harassment &ndash; the health of the internet is on the line. Sign up and Mozilla will keep you updated on how you can help.",
      "scene1_button_label": "Continue",
      "scene1_button_color": "#712b00",
      "scene1_button_background_color": "#ff9400",
      "scene2_title": "Let's do this!",
      "scene2_dismiss_button_text": "Dismiss",
      "scene2_text": "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      "scene2_privacy_html": "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      "scene2_newsletter": "mozilla-foundation",
      "form_action": "https://basket.mozilla.org/subscribe.json",
      "success_text": "Check your inbox for the confirmation!",
      "error_text": "Error!",
      "links": {"privacyLink": {"url": "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894"}},
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
      "scene1_button_color": "#712b00",
      "scene1_button_background_color": "#ff9400",
      "scene2_title": "Let's do this!",
      "locale": "en-CA",
      "scene2_dismiss_button_text": "Dismiss",
      "scene2_text": "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      "scene2_privacy_html": "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      "scene2_button_label": "Sign Me up",
      "scene2_email_placeholder_text": "Your email here",
      "scene2_newsletter": "mozilla-foundation",
      "form_action": "https://basket.mozilla.org/subscribe.json",
      "success_text": "Check your inbox for the confirmation!",
      "error_text": "Error!",
      "links": {"privacyLink": {"url": "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894"}},
    },
  },
  {
    "id": "FXA_SNIPPET_TEST_1",
    "template": "fxa_signup_snippet",
    "content": {
      "scene1_icon": TEST_ICON,
      "scene1_button_label": "Get connected with sync!",
      "scene1_button_color": "#712b00",
      "scene1_button_background_color": "#ff9400",

      "scene1_text": "Connect to Firefox by securely syncing passwords, bookmarks, and open tabs.",
      "scene1_title": "Browser better.",
      "scene1_title_icon": "",

      "scene2_text": "Connect to your Firefox account to securely sync passwords, bookmarks, and open tabs.",
      "scene2_title": "Title 123",
      "scene2_email_placeholder_text": "Your email",
      "scene2_button_label": "Continue",
      "scene2_dismiss_button_text": "Dismiss",
      "form_action": "https://basket.mozilla.org/subscribe.json",
    },
  },
  {
    id: "SNIPPETS_SEND_TO_DEVICE_TEST",
    template: "send_to_device_snippet",
    content: {
      include_sms: true,
      locale: "en-CA",
      country: "us",
      message_id_sms: "ff-mobilesn-download",
      message_id_email: "download-firefox-mobile",

      scene1_button_background_color: "#6200a4",
      scene1_button_color: "#FFFFFF",
      scene1_button_label: "Install now",
      scene1_icon: TEST_ICON,
      scene1_text: "Browse without compromise with Firefox Mobile.",
      scene1_title: "Full-featured. Customizable. Lightning fast",
      scene1_title_icon: "",

      scene2_button_label: "Send",
      scene2_disclaimer_html: "The intended recipient of the email must have consented. <privacyLink>Learn more.</privacyLink>",
      scene2_dismiss_button_text: "Dismiss",
      scene2_icon: TEST_ICON,
      scene2_input_placeholder: "Your email address or phone number",
      scene2_text: "Send Firefox to your phone and take a powerful independent browser with you.",
      scene2_title: "Let's do this!",

      error_text: "Oops, there was a problem.",
      success_title: "Your download link was sent.",
      success_text: "Check your device for the email message!",
      links: {"privacyLink": {"url": "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894"}},
    },
  },
  {
    "id": "EOY_TEST_1",
    "template": "eoy_snippet",
    "content": {
      "highlight_color": "#f05",
      "selected_button": "donation_amount_first",
      "icon": TEST_ICON,
      "button_label": "Donate",
      "monthly_checkbox_label_text": "Make my donation monthly",
      "currency_code": "usd",
      "donation_amount_first": 50,
      "donation_amount_second": 25,
      "donation_amount_third": 10,
      "donation_amount_fourth": 5,
      "donation_form_url": "https://donate.mozilla.org",
      "text": "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
    },
  },
  {
    "id": "EOY_BOLD_TEST_1",
    "template": "eoy_snippet",
    "content": {
      "icon": TEST_ICON,
      "selected_button": "donation_amount_second",
      "button_label": "Donate",
      "monthly_checkbox_label_text": "Make my donation monthly",
      "currency_code": "usd",
      "donation_amount_first": 50,
      "donation_amount_second": 25,
      "donation_amount_third": 10,
      "donation_amount_fourth": 5,
      "donation_form_url": "https://donate.mozilla.org",
      "text": "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
      "test": "bold",
    },
  },
  {
    "id": "EOY_TAKEOVER_TEST_1",
    "template": "eoy_snippet",
    "content": {
      "icon": TEST_ICON,
      "button_label": "Donate",
      "monthly_checkbox_label_text": "Make my donation monthly",
      "currency_code": "usd",
      "donation_amount_first": 50,
      "donation_amount_second": 25,
      "donation_amount_third": 10,
      "donation_amount_fourth": 5,
      "donation_form_url": "https://donate.mozilla.org",
      "text": "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
      "test": "takeover",
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
