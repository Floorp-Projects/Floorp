/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.a", "test value 1"],
      ["test.aboutconfig.ab", "test value 2"],
      ["test.aboutconfig.bc", "test value 3"],
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

    let filteredPrefArray = prefArray.filter(pref =>
      pref.includes("wser.down")
    );
    // Adding +1 to the list since button does not match an exact
    // preference name then a row is added for the user to add a
    // new button preference if desired
    Assert.equal(this.rows.length, filteredPrefArray.length + 1);

    // Show all preferences again after filtering.
    this.showAll();
    Assert.equal(this.searchInput.value, "");

    // The total number of preferences may change at any time because of
    // operations running in the background, so we only test approximately.
    // The change in count would be because of one or two added preferences,
    // but we tolerate a difference of up to 50 preferences just to be safe.
    // We want thousands of prefs instead of a few dozen that are filtered.
    Assert.greater(this.rows.length, prefArray.length - 50);

    // Pressing ESC while showing all preferences returns to the initial page.
    EventUtils.sendKey("escape");
    Assert.equal(this.rows.length, 0);

    // Test invalid search returns no preferences.
    // Expecting 1 row to be returned since it offers the ability to add.
    this.search("aJunkValueasdf");
    Assert.equal(this.rows.length, 1);

    // Pressing ESC clears the field and returns to the initial page.
    EventUtils.sendKey("escape");
    Assert.equal(this.searchInput.value, "");
    Assert.equal(this.rows.length, 0);

    // Two preferences match this filter, and one of those matches exactly.
    this.search("test.aboutconfig.a");
    Assert.equal(this.rows.length, 2);

    // When searching case insensitively, there is an additional row to add a
    // new preference with the same name but a different case.
    this.search("TEST.aboutconfig.a");
    Assert.equal(this.rows.length, 3);

    // Entering an empty string returns to the initial page.
    this.search("");
    Assert.equal(this.rows.length, 0);
  });
});

add_task(async function test_search_wildcard() {
  await AboutConfigTest.withNewTab(async function() {
    const extra = 1; // "Add" row

    // A trailing wildcard
    this.search("test.about*");
    Assert.equal(this.rows.length, 3 + extra);

    // A wildcard in middle
    this.search("test.about*a");
    Assert.equal(this.rows.length, 2 + extra);
    this.search("test.about*ab");
    Assert.equal(this.rows.length, 1 + extra);
    this.search("test.aboutcon*fig");
    Assert.equal(this.rows.length, 3 + extra);

    // Multiple wildcards in middle
    this.search("test.about*fig*ab");
    Assert.equal(this.rows.length, 1 + extra);
    this.search("test.about*config*ab");
    Assert.equal(this.rows.length, 1 + extra);
  });
});

add_task(async function test_search_delayed() {
  await AboutConfigTest.withNewTab(async function() {
    // Start with the initial empty page.
    this.search("");

    // We need to wait more than the search typing timeout to make sure that
    // nothing happens when entering a short string.
    EventUtils.synthesizeKey("t");
    EventUtils.synthesizeKey("e");
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 500));
    Assert.equal(this.rows.length, 0);

    // Pressing Enter will force a search to occur anyways.
    EventUtils.sendKey("return");
    Assert.greater(this.rows.length, 0);

    // Prepare the table and the search field for the next test.
    this.search("test.aboutconfig.a");
    Assert.equal(this.rows.length, 2);

    // The table is updated in a single microtask, so we don't need to wait for
    // specific mutations, we can just continue when any of the children or
    // their "hidden" attributes are updated.
    let prefsTableChanged = new Promise(resolve => {
      let observer = new MutationObserver(() => {
        observer.disconnect();
        resolve();
      });
      observer.observe(this.prefsTable, { childList: true });
      for (let element of this.prefsTable.children) {
        observer.observe(element, { attributes: true });
      }
    });

    // Add a character and test that the table is not updated immediately.
    EventUtils.synthesizeKey("b");
    Assert.equal(this.rows.length, 2);

    // The table will eventually be updated after a delay.
    await prefsTableChanged;
    Assert.equal(this.rows.length, 1);
  });
});

add_task(async function test_search_add_row_color() {
  await AboutConfigTest.withNewTab(async function() {
    // When the row is the only one displayed, it doesn't have the "odd" class.
    this.search("test.aboutconfig.add");
    Assert.equal(this.rows.length, 1);
    Assert.ok(!this.getRow("test.aboutconfig.add").hasClass("odd"));

    // When displayed with one other preference, the "odd" class is present.
    this.search("test.aboutconfig.b");
    Assert.equal(this.rows.length, 2);
    Assert.ok(this.getRow("test.aboutconfig.b").hasClass("odd"));
  });
});
