/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that installing adds it to the store.
add_task(async () => {
  Services.prefs.setBoolPref("browser.ssb.osintegration", true);

  let ssb = await SiteSpecificBrowser.createFromManifest(
    parseManifest("https://www.mozilla.org/", {
      icons: [
        {
          src: ICON32,
          sizes: "32x32",
        },
        {
          src: ICON48,
          sizes: "48x48",
        },
        {
          src: ICON128,
          sizes: "128x128",
        },
      ],
    })
  );
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

  if (AppConstants.platform == "win") {
    // Check that the shortcut is made and destroyed.
    let link = Services.dirsvc.get("Desk", Ci.nsIFile);
    link.append("www.mozilla.org.lnk");

    Assert.ok(link.isFile());

    let icon = gSSBData.clone();
    icon.append(ssb.id);
    icon.append("icon.ico");

    Assert.ok(icon.isFile());

    await ssb.uninstall();
    Assert.ok(!link.exists());
    let dir = gSSBData.clone();
    dir.append(ssb.id);
    Assert.ok(!dir.exists());
  }
});
