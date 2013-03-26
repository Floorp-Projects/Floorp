/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- /
/* vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CC = Components.classes;
const CI = Components.interfaces;
const CR = Components.results;

const XHTML_NS = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const NS_GFXINFO_CONTRACTID = "@mozilla.org/gfx/info;1";

var gContainingWindow = null;

var gBrowser;

function OnDocumentLoad() {
    gContainingWindow.close();
}

this.OnRecordingLoad = function OnRecordingLoad(win) {
    var prefs = Components.classes["@mozilla.org/preferences-service;1"].
                getService(Components.interfaces.nsIPrefBranch);

    if (win === undefined || win == null) {
        win = window;
    }
    if (gContainingWindow == null && win != null) {
        gContainingWindow = win;
    }

    gBrowser = gContainingWindow.document.getElementById("browser");

    var gfxInfo = (NS_GFXINFO_CONTRACTID in CC) && CC[NS_GFXINFO_CONTRACTID].getService(CI.nsIGfxInfo);
    var info = gfxInfo.getInfo();
    dump(info.AzureContentBackend);
    if (info.AzureContentBackend == "none") {
        alert("Page recordings may only be made with Azure content enabled.");
        gContainingWindow.close();
        return;
    }

    gContainingWindow.document.addEventListener("load", OnDocumentLoad, true);

    var args = window.arguments[0].wrappedJSObject;

    gBrowser.loadURI(args.uri);
}

function OnRecordingUnload() {
}
