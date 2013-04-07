/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource:///modules/HUDService.jsm", tempScope);
let HUDService = tempScope.HUDService;

function test()
{
  waitForExplicitFinish();

  // To test for this bug we open a Scratchpad window, save its
  // reference and then open another one. This way the first window
  // loses its focus.
  //
  // Then we open a web console and execute a console.log statement
  // from the first Scratch window (that's why we needed to save its
  // reference).
  //
  // Then we wait for our message to appear in the console and click
  // on the location link. After that we check which Scratchpad window
  // is currently active (it should be the older one).

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);

    openScratchpad(function () {
      let sw = gScratchpadWindow;

      openScratchpad(function () {
        function onWebConsoleOpen(subj) {
          Services.obs.removeObserver(onWebConsoleOpen,
            "web-console-created");
          subj.QueryInterface(Ci.nsISupportsString);

          let hud = HUDService.getHudReferenceById(subj.data);
          hud.jsterm.clearOutput(true);
          executeSoon(testFocus.bind(null, sw, hud));
        }

        Services.obs.
          addObserver(onWebConsoleOpen, "web-console-created", false);

        HUDService.consoleUI.toggleHUD();
      });
    });
  }, true);

  content.location = "data:text/html;charset=utf8,<p>test window focus for Scratchpad.";
}

function testFocus(sw, hud) {
  let sp = sw.Scratchpad;

  function onMessage(subj) {
    Services.obs.removeObserver(onMessage, "web-console-message-created");

    var loc = hud.jsterm.outputNode.querySelector(".webconsole-location");
    ok(loc, "location element exists");
    is(loc.value, sw.Scratchpad.uniqueName + ":1",
        "location value is correct");

    sw.addEventListener("focus", function onFocus() {
      sw.removeEventListener("focus", onFocus, true);

      let win = Services.wm.getMostRecentWindow("devtools:scratchpad");

      ok(win, "there are active Scratchpad windows");
      is(win.Scratchpad.uniqueName, sw.Scratchpad.uniqueName,
          "correct window is in focus");

      // gScratchpadWindow will be closed automatically but we need to
      // close the second window ourselves.
      sw.close();
      finish();
    }, true);

    // Simulate a click on the "Scratchpad/N:1" link.
    EventUtils.synthesizeMouse(loc, 2, 2, {}, hud.iframeWindow);
  }

  // Sending messages to web console is an asynchronous operation. That's
  // why we have to setup an observer here.
  Services.obs.addObserver(onMessage, "web-console-message-created", false);

  sp.setText("console.log('foo');");
  sp.run().then(function ([selection, error, result]) {
    is(selection, "console.log('foo');", "selection is correct");
    is(error, undefined, "error is correct");
    is(result, undefined, "result is correct");
  });
}
