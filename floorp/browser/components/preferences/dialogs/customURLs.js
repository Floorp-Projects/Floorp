/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from /toolkit/content/preferencesBindings.js */


function onSiteDelete() {
    let URLBoxs = document.getElementsByClassName("URLBox");
    for (let i = 0; i < URLBoxs.length; i++) {
        URLBoxs[i].value = "";
    }
}

function onLoad() {
    let elem = document.getElementsByClassName("URLBox");
    for (let i = 0; i < elem.length; i++) {
        var num = i;
        var url = Services.prefs.getStringPref(`floorp.browser.sidebar2.customurl${num}`, undefined);
        elem[i].value = url;
    }
}

function onunload() {
    let box = document.getElementsByClassName("URLBox");
    var remove_whitespace = /^\s+/
    var match_https = /^https?:\/\//
    var match_extension = /^moz-extension?:\/\//
    for (let i = 0; i < box.length; i++) {
        var box_value = box[i].value
        box_value = box_value.replace(remove_whitespace, ""); /* Removing whitespace from the beginning of a line */
        if (box_value == "") {

        }
        else if (!box_value.match(match_https)) { /* Checks if url in the sidebar contains https in the beginning of a line */
            if (!box_value.match(match_extension)) { /* Checks if given URL is an extension */
                box_value = `https://${box_value}`;
            }
        }
        var num = i;
        Services.prefs.setStringPref(`floorp.browser.sidebar2.customurl${num}`, box_value);
    }
}

Preferences.addAll([
    { id: "floorp.browser.sidebar2.customurl0.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl1.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl2.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl3.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl4.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl5.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl6.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl7.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl8.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl9.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl10.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl11.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl12.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl13.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl14.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl15.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl16.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl17.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl18.usercontext", type : "int"},
    { id: "floorp.browser.sidebar2.customurl19.usercontext", type : "int"},
]);