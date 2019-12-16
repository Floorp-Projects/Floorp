/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// A database that is too new should be wiped.
add_task(async () => {
  let kvstore = await getKVStore();
  kvstore.put(
    "_meta",
    JSON.stringify({
      version: 1000,
    })
  );

  storeSSB(kvstore, "a", parseManifest("https://www.mozilla.org/"), {});
  storeSSB(kvstore, "b", parseManifest("https://www.microsoft.com/"), {});

  let ssbs = await SiteSpecificBrowserService.list();
  Assert.equal(ssbs.length, 0);

  let meta = JSON.parse(await kvstore.get("_meta"));
  Assert.equal(meta.version, 1);
});
