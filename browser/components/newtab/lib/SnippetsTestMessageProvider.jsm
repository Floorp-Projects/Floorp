/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const TEST_ICON = "chrome://branding/content/icon64.png";
const TEST_ICON_16 = "chrome://branding/content/icon16.png";
const TEST_ICON_BW =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAQAAAC1+jfqAAAABGdBTUEAALGPC/xhBQAAACBjSFJNAAB6JgAAgIQAAPoAAACA6AAAdTAAAOpgAAA6mAAAF3CculE8AAAAAmJLR0QA/4ePzL8AAAAHdElNRQfjBQ8QDifrKGc/AAABf0lEQVQoz4WRO08UUQCFvztzd1AgG9jRgGwkhEoMIYGSygYt+A00tpZGY0jYxAJKEwkNjX9AK2xACx4dhFiQQCiMMRr2kYXdnQcz7L0z91qAMVac6hTfSU7OgVsk/prtyfSNfRb7ge2cd7dmVucP/wM2lwqVqoyICahRx9Nz71+8AnAAvlTct+dSYDBYcgJ+Fj68XFu/AfamnIoWFoHFYrAUuYMSn55/fAIOxIs1t4MhQpNxRYsUD0ld7r8DCfZph4QecrqkhCREgMLSeISQkAy0UBgE0CYgIkeRA9HdsCQhpEGCxichpItHigEcPH4XJLRbTf8STY0iiiuu60Ifxexx04F0N+aCgJCAhPQmD/cp/RC5A79WvUyhUHSIidAIoESv9VfAhW9n8+XqTCoyMsz1cviMMrGz9BrjAuboYHZajyXCInEocI8yvccbC+0muABanR4/tONjQz3DzgNKtj9sfv66XD9B/3tT9g/akb7h0bJwzxqqmlRHLr4rLPwBlYWoYj77l2AAAAAldEVYdGRhdGU6Y3JlYXRlADIwMTktMDUtMTVUMTY6MTQ6MzkrMDA6MDD5/4XBAAAAJXRFWHRkYXRlOm1vZGlmeQAyMDE5LTA1LTE1VDE2OjE0OjM5KzAwOjAwiKI9fQAAAABJRU5ErkJggg==";

const MESSAGES = () => [
  {
    template: "simple_snippet",
    template_version: "1.1.2",
    content: {
      text: "This is for <link0>preferences</link0> and <link1>about</link1>",
      icon:
        "https://snippets.cdn.mozilla.net/media/icons/1a8bb10e-8166-4e14-9e41-c1f85a41bcd2.png",
      button_label: "Button Label",
      section_title_icon:
        "https://snippets.cdn.mozilla.net/media/icons/5878847e-a1fb-4204-aad9-09f6cf7f99ee.png",
      section_title_text: "Messages from Firefox",
      section_title_url:
        "https://support.mozilla.org/kb/snippets-firefox-faq?utm_source=desktop-snippet&utm_medium=snippet&utm_campaign=&utm_term=&utm_content=",
      tall: false,
      block_button_text: "Remove this",
      do_not_autoblock: true,
      links: {
        link0: {
          action: "OPEN_PREFERENCES_PAGE",
          entrypoint_value: "snippet",
          args: "sync",
        },
        link1: {
          action: "OPEN_ABOUT_PAGE",
          args: "about",
          entrypoint_name: "entryPoint",
          entrypoint_value: "snippet",
        },
      },
      button_action: "OPEN_PREFERENCES_PAGE",
      button_entrypoint_value: "snippet",
    },
    id: "preview-13516_button_preferences",
  },
  {
    template: "simple_snippet",
    template_version: "1.1.2",
    content: {
      text: "This is for <link0>preferences</link0> and <link1>about</link1>",
      icon:
        "https://snippets.cdn.mozilla.net/media/icons/1a8bb10e-8166-4e14-9e41-c1f85a41bcd2.png",
      button_label: "Button Label",
      section_title_icon:
        "https://snippets.cdn.mozilla.net/media/icons/5878847e-a1fb-4204-aad9-09f6cf7f99ee.png",
      section_title_text: "Messages from Firefox",
      section_title_url:
        "https://support.mozilla.org/kb/snippets-firefox-faq?utm_source=desktop-snippet&utm_medium=snippet&utm_campaign=&utm_term=&utm_content=",
      tall: false,
      block_button_text: "Remove this",
      do_not_autoblock: true,
      links: {
        link0: {
          action: "OPEN_PREFERENCES_PAGE",
          entrypoint_value: "snippet",
        },
        link1: {
          action: "OPEN_ABOUT_PAGE",
          args: "about",
          entrypoint_name: "entryPoint",
          entrypoint_value: "snippet",
        },
      },
      button_action: "OPEN_ABOUT_PAGE",
      button_action_args: "logins",
      button_entrypoint_name: "entryPoint",
      button_entrypoint_value: "snippet",
    },
    id: "preview-13517_button_about",
  },
  {
    id: "SIMPLE_TEST_1",
    template: "simple_snippet",
    campaign: "test_campaign_blocking",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      title: "Firefox Account!",
      title_icon: TEST_ICON_16,
      title_icon_dark_theme: TEST_ICON_BW,
      text:
        "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_TEST_1_NO_DARK_THEME",
    template: "simple_snippet",
    campaign: "test_campaign_blocking",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: "",
      title: "Firefox Account!",
      title_icon: TEST_ICON_16,
      title_icon_dark_theme: "",
      text:
        "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_TEST_1_SAME_CAMPAIGN",
    template: "simple_snippet",
    campaign: "test_campaign_blocking",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      text:
        "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_TEST_TALL",
    template: "simple_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      text:
        "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      button_label: "Get one now!",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      block_button_text: "Block",
      tall: true,
    },
  },
  {
    id: "SIMPLE_TEST_BUTTON_URL_1",
    template: "simple_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Get one now!",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      text:
        "Sync it, link it, take it with you. All this and more with a Firefox Account.",
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_TEST_BUTTON_ACTION_1",
    template: "simple_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Open about:config",
      button_action: "OPEN_ABOUT_PAGE",
      button_action_args: "config",
      text: "Testing the OPEN_ABOUT_PAGE action",
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_WITH_TITLE_TEST_1",
    template: "simple_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      title: "Ready to sync?",
      text: "Get connected with a <syncLink>Firefox account</syncLink>.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "NEWSLETTER_TEST_DEFAULTS",
    template: "newsletter_snippet",
    content: {
      scene1_icon: TEST_ICON,
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_title: "Be a part of a movement.",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: TEST_ICON_BW,
      scene1_text:
        "Internet shutdowns, hackers, harassment &ndash; the health of the internet is on the line. Sign up and Mozilla will keep you updated on how you can help.",
      scene1_button_label: "Continue",
      scene1_button_color: "#712b00",
      scene1_button_background_color: "#ff9400",
      scene2_title: "Let's do this!",
      locale: "en-CA",
      scene2_dismiss_button_text: "Dismiss",
      scene2_text:
        "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      scene2_privacy_html:
        "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      scene2_newsletter: "mozilla-foundation",
      success_text: "Check your inbox for the confirmation!",
      error_text: "Error!",
      retry_button_label: "Try again?",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "NEWSLETTER_TEST_1",
    template: "newsletter_snippet",
    content: {
      scene1_icon: TEST_ICON,
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_title: "Be a part of a movement.",
      scene1_title_icon: "",
      scene1_text:
        "Internet shutdowns, hackers, harassment &ndash; the health of the internet is on the line. Sign up and Mozilla will keep you updated on how you can help.",
      scene1_button_label: "Continue",
      scene1_button_color: "#712b00",
      scene1_button_background_color: "#ff9400",
      scene2_title: "Let's do this!",
      locale: "en-CA",
      scene2_dismiss_button_text: "Dismiss",
      scene2_text:
        "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      scene2_privacy_html:
        "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      scene2_button_label: "Sign Me up",
      scene2_email_placeholder_text: "Your email here",
      scene2_newsletter: "mozilla-foundation",
      success_text: "Check your inbox for the confirmation!",
      error_text: "Error!",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "NEWSLETTER_TEST_SCENE1_SECTION_TITLE_ICON",
    template: "newsletter_snippet",
    content: {
      scene1_icon: TEST_ICON,
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_title: "Be a part of a movement.",
      scene1_title_icon: "",
      scene1_text:
        "Internet shutdowns, hackers, harassment &ndash; the health of the internet is on the line. Sign up and Mozilla will keep you updated on how you can help.",
      scene1_button_label: "Continue",
      scene1_button_color: "#712b00",
      scene1_button_background_color: "#ff9400",
      scene1_section_title_icon:
        "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
      scene1_section_title_text:
        "All the Firefox news that's fit to Firefox print!",
      scene2_title: "Let's do this!",
      locale: "en-CA",
      scene2_dismiss_button_text: "Dismiss",
      scene2_text:
        "Sign up for the Mozilla newsletter and we will keep you updated on how you can help.",
      scene2_privacy_html:
        "I'm okay with Mozilla handling my info as explained in this <privacyLink>Privacy Notice</privacyLink>.",
      scene2_button_label: "Sign Me up",
      scene2_email_placeholder_text: "Your email here",
      scene2_newsletter: "mozilla-foundation",
      success_text: "Check your inbox for the confirmation!",
      error_text: "Error!",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "FXA_SNIPPET_TEST_1",
    template: "fxa_signup_snippet",
    content: {
      scene1_icon: TEST_ICON,
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_button_label: "Get connected with sync!",
      scene1_button_color: "#712b00",
      scene1_button_background_color: "#ff9400",

      scene1_text:
        "Connect to Firefox by securely syncing passwords, bookmarks, and open tabs.",
      scene1_title: "Browser better.",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: TEST_ICON_BW,

      scene2_text:
        "Connect to your Firefox account to securely sync passwords, bookmarks, and open tabs.",
      scene2_title: "Title 123",
      scene2_email_placeholder_text: "Your email",
      scene2_button_label: "Continue",
      scene2_dismiss_button_text: "Dismiss",
    },
  },
  {
    id: "FXA_SNIPPET_TEST_TITLE_ICON",
    template: "fxa_signup_snippet",
    content: {
      scene1_icon: TEST_ICON,
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_button_label: "Get connected with sync!",
      scene1_button_color: "#712b00",
      scene1_button_background_color: "#ff9400",

      scene1_text:
        "Connect to Firefox by securely syncing passwords, bookmarks, and open tabs.",
      scene1_title: "Browser better.",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: TEST_ICON_BW,

      scene1_section_title_icon:
        "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
      scene1_section_title_text: "Firefox Accounts: Receivable benefits",

      scene2_text:
        "Connect to your Firefox account to securely sync passwords, bookmarks, and open tabs.",
      scene2_title: "Title 123",
      scene2_email_placeholder_text: "Your email",
      scene2_button_label: "Continue",
      scene2_dismiss_button_text: "Dismiss",
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
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_text: "Browse without compromise with Firefox Mobile.",
      scene1_title: "Full-featured. Customizable. Lightning fast",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: TEST_ICON_BW,

      scene2_button_label: "Send",
      scene2_disclaimer_html:
        "The intended recipient of the email must have consented. <privacyLink>Learn more</privacyLink>.",
      scene2_dismiss_button_text: "Dismiss",
      scene2_icon: TEST_ICON,
      scene2_icon_dark_theme: TEST_ICON_BW,
      scene2_input_placeholder: "Your email address or phone number",
      scene2_text:
        "Send Firefox to your phone and take a powerful independent browser with you.",
      scene2_title: "Let's do this!",

      error_text: "Oops, there was a problem.",
      success_title: "Your download link was sent.",
      success_text: "Check your device for the email message!",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "SNIPPETS_SEND_TO_DEVICE_TEST_NO_DARK_THEME",
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
      scene1_icon_dark_theme: "",
      scene1_text: "Browse without compromise with Firefox Mobile.",
      scene1_title: "Full-featured. Customizable. Lightning fast",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: "",

      scene2_button_label: "Send",
      scene2_disclaimer_html:
        "The intended recipient of the email must have consented. <privacyLink>Learn more</privacyLink>.",
      scene2_dismiss_button_text: "Dismiss",
      scene2_icon: TEST_ICON,
      scene2_icon_dark_theme: "",
      scene2_input_placeholder: "Your email address or phone number",
      scene2_text:
        "Send Firefox to your phone and take a powerful independent browser with you.",
      scene2_title: "Let's do this!",

      error_text: "Oops, there was a problem.",
      success_title: "Your download link was sent.",
      success_text: "Check your device for the email message!",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "SNIPPETS_SEND_TO_DEVICE_TEST_SECTION_TITLE_ICON",
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
      scene1_icon_dark_theme: TEST_ICON_BW,
      scene1_text: "Browse without compromise with Firefox Mobile.",
      scene1_title: "Full-featured. Customizable. Lightning fast",
      scene1_title_icon: TEST_ICON_16,
      scene1_title_icon_dark_theme: TEST_ICON_BW,
      scene1_section_title_icon:
        "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
      scene1_section_title_text: "Send Firefox to your mobile device!",

      scene2_button_label: "Send",
      scene2_disclaimer_html:
        "The intended recipient of the email must have consented. <privacyLink>Learn more</privacyLink>.",
      scene2_dismiss_button_text: "Dismiss",
      scene2_icon: TEST_ICON,
      scene2_icon_dark_theme: TEST_ICON_BW,
      scene2_input_placeholder: "Your email address or phone number",
      scene2_text:
        "Send Firefox to your phone and take a powerful independent browser with you.",
      scene2_title: "Let's do this!",

      error_text: "Oops, there was a problem.",
      success_title: "Your download link was sent.",
      success_text: "Check your device for the email message!",
      links: {
        privacyLink: {
          url:
            "https://www.mozilla.org/privacy/websites/?sample_rate=0.001&snippet_name=7894",
        },
      },
    },
  },
  {
    id: "EOY_TEST_1",
    template: "eoy_snippet",
    content: {
      highlight_color: "#f05",
      background_color: "#ddd",
      text_color: "yellow",
      selected_button: "donation_amount_first",
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Donate",
      monthly_checkbox_label_text: "Make my donation monthly",
      currency_code: "usd",
      donation_amount_first: 50,
      donation_amount_second: 25,
      donation_amount_third: 10,
      donation_amount_fourth: 5,
      donation_form_url:
        "https://donate.mozilla.org/pl/?utm_source=desktop-snippet&amp;utm_medium=snippet&amp;utm_campaign=donate&amp;utm_term=7556",
      text:
        "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
    },
  },
  {
    id: "EOY_BOLD_TEST_1",
    template: "eoy_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      selected_button: "donation_amount_second",
      button_label: "Donate",
      monthly_checkbox_label_text: "Make my donation monthly",
      currency_code: "usd",
      donation_amount_first: 50,
      donation_amount_second: 25,
      donation_amount_third: 10,
      donation_amount_fourth: 5,
      donation_form_url: "https://donate.mozilla.org",
      text:
        "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
      test: "bold",
    },
  },
  {
    id: "EOY_TAKEOVER_TEST_1",
    template: "eoy_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Donate",
      monthly_checkbox_label_text: "Make my donation monthly",
      currency_code: "usd",
      donation_amount_first: 50,
      donation_amount_second: 25,
      donation_amount_third: 10,
      donation_amount_fourth: 5,
      donation_form_url: "https://donate.mozilla.org",
      text:
        "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The <em>not-for-profit Mozilla Foundation</em> fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; <b>will you donate today</b>?",
      test: "takeover",
    },
  },
  {
    id: "SIMPLE_TEST_WITH_SECTION_HEADING",
    template: "simple_snippet",
    content: {
      button_label: "Get one now!",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      title: "Firefox Account!",
      text:
        "<syncLink>Sync it, link it, take it with you</syncLink>. All this and more with a Firefox Account.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
      section_title_icon:
        "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
      section_title_text: "Messages from Mozilla",
    },
  },
  {
    id: "SIMPLE_TEST_WITH_SECTION_HEADING_AND_LINK",
    template: "simple_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      title: "Firefox Account!",
      text:
        "Sync it, link it, take it with you. All this and more with a Firefox Account.",
      block_button_text: "Block",
      section_title_icon:
        "resource://activity-stream/data/content/assets/glyph-pocket-16.svg",
      section_title_text: "Messages from Mozilla (click for info)",
      section_title_url: "https://www.mozilla.org/about",
    },
  },
  {
    id: "SIMPLE_BELOW_SEARCH_TEST_1",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      text:
        "Securely store passwords, bookmarks, and more with a Firefox Account. <syncLink>Sign up</syncLink>",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_BELOW_SEARCH_TEST_2",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      text:
        "<syncLink>Connect your Firefox Account to Sync</syncLink> your protected passwords, open tabs and bookmarks, and they'll always be available to you - on all of your devices.",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SIMPLE_BELOW_SEARCH_TEST_TITLE",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      title: "See if you've been part of an online data breach.",
      text:
        "Securely store passwords, bookmarks, and more with a Firefox Account. <syncLink>Sign up</syncLink>",
      links: {
        syncLink: { url: "https://www.mozilla.org/en-US/firefox/accounts" },
      },
      block_button_text: "Block",
    },
  },
  {
    id: "SPECIAL_SNIPPET_BUTTON_1",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Find Out Now",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      title: "See if you've been part of an online data breach.",
      text: "Firefox Monitor tells you what hackers already know about you.",
      block_button_text: "Block",
    },
  },
  {
    id: "SPECIAL_SNIPPET_LONG_CONTENT",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Find Out Now",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      title: "See if you've been part of an online data breach.",
      text:
        "Firefox Monitor tells you what hackers already know about you. Here's some extra text to make the content really long.",
      block_button_text: "Block",
    },
  },
  {
    id: "SPECIAL_SNIPPET_NO_TITLE",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      icon_dark_theme: TEST_ICON_BW,
      button_label: "Find Out Now",
      button_url: "https://www.mozilla.org/en-US/firefox/accounts",
      text: "Firefox Monitor tells you what hackers already know about you.",
      block_button_text: "Block",
    },
  },
  {
    id: "SPECIAL_SNIPPET_MONITOR",
    template: "simple_below_search_snippet",
    content: {
      icon: TEST_ICON,
      title: "See if you've been part of an online data breach.",
      text: "Firefox Monitor tells you what hackers already know about you.",
      button_label: "Get monitor",
      button_action: "ENABLE_FIREFOX_MONITOR",
      button_action_args: {
        url:
          "https://monitor.firefox.com/oauth/init?utm_source=snippets&utm_campaign=monitor-snippet-test&form_type=email&entrypoint=newtab",
        flowRequestParams: {
          entrypoint: "snippets",
          utm_term: "monitor",
          form_type: "email",
        },
      },
      block_button_text: "Block",
    },
  },
];

const SnippetsTestMessageProvider = {
  getMessages() {
    return (
      MESSAGES()
        // Ensures we never actually show test except when triggered by debug tools
        .map(message => ({
          ...message,
          targeting: `providerCohorts.snippets_local_testing == "SHOW_TEST"`,
        }))
    );
  },
};
this.SnippetsTestMessageProvider = SnippetsTestMessageProvider;

const EXPORTED_SYMBOLS = ["SnippetsTestMessageProvider"];
