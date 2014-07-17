/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  Cu.import("resource://gre/modules/Services.jsm");
  let temp = {}
  Cu.import("resource:///modules/devtools/gDevTools.jsm", temp);
  let DevTools = temp.DevTools;
  Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm", temp);
  let LayoutHelpers = temp.LayoutHelpers;

  Cu.import("resource://gre/modules/devtools/Loader.jsm", temp);
  let devtools = temp.devtools;

  let Toolbox = devtools.Toolbox;

  let toolbox, iframe, target, tab;

  gBrowser.selectedTab = gBrowser.addTab();
  target = TargetFactory.forTab(gBrowser.selectedTab);

  window.addEventListener("message", onMessage);

  iframe = document.createElement("iframe");
  document.documentElement.appendChild(iframe);

  gBrowser.selectedBrowser.addEventListener("load", function onLoad(evt) {
    gBrowser.selectedBrowser.removeEventListener(evt.type, onLoad, true);
    let options = {customIframe: iframe};
    gDevTools.showToolbox(target, null, Toolbox.HostType.CUSTOM, options)
             .then(testCustomHost, console.error)
             .then(null, console.error);
  }, true);

  content.location = "data:text/html,test custom host";

  function onMessage(event) {
    info("onMessage: " + event.data);
    let json = JSON.parse(event.data);
    if (json.name == "toolbox-close") {
      ok("Got the `toolbox-close` message");
      cleanup();
    }
  }

  function testCustomHost(toolbox) {
    is(toolbox.doc.defaultView.top, window, "Toolbox is included in browser.xul");
    is(toolbox.doc, iframe.contentDocument, "Toolbox is in the custom iframe");
    executeSoon(() => gBrowser.removeCurrentTab());
  }

  function cleanup() {
    window.removeEventListener("message", onMessage);
    iframe.remove();
    finish();
  }
}
