/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Check that exceptions from scripts loaded with the addon-sdk loader are
// opened correctly in View Source from the Browser Console.
// See bug 866950.

const TEST_URI = "data:text/html;charset=utf8,<p>hello world from bug 866950";

function test()
{
  let webconsole, browserconsole;

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);

    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(hud)
  {
    ok(hud, "web console opened");
    webconsole = hud;
    HUDService.toggleBrowserConsole().then(browserConsoleOpened);
  }

  function browserConsoleOpened(hud)
  {
    ok(hud, "browser console opened");
    browserconsole = hud;

    // Cause an exception in a script loaded with the addon-sdk loader.
    let toolbox = gDevTools.getToolbox(webconsole.target);
    let oldPanels = toolbox._toolPanels;
    toolbox._toolPanels = null;
    function fixToolbox()
    {
      toolbox._toolPanels = oldPanels;
    }

    info("generate exception and wait for message");

    executeSoon(() => {
      executeSoon(fixToolbox);
      expectUncaughtException();
      toolbox.getToolPanels();
    });

    waitForMessages({
      webconsole: hud,
      messages: [
        {
          text: "TypeError: can't convert null to object",
          category: CATEGORY_JS,
          severity: SEVERITY_ERROR,
        },
      ],
    }).then((results) => {
      fixToolbox();
      onMessageFound(results);
    });
  }

  function onMessageFound(results)
  {
    let msg = [...results[0].matched][0];
    ok(msg, "message element found");
    let locationNode = msg.querySelector(".location");
    ok(locationNode, "message location element found");

    let title = locationNode.getAttribute("title");
    info("location node title: " + title);
    isnot(title.indexOf(" -> "), -1, "error comes from a subscript");

    let viewSource = browserconsole.viewSource;
    let URL = null;
    browserconsole.viewSource = (aURL) => URL = aURL;

    EventUtils.synthesizeMouse(locationNode, 2, 2, {},
                               browserconsole.iframeWindow);

    info("view-source url: " + URL);
    isnot(URL.indexOf("toolbox.js"), -1, "expected view source URL");
    is(URL.indexOf("->"), -1, "no -> in the URL given to view-source");

    browserconsole.viewSource = viewSource;

    finishTest();
  }
}
