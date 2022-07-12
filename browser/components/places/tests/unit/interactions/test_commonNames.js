/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL1 = "https://example.com/";
const TEST_URL2 = "https://mozilla.org:8080/12345";
const TEST_URL3 = "https://google.com/";
const TEST_URL4 = "https://example.com/1234";
const TEST_URL5 = "https://en.wikipedia.org/";
const TEST_URL6 = "https://maps.google.com/";
/**
 * Tests CommonNames.jsm, the module that returns a URL's "common name" —
 * usually, the site's brand name with correct spacing and capitalization.
 */

ChromeUtils.defineESModuleGetters(this, {
  PageDataService: "resource:///modules/pagedata/PageDataService.sys.mjs",
});

add_task(async function setup() {
  await Interactions.reset();
  await addInteractions([
    { url: TEST_URL1 },
    { url: TEST_URL2 },
    { url: TEST_URL3 },
    { url: TEST_URL4 },
    { url: TEST_URL5 },
    { url: TEST_URL6 },
  ]);

  registerCleanupFunction(async () => {
    await Interactions.reset();
  });
});

/**
 * Tests that we return site_name page data as a common name.
 */
add_task(async function fromMetadata() {
  let actor = {};
  PageDataService.lockEntry(actor, TEST_URL1);
  PageDataService.pageDataDiscovered({
    url: TEST_URL1,
    siteName: "Test Example",
  });
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL1], () =>
    Snapshots.add({ url: TEST_URL1 })
  );
  PageDataService.unlockEntry(actor, TEST_URL1);

  let snapshot = await Snapshots.get(TEST_URL1);
  Assert.equal(
    snapshot.commonName,
    "Test Example",
    "snapshot.commonName should contain the site_name data."
  );
});

/**
 * Tests that we return a value from our custom list if we have no site_name
 * metadata for a site.
 *
 * This subtest depends on mozilla.org being in the CUSTOM_LIST map in
 * CommonNames.jsm.
 */
add_task(async function customName_noMetadata() {
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL2], () =>
    Snapshots.add({ url: TEST_URL2 })
  );

  let snapshot = await Snapshots.get(TEST_URL2);
  Assert.equal(
    snapshot.commonName,
    "Mozilla",
    "snapshot.commonName should contain our custom label."
  );
});

/**
 * Tests that we return site_name page data as a common name even if that site
 * is in the CUSTOM_LIST map in CommonNames.jsm.
 *
 * This subtest depends on google.com being in the CUSTOM_LIST map in
 * CommonNames.jsm.
 */
add_task(async function customName_withMetadata() {
  let actor = {};
  PageDataService.lockEntry(actor, TEST_URL3);
  PageDataService.pageDataDiscovered({
    url: TEST_URL3,
    siteName: "Test Example 2",
  });
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL3], () =>
    Snapshots.add({ url: TEST_URL3 })
  );
  PageDataService.unlockEntry(actor, TEST_URL3);

  let snapshot = await Snapshots.get(TEST_URL3);
  Assert.equal(
    snapshot.commonName,
    "Test Example 2",
    "snapshot.commonName should contain site_name data even though it's on the custom list."
  );
});

/**
 * Tests that we return the URL without its TLD when we don't have any data
 * about a URL.
 */
add_task(async function shortUrl() {
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL4], () =>
    Snapshots.add({ url: TEST_URL4 })
  );

  let snapshot = await Snapshots.get(TEST_URL4);
  Assert.equal(
    snapshot.commonName,
    "example",
    "snapshot.commonName should contain the URL with the TLD stripped."
  );
});

/**
 * Tests that we strip subdomains from a snapshot's URL iff CUSTOM_LIST does not
 * contain a URL with those subdomains.
 *
 * This subtest depends on wikipedia.org and maps.google.com being in
 * CUSTOM_LIST.
 */
add_task(async function subdomains() {
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL5], () =>
    Snapshots.add({ url: TEST_URL5 })
  );
  await assertUrlNotification(TOPIC_ADDED, [TEST_URL6], () =>
    Snapshots.add({ url: TEST_URL6 })
  );

  let snapshot = await Snapshots.get(TEST_URL5);
  Assert.equal(
    snapshot.commonName,
    "Wikipedia",
    "snapshot.commonName should match wikipedia.org even though en.wikipedia.org was passed in."
  );

  snapshot = await Snapshots.get(TEST_URL6);
  Assert.equal(
    snapshot.commonName,
    "Google Maps",
    "snapshot.commonName should match Google Maps even though google.com is in the custom list."
  );
});
