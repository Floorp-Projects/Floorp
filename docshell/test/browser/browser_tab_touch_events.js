/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function*() {
  yield openUrl("data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>");

  let docshell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);

  is(docshell.touchEventsOverride, Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_NONE,
    "touchEventsOverride flag should be initially set to NONE");

  docshell.touchEventsOverride = Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_DISABLED;
  is(docshell.touchEventsOverride, Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_DISABLED,
    "touchEventsOverride flag should be changed to DISABLED");

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  docshell = frameWin.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIWebNavigation)
                     .QueryInterface(Ci.nsIDocShell);
  is(docshell.touchEventsOverride, Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_DISABLED,
    "touchEventsOverride flag should be passed on to frames.");

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  docshell = newFrameWin.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
  is(docshell.touchEventsOverride, Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_DISABLED,
    "Newly created frames should use the new touchEventsOverride flag");

  newFrameWin.location.reload();
  yield waitForEvent(newFrameWin, "load");

  docshell = newFrameWin.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebNavigation)
                        .QueryInterface(Ci.nsIDocShell);
  is(docshell.touchEventsOverride, Ci.nsIDocShell.TOUCHEVENTS_OVERRIDE_DISABLED,
    "New touchEventsOverride flag should persist across reloads");

  gBrowser.removeCurrentTab();
});

function waitForEvent(target, event) {
  return new Promise(function(resolve) {
    target.addEventListener(event, resolve);
  });
}

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
