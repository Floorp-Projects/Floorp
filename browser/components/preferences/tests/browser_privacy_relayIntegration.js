/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function initialState() {
  // check pref permutations to verify the UI opens in the correct state
  const prefTests = [
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", undefined],
        ["signon.rememberSignons", true],
      ],
      expected: "hidden",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "available"],
        ["signon.rememberSignons", true],
      ],
      expected: "checked",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "enabled"],
        ["signon.rememberSignons", true],
      ],
      expected: "checked",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "disabled"],
        ["signon.rememberSignons", true],
      ],
      expected: "unchecked",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", undefined],
        ["signon.rememberSignons", false],
      ],
      expected: "hidden",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "available"],
        ["signon.rememberSignons", false],
      ],
      expected: "checked",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "enabled"],
        ["signon.rememberSignons", false],
      ],
      expected: "checked",
    },
    {
      initialPrefs: [
        ["signon.firefoxRelay.feature", "disabled"],
        ["signon.rememberSignons", false],
      ],
      expected: "unchecked",
    },
  ];
  for (let test of prefTests) {
    // set initial pref values
    info("initialState, testing with: " + JSON.stringify(test));
    await SpecialPowers.pushPrefEnv({ set: test.initialPrefs });

    // open about:privacy in a tab
    // verify expected conditions
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: "about:preferences#privacy",
      },
      async function (browser) {
        const doc = browser.contentDocument;
        const relayGroup = doc.getElementById("relayIntegrationBox");
        const checkbox = doc.getElementById("relayIntegration");
        const savePasswords = doc.getElementById("savePasswords");
        doc.getElementById("passwordSettings").scrollIntoView();

        Assert.equal(
          checkbox.disabled,
          !savePasswords.checked,
          "#relayIntegration checkbox disabled when #passwordAutofillCheckbox is unchecked"
        );

        switch (test.expected) {
          case "hidden":
            is_element_hidden(relayGroup, "#relayIntegrationBox is hidden");
            break;
          case "checked":
            is_element_visible(relayGroup, "#relayIntegrationBox is visible");
            Assert.ok(
              checkbox.checked,
              "#relayIntegration checkbox is checked"
            );
            break;
          case "unchecked":
            is_element_visible(relayGroup, "#relayIntegrationBox is visible");
            Assert.ok(
              !checkbox.checked,
              "#relayIntegration checkbox is un-checked"
            );
            break;
          default:
            Assert.ok(false, "Unknown expected state: " + test.expected);
            break;
        }
      }
    );
    await SpecialPowers.popPrefEnv();
  }
});

add_task(async function toggleRelayIntegration() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["signon.firefoxRelay.feature", "enabled"],
      ["signon.rememberSignons", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:preferences#privacy",
    },
    async browser => {
      await SimpleTest.promiseFocus(browser);

      // the preferences "Search" bar obscures the checkbox if we scrollIntoView and try to click on it
      // so use keyboard events instead
      const doc = browser.contentDocument;
      const relayCheckbox = doc.getElementById("relayIntegration");
      relayCheckbox.focus();
      Assert.equal(doc.activeElement, relayCheckbox, "checkbox is focused");
      Assert.equal(
        relayCheckbox.checked,
        true,
        "#relayIntegration checkbox is not checked"
      );

      async function clickOnFeatureCheckbox(
        expectedPrefValue,
        expectedCheckValue,
        message
      ) {
        const prefChanged = TestUtils.waitForPrefChange(
          "signon.firefoxRelay.feature"
        );
        EventUtils.synthesizeKey(" ");
        await prefChanged;
        Assert.equal(
          Services.prefs.getStringPref("signon.firefoxRelay.feature"),
          expectedPrefValue,
          message
        );
        Assert.equal(
          relayCheckbox.checked,
          expectedCheckValue,
          `#relayIntegration checkbox is ${
            expectedCheckValue ? "checked" : "unchecked"
          }`
        );
      }

      await clickOnFeatureCheckbox(
        "disabled",
        false,
        'Turn integration off from "enabled" feature state'
      );
      await clickOnFeatureCheckbox(
        "available",
        true,
        'Turn integration on from "enabled" feature state'
      );
      await clickOnFeatureCheckbox(
        "disabled",
        false,
        'Turn integration off from "enabled" feature state'
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function toggleRememberSignon() {
  // toggling rememberSignons checkbox should make generation checkbox disabled
  SpecialPowers.pushPrefEnv({
    set: [
      ["signon.firefoxRelay.feature", "available"],
      ["signon.rememberSignons", true],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:preferences#privacy",
    },
    async function (browser) {
      const doc = browser.contentDocument;
      const checkbox = doc.getElementById("savePasswords");
      const relayCheckbox = doc.getElementById("relayIntegration");

      Assert.ok(
        !relayCheckbox.disabled,
        "generation checkbox is not initially disabled"
      );

      await SimpleTest.promiseFocus(browser);
      const prefChanged = TestUtils.waitForPrefChange("signon.rememberSignons");

      // the preferences "Search" bar obscures the checkbox if we scrollIntoView and try to click on it
      // so use keyboard events instead
      checkbox.focus();
      Assert.equal(doc.activeElement, checkbox, "checkbox is focused");
      EventUtils.synthesizeKey(" ");

      await prefChanged;
      Assert.ok(!checkbox.checked, "#savePasswords checkbox is un-checked");
      Assert.ok(
        relayCheckbox.disabled,
        "Relay integration checkbox becomes disabled"
      );
    }
  );
  await SpecialPowers.popPrefEnv();
});

add_task(async function testLockedRelayPreference() {
  // Locking relay preference should disable checkbox
  Services.prefs.lockPref("signon.firefoxRelay.feature");

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:preferences#privacy",
    },
    async function (browser) {
      const doc = browser.contentDocument;
      const relayCheckbox = doc.getElementById("relayIntegration");

      Assert.ok(relayCheckbox.disabled, "Relay checkbox should be disabled");
    }
  );

  Services.prefs.unlockPref("signon.firefoxRelay.feature");
});
