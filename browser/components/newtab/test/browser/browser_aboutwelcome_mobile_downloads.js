"use strict";

const BASE_CONTENT = {
  id: "MOBILE_DOWNLOADS",
  order: 0,
  content: {
    tiles: {
      type: "mobile_downloads",
      data: {
        QR_code: {
          image_url:
            "chrome://browser/content/preferences/more-from-mozilla-qr-code-simple.svg",
          alt_text: "Test alt",
        },
        email: {
          link_text: {
            string_id: "spotlight-focus-promo-email-link",
          },
        },
        marketplace_buttons: {},
      },
    },
  },
};

async function openAboutWelcome(json) {
  if (json) {
    await setAboutWelcomeMultiStage(json);
  }

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:welcome",
    true
  );
  registerCleanupFunction(() => {
    BrowserTestUtils.removeTab(tab);
  });
  return tab.linkedBrowser;
}

const ALT_TEXT = BASE_CONTENT.content.tiles.data.QR_code.alt_text;

/**
 * Test rendering a screen with a mobile downloads tile
 * including QR code, email, and marketplace elements
 */
add_task(async function test_aboutwelcome_mobile_downloads() {
  const TEST_JSON = JSON.stringify([BASE_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with all mobile download elements",
    // Expected selectors:
    [
      `img.qr-code-image[alt="${ALT_TEXT}"]`,
      "ul.mobile-download-buttons",
      "li.android",
      "li.ios",
      "button.email-link",
    ]
  );
});

/**
 * Test rendering a screen with a mobile downloads tile
 * including only a QR code and marketplace elements
 */
add_task(async function test_aboutwelcome_mobile_downloads() {
  const SCREEN_CONTENT = Object.assign({}, BASE_CONTENT);
  delete SCREEN_CONTENT.content.tiles.data.email;
  const TEST_JSON = JSON.stringify([SCREEN_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with QR code and marketplace badges",
    // Expected selectors:
    [
      `img.qr-code-image[alt="${ALT_TEXT}"]`,
      "ul.mobile-download-buttons",
      "li.android",
      "li.ios",
    ],
    // Unexpected selectors:
    [`button.email-link`]
  );
});

/**
 * Test rendering a screen with a mobile downloads tile
 * including only a QR code
 */
add_task(async function test_aboutwelcome_mobile_downloads() {
  let SCREEN_CONTENT = Object.assign({}, BASE_CONTENT);
  delete SCREEN_CONTENT.content.tiles.data.email;
  delete SCREEN_CONTENT.content.tiles.data.marketplace_buttons;
  const TEST_JSON = JSON.stringify([SCREEN_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with QR code",
    // Expected selectors:
    [`img.qr-code-image[alt="${ALT_TEXT}"]`],
    // Unexpected selectors:
    ["button.email-link", "li.android", "li.ios"]
  );
});
