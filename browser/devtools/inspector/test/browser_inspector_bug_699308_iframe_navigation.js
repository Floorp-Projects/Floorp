/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let iframe;
  let iframeLoads = 0;
  let checksAfterLoads = false;
  let inspector;

  function startTest() {
    openInspector(aInspector => {
      inspector = aInspector;
      runInspectorTests();
    });
  }

  function showHighlighter(cb) {
    inspector.toolbox.highlighterUtils.startPicker().then(() => {
      EventUtils.synthesizeMouse(content.document.body, 1, 1,
        {type: "mousemove"}, content);
      inspector.toolbox.once("highlighter-ready", cb);
    });
  }

  function runInspectorTests() {
    iframe = content.document.querySelector("iframe");
    ok(iframe, "found the iframe element");

    showHighlighter(() => {
      ok(isHighlighting(), "Inspector is highlighting");

      iframe.addEventListener("load", onIframeLoad, false);
      executeSoon(() => {
        iframe.contentWindow.location = "javascript:location.reload()";
      });
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
    info("Finished reloading iframe and inspector updated");

    ok(isHighlighting(), "Inspector is highlighting after iframe nav");

    checksAfterLoads = true;

    finishTest();
  }

  function finishTest() {
    is(iframeLoads, 2, "iframe loads");
    ok(checksAfterLoads, "the Inspector tests got the chance to run after iframe reloads");

    inspector.toolbox.highlighterUtils.stopPicker().then(() => {
      iframe = null;
      gBrowser.removeCurrentTab();
      executeSoon(finish);
    });
  }

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onBrowserLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onBrowserLoad, true);
    waitForFocus(startTest, content);
  }, true);

  content.location = "data:text/html;charset=utf-8," +
                     "<p>bug 699308 - test iframe navigation</p>" +
                     "<iframe src='data:text/html;charset=utf-8,hello world'></iframe>";
}
