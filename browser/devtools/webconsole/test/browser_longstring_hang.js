/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that very long strings do not hang the browser.

function test()
{
  waitForExplicitFinish();

  let DebuggerServer = Cu.import("resource://gre/modules/devtools/dbg-server.jsm",
                                 {}).DebuggerServer;

  addTab("http://example.com/browser/browser/devtools/webconsole/test/test-bug-859170-longstring-hang.html");

  let hud = null;

  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openConsole(null, performTest);
  }, true);

  function performTest(aHud)
  {
    hud = aHud;

    info("wait for the initial long string");

    waitForMessages({
      webconsole: hud,
      messages: [
        {
          name: "find 'foobar', no 'foobaz', in long string output",
          text: "foobar",
          noText: "foobaz",
          category: CATEGORY_WEBDEV,
          longString: true,
        },
      ],
    }).then(onInitialString);
  }

  function onInitialString(aResults)
  {
    let msg = [...aResults[0].matched][0];
    ok(msg, "console.log result message element");

    let clickable = msg.querySelector(".longStringEllipsis");
    ok(clickable, "long string ellipsis is shown");

    scrollToVisible(clickable);

    executeSoon(() => {
      EventUtils.synthesizeMouse(clickable, 2, 2, {}, hud.iframeWindow);

      info("wait for long string expansion");

      waitForMessages({
        webconsole: hud,
        messages: [
          {
            name: "find 'foobaz' after expand, but no 'boom!' at the end",
            text: "foobaz",
            noText: "boom!",
            category: CATEGORY_WEBDEV,
            longString: false,
          },
          {
            text: "too long to be displayed",
            longString: false,
          },
        ],
      }).then(finishTest);
    });
  }

  function scrollToVisible(aNode)
  {
    let richListBoxNode = aNode.parentNode;
    while (richListBoxNode.tagName != "richlistbox") {
      richListBoxNode = richListBoxNode.parentNode;
    }

    let boxObject = richListBoxNode.scrollBoxObject;
    let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    nsIScrollBoxObject.ensureElementIsVisible(aNode);
  }
}
