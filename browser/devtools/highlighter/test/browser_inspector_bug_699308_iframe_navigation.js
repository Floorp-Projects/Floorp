/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let iframe;
let iframeLoads = 0;
let checksAfterLoads = false;

function startTest() {
  ok(window.InspectorUI, "InspectorUI variable exists");
  Services.obs.addObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, null);
  InspectorUI.toggleInspectorUI();
}

function runInspectorTests() {
  Services.obs.removeObserver(runInspectorTests,
    InspectorUI.INSPECTOR_NOTIFICATIONS.OPENED, null);

  iframe = content.document.querySelector("iframe");
  ok(iframe, "found the iframe element");

  ok(InspectorUI.inspecting, "Inspector is highlighting");
  ok(InspectorUI.isInspectorOpen, "Inspector is open");

  Services.obs.addObserver(finishTest,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED, false);

  iframe.addEventListener("load", onIframeLoad, false);

  executeSoon(function() {
    iframe.contentWindow.location = "javascript:location.reload()";
  });
}

function onIframeLoad() {
  if (++iframeLoads != 2) {
    executeSoon(function() {
      iframe.contentWindow.location = "javascript:location.reload()";
    });
    return;
  }

  iframe.removeEventListener("load", onIframeLoad, false);

  ok(InspectorUI.inspecting, "Inspector is highlighting after iframe nav");
  ok(InspectorUI.isInspectorOpen, "Inspector Panel is open after iframe nav");

  checksAfterLoads = true;

  InspectorUI.closeInspectorUI();
}

function finishTest() {
  Services.obs.removeObserver(finishTest,
    InspectorUI.INSPECTOR_NOTIFICATIONS.CLOSED);

  is(iframeLoads, 2, "iframe loads");
  ok(checksAfterLoads, "the Inspector tests got the chance to run after iframe reloads");
  ok(!InspectorUI.isInspectorOpen, "Inspector Panel is not open");

  iframe = null;
  gBrowser.removeCurrentTab();
  executeSoon(finish);
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html,<p>bug 699308 - test iframe navigation" +
    "<iframe src='data:text/html,hello world'></iframe>";
}
