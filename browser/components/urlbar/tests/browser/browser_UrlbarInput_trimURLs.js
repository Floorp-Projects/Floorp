function testValues(trimmedProtocol, notTrimmedProtocol) {
  testVal(trimmedProtocol + "mozilla.org/", "mozilla.org");
  testVal(
    notTrimmedProtocol + "mozilla.org/",
    notTrimmedProtocol + "mozilla.org"
  );
  testVal(trimmedProtocol + "mözilla.org/", "mözilla.org");
  // This isn't a valid public suffix, thus we should untrim it or it would
  // end up doing a search.
  testVal(trimmedProtocol + "mozilla.imaginatory/");
  testVal(trimmedProtocol + "www.mozilla.org/", "www.mozilla.org");
  testVal(trimmedProtocol + "sub.mozilla.org/", "sub.mozilla.org");
  testVal(
    trimmedProtocol + "sub1.sub2.sub3.mozilla.org/",
    "sub1.sub2.sub3.mozilla.org"
  );
  testVal(trimmedProtocol + "mozilla.org/file.ext", "mozilla.org/file.ext");
  testVal(trimmedProtocol + "mozilla.org/sub/", "mozilla.org/sub/");

  testVal(trimmedProtocol + "ftp.mozilla.org/", "ftp.mozilla.org");
  testVal(trimmedProtocol + "ftp1.mozilla.org/", "ftp1.mozilla.org");
  testVal(trimmedProtocol + "ftp42.mozilla.org/", "ftp42.mozilla.org");
  testVal(trimmedProtocol + "ftpx.mozilla.org/", "ftpx.mozilla.org");
  testVal("ftp://ftp.mozilla.org/", "ftp://ftp.mozilla.org");
  testVal("ftp://ftp1.mozilla.org/", "ftp://ftp1.mozilla.org");
  testVal("ftp://ftp42.mozilla.org/", "ftp://ftp42.mozilla.org");
  testVal("ftp://ftpx.mozilla.org/", "ftp://ftpx.mozilla.org");

  testVal(
    notTrimmedProtocol + "user:pass@mozilla.org/",
    notTrimmedProtocol + "user:pass@mozilla.org"
  );
  testVal(
    notTrimmedProtocol + "user@mozilla.org/",
    notTrimmedProtocol + "user@mozilla.org"
  );

  testVal("mailto:admin@mozilla.org");
  testVal("gopher://mozilla.org/");
  testVal("about:config");
  testVal("jar:http://mozilla.org/example.jar!/");
  testVal("view-source:http://mozilla.org/");
}

add_task(async function () {
  const PREF_TRIM_URLS = "browser.urlbar.trimURLs";
  const PREF_TRIM_HTTPS = "browser.urlbar.trimHttps";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  registerCleanupFunction(async function () {
    BrowserTestUtils.removeTab(tab);
    Services.prefs.clearUserPref(PREF_TRIM_URLS);
    Services.prefs.clearUserPref(PREF_TRIM_HTTPS);
    gURLBar.setURI();
  });

  Services.prefs.setBoolPref(PREF_TRIM_HTTPS, false);

  // Avoid search service sync init warnings due to URIFixup, when running the
  // test alone.
  await Services.search.init();

  Services.prefs.setBoolPref(PREF_TRIM_URLS, true);

  testValues("http://", "https://");
  Services.prefs.setBoolPref(PREF_TRIM_HTTPS, true);
  testValues("https://", "http://");
  Services.prefs.setBoolPref(PREF_TRIM_HTTPS, false);

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

  // This is not trimmed because it's not in the domain whitelist.
  testVal(
    "http://localhost.localdomain/ foo bar baz",
    "http://localhost.localdomain/ foo bar baz"
  );
  Services.prefs.setBoolPref(PREF_TRIM_URLS, false);

  testVal("http://mozilla.org/");

  Services.prefs.setBoolPref(PREF_TRIM_URLS, true);

  let promiseLoaded = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    "http://example.com/"
  );
  BrowserTestUtils.startLoadingURIString(gBrowser, "http://example.com/");
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
