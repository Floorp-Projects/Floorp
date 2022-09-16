"use strict";

const BASE_CONTENT = {
  id: "MOBILE_DOWNLOADS",
  content: {
    tiles: {
      type: "mobile_downloads",
      data: {
        QR_code: {
          image_url: "chrome://browser/content/assets/focus-qr-code.svg",
          alt_text: "Test alt",
        },
        email: {
          link_text: {
            string_id: "spotlight-focus-promo-email-link",
          },
        },
        marketplace_buttons: ["ios", "android"],
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
add_task(async function test_aboutwelcome_mobile_downloads_all() {
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
add_task(
  async function test_aboutwelcome_mobile_downloads_qr_and_marketplace() {
    const SCREEN_CONTENT = structuredClone(BASE_CONTENT);
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
  }
);

/**
 * Test rendering a screen with a mobile downloads tile
 * including only a QR code
 */
add_task(async function test_aboutwelcome_mobile_downloads_qr() {
  let SCREEN_CONTENT = structuredClone(BASE_CONTENT);
  const QR_CODE_SRC = SCREEN_CONTENT.content.tiles.data.QR_code.image_url;

  delete SCREEN_CONTENT.content.tiles.data.email;
  delete SCREEN_CONTENT.content.tiles.data.marketplace_buttons;
  const TEST_JSON = JSON.stringify([SCREEN_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with QR code",
    // Expected selectors:
    [`img.qr-code-image[alt="${ALT_TEXT}"][src="${QR_CODE_SRC}"]`],
    // Unexpected selectors:
    ["button.email-link", "li.android", "li.ios"]
  );
});
