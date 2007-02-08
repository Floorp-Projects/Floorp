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

var gBrowser;
var gProgressListener;
var gCanvas;
var gURLs;
var gState;
var gPart1Key;

const EXPECTED_PASS = 0;
const EXPECTED_FAIL = 1;
const EXPECTED_RANDOM = 2;

function OnRefTestLoad()
{
    gBrowser = document.getElementById("browser");
    gProgressListener = new RefTestProgressListener;
    gBrowser.webProgress.addProgressListener(gProgressListener,
         CI.nsIWebProgress.NOTIFY_STATE_NETWORK);

    gCanvas = document.createElementNS(XHTML_NS, "canvas");
    var windowElem = document.documentElement;
    gCanvas.setAttribute("width", windowElem.getAttribute("width"));
    gCanvas.setAttribute("height", windowElem.getAttribute("height"));

    try {
        ReadTopManifest(window.arguments[0]);
        StartCurrentURI();
    } catch (ex) {
        gBrowser.loadURI('data:text/plain,' + ex);
    }
}

function OnRefTestUnload()
{
    gBrowser.webProgress.removeProgressListener(gProgressListener);
}

function ReadTopManifest(aFileURL)
{
    gURLs = new Array();
    var ios = CC[IO_SERVICE_CONTRACTID].getService(CI.nsIIOService);
    var url = ios.newURI(aFileURL, null, null);
    if (!url || !url.schemeIs("file"))
        throw "Expected a file URL for the manifest.";
    ReadManifest(url);
}

function ReadManifest(aURL)
{
    var ios = CC[IO_SERVICE_CONTRACTID].getService(CI.nsIIOService);
    var listURL = aURL.QueryInterface(CI.nsIFileURL);

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
        while (items[0].match(/^(fails|random)/)) {
            var item = items.shift();
            var stat;
            var cond;
            var m = item.match(/^(fails|random)-if(\(.*\))$/);
            if (m) {
                stat = m[1];
                // Note: m[2] contains the parentheses, and we want them.
                cond = Components.utils.evalInSandbox(m[2], sandbox);
            } else if (item.match(/^(fails|random)$/)) {
                stat = item;
                cond = true;
            } else {
                throw "Error in manifest file " + aURL + " line " + lineNo;
            }

            if (cond) {
                if (stat == "fails") {
                    expected_status = EXPECTED_FAIL;
                } else if (stat == "random") {
                    expected_status = EXPECTED_RANDOM;
                }
            }
        }

        if (items[0] == "include") {
            if (items.length != 2)
                throw "Error in manifest file " + aURL + " line " + lineNo;
            ReadManifest(ios.newURI(items[1], null, listURL));
        } else if (items[0] == "==" || items[0] == "!=") {
            if (items.length != 3)
                throw "Error in manifest file " + aURL + " line " + lineNo;
            gURLs.push( { equal: (items[0] == "=="),
                          expected: expected_status,
                          url1: ios.newURI(items[1], null, listURL),
                          url2: ios.newURI(items[2], null, listURL)} );
        } else {
            throw "Error in manifest file " + aURL + " line " + lineNo;
        }
    } while (more);
}

function StartCurrentURI()
{
    gState = 1;
    gBrowser.loadURI(gURLs[0].url1.spec);
}

function DoneTests()
{
    goQuitApplication();
}

function IFrameToKey()
{
    var ctx = gCanvas.getContext("2d");
    /* XXX This needs to be rgb(255,255,255) because otherwise we get
     * black bars at the bottom of every test that are different size
     * for the first test and the rest (scrollbar-related??) */
    ctx.drawWindow(gBrowser.contentWindow, 0, 0,
                   gCanvas.width, gCanvas.height, "rgb(255,255,255)");
    return gCanvas.toDataURL();
}

function DocumentLoaded()
{
    var key = IFrameToKey();
    switch (gState) {
        case 1:
            gPart1Key = key;

            gState = 2;
            gBrowser.loadURI(gURLs[0].url2.spec);
            break;
        case 2:
            var equal = (key == gPart1Key);
            var result = "REFTEST ";
            var test_passed = (equal == gURLs[0].equal);
            var expected = gURLs[0].expected;
            switch (expected) {
                case EXPECTED_PASS:
                    if (!test_passed) result += "UNEXPECTED ";
                    break;
                case EXPECTED_FAIL:
                    if (test_passed) result += "UNEXPECTED ";
                    break;
                case EXPECTED_RANDOM:
                    result += "(RESULT EXPECTED TO BE RANDOM) "
                    break;
            }
            if (test_passed) {
                result += "PASS: ";
            } else {
                result += "FAIL: ";
            }
            if (!gURLs[0].equal) {
                result += "(!=) ";
            }
            result += gURLs[0].url1.spec;
            dump(result + "\n");
            if (!test_passed && expected == EXPECTED_PASS) {
                dump("REFTEST   IMAGE 1 (TEST): " + gPart1Key + "\n");
                dump("REFTEST   IMAGE 2 (REFERENCE): " + key + "\n");
            }

            gPart1Key = undefined;
            gURLs.shift();
            if (gURLs.length == 0)
                DoneTests();
            else
                StartCurrentURI();
            break;
        default:
            throw "Unexpected state."
    }
}

function RefTestProgressListener()
{
}

RefTestProgressListener.prototype = {

    QueryInterface : function(aIID)
    {
      if (aIID.equals(CI.nsIWebProgressListener) ||
          aIID.equals(CI.nsISupportsWeakReference) ||
          aIID.equals(CI.nsISupports))
          return this;
      throw CR.NS_NOINTERFACE;
    },

    // nsIWebProgressListener implementation
    onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
        if (!(aStateFlags & CI.nsIWebProgressListener.STATE_IS_NETWORK) ||
            aWebProgress != gBrowser.webProgress)
            return;

        if (aStateFlags & CI.nsIWebProgressListener.STATE_START) {
            this.mLoading = true;
        } else if (aStateFlags & CI.nsIWebProgressListener.STATE_STOP) {
            if (this.mLoading) {
                this.mLoading = false;
                // Let other things happen in the first 20ms, since this
                // doesn't really seem to be when the page is done loading.
                setTimeout(DocumentLoaded, 20);
            }
        }
    },

    onProgressChange : function(aWebProgress, aRequest,
                                aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress)
    {
    },

    onLocationChange : function(aWebProgress, aRequest, aLocation)
    {
    },

    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
    {
    },

    onSecurityChange : function(aWebProgress, aRequest, aState)
    {
    },

    mLoading : false
}
