/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the selected colorway is still enabled after pressing the Set Colorway button
 * in the modal.
 */
add_task(async function colorwaycloset_modal_set_colorway() {
  await testInColorwayClosetModal(async (document, contentWindow) => {
    // Select colorway on modal
    info("Selecting colorway radio button");
    const {
      colorwayIntensities,
      setColorwayButton,
    } = getColorwayClosetTestElements(document);

    // Select new intensity
    // Wait for intensity button to be initialized
    await BrowserTestUtils.waitForMutationCondition(
      colorwayIntensities,
      { subtree: true, attributeFilter: ["value"] },
      () =>
        colorwayIntensities.querySelector(
          `input[value="${SOFT_COLORWAY_THEME_ID}"]`
        ),
      "Waiting for intensity button to be available"
    );
    let intensitiesChangedPromise = BrowserTestUtils.waitForEvent(
      colorwayIntensities,
      "change",
      "Waiting for intensities change event"
    );
    let themeChangedPromise = waitForAddonEnabled(SOFT_COLORWAY_THEME_ID);

    info("Selecting new intensity");
    colorwayIntensities
      .querySelector(`input[value="${SOFT_COLORWAY_THEME_ID}"]`)
      .click();
    await intensitiesChangedPromise;
    await themeChangedPromise;

    // Set colorway
    await BrowserTestUtils.waitForMutationCondition(
      setColorwayButton,
      { childList: true, attributeFilter: ["disabled"] },
      () => !setColorwayButton.disabled,
      "Waiting for set-colorway button to be available for selection"
    );
    let modalClosedPromise = BrowserTestUtils.waitForEvent(
      contentWindow,
      "unload",
      "Waiting for modal to close"
    );

    info("Selecting set colorway button");
    setColorwayButton.click();
    info("Closing modal");
    await modalClosedPromise;

    // Verify theme selection is saved
    const activeTheme = Services.prefs.getStringPref(
      "extensions.activeThemeID"
    );
    is(
      activeTheme,
      SOFT_COLORWAY_THEME_ID,
      "Current theme is still selected colorway"
    );
  });
});

/**
 * Tests that the selected colorway is not enabled after pressing the Cancel Button.
 * Theme should be reverted to previous option.
 */
add_task(async function colorwaycloset_modal_cancel() {
  const previousTheme = Services.prefs.getStringPref(
    "extensions.activeThemeID"
  );
  info(`Previous theme is ${previousTheme}`);

  await testInColorwayClosetModal(
    async (document, contentWindow) => {
      // Wait for colorway to load before checking modal UI
      await waitForAddonEnabled(NO_INTENSITY_COLORWAY_THEME_ID);

      // Cancel colorway selection
      const { cancelButton } = getColorwayClosetTestElements(document);
      let themeRevertedPromise = waitForAddonEnabled(previousTheme);
      let modalClosedPromise = BrowserTestUtils.waitForEvent(
        contentWindow,
        "unload",
        "Waiting for modal to close"
      );
      info("Selecting cancel button");
      cancelButton.click();

      info("Verifying that previous theme is restored");
      await themeRevertedPromise;
      info("Closing modal");
      await modalClosedPromise;
    },
    [NO_INTENSITY_COLORWAY_THEME_ID]
  );
});

/**
 * Tests that the Firefox homepage apply and undo options are visible
 * on the modal if a non-default setting for homepage is enabled.
 */
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

/**
 * Tests that the Firefox homepage apply and undo options are not visible
 * on the modal if the default setting for homepage is enabled.
 */
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
