/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* vim: set ts=8 et sw=4 tw=80: */

var gExpectedCharset;
var gLocalDir;

function CharsetDetectionTests(aTestFile, aExpectedCharset) {
  gExpectedCharset = aExpectedCharset;

  InitDetectorTests();

  var fileURI = gLocalDir + aTestFile;
  $("testframe").src = fileURI;

  SimpleTest.waitForExplicitFinish();
}

function InitDetectorTests() {
  var loader = Services.scriptloader;
  var ioService = Services.io;
  loader.loadSubScript("chrome://mochikit/content/chrome-harness.js");

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

function DoDetectionTest() {
  var iframeDoc = $("testframe").contentDocument;
  var charset = iframeDoc.characterSet;

  is(charset, gExpectedCharset, "decoded as " + gExpectedCharset);

  SimpleTest.finish();
}
