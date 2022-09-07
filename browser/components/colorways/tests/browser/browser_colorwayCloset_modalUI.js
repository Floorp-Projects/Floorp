/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that all colorway details such as the collection title, collection expiry date,
 * and colorway figure are displayed on the modal.
 */
add_task(async function colorwaycloset_show_colorway() {
  await testInColorwayClosetModal(async document => {
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

    info("Verifying figure src");
    await BrowserTestUtils.waitForMutationCondition(
      el.colorwayFigure,
      { childList: true, attributeFilter: ["src"] },
      () => el.colorwayFigure.src === MOCK_THEME_FIGURE_URL,
      "Waiting for figure image to have expected URL"
    );
  });
});

/**
 * Tests that modal details and theme are updated when a new colorway radio button is selected.
 */
add_task(async function colorwaycloset_modal_select_colorway() {
  await testInColorwayClosetModal(
    async document => {
      const {
        colorwaySelector,
        colorwayName,
        colorwayDescription,
        colorwayIntensities,
      } = getColorwayClosetTestElements(document);

      is(
        colorwaySelector.children.length,
        2,
        "There should be two colorway radio buttons"
      );

      // Select colorway on modal
      const colorwayGroupButton1 = colorwaySelector.querySelector(
        `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
      );
      let selectorChangedPromise = BrowserTestUtils.waitForEvent(
        colorwaySelector,
        "change",
        "Waiting for radio button change event"
      );
      let themeChangedPromise = waitForAddonEnabled(BALANCED_COLORWAY_THEME_ID);

      info("Selecting colorway radio button");
      colorwayGroupButton1.click();
      info("Waiting for radio button change event to resolve");
      await selectorChangedPromise;
      info("Waiting for theme to change");
      await themeChangedPromise;

      // Verify values of first colorway
      is(colorwayName.textContent, MOCK_THEME_NAME, "Theme name is correct");
      is(
        colorwayDescription.textContent,
        MOCK_THEME_DESCRIPTION,
        "Theme description is correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${SOFT_COLORWAY_THEME_ID}"]`
        ),
        "Soft intensity should be correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ),
        "Balanced intensity should be correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BOLD_COLORWAY_THEME_ID}"]`
        ),
        "Bold intensity should be correct"
      );

      // Select a new colorway
      const colorwayGroupButton2 = colorwaySelector.querySelector(
        `input[value="${BALANCED_COLORWAY_THEME_ID_2}"]`
      );
      selectorChangedPromise = BrowserTestUtils.waitForEvent(
        colorwaySelector,
        "change",
        "Waiting for radio button change event"
      );
      themeChangedPromise = waitForAddonEnabled(BALANCED_COLORWAY_THEME_ID_2);

      info("Selecting another colorway radio button");
      colorwayGroupButton2.click();
      info("Waiting for radio button change event to resolve");
      await selectorChangedPromise;
      info("Waiting for theme to change");
      await themeChangedPromise;

      // Verify values of new colorway
      is(colorwayName.textContent, MOCK_THEME_NAME_2, "Theme name is updated");
      is(
        colorwayDescription.textContent,
        MOCK_THEME_DESCRIPTION_2,
        "Theme description is updated"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${SOFT_COLORWAY_THEME_ID_2}"]`
        ),
        "Soft intensity should be updated"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID_2}"]`
        ),
        "Balanced intensity should be updated"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BOLD_COLORWAY_THEME_ID_2}"]`
        ),
        "Bold intensity should be updated"
      );
    },
    [
      SOFT_COLORWAY_THEME_ID,
      BALANCED_COLORWAY_THEME_ID,
      BOLD_COLORWAY_THEME_ID,
      SOFT_COLORWAY_THEME_ID_2,
      BALANCED_COLORWAY_THEME_ID_2,
      BOLD_COLORWAY_THEME_ID_2,
    ]
  );
});

/**
 * Tests that modal details and theme are updated when a new colorway radio button is selected,
 * but when the colorway has no intensity options.
 */
add_task(async function colorwaycloset_modal_select_colorway_no_intensity() {
  await testInColorwayClosetModal(
    async document => {
      // Select colorway on modal
      const {
        colorwaySelector,
        colorwayName,
        colorwayDescription,
        colorwayIntensities,
      } = getColorwayClosetTestElements(document);

      is(
        colorwaySelector.children.length,
        2,
        "There should be two colorway radio buttons"
      );

      const colorwayGroupButton1 = colorwaySelector.querySelector(
        `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
      );
      let selectorChangedPromise = BrowserTestUtils.waitForEvent(
        colorwaySelector,
        "change",
        "Waiting for radio button change event"
      );
      let themeChangedPromise = waitForAddonEnabled(BALANCED_COLORWAY_THEME_ID);

      info("Selecting colorway radio button");
      colorwayGroupButton1.click();
      info("Waiting for radio button change event to resolve");
      await selectorChangedPromise;
      info("Waiting for theme to change");
      await themeChangedPromise;

      // Verify values of first colorway
      is(colorwayName.textContent, MOCK_THEME_NAME, "Theme name is correct");
      is(
        colorwayDescription.textContent,
        MOCK_THEME_DESCRIPTION,
        "Theme description is correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${SOFT_COLORWAY_THEME_ID}"]`
        ),
        "Soft intensity should be correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ),
        "Balanced intensity should be correct"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BOLD_COLORWAY_THEME_ID}"]`
        ),
        "Bold intensity should be correct"
      );

      // Select a new colorway
      const colorwayGroupButton2 = colorwaySelector.querySelector(
        `input[value="${NO_INTENSITY_COLORWAY_THEME_ID}"]`
      );
      selectorChangedPromise = BrowserTestUtils.waitForEvent(
        colorwaySelector,
        "change",
        "Waiting for radio button change event"
      );
      themeChangedPromise = waitForAddonEnabled(NO_INTENSITY_COLORWAY_THEME_ID);

      info("Selecting another colorway radio button but with no intensity");
      colorwayGroupButton2.click();
      info("Waiting for radio button change event to resolve");
      await selectorChangedPromise;
      info("Waiting for theme to change");
      await themeChangedPromise;

      // Verify there are no intensities
      info("Verifying intensity button is not visible");
      let v = getColorwayClosetElementVisibility(document);
      ok(
        !v.colorwayIntensities.isVisible,
        "Colorway intensities should not be shown"
      );
    },
    [
      SOFT_COLORWAY_THEME_ID,
      BALANCED_COLORWAY_THEME_ID,
      BOLD_COLORWAY_THEME_ID,
      NO_INTENSITY_COLORWAY_THEME_ID,
    ]
  );
});

/**
 * Tests that an active colorway in the current collection is selected and displayed when
 * opening the modal.
 */
add_task(async function colorwaycloset_modal_show_active_colorway() {
  await testInColorwayClosetModal(
    async document => {
      const {
        colorwaySelector,
        colorwayIntensities,
      } = getColorwayClosetTestElements(document);
      info(
        "Verify that the correct colorway family button is checked by default"
      );
      ok(
        colorwaySelector.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ).checked,
        "Colorway group button should be checked by default"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${SOFT_COLORWAY_THEME_ID}"]`
        ).checked,
        "Soft intensity should be checked by default"
      );
    },
    undefined,
    SOFT_COLORWAY_THEME_ID
  );
});

/**
 * Tests that a colorway available in the active collection is displayed in the
 * colorway closet modal if the current theme is an expired colorway.
 */
add_task(async function colorwaycloset_modal_expired_colorway() {
  const previousTheme = Services.prefs.getStringPref(
    "extensions.activeThemeID"
  );
  info(`Previous theme is ${previousTheme}`);

  await testInColorwayClosetModal(
    async document => {
      const {
        colorwaySelector,
        colorwayIntensities,
      } = getColorwayClosetTestElements(document);

      // Wait for colorway to load before checking modal UI
      await waitForAddonEnabled(BALANCED_COLORWAY_THEME_ID);

      ok(
        colorwaySelector.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ).checked,
        "Colorway group button should be checked by default"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ).checked,
        "Correct intensity should be checked by default"
      );
    },
    [
      SOFT_COLORWAY_THEME_ID,
      BALANCED_COLORWAY_THEME_ID,
      BOLD_COLORWAY_THEME_ID,
      NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID,
    ],
    NO_INTENSITY_EXPIRED_COLORWAY_THEME_ID
  );
});

/**
 * Tests that bold intensity is checked by default upon the opening the modal
 * if the current theme has a dark scheme.
 */
add_task(async function colorwaycloset_modal_dark_scheme() {
  await testInColorwayClosetModal(
    async document => {
      const {
        colorwaySelector,
        colorwayIntensities,
      } = getColorwayClosetTestElements(document);

      const previousTheme = Services.prefs.getStringPref(
        "extensions.activeThemeID"
      );
      info(`Previous theme is ${previousTheme}`);

      // Wait for theme to load before checking modal UI
      await waitForAddonEnabled(BOLD_COLORWAY_THEME_ID);

      ok(
        colorwaySelector.querySelector(
          `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
        ).checked,
        "Colorway group button should be checked by default"
      );
      ok(
        colorwayIntensities.querySelector(
          `input[value="${BOLD_COLORWAY_THEME_ID}"]`
        ).checked,
        "Bold intensity should be checked by default"
      );
    },
    [
      SOFT_COLORWAY_THEME_ID,
      BALANCED_COLORWAY_THEME_ID,
      BOLD_COLORWAY_THEME_ID,
      MOCK_DARK_THEME_ID,
    ],
    MOCK_DARK_THEME_ID
  );
});
