/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla's layout acceptance tests.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const CC = Components.classes;
const CI = Components.interfaces;
const CR = Components.results;

const XHTML_NS = "http://www.w3.org/1999/xhtml";

const NS_LOCAL_FILE_CONTRACTID = "@mozilla.org/file/local;1";
const IO_SERVICE_CONTRACTID = "@mozilla.org/network/io-service;1";
const NS_LOCALFILEINPUTSTREAM_CONTRACTID =
          "@mozilla.org/network/file-input-stream;1";
const NS_SCRIPTSECURITYMANAGER_CONTRACTID =
          "@mozilla.org/scriptsecuritymanager;1";
const NS_REFTESTHELPER_CONTRACTID =
          "@mozilla.org/reftest-helper;1";

const LOAD_FAILURE_TIMEOUT = 30000; // ms

var gBrowser;
var gCanvas1, gCanvas2;
var gURLs;
var gState;
var gFailureTimeout;
var gServer;
var gCount = 0;

var gIOService;
var gReftestHelper;

const EXPECTED_PASS = 0;
const EXPECTED_FAIL = 1;
const EXPECTED_RANDOM = 2;
const EXPECTED_DEATH = 3;  // test must be skipped to avoid e.g. crash/hang

const HTTP_SERVER_PORT = 4444;

function OnRefTestLoad()
{
    gBrowser = document.getElementById("browser");

    gBrowser.addEventListener("load", OnDocumentLoad, true);

     try {
        gReftestHelper = CC[NS_REFTESTHELPER_CONTRACTID].getService(CI.nsIReftestHelper);
    } catch (e) {
        gReftestHelper = null;
    }

    var windowElem = document.documentElement;

    gCanvas1 = document.createElementNS(XHTML_NS, "canvas");
    gCanvas1.setAttribute("width", windowElem.getAttribute("width"));
    gCanvas1.setAttribute("height", windowElem.getAttribute("height"));

    gCanvas2 = document.createElementNS(XHTML_NS, "canvas");
    gCanvas2.setAttribute("width", windowElem.getAttribute("width"));
    gCanvas2.setAttribute("height", windowElem.getAttribute("height"));

    gIOService = CC[IO_SERVICE_CONTRACTID].getService(CI.nsIIOService);

    try {
        ReadTopManifest(window.arguments[0]);
        if (gServer)
            gServer.start(HTTP_SERVER_PORT);
        StartCurrentTest();
    } catch (ex) {
        //gBrowser.loadURI('data:text/plain,' + ex);
        dump("REFTEST EXCEPTION: " + ex);
        DoneTests();
    }
}

function OnRefTestUnload()
{
    gBrowser.removeEventListener("load", OnDocumentLoad, true);
}

function ReadTopManifest(aFileURL)
{
    gURLs = new Array();
    var url = gIOService.newURI(aFileURL, null, null);
    if (!url || !url.schemeIs("file"))
        throw "Expected a file URL for the manifest.";
    ReadManifest(url);
}

function ReadManifest(aURL)
{
    var listURL = aURL.QueryInterface(CI.nsIFileURL);

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    var fis = CC[NS_LOCALFILEINPUTSTREAM_CONTRACTID].
                  createInstance(CI.nsIFileInputStream);
    fis.init(listURL.file, -1, -1, false);
    var lis = fis.QueryInterface(CI.nsILineInputStream);

    var sandbox = new Components.utils.Sandbox(aURL.spec);
    for (var prop in gAutoconfVars)
        sandbox[prop] = gAutoconfVars[prop];

    var line = {value:null};
    var lineNo = 0;
    do {
        var more = lis.readLine(line);
        ++lineNo;
        var str = line.value;
        str = /^[^#]*/.exec(str)[0]; // strip everything after "#"
        if (!str)
            continue; // entire line was a comment
        // strip leading and trailing whitespace
        str = str.replace(/^\s*/, '').replace(/\s*$/, '');
        if (!str || str == "")
            continue;
        var items = str.split(/\s+/); // split on whitespace

        var expected_status = EXPECTED_PASS;
        while (items[0].match(/^(fails|random|skip)/)) {
            var item = items.shift();
            var stat;
            var cond;
            var m = item.match(/^(fails|random|skip)-if(\(.*\))$/);
            if (m) {
                stat = m[1];
                // Note: m[2] contains the parentheses, and we want them.
                cond = Components.utils.evalInSandbox(m[2], sandbox);
            } else if (item.match(/^(fails|random|skip)$/)) {
                stat = item;
                cond = true;
            } else {
                throw "Error in manifest file " + aURL.spec + " line " + lineNo;
            }

            if (cond) {
                if (stat == "fails") {
                    expected_status = EXPECTED_FAIL;
                } else if (stat == "random") {
                    expected_status = EXPECTED_RANDOM;
                } else if (stat == "skip") {
                    expected_status = EXPECTED_DEATH;
                }
            }
        }

        var runHttp = items[0] == "HTTP";
        if (runHttp)
            items.shift();

        if (items[0] == "include") {
            if (items.length != 2 || runHttp)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo;
            var incURI = gIOService.newURI(items[1], null, listURL);
            secMan.checkLoadURI(aURL, incURI,
                                CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            ReadManifest(incURI);
        } else if (items[0] == "==" || items[0] == "!=") {
            if (items.length != 3)
                throw "Error in manifest file " + aURL.spec + " line " + lineNo;
            var [testURI, refURI] = runHttp
                                  ? ServeFiles(aURL,
                                               listURL.file.parent, items[1], items[2])
                                  : [gIOService.newURI(items[1], null, listURL),
                                     gIOService.newURI(items[2], null, listURL)];
            var prettyPath = runHttp
                           ? gIOService.newURI(items[1], null, listURL).spec
                           : testURI.spec;
            secMan.checkLoadURI(aURL, testURI,
                                CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            secMan.checkLoadURI(aURL, refURI,
                                CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
            gURLs.push( { equal: (items[0] == "=="),
                          expected: expected_status,
                          prettyPath: prettyPath,
                          url1: testURI,
                          url2: refURI } );
        } else {
            throw "Error in manifest file " + aURL.spec + " line " + lineNo;
        }
    } while (more);
}

function ServeFiles(manifestURL, directory, file1, file2)
{
    if (!gServer)
        gServer = CC["@mozilla.org/server/jshttp;1"].
                      createInstance(CI.nsIHttpServer);

    gCount++;
    var path = "/" + gCount + "/";
    gServer.registerDirectory(path, directory);

    var testURI = gIOService.newURI("http://localhost:" + HTTP_SERVER_PORT +
                                    path + file1,
                                    null, null);
    var refURI = gIOService.newURI("http://localhost:" + HTTP_SERVER_PORT +
                                   path + file2,
                                   null, null);

    var secMan = CC[NS_SCRIPTSECURITYMANAGER_CONTRACTID]
                     .getService(CI.nsIScriptSecurityManager);

    // XXX necessary?  manifestURL guaranteed to be file, others always HTTP
    secMan.checkLoadURI(manifestURL, testURI,
                        CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);
    secMan.checkLoadURI(manifestURL, refURI,
                        CI.nsIScriptSecurityManager.DISALLOW_SCRIPT);

    return [testURI, refURI];
}

function StartCurrentTest()
{
    // make sure we don't run tests that are expected to kill the browser
    while (gURLs.length > 0 && gURLs[0].expected == EXPECTED_DEATH) {
        dump("REFTEST KNOWN FAIL (SKIP): " + gURLs[0].url1.spec + "\n");
        gURLs.shift();
    }

    if (gURLs.length == 0)
        DoneTests();
    else
        StartCurrentURI(1);
}

function StartCurrentURI(aState)
{
    gFailureTimeout = setTimeout(LoadFailed, LOAD_FAILURE_TIMEOUT);

    gState = aState;
    gBrowser.loadURI(gURLs[0]["url" + aState].spec);
}

function DoneTests()
{
    if (gServer)
        gServer.stop();
    goQuitApplication();
}

function CanvasToURL(canvas)
{
    var ctx = whichCanvas.getContext("2d");
    return canvas.toDataURL();
}

function OnDocumentLoad(event)
{
    if (event.target != gBrowser.contentDocument)
        // Ignore load events for subframes.
        return;

    var contentRootElement = gBrowser.contentDocument.documentElement;

    function shouldWait() {
        // use getAttribute because className works differently in HTML and SVG
        return contentRootElement.hasAttribute('class') &&
               contentRootElement.getAttribute('class').split(/\s+/)
                                 .indexOf("reftest-wait") != -1;
    }

    function doPrintMode() {
        // use getAttribute because className works differently in HTML and SVG
        return contentRootElement.hasAttribute('class') &&
               contentRootElement.getAttribute('class').split(/\s+/)
                                 .indexOf("reftest-print") != -1;
    }

    if (shouldWait()) {
        // The testcase will let us know when the test snapshot should be made.
        // Register a mutation listener to know when the 'reftest-wait' class
        // gets removed.
        contentRootElement.addEventListener(
            "DOMAttrModified",
            function(event) {
                if (!shouldWait()) {
                    contentRootElement.removeEventListener(
                        "DOMAttrModified",
                        arguments.callee,
                        false);
                    setTimeout(DocumentLoaded, 0);
                }
            }, false);
    } else {
        if (doPrintMode()) {
            var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                    .getService(Components.interfaces.nsIPrintSettingsService);
            var ps = PSSVC.newPrintSettings;
            ps.paperWidth = 5;
            ps.paperHeight = 3;
            ps.headerStrLeft = "";
            ps.headerStrCenter = "";
            ps.headerStrRight = "";
            ps.footerStrLeft = "";
            ps.footerStrCenter = "";
            ps.footerStrRight = "";
            gBrowser.docShell.contentViewer.setPageMode(true, ps);
        }

        // Since we can't use a bubbling-phase load listener from chrome,
        // this is a capturing phase listener.  So do setTimeout twice, the
        // first to get us after the onload has fired in the content, and
        // the second to get us after any setTimeout(foo, 0) in the content.
        setTimeout(setTimeout, 0, DocumentLoaded, 0);
    }
}

function DocumentLoaded()
{
    clearTimeout(gFailureTimeout);

    var canvas;

    if (gState == 1)
        canvas = gCanvas1;
    else
        canvas = gCanvas2;

    /* XXX This needs to be rgb(255,255,255) because otherwise we get
     * black bars at the bottom of every test that are different size
     * for the first test and the rest (scrollbar-related??) */
    canvas.getContext("2d").drawWindow(gBrowser.contentWindow, 0, 0,
                                       canvas.width, canvas.height, "rgb(255,255,255)");

    switch (gState) {
        case 1:
            // First document has been loaded.
            // Proceed to load the second document.

            StartCurrentURI(2);
            break;
        case 2:
            // Both documents have been loaded. Compare the renderings and see
            // if the comparison result matches the expected result specified
            // in the manifest.

            // number of different pixels
            var differences;
            // whether the two renderings match:
            var equal;

            if (gReftestHelper) {
                differences = gReftestHelper.compareCanvas(gCanvas1, gCanvas2);
                equal = (differences == 0);
            } else {
                differences = -1;
                var k1 = gCanvas1.toDataURL();
                var k2 = gCanvas2.toDataURL();
                equal = (k1 == k2);
            }

            // whether the comparison result matches what is in the manifest
            var test_passed = (equal == gURLs[0].equal);
            // what is expected on this platform (PASS, FAIL, or RANDOM)
            var expected = gURLs[0].expected;
            
            var outputs = {};
            const randomMsg = " (RESULT EXPECTED TO BE RANDOM)";
            outputs[EXPECTED_PASS] = {true: "PASS",
                                      false: "UNEXPECTED FAIL"};
            outputs[EXPECTED_FAIL] = {true: "UNEXPECTED PASS",
                                      false: "KNOWN FAIL"};
            outputs[EXPECTED_RANDOM] = {true: "PASS" + randomMsg,
                                        false: "KNOWN FAIL" + randomMsg};
            
            var result = "REFTEST " + outputs[expected][test_passed] + ": ";
            if (!gURLs[0].equal) {
                result += "(!=) ";
            }
            result += gURLs[0].prettyPath; // the URL being tested
            dump(result + "\n");
            if (!test_passed && expected == EXPECTED_PASS) {
                dump("REFTEST   IMAGE 1 (TEST): " + gCanvas1.toDataURL() + "\n");
                dump("REFTEST   IMAGE 2 (REFERENCE): " + gCanvas2.toDataURL() + "\n");
                dump("REFTEST number of differing pixels: " + differences + "\n");
            }

            gURLs.shift();
            StartCurrentTest();
            break;
        default:
            throw "Unexpected state.";
    }
}

function LoadFailed()
{
    dump("REFTEST UNEXPECTED FAIL (LOADING): " +
         gURLs[0]["url" + gState].spec + "\n");
    gURLs.shift();
    StartCurrentTest();
}
