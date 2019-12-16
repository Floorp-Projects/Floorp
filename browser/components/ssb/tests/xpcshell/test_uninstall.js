/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that uninstalling removes from the store.
add_task(async () => {
  let kvstore = await getKVStore();
  kvstore.put(
    "_meta",
    JSON.stringify({
      version: 1,
    })
  );

  storeSSB(kvstore, "a", parseManifest("https://www.mozilla.org/"), {});

  let ssb = await SiteSpecificBrowser.load("a");

  let ssbs = await SiteSpecificBrowserService.list();
  Assert.equal(ssbs.length, 1);
  Assert.equal(ssbs[0], ssb);

  Assert.ok(ssb.canLoad(uri("https://www.mozilla.org/test/")));
  Assert.ok(!ssb.canLoad(uri("https://www.microsoft.com/test/")));

  await ssb.uninstall();

  ssbs = await SiteSpecificBrowserService.list();
  Assert.equal(ssbs.length, 0);

  Assert.ok(!(await kvstore.has(storeKey("a"))));
});
