/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

// Floorp のデザイン設定を処理
  setBrowserDesign();

//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------

Services.prefs.addObserver("floorp.browser.user.interface", function(){
  document.getElementById("browserdesgin").remove(); 
  setBrowserDesign();
  URLbarrecalculation();
 }   
)