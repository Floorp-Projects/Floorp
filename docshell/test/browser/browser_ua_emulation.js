/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

waitForExplicitFinish();

// Test that the docShell UA emulation works
var test = Task.async(function*() {
  yield openUrl("data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>");

  let docshell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
  is(docshell.customUserAgent, "", "There should initially be no customUserAgent");

  docshell.customUserAgent = "foo";
  is(content.navigator.userAgent, "foo", "The user agent should be changed to foo");

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  is(frameWin.navigator.userAgent, "foo", "The UA should be passed on to frames.");

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  is(newFrameWin.navigator.userAgent, "foo", "Newly created frames should use the new UA");

  newFrameWin.location.reload();
  newFrameWin.addEventListener("load", () => {
    is(newFrameWin.navigator.userAgent, "foo", "New UA should persist across reloads");
    gBrowser.removeCurrentTab();
    finish();
  });
});

function openUrl(url) {
  return new Promise(function(resolve, reject) {
    window.focus();

    let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onload() {
      linkedBrowser.removeEventListener("load", onload, true);
      resolve(tab);
    }, true);
  });
}
