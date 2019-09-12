/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.modify.boolean", true],
      ["test.aboutconfig.modify.number", 1337],
      [
        "test.aboutconfig.modify.string",
        "the answer to the life the universe and everything",
      ],
    ],
  });

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(PREF_BOOLEAN_DEFAULT_TRUE);
    Services.prefs.clearUserPref(PREF_NUMBER_DEFAULT_ZERO);
    Services.prefs.clearUserPref(PREF_STRING_DEFAULT_EMPTY);
  });
});

add_task(async function test_add_user_pref() {
  Assert.equal(
    Services.prefs.getPrefType(PREF_NEW),
    Ci.nsIPrefBranch.PREF_INVALID
  );

  await AboutConfigTest.withNewTab(async function() {
    // The row for a new preference appears when searching for its name.
    Assert.ok(!this.getRow(PREF_NEW));
    this.search(PREF_NEW);
    let row = this.getRow(PREF_NEW);
    Assert.ok(row.hasClass("deleted"));

    for (let [radioIndex, expectedValue, expectedEditingMode] of [
      [0, true, false],
      [1, 0, true],
      [2, "", true],
    ]) {
      // Adding the preference should set the default for the data type.
      row.element.querySelectorAll("input")[radioIndex].click();
      row.editColumnButton.click();
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(Preferences.get(PREF_NEW) === expectedValue);

      // Number and String preferences should be in edit mode.
      Assert.equal(!!row.valueInput, expectedEditingMode);

      // Repeat the search to verify that the preference remains.
      this.search(PREF_NEW);
      row = this.getRow(PREF_NEW);
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(!row.valueInput);

      // Reset the preference, then continue by adding a different type.
      row.resetColumnButton.click();
      Assert.equal(
        Services.prefs.getPrefType(PREF_NEW),
        Ci.nsIPrefBranch.PREF_INVALID
      );
    }
  });
});

add_task(async function test_delete_user_pref() {
  for (let [radioIndex, testValue] of [[0, false], [1, -1], [2, "value"]]) {
    Preferences.set(PREF_NEW, testValue);
    await AboutConfigTest.withNewTab(async function() {
      // Deleting the preference should keep the row.
      let row = this.getRow(PREF_NEW);
      row.resetColumnButton.click();
      Assert.ok(row.hasClass("deleted"));
      Assert.equal(
        Services.prefs.getPrefType(PREF_NEW),
        Ci.nsIPrefBranch.PREF_INVALID
      );

      // Re-adding the preference should keep the same value.
      Assert.ok(row.element.querySelectorAll("input")[radioIndex].checked);
      row.editColumnButton.click();
      Assert.ok(!row.hasClass("deleted"));
      Assert.ok(Preferences.get(PREF_NEW) === testValue);

      // Filtering again after deleting should remove the row.
      row.resetColumnButton.click();
      this.showAll();
      Assert.ok(!this.getRow(PREF_NEW));
    });
  }
});

add_task(async function test_click_type_label_multiple_forms() {
  // This test displays the row to add a preference while other preferences are
  // also displayed, and tries to select the type of the new preference by
  // clicking the label next to the radio button. This should work even if the
  // user has deleted a different preference, and multiple forms are displayed.
  const PREF_TO_DELETE = "test.aboutconfig.modify.boolean";
  const PREF_NEW_WHILE_DELETED = "test.aboutconfig.modify.";

  await AboutConfigTest.withNewTab(async function() {
    this.search(PREF_NEW_WHILE_DELETED);

    // This preference will remain deleted during the test.
    let existingRow = this.getRow(PREF_TO_DELETE);
    existingRow.resetColumnButton.click();

    let newRow = this.getRow(PREF_NEW_WHILE_DELETED);

    for (let [radioIndex, expectedValue] of [[0, true], [1, 0], [2, ""]]) {
      let radioLabels = newRow.element.querySelectorAll("label > span");
      await this.document.l10n.translateElements(radioLabels);

      // Even if this is the second form on the page, the click should select
      // the radio button next to the label, not the one on the first form.
      await BrowserTestUtils.synthesizeMouseAtCenter(
        radioLabels[radioIndex],
        {},
        this.browser
      );

      // Adding the preference should set the default for the data type.
      newRow.editColumnButton.click();
      Assert.ok(Preferences.get(PREF_NEW_WHILE_DELETED) === expectedValue);

      // Reset the preference, then continue by adding a different type.
      newRow.resetColumnButton.click();
    }

    // Re-adding the deleted preference should restore the value.
    existingRow.editColumnButton.click();
    Assert.ok(Preferences.get(PREF_TO_DELETE) === true);
  });
});

add_task(async function test_reset_user_pref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_BOOLEAN_DEFAULT_TRUE, false],
      [PREF_STRING_LOCALIZED_MISSING, "user-value"],
    ],
  });

  await AboutConfigTest.withNewTab(async function() {
    // Click reset.
    let row = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    row.resetColumnButton.click();
    // Check new layout and reset.
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.ok(!Services.prefs.prefHasUserValue(PREF_BOOLEAN_DEFAULT_TRUE));
    Assert.equal(this.getRow(PREF_BOOLEAN_DEFAULT_TRUE).value, "true");

    // Filter again to test the preference cache.
    this.showAll();
    row = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.equal(this.getRow(PREF_BOOLEAN_DEFAULT_TRUE).value, "true");

    // Clicking reset on a localized preference without a corresponding value.
    row = this.getRow(PREF_STRING_LOCALIZED_MISSING);
    Assert.equal(row.value, "user-value");
    row.resetColumnButton.click();
    // Check new layout and reset.
    Assert.ok(!row.hasClass("has-user-value"));
    Assert.ok(!row.resetColumnButton);
    Assert.ok(!Services.prefs.prefHasUserValue(PREF_STRING_LOCALIZED_MISSING));
    Assert.equal(this.getRow(PREF_STRING_LOCALIZED_MISSING).value, "");
  });
});

add_task(async function test_modify() {
  await AboutConfigTest.withNewTab(async function() {
    // Test toggle for boolean prefs.
    for (let nameOfBoolPref of [
      "test.aboutconfig.modify.boolean",
      PREF_BOOLEAN_DEFAULT_TRUE,
    ]) {
      let row = this.getRow(nameOfBoolPref);
      // Do this a two times to reset the pref.
      for (let i = 0; i < 2; i++) {
        row.editColumnButton.click();
        // Check new layout and saving in backend.
        Assert.equal(
          this.getRow(nameOfBoolPref).value,
          "" + Preferences.get(nameOfBoolPref)
        );
        let prefHasUserValue = Services.prefs.prefHasUserValue(nameOfBoolPref);
        Assert.equal(row.hasClass("has-user-value"), prefHasUserValue);
        Assert.equal(!!row.resetColumnButton, prefHasUserValue);
      }
    }

    // Test abort of edit by starting with string and continuing with editing Int pref.
    let row = this.getRow("test.aboutconfig.modify.string");
    row.editColumnButton.click();
    row.valueInput.value = "test";
    let intRow = this.getRow("test.aboutconfig.modify.number");
    intRow.editColumnButton.click();
    Assert.equal(
      intRow.valueInput.value,
      Preferences.get("test.aboutconfig.modify.number")
    );
    Assert.ok(!row.valueInput);
    Assert.equal(row.value, Preferences.get("test.aboutconfig.modify.string"));

    // Test validation of integer values.
    for (let invalidValue of [
      "",
      " ",
      "a",
      "1.5",
      "-2147483649",
      "2147483648",
    ]) {
      intRow.valueInput.value = invalidValue;
      intRow.editColumnButton.click();
      // We should still be in edit mode.
      Assert.ok(intRow.valueInput);
    }

    // Test correct saving and DOM-update.
    for (let [prefName, willDelete] of [
      ["test.aboutconfig.modify.string", true],
      ["test.aboutconfig.modify.number", true],
      [PREF_NUMBER_DEFAULT_ZERO, false],
      [PREF_STRING_DEFAULT_EMPTY, false],
    ]) {
      row = this.getRow(prefName);
      // Activate edit and check displaying.
      row.editColumnButton.click();
      Assert.equal(row.valueInput.value, Preferences.get(prefName));
      row.valueInput.value = "42";
      // Save and check saving.
      row.editColumnButton.click();
      Assert.equal(Preferences.get(prefName), "42");
      Assert.equal(row.value, "42");
      Assert.ok(row.hasClass("has-user-value"));
      // Reset or delete the preference while editing.
      row.editColumnButton.click();
      Assert.equal(row.valueInput.value, Preferences.get(prefName));
      row.resetColumnButton.click();
      Assert.ok(!row.hasClass("has-user-value"));
      Assert.equal(row.hasClass("deleted"), willDelete);
    }
  });
});
