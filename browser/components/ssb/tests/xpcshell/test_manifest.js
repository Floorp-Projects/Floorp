/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function empty_manifest() {
  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/")
  );

  Assert.equal(ssb.startURI.spec, "https://www.mozilla.org/");

  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/bar")));
  Assert.ok(!ssb.canLoad(uri("http://www.mozilla.org/")));
  Assert.ok(!ssb.canLoad(uri("https://test.mozilla.org/")));
});

add_task(async function manifest_with_scope() {
  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/foo/bar", {
      scope: "https://www.mozilla.org/foo",
    })
  );

  Assert.equal(ssb.startURI.spec, "https://www.mozilla.org/foo/bar");

  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/baz")));

  // Note: scopes are simple path prefixes.
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/food")));

  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/")));
  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/bar")));
  Assert.ok(!ssb.canLoad(uri("http://www.mozilla.org/")));
  Assert.ok(!ssb.canLoad(uri("https://test.mozilla.org/")));
});

add_task(async function manifest_with_start_url() {
  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/foo/bar", {
      start_url: "https://www.mozilla.org/foo/",
    })
  );

  Assert.equal(ssb.startURI.spec, "https://www.mozilla.org/foo/");

  // scope should be "https://www.mozilla.org/foo/"
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/baz")));

  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/foo")));
  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/")));
  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/bar")));
  Assert.ok(!ssb.canLoad(uri("http://www.mozilla.org/")));
  Assert.ok(!ssb.canLoad(uri("https://test.mozilla.org/")));
});

add_task(async function update_manifest() {
  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/foo/bar/bas", {
      start_url: "https://www.mozilla.org/foo/bar/bas",
      scope: "https://www.mozilla.org/foo/bar/",
    })
  );

  Assert.equal(ssb.startURI.spec, "https://www.mozilla.org/foo/bar/bas");

  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar/")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar/foo")));

  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/foo")));
  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/foo/bar")));

  await ssb.updateFromManifest(
    parseManifest("https://www.mozilla.org/foo/bar/bas", {
      start_url: "https://www.mozilla.org/foo/bar/bas",
      scope: "https://www.mozilla.org/foo/",
    })
  );

  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar/")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar/foo")));

  Assert.ok(!ssb.canLoad(uri("https://www.mozilla.org/foo")));
  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/foo/bar")));
});
