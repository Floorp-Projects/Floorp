/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

function onSiteDelete() {
  let URLBoxs = document.getElementsByClassName("URLBox");
   for (let i = 0; i < URLBoxs.length; i++) {
     URLBoxs[i].value = "";
   }
}

function onLoad(){
   let elem = document.getElementsByClassName("URLBox");
    for (let i = 0; i < elem.length; i++) {
        var num = i + 1;
        var url = Services.prefs.getStringPref("floorp.browser.sidebar2.customurl" + num ,undefined);
        elem[i].value = url;
     }
}

function onunload() {
    let boxs = document.getElementsByClassName("URLBox");
    for (let i = 0; i < boxs.length; i++) {
      if(boxs[i].value == ""){
        
      }
      else if(!boxs[i].value.match(/https?:\/\//)){
        boxs[i].value = "https://" + boxs[i].value;
      }
      var num = i + 1;
      Services.prefs.setStringPref("floorp.browser.sidebar2.customurl" + num, boxs[i].value);
    }
}
