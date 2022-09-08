/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { ColorwayClosetOpener } = ChromeUtils.import(
  "resource:///modules/ColorwayClosetOpener.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const MOCK_COLLECTION_TEST_CARD_IMAGE_PATH = "mockCollectionPreview.avif";
const MOCK_THEME_NAME = "Mock Theme";
const MOCK_THEME_NAME_2 = "Mock Theme 2";
const MOCK_THEME_DESCRIPTION = "Mock Theme Description";
const MOCK_THEME_DESCRIPTION_2 = "Mock Theme 2 Description";
const MOCK_THEME_FIGURE_URL = "https://www.example.com/figure.avif";

// fluent string ids
const EXPIRY_DATE_L10N_ID = "colorway-collection-expiry-label";
const MOCK_COLLECTION_L10N_TITLE = "mock-collection-l10n";
const MOCK_COLLECTION_L10N_DESCRIPTION = "mock-collection-l10n-description";
const MOCK_COLLECTION_L10N_SHORT_DESCRIPTION =
  "mock-collection-l10n-short-description";

// colorway theme ids
const SOFT_COLORWAY_THEME_ID = "mocktheme-soft-colorway@mozilla.org";
const BALANCED_COLORWAY_THEME_ID = "mocktheme-balanced-colorway@mozilla.org";
const BOLD_COLORWAY_THEME_ID = "mocktheme-bold-colorway@mozilla.org";
const SOFT_COLORWAY_THEME_ID_2 = "mocktheme2-soft-colorway@mozilla.org";
const BALANCED_COLORWAY_THEME_ID_2 = "mocktheme2-balanced-colorway@mozilla.org";
const BOLD_COLORWAY_THEME_ID_2 = "mocktheme2-bold-colorway@mozilla.org";
const NO_INTENSITY_COLORWAY_THEME_ID = "mocktheme-colorway@mozilla.org";
const NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID =
  "expiredmocktheme-colorway@mozilla.org";

// non-colorway theme ids
const MOCK_DARK_THEME_ID = "mocktheme-dark@mozilla.org";

// collections
const MOCK_COLLECTION_ID = "mock-collection";
const TEST_COLORWAY_COLLECTION = {
  id: MOCK_COLLECTION_ID,
  expiry: new Date("3000-01-01"),
  l10nId: {
    title: MOCK_COLLECTION_L10N_TITLE,
    description: MOCK_COLLECTION_L10N_DESCRIPTION,
  },
  cardImagePath: MOCK_COLLECTION_TEST_CARD_IMAGE_PATH,
};

function installTestTheme(id) {
  let name, description;
  if (
    [
      SOFT_COLORWAY_THEME_ID_2,
      BALANCED_COLORWAY_THEME_ID_2,
      BOLD_COLORWAY_THEME_ID_2,
    ].includes(id)
  ) {
    name = MOCK_THEME_NAME_2;
    description = MOCK_THEME_DESCRIPTION_2;
  } else {
    name = MOCK_THEME_NAME;
    description = MOCK_THEME_DESCRIPTION;
  }
  let xpi = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      name,
      description,
      applications: { gecko: { id } },
      theme:
        id === MOCK_DARK_THEME_ID
          ? { properties: { color_scheme: "dark" } }
          : {},
    },
  });
  return AddonTestUtils.promiseInstallFile(xpi);
}

/**
 * Creates stubs for BuiltInThemes
 * @param {Boolean} hasActiveCollection false to create an expired collection; has default value of true to create an active collection
 * @returns {Object} sinon sandbox containing all stubs for BuiltInThemes
 * @see BuiltInThemes
 */
function initBuiltInThemesStubs(hasActiveCollection = true) {
  info("Creating BuiltInThemes stubs");
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    info("Restoring BuiltInThemes sandbox for cleanup");
    sandbox.restore();
  });
  sandbox.stub(BuiltInThemes, "getLocalizedColorwayGroupName").callsFake(id => {
    if (
      [
        SOFT_COLORWAY_THEME_ID_2,
        BALANCED_COLORWAY_THEME_ID_2,
        BOLD_COLORWAY_THEME_ID_2,
      ].includes(id)
    ) {
      return MOCK_THEME_NAME_2;
    }

    return MOCK_THEME_NAME;
  });
  sandbox
    .stub(BuiltInThemes, "getLocalizedColorwayDescription")
    .callsFake(id => {
      if (
        [
          SOFT_COLORWAY_THEME_ID_2,
          BALANCED_COLORWAY_THEME_ID_2,
          BOLD_COLORWAY_THEME_ID_2,
        ].includes(id)
      ) {
        return MOCK_THEME_DESCRIPTION_2;
      }

      return MOCK_THEME_DESCRIPTION;
    });
  sandbox.stub(BuiltInThemes.builtInThemeMap, "get").callsFake(id => {
    let mockThemeProperties = {
      collection: MOCK_COLLECTION_ID,
      figureUrl: MOCK_THEME_FIGURE_URL,
    };
    if (id === NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID) {
      mockThemeProperties.expiry = new Date("1970-01-01");
    }
    return mockThemeProperties;
  });

  if (hasActiveCollection) {
    sandbox
      .stub(BuiltInThemes, "findActiveColorwayCollection")
      .returns(TEST_COLORWAY_COLLECTION);
  } else {
    sandbox.stub(BuiltInThemes, "findActiveColorwayCollection").returns(null);
  }

  sandbox
    .stub(BuiltInThemes, "isColorwayFromCurrentCollection")
    .callsFake(id =>
      [
        SOFT_COLORWAY_THEME_ID,
        BALANCED_COLORWAY_THEME_ID,
        BOLD_COLORWAY_THEME_ID,
        NO_INTENSITY_COLORWAY_THEME_ID,
        SOFT_COLORWAY_THEME_ID_2,
        BALANCED_COLORWAY_THEME_ID_2,
        BOLD_COLORWAY_THEME_ID_2,
      ].includes(id)
    );
  return sandbox.restore.bind(sandbox);
}

/**
 * Creates stubs for ColorwayClosetOpener
 * @returns {Object} sinon sandbox containing all stubs for ColorwayClosetOpener
 * @see ColorwayClosetOpener
 */
function initColorwayClosetOpenerStubs() {
  info("Creating ColorwayClosetOpener stubs");
  const sandbox = sinon.createSandbox();
  registerCleanupFunction(() => {
    info("Restoring ColorwayClosetOpener sandbox for cleanup");
    sandbox.restore();
  });
  sandbox.stub(ColorwayClosetOpener, "openModal").resolves({});
  return sandbox.restore.bind(sandbox);
}

function getColorwayClosetTestElements(document) {
  return {
    colorwayFigure: document.getElementById("colorway-figure"),
    collectionTitle: document.getElementById("collection-title"),
    expiryDateSpan: document.querySelector("#collection-expiry-date > span"),
    colorwaySelector: document.querySelector("#colorway-selector"),
    colorwayIntensities: document.querySelector("#colorway-intensity-radios"),
    setColorwayButton: document.getElementById("set-colorway"),
    cancelButton: document.getElementById("cancel"),
    colorwayName: document.querySelector("#colorway-name"),
    colorwayDescription: document.querySelector("#colorway-description"),
    homepageResetContainer: document.getElementById("homepage-reset-container"),
    homepageResetSuccessMessage: document.querySelector(
      "#homepage-reset-success > span"
    ),
    homepageResetUndoButton: document.querySelector(
      "#homepage-reset-success > button"
    ),
    homepageResetMessage: document.querySelector(
      "#homepage-reset-prompt > span"
    ),
    homepageResetApplyButton: document.querySelector(
      "#homepage-reset-prompt > button"
    ),
  };
}

function getColorwayClosetElementVisibility(document) {
  const elements = getColorwayClosetTestElements(document);
  let v = {};
  for (const k in elements) {
    const isVisible = BrowserTestUtils.is_visible(elements[k]);
    v[k] = {
      isVisible,
      isHidden: !isVisible,
    };
  }
  return v;
}

async function testInColorwayClosetModal(
  testMethod,
  themesToInstall = [
    SOFT_COLORWAY_THEME_ID,
    BALANCED_COLORWAY_THEME_ID,
    BOLD_COLORWAY_THEME_ID,
  ],
  themeToEnable = undefined
) {
  const clearBuiltInThemesStubs = initBuiltInThemesStubs();
  let themesToUninstall = [];
  for (let theme of themesToInstall) {
    info(`Installing ${theme}`);
    const { addon } = await installTestTheme(theme);
    // For some tests, we might want a colorway theme already enabled
    // before opening the modal. If there is such a theme, be sure
    // to enable it after installing all mock themes.
    if (themeToEnable && themeToEnable === theme) {
      info(`Enabling ${addon.id}`);
      await addon.enable();
    } else {
      info(`Disabling ${addon.id}`);
      await addon.disable();
    }
    themesToUninstall.push(addon);
  }

  const { closedPromise, dialog } = ColorwayClosetOpener.openModal();
  await dialog._dialogReady;
  const document = dialog._frame.contentDocument;
  let contentWindow = dialog._frame.contentWindow;
  try {
    await testMethod(document, contentWindow);
  } finally {
    // If the window is still open after the test, close it.
    contentWindow = dialog._frame.contentWindow;
    if (contentWindow && !contentWindow.closed) {
      info("Modal is still open. Closing modal before ending test.");
      document.getElementById("cancel").click();
    } else {
      info("Modal is already closed");
    }
    await closedPromise;
    for (let addon of themesToUninstall) {
      info(`Uninstalling theme ${addon.id}`);
      await addon.disable();
      await addon.uninstall();
    }
    clearBuiltInThemesStubs();
    // Clear any telemetry events recorded after closing the modal
    Services.telemetry.clearEvents();
  }
}

/**
 * Registers mock Fluent locale strings for colorway collections.
 */
async function registerMockCollectionL10nIds() {
  info("Register mock fluent locale strings");

  let tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
  tmpDir.append("l10n-colorwaycloset-mocks");

  await IOUtils.makeDirectory(tmpDir.path, { ignoreExisting: true });
  await IOUtils.writeUTF8(
    PathUtils.join(tmpDir.path, "mock-colorways.ftl"),
    [
      `${MOCK_COLLECTION_L10N_TITLE} = Mock collection title`,
      `${MOCK_COLLECTION_L10N_SHORT_DESCRIPTION} = Mock collection subheading`,
    ].join("\n")
  );

  let resProto = Services.io
    .getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);

  resProto.setSubstitution(
    "l10n-colorwaycloset-mocks",
    Services.io.newFileURI(tmpDir)
  );

  let mockSource = new L10nFileSource(
    "colorwayscloset-mocks",
    "app",
    ["en-US"],
    "resource://l10n-colorwaycloset-mocks/"
  );

  let l10nReg = L10nRegistry.getInstance();
  l10nReg.registerSources([mockSource]);

  registerCleanupFunction(async () => {
    l10nReg.removeSources([mockSource]);
    resProto.setSubstitution("l10n-colorwaycloset-mocks", null);
    info(`Clearing temporary directory ${tmpDir.path}`);
    await IOUtils.remove(tmpDir.path, { recursive: true, ignoreAbsent: true });
  });

  // Confirm that the mock fluent resources are available as expected.
  let bundles = l10nReg.generateBundles(["en-US"], ["mock-colorways.ftl"]);
  let bundle0 = (await bundles.next()).value;
  is(
    bundle0.locales[0],
    "en-US",
    "Got the expected locale in the mock L10nFileSource"
  );
  ok(
    bundle0.hasMessage(MOCK_COLLECTION_L10N_TITLE),
    "Got the expected l10n id in the mock L10nFileSource"
  );
}

/**
 * Waits for an addon to be enabled.
 * @param {String} addonId the string id of an addon
 * @returns {Promise} resolved promise when the the specified addon is enabled
 */
function waitForAddonEnabled(addonId) {
  return new Promise(resolve => {
    let listener = {
      onEnabled(enabledAddon) {
        if (enabledAddon.id == addonId) {
          AddonManager.removeAddonListener(listener);
          info(`Addon ${addonId} enabled. Removing listener and resolving.`);
          resolve();
        }
      },
    };
    info(`Adding onEnabled listener for "${addonId}`);
    AddonManager.addAddonListener(listener);
  });
}

/**
 * Waits for a Colorways telemetry event to trigger.
 * @returns {Promise} promise from BrowserTestUtils.waitForCondition
 */
async function waitForColorwaysTelemetryPromise() {
  return BrowserTestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    ).parent;
    let colorwayEvents = events.filter(e => e[1] === "colorways_modal");
    return colorwayEvents && colorwayEvents.length;
  }, "Waiting for Colorways events ping");
}
