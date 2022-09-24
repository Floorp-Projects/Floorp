/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

 const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

 function onSiteDelete() {
     let URLBoxs = document.getElementsByClassName("URLBox");
     for (let i = 0; i < URLBoxs.length; i++) {
         URLBoxs[i].value = "";
     }
 }
 
 function onLoad() {
     let elem = document.getElementsByClassName("URLBox");
     for (let i = 0; i < elem.length; i++) {
         var num = i + 1;
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
         //   if(boxs[i].value == ""){
 
         //   }
         if (!box_value.match(match_https)) { /* Checks if url in the sidebar contains https in the beginning of a line */
             if (!box_value.match(match_extension)) { /* Checks if given URL is an extension */
                 box_value = `https://${box_value}`;
             }
         }
         var num = i + 1;
         Services.prefs.setStringPref(`floorp.browser.sidebar2.customurl${num}`, box_value);
     }
 }