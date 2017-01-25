/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const BUTTON_POSITION_CANCEL = 1;
const BUTTON_POSITION_DONT_SAVE = 2;


function test()
{
  waitForExplicitFinish();

  // Observer must be attached *before* Scratchpad is opened.
  CloseObserver.init();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function () {
    openScratchpad(runTests);
  }, {capture: true, once: true});

  content.location = "data:text/html;charset=utf8,<p>test browser last window closing</p>";
}



function runTests({ Scratchpad })
{
  let browser = Services.wm.getEnumerator("navigator:browser").getNext();
  let oldPrompt = Services.prompt;
  let button;

  Services.prompt = {
    confirmEx: () => button
  };


  Scratchpad.dirty = true;

  // Test canceling close.
  button = BUTTON_POSITION_CANCEL;
  CloseObserver.expectedValue = true;
  browser.BrowserTryToCloseWindow();

  // Test accepting close.
  button = BUTTON_POSITION_DONT_SAVE;
  CloseObserver.expectedValue = false;
  browser.BrowserTryToCloseWindow();

  // Test closing without prompt needed.
  Scratchpad.dirty = false;
  browser.BrowserTryToCloseWindow();

  Services.prompt = oldPrompt;
  CloseObserver.uninit();
  finish();
}


var CloseObserver = {
  expectedValue: null,
  init: function ()
  {
    Services.obs.addObserver(this, "browser-lastwindow-close-requested", false);
  },

  observe: function (aSubject)
  {
    aSubject.QueryInterface(Ci.nsISupportsPRBool);
    let message = this.expectedValue ? "close" : "stay open";
    ok(this.expectedValue === aSubject.data, "Expected browser to " + message);
    aSubject.data = true;
  },

  uninit: function ()
  {
    Services.obs.removeObserver(this, "browser-lastwindow-close-requested");
  },
};
