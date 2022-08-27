/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

add_setup(async function setup_tests() {
  Services.telemetry.clearEvents();
});

/**
 * Tests that telemetry is registered when the Cancel button is selected on the Colorway Closet modal.
 */
add_task(async function colorwaycloset_modal_cancel() {
  await testInColorwayClosetModal(async (document, contentWindow) => {
    Services.telemetry.clearEvents();
    registerCleanupFunction(() => {
      Services.telemetry.clearEvents();
    });

    const { cancelButton } = getColorwayClosetTestElements(document);
    let modalClosedPromise = BrowserTestUtils.waitForEvent(
      contentWindow,
      "unload",
      "Waiting for modal to close"
    );

    cancelButton.click();
    info("Closing modal");
    await modalClosedPromise;
    await waitForColorwaysTelemetryPromise();

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "colorways_modal",
          method: "cancel",
          object: "modal",
        },
      ],
      { category: "colorways_modal", object: "modal" }
    );
    Services.telemetry.clearEvents();
  });
});

/**
 * Tests that telemetry is registered when the Set Colorway button is selected on the Colorway Closet modal.
 */
add_task(async function colorwaycloset_modal_set_colorway() {
  await testInColorwayClosetModal(async (document, contentWindow) => {
    Services.telemetry.clearEvents();
    registerCleanupFunction(() => {
      Services.telemetry.clearEvents();
    });

    // Select colorway on modal
    info("Selecting colorway radio button");
    const {
      colorwaySelector,
      colorwayIntensities,
      setColorwayButton,
    } = getColorwayClosetTestElements(document);
    const colorwayFamilyButton = colorwaySelector.querySelector(
      `input[value="${BALANCED_COLORWAY_THEME_ID}"]`
    );
    colorwayFamilyButton.click();

    // Select new intensity
    info("Selecting new intensity button");
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
    let themeChangedPromise = BrowserTestUtils.waitForCondition(() => {
      const activeTheme = Services.prefs.getStringPref(
        "extensions.activeThemeID"
      );
      return activeTheme === SOFT_COLORWAY_THEME_ID;
    }, "Waiting for the current theme to change after new intensity");

    colorwayIntensities
      .querySelector(`input[value="${SOFT_COLORWAY_THEME_ID}"]`)
      .click();
    await intensitiesChangedPromise;
    await themeChangedPromise;

    // Set colorway
    info("Selecting set colorway button");
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

    setColorwayButton.click();
    await waitForColorwaysTelemetryPromise();
    info("Closing modal");
    await modalClosedPromise;

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "colorways_modal",
          method: "set_colorway",
          object: "modal",
          value: null,
          extra: {
            colorway_id: SOFT_COLORWAY_THEME_ID,
          },
        },
      ],
      { category: "colorways_modal", object: "modal" }
    );

    Services.telemetry.clearEvents();
  });
});

/**
 * Tests that telemetry is registered when the Set Colorway button is selected on the Colorway Closet modal,
 * but for a selected colorway theme that does not have an intensity.
 */
add_task(async function colorwaycloset_modal_set_colorway_no_intensity() {
  await testInColorwayClosetModal(
    async (document, contentWindow) => {
      Services.telemetry.clearEvents();
      registerCleanupFunction(() => {
        Services.telemetry.clearEvents();
      });
      const {
        colorwaySelector,
        setColorwayButton,
      } = getColorwayClosetTestElements(document);

      // Select colorway on modal
      info("Selecting colorway radio button");
      const colorwayFamilyButton = colorwaySelector.querySelector(
        `input[value="${NO_INTENSITY_COLORWAY_THEME_ID}"]`
      );
      let themeChangedPromise = BrowserTestUtils.waitForCondition(() => {
        const activeTheme = Services.prefs.getStringPref(
          "extensions.activeThemeID"
        );
        return activeTheme === NO_INTENSITY_COLORWAY_THEME_ID;
      }, "Waiting for the current theme to change after new intensity");
      colorwayFamilyButton.click();
      await themeChangedPromise;

      // Set colorway
      info("Selecting set colorway button");
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

      setColorwayButton.click();
      await waitForColorwaysTelemetryPromise();
      info("Closing modal");
      await modalClosedPromise;

      TelemetryTestUtils.assertEvents(
        [
          {
            category: "colorways_modal",
            method: "set_colorway",
            object: "modal",
            value: null,
            extra: {
              colorway_id: NO_INTENSITY_COLORWAY_THEME_ID,
            },
          },
        ],
        { category: "colorways_modal", object: "modal" }
      );

      Services.telemetry.clearEvents();
    },
    [NO_INTENSITY_COLORWAY_THEME_ID]
  );
});

/**
 * Tests that telemetry is registered when the Firefox Home apply and undo buttons are selected on the Colorway Closet modal.
 */
add_task(async function colorwaycloset_modal_firefox_home() {
  // Set homepage to NOT Firefox Home so that banner appears on the modal
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.homepage", "about:blank"]],
  });
  await testInColorwayClosetModal(async document => {
    Services.telemetry.clearEvents();
    registerCleanupFunction(() => {
      Services.telemetry.clearEvents();
    });

    // Ensure Firefox Home banner is visible first on the modal
    const {
      homepageResetContainer,
      homepageResetApplyButton,
      homepageResetUndoButton,
    } = getColorwayClosetTestElements(document);

    ok(homepageResetContainer, "Firefox Home banner is visible on the modal");
    ok(homepageResetApplyButton, "Firefox Home Apply button should be visible");

    homepageResetApplyButton.click();
    await waitForColorwaysTelemetryPromise();

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "colorways_modal",
          method: "homepage_reset",
          object: "modal",
        },
      ],
      { category: "colorways_modal", object: "modal" }
    );

    ok(homepageResetUndoButton, "Firefox Home Undo button should be visible");

    homepageResetUndoButton.click();
    await waitForColorwaysTelemetryPromise();

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "colorways_modal",
          method: "homepage_reset_undo",
          object: "modal",
        },
      ],
      { category: "colorways_modal", object: "modal" }
    );

    Services.telemetry.clearEvents();
  });
});

/**
 * Tests that there is an event telemetry object "unknown" when no source is defined upon
 * opening the modal.
 */
add_task(async function colorwaycloset_modal_unknown_source() {
  await testInColorwayClosetModal(async document => {
    // Since we already open the modal in testInColorwayClosetModal,
    // do not clear telemetry events until after we verify
    // the event with "unknown" source.
    registerCleanupFunction(() => {
      Services.telemetry.clearEvents();
    });

    await waitForColorwaysTelemetryPromise();
    TelemetryTestUtils.assertNumberOfEvents(1, {
      category: "colorways_modal",
      object: "unknown",
    });

    Services.telemetry.clearEvents();
  });
});
