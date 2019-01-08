/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.a", "test value 1"],
      ["test.aboutconfig.ab", "test value 2"],
      ["test.aboutconfig.b", "test value 3"],
    ],
  });
});

add_task(async function test_search() {
  await AboutConfigTest.withNewTab(async function() {
    let prefArray = Services.prefs.getChildList("");

    // The total number of preferences may change at any time because of
    // operations running in the background, so we only test approximately.
    // The change in count would be because of one or two added preferences,
    // but we tolerate a difference of up to 50 preferences just to be safe.
    // We want thousands of prefs instead of a few dozen that are filtered.
    Assert.greater(this.rows.length, prefArray.length - 50);

    // Filter a subset of preferences. The "browser.download." branch is
    // chosen because it is very unlikely that its preferences would be
    // modified by other code during the execution of this test.
    this.search("Wser.down   ");

    let filteredPrefArray =
        prefArray.filter(pref => pref.includes("wser.down"));
    // Adding +1 to the list since button does not match an exact
    // preference name then a row is added for the user to add a
    // new button preference if desired
    Assert.equal(this.rows.length, filteredPrefArray.length + 1);

    // Test empty search returns all preferences.
    this.search();

    // The total number of preferences may change at any time because of
    // operations running in the background, so we only test approximately.
    // The change in count would be because of one or two added preferences,
    // but we tolerate a difference of up to 50 preferences just to be safe.
    // We want thousands of prefs instead of a few dozen that are filtered.
    Assert.greater(this.rows.length, prefArray.length - 50);

    // Test invalid search returns no preferences.
    // Expecting 1 row to be returned since it offers the ability to add.
    this.search("aJunkValueasdf");
    Assert.equal(this.rows.length, 1);

    // Two preferences match this filter, and one of those matches exactly.
    this.search("test.aboutconfig.a");
    Assert.equal(this.rows.length, 2);

    // When searching case insensitively, there is an additional row to add a
    // new preference with the same name but a different case.
    this.search("TEST.aboutconfig.a");
    Assert.equal(this.rows.length, 3);
  });
});
