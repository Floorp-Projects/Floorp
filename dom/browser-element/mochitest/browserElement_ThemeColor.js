/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowsermetachange event for theme-color works.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  function loadFrameScript(script) {
    SpecialPowers.getBrowserFrameMessageManager(iframe1)
                 .loadFrameScript("data:," + script,
                                  /* allowDelayedLoad = */ false);
  }

  let iframe1 = document.createElement("iframe");
  iframe1.setAttribute("mozbrowser", "true");
  iframe1.src = "http://test/tests/dom/browser-element/mochitest/file_browserElement_ThemeColor.html";
  iframe1.addEventListener("mozbrowsermetachange", tests);
  document.body.appendChild(iframe1);

  let numMetaChanges = 0;
  function tests(e) {
    let detail = e.detail;

    switch (numMetaChanges++) {
      case 0: {
        is(detail.name, "theme-color", "name matches");
        is(detail.content, "pink", "content matches");
        is(detail.type, "added", "type matches");

        let script =
          "var meta = content.document.head.querySelector('meta');" +
          "meta.content = 'green';";
        loadFrameScript(script);
        break;
      }

      case 1: {
        is(detail.name, "theme-color", "name matches");
        is(detail.content, "green", "content matches");
        is(detail.type, "changed", "type matches");

        let script =
          "var meta = content.document.createElement('meta');" +
          "meta.name = 'theme-group';" +
          "meta.content = 'theme-productivity';" +
          "content.document.head.appendChild(meta)";
        loadFrameScript(script);
        break;
      }

      case 2: {
        is(detail.name, "theme-group", "name matches");
        is(detail.content, "theme-productivity", "content matches");
        is(detail.type, "added", "type matches");

        let script =
          "var meta = content.document.head.querySelector('meta');" +
          "meta.parentNode.removeChild(meta);";
        loadFrameScript(script);
        break;
      }

      case 3: {
        is(detail.name, "theme-color", "name matches");
        is(detail.content, "green", "content matches");
        is(detail.type, "removed", "type matches");

        SimpleTest.finish();
        break;
      }

      default: {
        ok(false, "Too many metachange events.");
        break;
      }
    }
  }
}

window.addEventListener("testready", runTest);
