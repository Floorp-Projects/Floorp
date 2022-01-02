add_task(async function() {
  const PREF_TRIMURLS = "browser.urlbar.trimURLs";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    Services.prefs.clearUserPref(PREF_TRIMURLS);
    gURLBar.setURI();
  });

  // Avoid search service sync init warnings due to URIFixup, when running the
  // test alone.
  await Services.search.init();

  Services.prefs.setBoolPref(PREF_TRIMURLS, true);

  testVal("http://mozilla.org/", "mozilla.org");
  testVal("https://mozilla.org/", "https://mozilla.org");
  testVal("http://mözilla.org/", "mözilla.org");
  // This isn't a valid public suffix, thus we should untrim it or it would
  // end up doing a search.
  testVal("http://mozilla.imaginatory/");
  testVal("http://www.mozilla.org/", "www.mozilla.org");
  testVal("http://sub.mozilla.org/", "sub.mozilla.org");
  testVal("http://sub1.sub2.sub3.mozilla.org/", "sub1.sub2.sub3.mozilla.org");
  testVal("http://mozilla.org/file.ext", "mozilla.org/file.ext");
  testVal("http://mozilla.org/sub/", "mozilla.org/sub/");

  testVal("http://ftp.mozilla.org/", "ftp.mozilla.org");
  testVal("http://ftp1.mozilla.org/", "ftp1.mozilla.org");
  testVal("http://ftp42.mozilla.org/", "ftp42.mozilla.org");
  testVal("http://ftpx.mozilla.org/", "ftpx.mozilla.org");
  testVal("ftp://ftp.mozilla.org/", "ftp://ftp.mozilla.org");
  testVal("ftp://ftp1.mozilla.org/", "ftp://ftp1.mozilla.org");
  testVal("ftp://ftp42.mozilla.org/", "ftp://ftp42.mozilla.org");
  testVal("ftp://ftpx.mozilla.org/", "ftp://ftpx.mozilla.org");

  testVal("https://user:pass@mozilla.org/", "https://user:pass@mozilla.org");
  testVal("https://user@mozilla.org/", "https://user@mozilla.org");
  testVal("http://user:pass@mozilla.org/", "user:pass@mozilla.org");
  testVal("http://user@mozilla.org/", "user@mozilla.org");
  testVal("http://sub.mozilla.org:666/", "sub.mozilla.org:666");

  testVal("https://[fe80::222:19ff:fe11:8c76]/file.ext");
  testVal("http://[fe80::222:19ff:fe11:8c76]/", "[fe80::222:19ff:fe11:8c76]");
  testVal("https://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext");
  testVal(
    "http://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext",
    "user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext"
  );

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");
  testVal("jar:http://mozilla.org/example.jar!/");
  testVal("view-source:http://mozilla.org/");

  // Behaviour for hosts with no dots depends on the whitelist:
  let fixupWhitelistPref = "browser.fixup.domainwhitelist.localhost";
  Services.prefs.setBoolPref(fixupWhitelistPref, false);
  testVal("http://localhost");
  Services.prefs.setBoolPref(fixupWhitelistPref, true);
  testVal("http://localhost", "localhost");
  Services.prefs.clearUserPref(fixupWhitelistPref);

  testVal("http:// invalid url");

  testVal("http://someotherhostwithnodots");

  // This host is whitelisted, it can be trimmed.
  testVal("http://localhost/ foo bar baz", "localhost/ foo bar baz");

  // This is not trimmed because it's not in the domain whitelist.
  testVal(
    "http://localhost.localdomain/ foo bar baz",
    "http://localhost.localdomain/ foo bar baz"
  );

  Services.prefs.setBoolPref(PREF_TRIMURLS, false);

  testVal("http://mozilla.org/");

  Services.prefs.setBoolPref(PREF_TRIMURLS, true);

  let promiseLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "http://example.com/"
  );
  BrowserTestUtils.loadURI(gBrowser, "http://example.com/");
  await promiseLoaded;

  await testCopy("example.com", "http://example.com/");

  gURLBar.setPageProxyState("invalid");
  gURLBar.valueIsTyped = true;
  await testCopy("example.com", "example.com");
});

function testVal(originalValue, targetValue) {
  gURLBar.value = originalValue;
  gURLBar.valueIsTyped = false;
  let trimmedValue = UrlbarPrefs.get("trimURLs")
    ? BrowserUIUtils.trimURL(originalValue)
    : originalValue;
  Assert.equal(gURLBar.value, trimmedValue, "url bar value set");
  // Now focus the urlbar and check the inputField value is properly set.
  gURLBar.focus();
  Assert.equal(
    gURLBar.value,
    targetValue || originalValue,
    "Check urlbar value on focus"
  );
  // On blur we should trim again.
  gURLBar.blur();
  Assert.equal(gURLBar.value, trimmedValue, "Check urlbar value on blur");
}

function testCopy(originalValue, targetValue) {
  return SimpleTest.promiseClipboardChange(targetValue, () => {
    Assert.equal(gURLBar.value, originalValue, "url bar copy value set");
    gURLBar.focus();
    gURLBar.select();
    goDoCommand("cmd_copy");
  });
}
