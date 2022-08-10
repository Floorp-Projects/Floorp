/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function setup_tests() {
  initBuiltInThemesStubs();
});

add_task(async function colorwaycloset_show_colorway() {
  await testInColorwayClosetModal(document => {
    is(
      document.documentElement.style.width,
      "",
      "In order for the modal layout to be responsive, the modal document " +
        "should not have a width set after the dialog frame has been set up"
    );

    const el = getColorwayClosetTestElements(document);
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
    is(el.colorwayFigure.src, MOCK_THEME_FIGURE_URL, "figure image is shown");
  });
});

add_task(async function colorwaycloset_custom_home_page() {
  await HomePage.set("https://www.example.com");
  await testInColorwayClosetModal(document => {
    const { homepageResetApplyButton } = getColorwayClosetTestElements(
      document
    );
    let v = getColorwayClosetElementVisibility(document);
    ok(
      v.homepageResetContainer.isVisible,
      "Homepage reset prompt should be shown"
    );
    ok(
      v.homepageResetSuccessMessage.isHidden,
      "Success message should not be shown"
    );
    ok(v.homepageResetUndoButton.isHidden, "Undo button should not be shown");
    ok(v.homepageResetMessage.isVisible, "Reset message should be shown");
    ok(v.homepageResetApplyButton.isVisible, "Apply button should be shown");

    homepageResetApplyButton.click();
    v = getColorwayClosetElementVisibility(document);

    ok(
      v.homepageResetSuccessMessage.isVisible,
      "Success message should be shown"
    );
    ok(v.homepageResetUndoButton.isVisible, "Undo button should be shown");
    ok(v.homepageResetMessage.isHidden, "Reset message should not be shown");
    ok(v.homepageResetApplyButton.isHidden, "Apply button should not be shown");
  });
});

add_task(async function colorwaycloset_default_home_page() {
  await HomePage.set(HomePage.getOriginalDefault());
  await testInColorwayClosetModal(document => {
    const { homepageResetContainer } = getColorwayClosetTestElements(document);
    ok(
      BrowserTestUtils.is_hidden(homepageResetContainer),
      "Homepage reset prompt should be hidden"
    );
  });
});
