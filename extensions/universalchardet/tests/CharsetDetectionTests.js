/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 et sw=4 tw=80: */
var gExpectedCharset;
var gOldPref;
var gDetectorList;
var gTestIndex;
var gLocalDir;
const Cc = Components.classes;
const Ci = Components.interfaces;

function CharsetDetectionTests(aTestFile, aExpectedCharset, aDetectorList)
{
    gExpectedCharset = aExpectedCharset;
    gDetectorList = aDetectorList;

    InitDetectorTests();

    var fileURI = gLocalDir + aTestFile;
    $("testframe").src = fileURI;

    SimpleTest.waitForExplicitFinish();
}

function InitDetectorTests()
{
    var prefService = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch);
    var str = Cc["@mozilla.org/supports-string;1"]
        .createInstance(Ci.nsISupportsString);
    var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
        .getService(Ci.mozIJSSubScriptLoader);
    var ioService = Cc['@mozilla.org/network/io-service;1']
        .getService(Ci.nsIIOService);
    loader.loadSubScript("chrome://mochikit/content/chrome-harness.js");

    try {
        gOldPref = prefService
            .getComplexValue("intl.charset.detector",
                             Ci.nsIPrefLocalizedString).data;
    } catch (e) {
        gOldPref = "";
    }
    SetDetectorPref(gDetectorList[0]);
    gTestIndex = 0;
    $("testframe").onload = DoDetectionTest;

    if (gExpectedCharset == "default") {
        try {
            gExpectedCharset = prefService
                .getComplexValue("intl.charset.default",
                                 Ci.nsIPrefLocalizedString)
                .data;
            if (gExpectedCharset == "ISO-8859-1") {
                gExpectedCharset = "windows-1252";
            }
        } catch (e) {
            gExpectedCharset = "windows-1252";
        }
    }

    // Get the local directory. This needs to be a file: URI because chrome:
    // URIs are always UTF-8 (bug 617339) and we are testing decoding from other
    // charsets.
    var jar = getJar(getRootDirectory(window.location.href));
    var dir = jar ?
                extractJarToTmp(jar) :
                getChromeDir(getResolvedURI(window.location.href));
    gLocalDir = ioService.newFileURI(dir).spec;
}

function SetDetectorPref(aPrefValue)
{
    var prefService = Cc["@mozilla.org/preferences-service;1"]
                      .getService(Ci.nsIPrefBranch);
    var str = Cc["@mozilla.org/supports-string;1"]
              .createInstance(Ci.nsISupportsString);
    str.data = aPrefValue;
    prefService.setComplexValue("intl.charset.detector",
                                Ci.nsISupportsString, str);
    gCurrentDetector = aPrefValue;
}

function DoDetectionTest() {
    var iframeDoc = $("testframe").contentDocument;
    var charset = iframeDoc.characterSet;

    is(charset, gExpectedCharset,
       "decoded as " + gExpectedCharset + " by " + gDetectorList[gTestIndex]);

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

