/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);
const { SearchUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);
SearchTestUtils.init(this);

function findRow(tree, expectedName) {
  for (let i = 0; i < tree.view.rowCount; i++) {
    let name = tree.view.getCellText(
      i,
      tree.columns.getNamedColumn("engineName")
    );

    if (name == expectedName) {
      return i;
    }
  }
  return -1;
}

add_task(async function test_change_engine() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;

  let extension1 = await SearchTestUtils.installSearchExtension({
    id: "example@tests.mozilla.org",
    name: "Example",
    version: "1.0",
    keyword: "foo",
    favicon_url: "img123.png",
  });
  await AddonTestUtils.waitForSearchProviderStartup(extension1);

  let tree = doc.querySelector("#engineList");

  let row = findRow(tree, "Example");

  Assert.notEqual(row, -1, "Should have found the entry");
  Assert.ok(
    tree.view
      .getImageSrc(row, tree.columns.getNamedColumn("engineName"))
      .includes("img123.png"),
    "Should have the correct image URL"
  );
  Assert.equal(
    tree.view.getCellText(row, tree.columns.getNamedColumn("engineKeyword")),
    "foo",
    "Should show the correct keyword"
  );

  let updatedPromise = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
  let extension2 = await SearchTestUtils.installSearchExtension({
    id: "example@tests.mozilla.org",
    name: "Example 2",
    version: "2.0",
    keyword: "bar",
    favicon_url: "img456.png",
  });

  await AddonTestUtils.waitForSearchProviderStartup(extension2);
  await updatedPromise;

  row = findRow(tree, "Example 2");

  Assert.notEqual(row, -1, "Should have found the updated entry");
  Assert.ok(
    tree.view
      .getImageSrc(row, tree.columns.getNamedColumn("engineName"))
      .includes("img456.png"),
    "Should have the correct image URL"
  );
  Assert.equal(
    tree.view.getCellText(row, tree.columns.getNamedColumn("engineKeyword")),
    "bar",
    "Should show the correct keyword"
  );

  gBrowser.removeCurrentTab();
});
