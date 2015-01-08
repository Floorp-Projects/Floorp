/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1111433: Send out the SelectionStateChanged event with Blur state

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.setSelectionChangeEnabledPref(true);
browserElementTestHelpers.addPermission();

var mm;
var iframe;

var changefocus = function () {
  var elt = content.document.getElementById("text");
  if (elt) {
    elt.focus();
    elt.select();
    elt.blur();
  }
}

function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);

  mm = SpecialPowers.getBrowserFrameMessageManager(iframe);

  iframe.src = "data:text/html,<html><body>" +
               "<textarea id='text'> Bug 1111433 </textarea>"+
               "</body></html>";

  var loadtime = 0;
  iframe.addEventListener("mozbrowserloadend", function onloadend(e) {
    loadtime++;
    if (loadtime === 2) {
      iframe.removeEventListener("mozbrowserloadend", onloadend);
      SimpleTest.executeSoon(function() { testBlur(e); });
    }
  });
}

function testBlur(e) {
  iframe.addEventListener("mozbrowserselectionstatechanged", function selectionstatechanged(e) {
    iframe.removeEventListener("mozbrowserselectionstatechanged", selectionstatechanged, true);
    ok(e.detail.states.indexOf('blur') == 0, "received state  " + e.detail.states);
    SimpleTest.finish();
  }, true);

  iframe.focus();
  mm.loadFrameScript('data:,(' + changefocus.toString() + ')();', false);
}

addEventListener('testready', runTest);
