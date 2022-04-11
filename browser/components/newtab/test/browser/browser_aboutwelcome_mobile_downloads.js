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

function setLocale(locale) {
  Services.locale.availableLocales = [locale];
  Services.locale.requestedLocales = [locale];
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

/**
 * Test rendering QR code image override for a locale
 */
add_task(async function test_aboutwelcome_localized_qr_override() {
  const TEST_LOCALE = "de";
  setLocale(TEST_LOCALE);

  const DE_QR_CODE_SRC = "chrome://browser/content/assets/klar-qr-code.svg";
  let SCREEN_CONTENT = structuredClone(BASE_CONTENT);
  SCREEN_CONTENT.content.tiles.data.QR_code.image_overrides = {
    de: DE_QR_CODE_SRC,
  };

  const TEST_JSON = JSON.stringify([SCREEN_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with localized QR code",
    // Expected selectors:
    [`img.qr-code-image[src="${DE_QR_CODE_SRC}"]`]
  );

  setLocale("en-US"); // revert locale change
});

/**
 * Test rendering localized app marketplace buttons for supported locale
 */
add_task(async function test_aboutwelcome_localized_marketplace_buttons() {
  const TEST_LOCALE = "es-ES";
  setLocale(TEST_LOCALE);

  const TEST_JSON = JSON.stringify([BASE_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_element_styles(
    browser,
    "li.ios button",
    // Expected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/es/ios.svg")`,
    },
    // Unexpected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/en/ios.svg")`,
    }
  );

  await test_element_styles(
    browser,
    "li.android button",
    // Expected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/es/android.png")`,
    },
    // Unexpected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/en/android.png")`,
    }
  );

  setLocale("en-US"); // revert locale change
});

/**
 * Test rendering default en-US marketplace buttons for unsupported locale
 */
add_task(async function test_aboutwelcome_fallback_marketplace_buttons() {
  const TEST_LOCALE = "mt-MT";
  setLocale(TEST_LOCALE);

  const FALLBACK_LANGUAGE = "en";
  const MISSING_LANGUAGE = "mt";
  const TEST_JSON = JSON.stringify([BASE_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_element_styles(
    browser,
    "li.ios button",
    // Expected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/${FALLBACK_LANGUAGE}/ios.svg")`,
    },
    // Unexpected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/${MISSING_LANGUAGE}/ios.svg")`,
    }
  );

  await test_element_styles(
    browser,
    "li.android button",
    // Expected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/${FALLBACK_LANGUAGE}/android.png")`,
    },
    // Unexpected styles:
    {
      "background-image": `url("chrome://activity-stream/content/data/content/assets/app-marketplace-icons/${MISSING_LANGUAGE}/android.png")`,
    }
  );

  setLocale("en-US"); // revert locale change
});
