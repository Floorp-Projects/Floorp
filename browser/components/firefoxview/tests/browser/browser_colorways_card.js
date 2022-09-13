/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);
const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

const TEST_COLLECTION_FIGURE_URL = "https://www.example.com/collection.avif";
const TEST_COLORWAY_FIGURE_URL = "https://www.example.com/colorway.avif";

const TEST_COLORWAY_COLLECTION = {
  id: "independent-voices",
  expiry: new Date("3000-01-01"),
  l10nId: {
    title: "colorway-collection-independent-voices",
    description: "colorway-collection-independent-voices-description",
  },
  figureUrl: TEST_COLLECTION_FIGURE_URL,
};

const SOFT_COLORWAY_THEME_ID = "mocktheme-soft-colorway@mozilla.org";
const BALANCED_COLORWAY_THEME_ID = "mocktheme-balanced-colorway@mozilla.org";
const BOLD_COLORWAY_THEME_ID = "mocktheme-bold-colorway@mozilla.org";
const NO_INTENSITY_COLORWAY_THEME_ID = "mocktheme-colorway@mozilla.org";
const OUTDATED_COLORWAY_THEME_ID = "outdatedtheme-colorway@mozilla.org";

const EXPIRY_DATE_L10N_ID = "colorway-collection-expiry-label";
const COLORWAY_DESCRIPTION_L10N_ID = "firefoxview-colorway-description";
const MOCK_THEME_L10N_VALUE = "Mock Theme";
const SOFT_L10N_VALUE = "Soft";

const TRY_COLORWAYS_EVENT = [
  ["colorways_modal", "try_colorways", "firefoxview", undefined],
];

const CHANGE_COLORWAY_EVENT = [
  ["colorways_modal", "change_colorway", "firefoxview", undefined],
];

function getTestElements(document) {
  return {
    container: document.getElementById("colorways"),
    title: document.getElementById("colorways-collection-title"),
    description: document.getElementById("colorways-collection-description"),
    expiryPill: document.querySelector("#colorways-collection-expiry-date"),
    expiry: document.querySelector("#colorways-collection-expiry-date > span"),
    figure: document.getElementById("colorways-collection-figure"),
    noCollectionNotice: document.getElementById(
      "no-current-colorway-collection-notice"
    ),
  };
}

async function createTempTheme(id) {
  const xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name: "Monochromatic Theme",
      applications: { gecko: { id } },
      theme: {},
    },
  });
  return AddonTestUtils.promiseInstallFile(xpi);
}

let gCollectionEnabled = true;

// TODO: use Colorway Closet mocks and helper functions (Bug 1783675)

add_setup(async function setup_tests() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(BuiltInThemes, "findActiveColorwayCollection")
    .callsFake(() => (gCollectionEnabled ? TEST_COLORWAY_COLLECTION : null));
  sandbox
    .stub(BuiltInThemes, "isColorwayFromCurrentCollection")
    .callsFake(
      id =>
        id === SOFT_COLORWAY_THEME_ID ||
        id === BALANCED_COLORWAY_THEME_ID ||
        id === BOLD_COLORWAY_THEME_ID ||
        id === NO_INTENSITY_COLORWAY_THEME_ID
    );
  sandbox
    .stub(BuiltInThemes, "getLocalizedColorwayGroupName")
    .returns(MOCK_THEME_L10N_VALUE);
  sandbox.stub(BuiltInThemes.builtInThemeMap, "get").returns({
    figureUrl: TEST_COLORWAY_FIGURE_URL,
  });
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", true]],
  });
  const tempThemes = await Promise.all(
    [
      SOFT_COLORWAY_THEME_ID,
      BALANCED_COLORWAY_THEME_ID,
      BOLD_COLORWAY_THEME_ID,
      NO_INTENSITY_COLORWAY_THEME_ID,
      OUTDATED_COLORWAY_THEME_ID,
    ].map(createTempTheme)
  );
  registerCleanupFunction(async () => {
    sandbox.restore();
    await SpecialPowers.popPrefEnv();
    for (const { addon } of tempThemes) {
      await addon.disable();
      await addon.uninstall(true);
    }
  });
});

add_task(async function no_collection_test() {
  gCollectionEnabled = false;
  try {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const { noCollectionNotice, description } = getTestElements(document);
        ok(
          BrowserTestUtils.is_visible(noCollectionNotice),
          "No Active Colorway Collection Notice should be visible"
        );
        is(description, null, "Colorway description should be hidden");
      }
    );
  } finally {
    gCollectionEnabled = true;
  }
});

add_task(async function colorway_closet_disabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.theme.colorway-closet", false]],
  });
  try {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const { noCollectionNotice, description } = getTestElements(document);
        ok(
          BrowserTestUtils.is_visible(noCollectionNotice),
          "No Active Colorway Collection Notice should be visible when Colorway Closet is disabled"
        );
        is(
          description,
          null,
          "Colorway description should be hidden when Colorway Closet is disabled"
        );
      }
    );
  } finally {
    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function no_active_colorway_test() {
  // Set to default theme to unapply any enabled colorways
  const theme = await AddonManager.getAddonByID("default-theme@mozilla.org");
  await theme.enable();
  try {
    await clearAllParentTelemetryEvents();
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Colorway description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLLECTION_FIGURE_URL,
          "Collection figure should be shown"
        );
        is(
          document.l10n.getAttributes(el.title).id,
          TEST_COLORWAY_COLLECTION.l10nId.title,
          "Collection title should be shown"
        );
        is(
          document.l10n.getAttributes(el.description).id,
          TEST_COLORWAY_COLLECTION.l10nId.description,
          "Collection description should be shown"
        );
        ok(!el.expiryPill.hidden, "Expiry pill is shown");
        const expiryL10nAttributes = document.l10n.getAttributes(el.expiry);
        is(
          expiryL10nAttributes.args.expiryDate,
          TEST_COLORWAY_COLLECTION.expiry.getTime(),
          "Correct expiry date should be shown"
        );
        is(
          expiryL10nAttributes.id,
          EXPIRY_DATE_L10N_ID,
          "Correct expiry date format should be shown"
        );

        document.querySelector("#colorways-button").click();

        await TestUtils.waitForCondition(
          () => {
            let events = Services.telemetry.snapshotEvents(
              Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
              false
            ).parent;
            let colorwayEvents = events.filter(e => e[1] === "colorways_modal");
            return colorwayEvents && colorwayEvents.length;
          },
          "Waiting for try_colorways colorways telemetry event.",
          200,
          100
        );

        let events = Services.telemetry.snapshotEvents(
          Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
          false
        ).parent;
        let colorwayEvents = events.filter(e => e[1] === "colorways_modal");

        info(JSON.stringify(colorwayEvents));

        TelemetryTestUtils.assertEvents(
          TRY_COLORWAYS_EVENT,
          { category: "colorways_modal" },
          { clear: true, process: "parent" }
        );
      }
    );
  } finally {
    await theme.disable();
  }
});

add_task(async function active_colorway_test() {
  const theme = await AddonManager.getAddonByID(SOFT_COLORWAY_THEME_ID);
  await theme.enable();
  try {
    await clearAllParentTelemetryEvents();
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Colorway description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLORWAY_FIGURE_URL,
          "Colorway figure should be shown"
        );
        is(
          el.title.textContent,
          MOCK_THEME_L10N_VALUE,
          "Colorway title should be shown"
        );
        const descriptionL10nAttributes = document.l10n.getAttributes(
          el.description
        );
        is(
          descriptionL10nAttributes.id,
          COLORWAY_DESCRIPTION_L10N_ID,
          "Colorway description should be shown"
        );
        is(
          descriptionL10nAttributes.args.intensity,
          SOFT_L10N_VALUE,
          "Colorway intensity should be shown"
        );
        is(
          descriptionL10nAttributes.args.collection,
          "Independent Voices",
          "Collection name should be shown"
        );
        ok(el.expiryPill.hidden, "Expiry pill is hidden");

        document.querySelector("#colorways-button").click();

        await TestUtils.waitForCondition(
          () => {
            let events = Services.telemetry.snapshotEvents(
              Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
              false
            ).parent;
            let colorwayEvents = events.filter(e => e[1] === "colorways_modal");
            return colorwayEvents && colorwayEvents.length;
          },
          "Waiting for change_colorway colorways telemetry event.",
          200,
          100
        );

        TelemetryTestUtils.assertEvents(
          CHANGE_COLORWAY_EVENT,
          { category: "colorways_modal" },
          { clear: true, process: "parent" }
        );
      }
    );
  } finally {
    await theme.disable();
  }
});

add_task(async function active_colorway_without_intensity_test() {
  const theme = await AddonManager.getAddonByID(NO_INTENSITY_COLORWAY_THEME_ID);
  await theme.enable();
  try {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Colorway description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLORWAY_FIGURE_URL,
          "Colorway figure should be shown"
        );
        is(
          el.title.textContent,
          MOCK_THEME_L10N_VALUE,
          "Colorway title should be shown"
        );
        is(
          document.l10n.getAttributes(el.description).id,
          TEST_COLORWAY_COLLECTION.l10nId.title,
          "Collection name should be shown as the description"
        );
        ok(el.expiryPill.hidden, "Expiry pill is hidden");
      }
    );
  } finally {
    await theme.disable();
  }
});

add_task(async function active_colorway_is_outdated_test() {
  const theme = await AddonManager.getAddonByID(OUTDATED_COLORWAY_THEME_ID);
  await theme.enable();
  try {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        const { document } = browser.contentWindow;
        const el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLLECTION_FIGURE_URL,
          "Collection figure should be shown"
        );
        is(
          document.l10n.getAttributes(el.title).id,
          TEST_COLORWAY_COLLECTION.l10nId.title,
          "Collection title should be shown"
        );
        is(
          document.l10n.getAttributes(el.description).id,
          TEST_COLORWAY_COLLECTION.l10nId.description,
          "Collection description should be shown"
        );
        ok(!el.expiryPill.hidden, "Expiry pill is shown");
        const expiryL10nAttributes = document.l10n.getAttributes(el.expiry);
        is(
          expiryL10nAttributes.args.expiryDate,
          TEST_COLORWAY_COLLECTION.expiry.getTime(),
          "Correct expiry date should be shown"
        );
        is(
          expiryL10nAttributes.id,
          EXPIRY_DATE_L10N_ID,
          "Correct expiry date format should be shown"
        );
      }
    );
  } finally {
    await theme.disable();
  }
});

add_task(async function change_active_colorway_test() {
  let theme = await AddonManager.getAddonByID(NO_INTENSITY_COLORWAY_THEME_ID);
  await theme.enable();
  try {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:firefoxview",
      },
      async browser => {
        info("Start with no intensity theme");
        const { document } = browser.contentWindow;
        let el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Colorway description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLORWAY_FIGURE_URL,
          "Colorway figure should be shown"
        );
        is(
          el.title.textContent,
          MOCK_THEME_L10N_VALUE,
          "Colorway title should be shown"
        );
        info("Revert to default theme");
        await theme.disable();
        el = getTestElements(document);
        is(
          el.noCollectionNotice,
          null,
          "No Active Colorway Collection Notice should be hidden"
        );
        ok(
          BrowserTestUtils.is_visible(el.description),
          "Colorway description should be visible"
        );
        is(
          el.figure.src,
          TEST_COLLECTION_FIGURE_URL,
          "Collection figure should be shown"
        );
        is(
          document.l10n.getAttributes(el.title).id,
          TEST_COLORWAY_COLLECTION.l10nId.title,
          "Collection title should be shown"
        );
        is(
          document.l10n.getAttributes(el.description).id,
          TEST_COLORWAY_COLLECTION.l10nId.description,
          "Collection description should be shown"
        );
        info("Enable a different theme");
        theme = await AddonManager.getAddonByID(SOFT_COLORWAY_THEME_ID);
        await theme.enable();
        is(
          el.title.textContent,
          MOCK_THEME_L10N_VALUE,
          "Colorway title should be shown"
        );
        const descriptionL10nAttributes = document.l10n.getAttributes(
          el.description
        );
        is(
          descriptionL10nAttributes.id,
          COLORWAY_DESCRIPTION_L10N_ID,
          "Colorway description should be shown"
        );
        is(
          descriptionL10nAttributes.args.intensity,
          SOFT_L10N_VALUE,
          "Colorway intensity should be shown"
        );
        is(
          descriptionL10nAttributes.args.collection,
          "Independent Voices",
          "Collection name should be shown"
        );
      }
    );
  } finally {
    await theme.disable();
  }
});
