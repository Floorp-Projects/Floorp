/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 et sw=4 tw=80: */
var gExpectedCharset;
var gOldPref;
var gDetectorList;
var gTestIndex;

function CharsetDetectionTests(aTestFile, aExpectedCharset, aDetectorList)
{
    gExpectedCharset = aExpectedCharset;
    gDetectorList = aDetectorList;

    InitDetectorTests();

    $("testframe").src = aTestFile;

    SimpleTest.waitForExplicitFinish();
}

function InitDetectorTests()
{
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
        .getService(Components.interfaces.nsIPrefBranch);
    var str = Components.classes["@mozilla.org/supports-string;1"]
        .createInstance(Components.interfaces.nsISupportsString);

    try {
        gOldPref = prefService
            .getComplexValue("intl.charset.detector",
                             Components.interfaces.nsIPrefLocalizedString).data;
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
                                 Components.interfaces.nsIPrefLocalizedString)
                .data;
        } catch (e) {
            gExpectedCharset = "ISO-8859-8";
        }
    }
}

function SetDetectorPref(aPrefValue)
{
    var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                      .getService(Components.interfaces.nsIPrefBranch);
    var str = Components.classes["@mozilla.org/supports-string;1"]
              .createInstance(Components.interfaces.nsISupportsString);
    str.data = aPrefValue;
    prefService.setComplexValue("intl.charset.detector",
                                Components.interfaces.nsISupportsString, str);
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

