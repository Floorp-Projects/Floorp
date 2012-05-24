/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 et sw=4 tw=80: */
var gExpectedCharset;
var gOldPref;
var gDetectorList;
var gTestIndex;
const Cc = Components.classes;
const Ci = Components.interfaces;

function CharsetDetectionTests(aTestFile, aExpectedCharset, aDetectorList)
{
    gExpectedCharset = aExpectedCharset;
    gDetectorList = aDetectorList;

    InitDetectorTests();

    // convert aTestFile to a file:// URI
    var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
        .getService(Ci.mozIJSSubScriptLoader);
    var ioService = Cc['@mozilla.org/network/io-service;1']
        .getService(Ci.nsIIOService);

    loader.loadSubScript("chrome://mochikit/content/chrome-harness.js");
    var jar = getJar(getRootDirectory(window.location.href));
    var dir = jar ?
                extractJarToTmp(jar) :
                getChromeDir(getResolvedURI(window.location.href));
    var fileURI = ioService.newFileURI(dir).spec + aTestFile;

    $("testframe").src = fileURI;

    SimpleTest.waitForExplicitFinish();
}

function InitDetectorTests()
{
    var prefService = Cc["@mozilla.org/preferences-service;1"]
        .getService(Ci.nsIPrefBranch);
    var str = Cc["@mozilla.org/supports-string;1"]
        .createInstance(Ci.nsISupportsString);

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
        } catch (e) {
            gExpectedCharset = "ISO-8859-8";
        }
    }
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

