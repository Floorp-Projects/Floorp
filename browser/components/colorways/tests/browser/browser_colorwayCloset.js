/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");
const { BuiltInThemes } = ChromeUtils.import(
  "resource:///modules/BuiltInThemes.jsm"
);
const { ColorwayClosetOpener } = ChromeUtils.import(
  "resource:///modules/ColorwayClosetOpener.jsm"
);

const TEST_COLORWAY_COLLECTION = {
  id: "independent-voices",
  expiry: new Date("3000-01-01"),
  l10nId: {
    title: "colorway-collection-independent-voices",
    description: "colorway-collection-independent-voices-description",
  },
};

const EXPIRY_DATE_L10N_ID = "colorway-collection-expiry-date-span";
const TEST_FIGURE_URL = "https://www.example.com/figure.avif";

function getTestElements(document) {
  return {
    colorwayFigure: document.getElementById("colorway-figure"),
    collectionTitle: document.getElementById("collection-title"),
    expiryDateSpan: document.querySelector("#collection-expiry-date > span"),
    useFXHomeControls: document.getElementById("use-fx-home-controls"),
    useFXHomeSuccessMessage: document.querySelector(
      "#use-fx-home-controls > .success-prompt > span"
    ),
    useFXHomeSuccessUndoButton: document.querySelector(
      "#use-fx-home-controls > .success-prompt > button"
    ),
    useFXHomeResetMessage: document.querySelector(
      "#use-fx-home-controls > .reset-prompt > span"
    ),
    useFXHomeResetApplyButton: document.querySelector(
      "#use-fx-home-controls > .reset-prompt > button"
    ),
  };
}

function getElementVisibility(document) {
  const elements = getTestElements(document);
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

async function testInColorwayClosetModal(testMethod) {
  const { closedPromise, dialog } = ColorwayClosetOpener.openModal();
  await dialog._dialogReady;
  const document = dialog._frame.contentDocument;
  try {
    testMethod(document);
  } finally {
    document.getElementById("cancel").click();
    await closedPromise;
  }
}

add_setup(async function setup_tests() {
  const sandbox = sinon.createSandbox();
  sandbox
    .stub(BuiltInThemes, "findActiveColorwayCollection")
    .returns(TEST_COLORWAY_COLLECTION);
  sandbox.stub(BuiltInThemes, "isColorwayFromCurrentCollection").returns(true);
  sandbox.stub(BuiltInThemes.builtInThemeMap, "get").returns({
    figureUrl: TEST_FIGURE_URL,
  });
  registerCleanupFunction(() => {
    sandbox.restore();
  });
});

add_task(async function colorwaycloset_show_colorway() {
  await testInColorwayClosetModal(document => {
    const el = getTestElements(document);
    const expiryL10nAttributes = document.l10n.getAttributes(el.expiryDateSpan);
    is(
      document.l10n.getAttributes(el.collectionTitle).id,
      TEST_COLORWAY_COLLECTION.l10nId.title,
      "Correct collection title should be shown"
    );
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
    is(el.colorwayFigure.src, TEST_FIGURE_URL, "figure image is shown");
  });
});

add_task(async function colorwaycloset_custom_home_page() {
  await HomePage.set("https://www.example.com");
  await testInColorwayClosetModal(document => {
    const { useFXHomeResetApplyButton } = getTestElements(document);
    let v = getElementVisibility(document);
    ok(
      v.useFXHomeControls.isVisible,
      '"Use Firefox home" controls should be shown'
    );
    ok(
      v.useFXHomeSuccessMessage.isHidden,
      "Success message should not be shown"
    );
    ok(
      v.useFXHomeSuccessUndoButton.isHidden,
      "Undo button should not be shown"
    );
    ok(v.useFXHomeResetMessage.isVisible, "Reset message should be shown");
    ok(v.useFXHomeResetApplyButton.isVisible, "Apply button should be shown");

    useFXHomeResetApplyButton.click();
    v = getElementVisibility(document);

    ok(v.useFXHomeSuccessMessage.isVisible, "Success message should be shown");
    ok(v.useFXHomeSuccessUndoButton.isVisible, "Undo button should be shown");
    ok(v.useFXHomeResetMessage.isHidden, "Reset message should not be shown");
    ok(
      v.useFXHomeResetApplyButton.isHidden,
      "Apply button should not be shown"
    );
  });
});

add_task(async function colorwaycloset_default_home_page() {
  await HomePage.set(HomePage.getOriginalDefault());
  await testInColorwayClosetModal(document => {
    const { useFXHomeControls } = getTestElements(document);
    ok(
      BrowserTestUtils.is_hidden(useFXHomeControls),
      '"Use Firefox home" controls should be hidden'
    );
  });
});
