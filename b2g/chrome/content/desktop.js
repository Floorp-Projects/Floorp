/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Enable touch event shim on desktop that translates mouse events
// into touch ones
function enableTouch() {
  let require = Cu.import('resource://gre/modules/devtools/Loader.jsm', {})
                  .devtools.require;
  let { TouchEventHandler } = require('devtools/touch-events');
  let chromeEventHandler = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                 .getInterface(Ci.nsIWebNavigation)
                                 .QueryInterface(Ci.nsIDocShell)
                                 .chromeEventHandler || window;
  let touchEventHandler = new TouchEventHandler(chromeEventHandler);
  touchEventHandler.start();
}

function setupButtons() {
  let homeButton = document.getElementById('home-button');
  if (!homeButton) {
    // The toolbar only exists in b2g desktop build with
    // FXOS_SIMULATOR turned on.
    return;
  }
  // The touch event helper is enabled on shell.html document,
  // so that click events are delayed and it is better to
  // listen for touch events.
  homeButton.addEventListener('touchstart', function() {
    shell.sendChromeEvent({type: 'home-button-press'});
    homeButton.classList.add('active');
  });
  homeButton.addEventListener('touchend', function() {
    shell.sendChromeEvent({type: 'home-button-release'});
    homeButton.classList.remove('active');
  });

  Cu.import("resource://gre/modules/GlobalSimulatorScreen.jsm");
  let rotateButton = document.getElementById('rotate-button');
  rotateButton.addEventListener('touchstart', function () {
    rotateButton.classList.add('active');
  });
  rotateButton.addEventListener('touchend', function() {
    GlobalSimulatorScreen.flipScreen();
    rotateButton.classList.remove('active');
  });
}

window.addEventListener('ContentStart', function() {
  enableTouch();
  setupButtons();
});
