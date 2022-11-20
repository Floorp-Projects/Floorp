/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
var { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

// startup fnc
  setBrowserDesign();

//-------------------------------------------------------------------------Observer----------------------------------------------------------------------------
function updateDesign(){
  let designID = document.getElementById("browserdesgin");
  if (designID)designID.remove();
  setBrowserDesign();
  URLbarrecalculation();
  window.setTimeout(setMultirowTabMaxHeight, 1000);
 }

Services.prefs.addObserver("floorp.browser.user.interface",updateDesign);
Services.obs.addObserver(updateDesign, "update-photon-pref");
