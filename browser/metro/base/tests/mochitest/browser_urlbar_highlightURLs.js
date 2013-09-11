// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const kHighlightPref = "browser.urlbar.formatting.enabled";
var gHighlightPrefValue;

function test() {
  runTests();
}

function setUp() {
  gHighlightPrefValue = SpecialPowers.getBoolPref(kHighlightPref);
  SpecialPowers.setBoolPref(kHighlightPref, true);
  yield addTab("about:blank");
}

function tearDown() {
  SpecialPowers.setBoolPref(kHighlightPref, gHighlightPrefValue);
  Browser.closeTab(Browser.selectedTab, { forceClose: true });
}

function testHighlight(aExpected) {
  let urlbar = BrowserUI._edit;
  urlbar.value = aExpected.replace(/[<>]/g, "");

  let selectionController = urlbar.editor.selectionController;
  let selection = selectionController.getSelection(selectionController.SELECTION_URLSECONDARY);
  let value = urlbar.editor.rootElement.textContent;

  let result = "";
  for (let i = 0; i < selection.rangeCount; i++) {
    let range = selection.getRangeAt(i).toString();
    let pos = value.indexOf(range);
    result += value.substring(0, pos) + "<" + range + ">";
    value = value.substring(pos + range.length);
  }
  result += value;
  is(result, aExpected, "test highight");
}

gTests.push({
  desc: "Domain-based URIs (not in editing mode)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    let testcases = [
      "<https://>mozilla.org",
      "<https://>mözilla.org",
      "<https://>mozilla.imaginatory",

      "<https://www.>mozilla.org",
      "<https://sub.>mozilla.org",
      "<https://sub1.sub2.sub3.>mozilla.org",
      "<www.>mozilla.org",
      "<sub.>mozilla.org",
      "<sub1.sub2.sub3.>mozilla.org",

      "<http://ftp.>mozilla.org",
      "<ftp://ftp.>mozilla.org",

      "<https://sub.>mozilla.org",
      "<https://sub1.sub2.sub3.>mozilla.org",
      "<https://user:pass@sub1.sub2.sub3.>mozilla.org",
      "<https://user:pass@>mozilla.org",

      "<https://>mozilla.org</file.ext>",
      "<https://>mozilla.org</sub/file.ext>",
      "<https://>mozilla.org</sub/file.ext?foo>",
      "<https://>mozilla.org</sub/file.ext?foo&bar>",
      "<https://>mozilla.org</sub/file.ext?foo&bar#top>",
      "<https://>mozilla.org</sub/file.ext?foo&bar#top>",

      "<https://sub.>mozilla.org<:666/file.ext>",
      "<sub.>mozilla.org<:666/file.ext>",
      "localhost<:666/file.ext>",

      "mailto:admin@mozilla.org",
      "gopher://mozilla.org/",
      "about:config",
      "jar:http://mozilla.org/example.jar!/",
      "view-source:http://mozilla.org/",
      "foo9://mozilla.org/",
      "foo+://mozilla.org/",
      "foo.://mozilla.org/",
      "foo-://mozilla.org/"
    ];

    testcases.forEach(testHighlight);
  }
});

gTests.push({
  desc: "IP-based URIs (not in editing mode)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    let ips = [
      "192.168.1.1",
      "[::]",
      "[::1]",
      "[1::]",
      "[::]",
      "[::1]",
      "[1::]",
      "[1:2:3:4:5:6:7::]",
      "[::1:2:3:4:5:6:7]",
      "[1:2:a:B:c:D:e:F]",
      "[1::8]",
      "[1:2::8]",
      "[fe80::222:19ff:fe11:8c76]",
      "[0000:0123:4567:89AB:CDEF:abcd:ef00:0000]",
      "[::192.168.1.1]",
      "[1::0.0.0.0]",
      "[1:2::255.255.255.255]",
      "[1:2:3::255.255.255.255]",
      "[1:2:3:4::255.255.255.255]",
      "[1:2:3:4:5::255.255.255.255]",
      "[1:2:3:4:5:6:255.255.255.255]"
    ];

    let formats = [
      "{ip}</file.ext>",
      "{ip}<:666/file.ext>",
      "<https://>{ip}",
      "<https://>{ip}</file.ext>",
      "<https://user:pass@>{ip}<:666/file.ext>",
      "<http://user:pass@>{ip}<:666/file.ext>"
    ];

    function testHighlightAllFormats(aIP) {
      formats.forEach((aFormat) => testHighlight(aFormat.replace("{ip}", aIP)));
    }

    ips.forEach(testHighlightAllFormats);
  }
});

gTests.push({
  desc: "no highlighting (in editing mode)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    testHighlight("<https://>mozilla.org");

    BrowserUI._edit.focus();
    testHighlight("https://mozilla.org");

    Browser.selectedBrowser.focus();
    testHighlight("<https://>mozilla.org");
  }
});

gTests.push({
  desc: "no higlighting (pref disabled)",
  setUp: setUp,
  tearDown: tearDown,
  run: function () {
    testHighlight("<https://>mozilla.org");

    SpecialPowers.setBoolPref(kHighlightPref, false);
    testHighlight("https://mozilla.org");

    SpecialPowers.setBoolPref(kHighlightPref, true);
    testHighlight("<https://>mozilla.org");
  }
});

