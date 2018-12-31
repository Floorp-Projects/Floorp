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
    await this.document.querySelector("button").click();

    // The total number of preferences may change at any time because of
    // operations running in the background, so we only test approximately.
    // The change in count would be because of one or two added preferences,
    // but we tolerate a difference of up to 50 preferences just to be safe.
    // We want thousands of prefs instead of a few dozen that are filtered.
    Assert.greater(this.document.getElementById("prefs").childElementCount,
                   prefArray.length - 50);

    // Filter a subset of preferences. The "browser.download." branch is
    // chosen because it is very unlikely that its preferences would be
    // modified by other code during the execution of this test.
    let search = this.document.getElementById("search");
    search.value = "wser.down   ";
    search.focus();
    EventUtils.sendKey("return");

    let filteredPrefArray =
        prefArray.filter(pref => pref.includes("wser.down"));
    // Adding +1 to the list since button does not match an exact
    // preference name then a row is added for the user to add a
    // new button preference if desired
    Assert.equal(this.document.getElementById("prefs").childElementCount,
                 filteredPrefArray.length + 1);

    // Test empty search returns all preferences.
    search.value = "";
    search.focus();
    EventUtils.sendKey("return");

    // The total number of preferences may change at any time because of
    // operations running in the background, so we only test approximately.
    // The change in count would be because of one or two added preferences,
    // but we tolerate a difference of up to 50 preferences just to be safe.
    // We want thousands of prefs instead of a few dozen that are filtered.
    Assert.greater(this.document.getElementById("prefs").childElementCount,
                   prefArray.length - 50);

    // Test invalid search returns no preferences.
    search.value = "aJunkValueasdf";
    search.focus();
    EventUtils.sendKey("return");

    // Expecting 1 row to be returned since it offers the ability to add
    Assert.equal(this.document.getElementById("prefs").childElementCount,
                 1);

    // Test added preferences search returns 2 preferences.
    search.value = "test.aboutconfig.a";
    search.focus();
    EventUtils.sendKey("return");

    Assert.equal(this.document.getElementById("prefs").childElementCount,
                 2);
  });
});
