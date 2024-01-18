/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that sanitizing history will clear storage access permissions
// for sites without cookies or site data.
add_task(async function sanitizeStorageAccessPermissions() {
  let categories = ["history", "historyFormDataAndDownloads"];

  for (let pref of categories) {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, resolve);
    });

    await SiteDataTestUtils.addToIndexedDB("https://sub.example.org");
    await SiteDataTestUtils.addToCookies({ origin: "https://example.com" });

    PermissionTestUtils.add(
      "https://example.org",
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      "https://example.com",
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );
    PermissionTestUtils.add(
      "http://mochi.test",
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );

    // Add some time in between taking the snapshot of the timestamp
    // to avoid flakyness.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(c => setTimeout(c, 100));
    let timestamp = Date.now();
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(c => setTimeout(c, 100));

    PermissionTestUtils.add(
      "http://example.net",
      "storageAccessAPI",
      Services.perms.ALLOW_ACTION
    );

    await Sanitizer.sanitize([pref], {
      // Sanitizer and ClearDataService work with time range in PRTime (microseconds)
      range: [timestamp * 1000, Date.now() * 1000],
    });

    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "http://example.net",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "http://mochi.test",
        "storageAccessAPI"
      ),
      Services.perms.ALLOW_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.com",
        "storageAccessAPI"
      ),
      Services.perms.ALLOW_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.org",
        "storageAccessAPI"
      ),
      Services.perms.ALLOW_ACTION
    );

    await Sanitizer.sanitize([pref]);

    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "http://mochi.test",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "http://example.net",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.com",
        "storageAccessAPI"
      ),
      Services.perms.ALLOW_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.org",
        "storageAccessAPI"
      ),
      Services.perms.ALLOW_ACTION
    );

    await Sanitizer.sanitize([pref, "siteSettings"]);

    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "http://mochi.test",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.com",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
    Assert.equal(
      PermissionTestUtils.testExactPermission(
        "https://example.org",
        "storageAccessAPI"
      ),
      Services.perms.UNKNOWN_ACTION
    );
  }
});
