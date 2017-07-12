/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(setup_UITourTest);

var gTestTab;
var gContentAPI;
var gContentWindow;

add_UITour_task(async function test_openSearchPanel() {
  let urlbar = window.document.getElementById("urlbar");
  urlbar.focus();
  await showMenuPromise("urlbar");
  is(urlbar.popup.state, "open", "Popup was opened");
  is(urlbar.controller.searchString, "Firefox", "Search string is Firefox");
  urlbar.popup.closePopup();
  is(urlbar.popup.state, "closed", "Popup was closed");
});
