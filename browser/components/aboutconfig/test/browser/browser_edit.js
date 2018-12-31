/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.modify.boolean", true],
      ["test.aboutconfig.modify.number", 1337],
      ["test.aboutconfig.modify.string", "the answer to the life the universe and everything"],
    ],
  });

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("accessibility.typeaheadfind.autostart");
    Services.prefs.clearUserPref("accessibility.typeaheadfind.soundURL");
    Services.prefs.clearUserPref("accessibility.typeaheadfind.casesensitive");
  });
});

add_task(async function test_add_user_pref() {
  await AboutConfigTest.withNewTab(async function() {
    Assert.ok(!Services.prefs.getChildList("").find(pref => pref == "testPref"));
    let search = this.document.getElementById("search");
    search.value = "testPref";
    search.focus();

    for (let [buttonSelector, expectedValue] of [
      [".add-true", true],
      [".add-false", false],
      [".add-Number", 0],
      [".add-String", ""],
    ]) {
      EventUtils.sendKey("return");

      this.document.querySelector("#prefs button" + buttonSelector).click();
      Assert.ok(Services.prefs.getChildList("").find(pref => pref == "testPref"));
      Assert.ok(Preferences.get("testPref") === expectedValue);
      this.document.querySelector("#prefs button[data-l10n-id='about-config-pref-delete']").click();
      search = this.document.getElementById("search");
      search.value = "testPref";
      search.focus();
    }
  });
});

add_task(async function test_delete_user_pref() {
  Services.prefs.setBoolPref("userAddedPref", true);
  await AboutConfigTest.withNewTab(async function() {
    let list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    function getRow(name) {
      return list.find(row => row.querySelector("td").textContent == name);
    }
    Assert.ok(getRow("userAddedPref"));
    getRow("userAddedPref").lastChild.lastChild.click();
    list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    Assert.ok(!getRow("userAddedPref"));
    Assert.ok(!Services.prefs.getChildList("").includes("userAddedPref"));

    // Search for nothing to test gPrefArray
    let search = this.document.getElementById("search");
    search.focus();
    EventUtils.sendKey("return");

    list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    Assert.ok(!getRow("userAddedPref"));
  });
});

add_task(async function test_reset_user_pref() {
  await SpecialPowers.pushPrefEnv({"set": [["browser.autofocus", false]]});

  await AboutConfigTest.withNewTab(async function() {
    function getRow(name) {
      return list.find(row => row.querySelector("td").textContent == name);
    }
    function getValue(name) {
      return getRow(name).querySelector("td.cell-value").textContent;
    }
    let testPref = "browser.autofocus";
    // Click reset.
    let list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    let row = getRow(testPref);
    row.lastChild.lastChild.click();
    // Check new layout and reset.
    list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    Assert.ok(!row.classList.contains("has-user-value"));
    Assert.equal(row.lastChild.childNodes.length, 0);
    Assert.ok(!Services.prefs.prefHasUserValue(testPref));
    Assert.equal(getValue(testPref), "" + Preferences.get(testPref));

    // Search for nothing to test gPrefArray
    let search = this.document.getElementById("search");
    search.focus();
    EventUtils.sendKey("return");

    // Check new layout and reset.
    list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    row = getRow(testPref);
    Assert.ok(!row.classList.contains("has-user-value"));
    Assert.equal(row.lastChild.childNodes.length, 0);
    Assert.equal(getValue(testPref), "" + Preferences.get(testPref));
  });
});

add_task(async function test_modify() {
  await AboutConfigTest.withNewTab(async function() {
    function getRow(name, list) {
      return list.find(row => row.querySelector("td").textContent == name);
    }
    function getValue(name, list) {
      return getRow(name, list).querySelector("td.cell-value").textContent;
    }

    // Test toggle for boolean prefs.
    let list = [...this.document.getElementById("prefs")
      .getElementsByTagName("tr")];
    for (let nameOfBoolPref of [
      "test.aboutconfig.modify.boolean",
      "accessibility.typeaheadfind.autostart",
    ]) {
      let row = getRow(nameOfBoolPref, list);
      // Do this a two times to reset the pref.
      for (let i = 0; i < 2; i++) {
        row.querySelector("td.cell-edit").firstChild.click();
        // Check new layout and saving in backend.
        Assert.equal(getValue(nameOfBoolPref, list),
          "" + Preferences.get(nameOfBoolPref));
        let prefHasUserValue = Services.prefs.prefHasUserValue(nameOfBoolPref);
        Assert.equal(row.classList.contains("has-user-value"), prefHasUserValue);
        Assert.equal(row.lastChild.childNodes.length > 0, prefHasUserValue);
      }
    }

    // Test abort of edit by starting with string and continuing with editing Int pref.
    let row = getRow("test.aboutconfig.modify.string", list);
    row.querySelector("td.cell-edit").firstChild.click();
    row.querySelector("td.cell-value").firstChild.firstChild.value = "test";
    let intRow = getRow("test.aboutconfig.modify.number", list);
    intRow.querySelector("td.cell-edit").firstChild.click();
    Assert.equal(intRow.querySelector("td.cell-value").firstChild.firstChild.value,
      Preferences.get("test.aboutconfig.modify.number"));
    Assert.equal(getValue("test.aboutconfig.modify.string", list),
      "" + Preferences.get("test.aboutconfig.modify.string"));
    Assert.equal(row.querySelector("td.cell-value").textContent,
      Preferences.get("test.aboutconfig.modify.string"));

    // Test regex check for Int pref.
    intRow.querySelector("td.cell-value").firstChild.firstChild.value += "a";
    intRow.querySelector("td.cell-edit").firstChild.click();
    Assert.ok(!intRow.querySelector("td.cell-value").firstChild.firstChild.checkValidity());

    // Test correct saving and DOM-update.
    for (let prefName of [
      "test.aboutconfig.modify.string",
      "test.aboutconfig.modify.number",
      "accessibility.typeaheadfind.soundURL",
      "accessibility.typeaheadfind.casesensitive",
    ]) {
      row = getRow(prefName, list);
      // Activate edit and check displaying.
      row.querySelector("td.cell-edit").firstChild.click();
      Assert.equal(row.querySelector("td.cell-value").firstChild.firstChild.value,
        Preferences.get(prefName));
      row.querySelector("td.cell-value").firstChild.firstChild.value = "42";
      // Save and check saving.
      row.querySelector("td.cell-edit").firstChild.click();
      Assert.equal(getValue(prefName, list),
        "" + Preferences.get(prefName));
      let prefHasUserValue = Services.prefs.prefHasUserValue(prefName);
      Assert.equal(row.lastChild.childNodes.length > 0, prefHasUserValue);
      Assert.equal(row.classList.contains("has-user-value"), prefHasUserValue);
    }
  });
});
