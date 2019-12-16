/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that installing adds it to the store.
add_task(async () => {
  let ssb = SiteSpecificBrowser.createFromURI(uri("https://www.mozilla.org/"));
  await ssb.install();

  let ssbs = await SiteSpecificBrowserService.list();
  Assert.equal(ssbs.length, 1);
  Assert.equal(ssbs[0], ssb);

  let kvstore = await getKVStore();
  Assert.ok(await kvstore.has(storeKey(ssb.id)));

  // Don't want to rely on the structure too much, just make sure it looks sane.
  let data = JSON.parse(await kvstore.get(`ssb:${ssb.id}`));
  Assert.ok("manifest" in data);
  Assert.ok("config" in data);
});
