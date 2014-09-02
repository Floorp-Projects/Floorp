/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
let isMulet = "ResponsiveUI" in browserWindow;

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

function checkDebuggerPort() {
  // XXX: To be removed once bug 942756 lands.
  // We are hacking 'unix-domain-socket' pref by setting a tcp port (number).
  // DebuggerServer.openListener detects that it isn't a file path (string),
  // and starts listening on the tcp port given here as command line argument.

  // Get the command line arguments that were passed to the b2g client
  let args;
  try {
    let service = Cc["@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"].getService(Ci.nsISupports);
    args = service.wrappedJSObject.cmdLine;
  } catch(e) {}

  if (!args) {
    return;
  }

  let dbgport;
  try {
    dbgport = args.handleFlagWithParam('start-debugger-server', false);
  } catch(e) {}

  if (dbgport) {
    dump('Opening debugger server on ' + dbgport + '\n');
    Services.prefs.setCharPref('devtools.debugger.unix-domain-socket', dbgport);
    navigator.mozSettings.createLock().set(
      {'debugger.remote-mode': 'adb-devtools'});
  }
}


function initResponsiveDesign() {
  Cu.import('resource:///modules/devtools/responsivedesign.jsm');
  ResponsiveUIManager.on('on', function(event, {tab:tab}) {
    let responsive = tab.__responsiveUI;
    let document = tab.ownerDocument;

    // Only tweak reponsive mode for shell.html tabs.
    if (tab.linkedBrowser.contentWindow != window) {
      return;
    }

    responsive.buildPhoneUI();

    responsive.rotatebutton.addEventListener('command', function (evt) {
      GlobalSimulatorScreen.flipScreen();
      evt.stopImmediatePropagation();
      evt.preventDefault();
    }, true);

    // Enable touch events
    browserWindow.gBrowser.selectedTab.__responsiveUI.enableTouch();
  });

  // Automatically toggle responsive design mode
  let width = 320, height = 480;
  // We have to take into account padding and border introduced with the
  // device look'n feel:
  width += 15*2; // Horizontal padding
  width += 1*2; // Vertical border
  height += 60; // Top Padding
  height += 1; // Top border
  let args = {'width': width, 'height': height};
  let mgr = browserWindow.ResponsiveUI.ResponsiveUIManager;
  mgr.toggle(browserWindow, browserWindow.gBrowser.selectedTab);
  let responsive = browserWindow.gBrowser.selectedTab.__responsiveUI;
  responsive.setSize(width, height);

}

function openDevtools() {
  // Open devtool panel while maximizing its size according to screen size
  Services.prefs.setIntPref('devtools.toolbox.sidebar.width',
                            browserWindow.outerWidth - 550);
  Services.prefs.setCharPref('devtools.toolbox.host', 'side');
  let {gDevTools} = Cu.import('resource:///modules/devtools/gDevTools.jsm', {});
  let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  let target = devtools.TargetFactory.forTab(browserWindow.gBrowser.selectedTab);
  gDevTools.showToolbox(target);
}

window.addEventListener('ContentStart', function() {
  // On Firefox Mulet, touch events are enabled within the responsive mode
  if (!isMulet) {
    enableTouch();
  }
  setupButtons();
  checkDebuggerPort();
  // On Firefox mulet, we automagically enable the responsive mode
  // and show the devtools
  if (isMulet) {
    initResponsiveDesign(browserWindow);
    openDevtools();
  }
});
