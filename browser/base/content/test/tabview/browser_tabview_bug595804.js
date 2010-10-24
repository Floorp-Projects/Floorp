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
 * The Original Code is tabview Option For Not Animating Zoom (Bug 595804) test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
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

var prefsBranch = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService).
                  getBranch("browser.panorama.");

function animateZoom() prefsBranch.getBoolPref("animate_zoom");

let contentWindow = null;

registerCleanupFunction(function() {
  // reset to default: true
  prefsBranch.setBoolPref("animate_zoom", true);
});

function test() {
  waitForExplicitFinish();
  
  ok(!TabView.isVisible(), "Tab View is not visible");
  
  window.addEventListener("tabviewshown", startTesting, false);
  TabView.toggle();
}

function startTesting() {
  window.removeEventListener("tabviewshown", startTesting, false);
  
  contentWindow = document.getElementById("tab-view").contentWindow;
  
  ok(TabView.isVisible(), "Tab View is visible");
  
  ok(animateZoom(), "By default, we want to animate");
  
  function finishOnHidden() {
    contentWindow.removeEventListener("tabviewhidden", finishOnHidden, false);
    ok(!TabView.isVisible(), "Tab View is not visible");
    finish();
  }
  
  function wrapup() {
    contentWindow.addEventListener("tabviewhidden", finishOnHidden, false);
    TabView.hide();
  }

  function part2() {
    prefsBranch.setBoolPref("animate_zoom", false);
    ok(!animateZoom(), "animate_zoom = false");
    zoomInAndOut(false, wrapup);
  }

  function part1() {
    prefsBranch.setBoolPref("animate_zoom",true);
    ok(animateZoom(), "animate_zoom = true");
    zoomInAndOut(true, part2);
  }
    
  // start it up!
  part1();
}
  
function zoomInAndOut(transitionsExpected, callback) {
  let transitioned = 0;

  contentWindow.document.addEventListener("transitionend", onTransitionEnd, false);
  function onTransitionEnd(event) {
    transitioned++;
  }

  let onHidden = function() {
    contentWindow.removeEventListener("tabviewhidden", onHidden, false);
    if (transitionsExpected)
      ok(transitioned >= 0, "There can be transitions");
    else
      ok(!transitioned, "There should have been no transitions");
    TabView.toggle();
  };
  
  let onShownAgain = function() {
    contentWindow.removeEventListener("tabviewshown", onShownAgain, false);
    if (transitionsExpected)
      ok(transitioned >= 0, "There can be transitions");
    else
      ok(!transitioned, "There should have been no transitions");
    callback();
  };
  
  contentWindow.addEventListener("tabviewhidden", onHidden, false);
  contentWindow.addEventListener("tabviewshown", onShownAgain, false);
  
  // get this party started by hiding tab view
  TabView.toggle();
}
