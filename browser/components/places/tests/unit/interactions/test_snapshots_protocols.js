/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests for snapshot creation and removal.
 */

const TEST_HTTPS_URL = "https://example.com/";
const TEST_HTTP_URL = "http://example.com/";
const TEST_HTTPS_PDF_URL = "https://example.com/test.pdf";
const TEST_HTTP_PDF_URL = "https://example.com/test.pdf";
const TEST_LOCAL_PDF_FILE = "file:///Users/user/test.pdf";
const TEST_LOCAL_TEXT_FILE = "file:///Users/user/test.txt";
const TEST_LOCAL_JSON_FILE = "file:///Users/user/test.json";

/**
 * Creates an interaction and snapshot for the given url
 * Returns the result of a query for the snapshot
 * @param {string} url
 *   The url to create the interaction and snapshot for
 */
async function create_interaction_and_snapshot(url) {
  let now = Date.now();
  await addInteractions([
    {
      url,
      created_at: now,
      updated_at: now,
    },
  ]);

  await Snapshots.add({ url });
  return Snapshots.get(url);
}

add_task(async function test_https() {
  let snapshot = await create_interaction_and_snapshot(TEST_HTTPS_URL);
  assertSnapshot(snapshot, { url: TEST_HTTPS_URL });
});

add_task(async function test_http() {
  let snapshot = await create_interaction_and_snapshot(TEST_HTTP_URL);
  assertSnapshot(snapshot, { url: TEST_HTTP_URL });
});

add_task(async function test_https_pdf() {
  let snapshot = await create_interaction_and_snapshot(TEST_HTTPS_PDF_URL);
  assertSnapshot(snapshot, { url: TEST_HTTPS_PDF_URL });
});

add_task(async function test_http_pdf() {
  let snapshot = await create_interaction_and_snapshot(TEST_HTTP_PDF_URL);
  assertSnapshot(snapshot, { url: TEST_HTTP_PDF_URL });
});

add_task(async function test_local_pdf() {
  let snapshot = await create_interaction_and_snapshot(TEST_LOCAL_PDF_FILE);
  assertSnapshot(snapshot, { url: TEST_LOCAL_PDF_FILE });
});

add_task(async function test_local_text_file() {
  try {
    let snapshot = await create_interaction_and_snapshot(TEST_LOCAL_TEXT_FILE);
    Assert.ok(
      !snapshot,
      "Should not have found a snapshot for a local text file"
    );
  } catch (e) {
    Assert.ok(true, "Should not have found a snapshot for a local text file");
  }
});

add_task(async function test_local_json_file() {
  try {
    let snapshot = await create_interaction_and_snapshot(TEST_LOCAL_JSON_FILE);
    Assert.ok(
      !snapshot,
      "Should not have found a snapshot for a local json file"
    );
  } catch (e) {
    Assert.ok(true, "Should not have found a snapshot for a local json file");
  }
});
