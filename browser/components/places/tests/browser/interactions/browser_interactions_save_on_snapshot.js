/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests page view time recording for interactions.
 */

const { Snapshots } = ChromeUtils.import("resource:///modules/Snapshots.jsm");
const { PlacesTestUtils } = ChromeUtils.import(
  "resource://testing-common/PlacesTestUtils.jsm"
);

async function reset() {
  await Snapshots.reset();
  await Interactions.reset();
  registerCleanupFunction(async () => {
    await Snapshots.reset();
    await Interactions.reset();
  });
}

add_task(async function test_manual_snapshot_saves_interactions() {
  await reset();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.places.interactions.saveInterval", 0]],
  });

  const url = "https://example.com/";
  await BrowserTestUtils.withNewTab(url, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    let promises = [
      Interactions.interactionUpdatePromise,
      TestUtils.topicObserved("places-metadata-updated", () => true),
    ];
    await Snapshots.add({
      url,
      userPersisted: Snapshots.USER_PERSISTED.MANUAL,
    });
    await Promise.all(promises);

    await assertDatabaseValues(
      [
        {
          url,
          totalViewTime: 10000,
        },
      ],
      { dontFlush: true }
    );
  });
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_automatic_snapshot_dont_save_interactions() {
  await reset();
  await SpecialPowers.pushPrefEnv({
    set: [["browser.places.interactions.saveInterval", 0]],
  });

  // Inserting automatic snapshots requires an interaction to exist.
  const url = "https://example.com/";
  await PlacesTestUtils.addVisits(url);
  await PlacesUtils.withConnectionWrapper(
    "InsertDummyInteraction",
    async db => {
      await db.execute(
        `
        INSERT INTO moz_places_metadata (
          place_id, created_at, updated_at, document_type, total_view_time, typing_time, key_presses, scrolling_time, scrolling_distance
      ) VALUES (
          (SELECT id FROM moz_places WHERE url_hash = hash(:url)), :now, :now, 0, 6000, 0, 0, 0, 0
      )`,
        { url, now: Date.now() }
      );
    }
  );
  await BrowserTestUtils.withNewTab(url, async browser => {
    Interactions._pageViewStartTime = Cu.now() - 10000;

    await Snapshots.add({
      url,
      userPersisted: Snapshots.USER_PERSISTED.NO,
    });
    // Use a timer to check the interaction is not immediately stored by adding
    // the snapshot.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(r => setTimeout(r, 1000));
    await assertDatabaseValues(
      [
        {
          url,
          totalViewTime: 6000,
        },
      ],
      { dontFlush: true }
    );
  });
  await SpecialPowers.popPrefEnv();
});
