// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kTrimPref = "browser.urlbar.trimURLs";
var gTrimPrefValue;

function test() {
  runTests();
}

function setUp() {
  gTrimPrefValue = SpecialPowers.getBoolPref(kTrimPref);
  SpecialPowers.setBoolPref(kTrimPref, true);
  yield addTab("about:blank");
}

function tearDown() {
  SpecialPowers.setBoolPref(kTrimPref, gTrimPrefValue);
  Browser.closeTab(Browser.selectedTab, { forceClose: true });
}

function testTrim(aOriginal, aTarget) {
  let urlbar = BrowserUI._edit;
  urlbar.value = aOriginal;
  urlbar.valueIsTyped = false;
  is(urlbar.value, aTarget || aOriginal, "url bar value set");
}

gTests.push({
  desc: "URIs - trimming (pref enabled)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    let testcases = [
      ["http://mozilla.org/", "mozilla.org"],
      ["https://mozilla.org/", "https://mozilla.org"],
      ["http://mözilla.org/", "mözilla.org"],
      ["http://mozilla.imaginatory/", "mozilla.imaginatory"],
      ["http://www.mozilla.org/", "www.mozilla.org"],
      ["http://sub.mozilla.org/", "sub.mozilla.org"],
      ["http://sub1.sub2.sub3.mozilla.org/", "sub1.sub2.sub3.mozilla.org"],
      ["http://mozilla.org/file.ext", "mozilla.org/file.ext"],
      ["http://mozilla.org/sub/", "mozilla.org/sub/"],

      ["http://ftp.mozilla.org/", "http://ftp.mozilla.org"],
      ["http://ftp1.mozilla.org/", "http://ftp1.mozilla.org"],
      ["http://ftp42.mozilla.org/", "http://ftp42.mozilla.org"],
      ["http://ftpx.mozilla.org/", "ftpx.mozilla.org"],
      ["ftp://ftp.mozilla.org/", "ftp://ftp.mozilla.org"],
      ["ftp://ftp1.mozilla.org/", "ftp://ftp1.mozilla.org"],
      ["ftp://ftp42.mozilla.org/", "ftp://ftp42.mozilla.org"],
      ["ftp://ftpx.mozilla.org/", "ftp://ftpx.mozilla.org"],

      ["https://user:pass@mozilla.org/", "https://user:pass@mozilla.org"],
      ["http://user:pass@mozilla.org/", "http://user:pass@mozilla.org"],
      ["http://sub.mozilla.org:666/", "sub.mozilla.org:666"],

      ["https://[fe80::222:19ff:fe11:8c76]/file.ext"],
      ["http://[fe80::222:19ff:fe11:8c76]/", "[fe80::222:19ff:fe11:8c76]"],
      ["https://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext"],
      ["http://user:pass@[fe80::222:19ff:fe11:8c76]:666/file.ext"],

      ["mailto:admin@mozilla.org"],
      ["gopher://mozilla.org/"],
      ["about:config"],
      ["jar:http://mozilla.org/example.jar!/"],
      ["view-source:http://mozilla.org/"]
    ];

    for (let [original, target] of testcases)
      testTrim(original, target);
  }
});

gTests.push({
  desc: "URIs - no trimming (pref disabled)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    SpecialPowers.setBoolPref(kTrimPref, false);
    testTrim("http://mozilla.org/");

    SpecialPowers.setBoolPref(kTrimPref, true);
    testTrim("http://mozilla.org/", "mozilla.org");
  }
});

gTests.push({
  desc: "Loaded URI - copy/paste behavior",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    let urlbar = BrowserUI._edit;

    BrowserUI.goToURI("http://example.com/");
    let pageLoaded = yield waitForCondition(
      () => Browser.selectedBrowser.currentURI.spec == "http://example.com/");

    ok(pageLoaded, "expected page should have loaded");
    is(urlbar.value, "example.com", "trimmed value set");

    yield showNavBar();

    function clipboardCondition(aExpected) {
      return () => aExpected == SpecialPowers.getClipboardData("text/unicode");
    }

    // Value set by browser -- should copy entire url (w/ scheme) on full select

    urlbar.focus();
    urlbar.select();
    CommandUpdater.doCommand("cmd_copy");

    let copy = yield waitForCondition(clipboardCondition("http://example.com/"));
    ok(copy, "should copy entire url (w/ scheme) on full select");

    // Value set by browser -- should copy selected text on partial select

    urlbar.focus();
    urlbar.select();
    urlbar.selectionStart = 2;
    CommandUpdater.doCommand("cmd_copy");

    copy = yield waitForCondition(clipboardCondition("ample.com"));
    ok(copy, "should copy selected text on partial select");

    // Value set by user -- should not copy full string

    urlbar.valueIsTyped = true;
    urlbar.focus();
    urlbar.select();
    CommandUpdater.doCommand("cmd_copy");

    copy = yield waitForCondition(clipboardCondition("example.com"));
    ok(copy, "should not copy full string");
  }
});