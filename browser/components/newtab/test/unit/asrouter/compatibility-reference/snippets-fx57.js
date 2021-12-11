/**
 * IMPORTANT NOTE!!!
 *
 * Please DO NOT introduce breaking changes file without contacting snippets endpoint engineers
 * and changing the schema version to reflect a breaking change.
 *
 */

const DATA_URI_IMAGE =
  "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";

export const expectedValues = {
  // Simple Snippet (https://github.com/mozmeao/snippets/blob/master/activity-stream/simple-snippet.html)
  simple_snippet: {
    icon: DATA_URI_IMAGE,
    button_label: "Click me",
    button_url: "https://mozilla.org",
    button_background_color: "#FF0000",
    button_color: "#FFFFFF",
    text: "Hello world",
    title: "Hi!",
    title_icon: DATA_URI_IMAGE,
    tall: true,
  },

  // FXA Snippet (https://github.com/mozmeao/snippets/blob/master/activity-stream/fxa.html)
  fxa_signup_snippet: {
    scene1_icon: DATA_URI_IMAGE,
    scene1_button_label: "Click me",
    scene1_button_background_color: "#FF0000",
    scene1_button_color: "#FFFFFF",
    scene1_text: "Hello <em>world</em>",
    scene1_title: "Hi!",
    scene1_title_icon: DATA_URI_IMAGE,

    scene2_text: "Second scene",
    scene2_title: "Second scene title",
    scene2_email_placeholder_text: "Email here",
    scene2_button_label: "Sign Me Up",
    scene2_dismiss_button_text: "Dismiss",

    utm_campaign: "snippets123",
    utm_term: "123term",
  },

  // Send To Device Snippet (https://github.com/mozmeao/snippets/blob/master/activity-stream/send-to-device.html)
  send_to_device_snippet: {
    include_sms: true,
    locale: "de",
    country: "DE",
    message_id_sms: "foo",
    message_id_email: "foo",
    scene1_button_background_color: "#FF0000",
    scene1_button_color: "#FFFFFF",
    scene1_button_label: "Click me",
    scene1_icon: DATA_URI_IMAGE,
    scene1_text: "Hello world",
    scene1_title: "Hi!",
    scene1_title_icon: DATA_URI_IMAGE,

    scene2_button_label: "Sign Me Up",
    scene2_disclaimer_html: "Hello <em>world</em>",
    scene2_dismiss_button_text: "Dismiss",
    scene2_icon: DATA_URI_IMAGE,
    scene2_input_placeholder: "Email here",

    scene2_text: "Second scene",
    scene2_title: "Second scene title",

    error_text: "error",
    success_text: "all good",
    success_title: "Ok!",
  },

  // Newsletter Snippet (https://github.com/mozmeao/snippets/blob/master/activity-stream/newsletter-subscribe.html)
  newsletter_snippet: {
    scene1_icon: DATA_URI_IMAGE,
    scene1_button_label: "Click me",
    scene1_button_background_color: "#FF0000",
    scene1_button_color: "#FFFFFF",
    scene1_text: "Hello world",
    scene1_title: "Hi!",
    scene1_title_icon: DATA_URI_IMAGE,

    scene2_text: "Second scene",
    scene2_title: "Second scene title",
    scene2_newsletter: "foo",
    scene2_email_placeholder_text: "Email here",
    scene2_button_label: "Sign Me Up",
    scene2_privacy_html: "Hello <em>world</em>",
    scene2_dismiss_button_text: "Dismiss",

    locale: "de",

    error_text: "error",
    success_text: "all good",
  },

  // EOY Snippet (https://github.com/mozmeao/snippets/blob/master/activity-stream/mofo-eoy-2017.html)
  eoy_snippet: {
    block_button_text: "Block",

    donation_form_url: "https://donate.mozilla.org/",
    text:
      "Big corporations want to restrict how we access the web. Fake news is making it harder for us to find the truth. Online bullies are silencing inspired voices. The not-for-profit Mozilla Foundation fights for a healthy internet with programs like our Tech Policy Fellowships and Internet Health Report; will you donate today?",
    icon: DATA_URI_IMAGE,
    button_label: "Donate",
    monthly_checkbox_label_text: "Make my donation monthly",
    button_background_color: "#0060DF",
    button_color: "#FFFFFF",
    background_color: "#FFFFFF",
    text_color: "#000000",
    highlight_color: "#FFE900",

    locale: "en-US",
    currency_code: "usd",

    donation_amount_first: 50,
    donation_amount_second: 25,
    donation_amount_third: 10,
    donation_amount_fourth: 3,
    selected_button: "donation_amount_second",

    test: "bold",
  },
};
