/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* vim: set ts=8 et sw=4 tw=80: */
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var gExpectedCharset;
var gOldPref;
var gDetectorList;
var gTestIndex;
var gLocalDir;

function CharsetDetectionTests(aTestFile, aExpectedCharset, aDetectorList) {
  gExpectedCharset = aExpectedCharset;
  gDetectorList = aDetectorList;

  InitDetectorTests();

  var fileURI = gLocalDir + aTestFile;
  $("testframe").src = fileURI;

  SimpleTest.waitForExplicitFinish();
}

function InitDetectorTests() {
  var prefService = Services.prefs;
  var loader = Services.scriptloader;
  var ioService = Services.io;
  loader.loadSubScript("chrome://mochikit/content/chrome-harness.js");

  try {
    gOldPref = prefService.getComplexValue(
      "intl.charset.detector",
      Ci.nsIPrefLocalizedString
    ).data;
  } catch (e) {
    gOldPref = "";
  }
  SetDetectorPref(gDetectorList[0]);
  gTestIndex = 0;
  $("testframe").onload = DoDetectionTest;

  if (gExpectedCharset == "default") {
    // No point trying to be generic here, because we have plenty of other
    // unit tests that fail if run using a non-windows-1252 locale.
    gExpectedCharset = "windows-1252";
  }

  // Get the local directory. This needs to be a file: URI because chrome:
  // URIs are always UTF-8 (bug 617339) and we are testing decoding from other
  // charsets.
  var jar = getJar(getRootDirectory(window.location.href));
  var dir = jar
    ? extractJarToTmp(jar)
    : getChromeDir(getResolvedURI(window.location.href));
  gLocalDir = ioService.newFileURI(dir).spec;
}

function SetDetectorPref(aPrefValue) {
  var fallback = "";
  if (aPrefValue == "ja_parallel_state_machine") {
    fallback = "Shift_JIS";
  } else if (aPrefValue == "ruprob" || aPrefValue == "ukprob") {
    fallback = "windows-1251";
  }
  var prefService = Services.prefs;
  prefService.setStringPref("intl.charset.detector", aPrefValue);
  prefService.setStringPref("intl.charset.fallback.override", fallback);
}

function DoDetectionTest() {
  var iframeDoc = $("testframe").contentDocument;
  var charset = iframeDoc.characterSet;

  is(
    charset,
    gExpectedCharset,
    "decoded as " + gExpectedCharset + " by " + gDetectorList[gTestIndex]
  );

  if (++gTestIndex < gDetectorList.length) {
    SetDetectorPref(gDetectorList[gTestIndex]);
    iframeDoc.location.reload();
  } else {
    CleanUpDetectionTests();
  }
}

function CleanUpDetectionTests() {
  SetDetectorPref(gOldPref);
  SimpleTest.finish();
}
