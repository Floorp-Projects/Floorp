/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * Check that panes that aren't the default have their bindings initialized
 * and 'preference' attributes are processed correctly.
 */
add_task(async function test_prefs_bindings_initted() {
  await openPreferencesViaOpenPreferencesAPI("general", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let prefReader = doc.ownerGlobal.Preferences;
  let prefBasedCheckboxes = Array.from(
    doc.querySelectorAll("checkbox[preference]")
  );

  // Then check all the preferences:
  for (let checkbox of prefBasedCheckboxes) {
    let pref = checkbox.getAttribute("preference");
    if (Services.prefs.getPrefType(pref) == Services.prefs.PREF_BOOL) {
      info(`Waiting for checkbox to match pref ${pref}`);
      // Ensure the task setting up prefs has run.
      await BrowserTestUtils.waitForMutationCondition(
        checkbox,
        { attributeFilter: ["checked"] },
        () => checkbox.checked == prefReader.get(pref).value
      );
      is(
        checkbox.checked,
        prefReader.get(pref).value,
        `Checkbox value should match preference (${pref}).`
      );
      // Ignore all other types. The mapping is normally non-trivial and done
      // using syncfrompreference handlers in JS.
    }
  }

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
