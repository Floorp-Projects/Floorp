function notify(event)
{
  if (event.target.location == "about:blank")
    return;

  var eventname = event.type;
  if (eventname == "pagehide")
    details.pagehides++;
  else if (eventname == "beforeunload")
    details.beforeunloads++;
  else if (eventname == "unload")
    details.unloads++;
}

var details;

var gUseFrame = false;

const windowMediator = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

const TEST_BASE_URL = "data:text/html,<script>" +
                      "function note(event) { try { alert(event.type); } catch(ex) { return; } throw 'alert appeared'; }" +
                      "</script>" +
                      "<body onpagehide='note(event)' onbeforeunload='alert(event.type);' onunload='note(event)'>";

const TEST_URL = TEST_BASE_URL + "Test</body>";
const TEST_FRAME_URL = TEST_BASE_URL + "Frames</body>";

function test()
{
  waitForExplicitFinish();
  windowMediator.addListener(promptListener);
  runTest();
}

function runTest()
{
  details = {
    testNumber : 0,
    beforeunloads : 0,
    pagehides : 0,
    unloads : 0,
    prompts : 0
  };

  var tab = gBrowser.addTab(TEST_URL);
  gBrowser.selectedTab = tab;
  tab.linkedBrowser.addEventListener("pageshow", shown, true);

  tab.linkedBrowser.addEventListener("pagehide", notify, true);
  tab.linkedBrowser.addEventListener("beforeunload", notify, true);
  tab.linkedBrowser.addEventListener("unload", notify, true);
}

function shown(event)
{
  if (details.testNumber == 0) {
    var browser;
    var iframe;
    if (gUseFrame) {
      iframe = event.target.createElement("iframe");
      iframe.src = TEST_FRAME_URL;
      event.target.documentElement.appendChild(iframe);
      browser = iframe.contentWindow;
    }
    else {
      browser = gBrowser.selectedTab.linkedBrowser;
      details.testNumber = 1; // Move onto to the next step immediately
    }
  }

  if (details.testNumber == 1) {
    // Test going to another page
    executeSoon(function () {
      const urlToLoad = "data:text/html,<body>Another Page</body>";
      if (gUseFrame) {
        event.target.location = urlToLoad;
      }
      else {
        gBrowser.selectedBrowser.loadURI(urlToLoad);
      }
    });
  }
  else if (details.testNumber == 2) {
    is(details.pagehides, 1, "pagehides after next page")
    is(details.beforeunloads, 1, "beforeunloads after next page")
    is(details.unloads, 1, "unloads after next page")
    is(details.prompts, 1, "prompts after next page")

    executeSoon(function () gUseFrame ? gBrowser.goBack() : event.target.defaultView.back());
  }
  else if (details.testNumber == 3) {
    is(details.pagehides, 2, "pagehides after back")
    is(details.beforeunloads, 2, "beforeunloads after back")
    // No cache, so frame is unloaded
    is(details.unloads, gUseFrame ? 2 : 1, "unloads after back")
    is(details.prompts, 1, "prompts after back")

    // Test closing the tab
    gBrowser.selectedBrowser.removeEventListener("pageshow", shown, true);
    gBrowser.removeTab(gBrowser.selectedTab);

    // When the frame is present, there is are two beforeunload and prompts,
    // one for the frame and the other for the parent.
    is(details.pagehides, 3, "pagehides after close")
    is(details.beforeunloads, gUseFrame ? 4 : 3, "beforeunloads after close")
    is(details.unloads, gUseFrame ? 3 : 2, "unloads after close")
    is(details.prompts, gUseFrame ? 3 : 2, "prompts after close")

    // Now run the test again using a child frame.
    if (gUseFrame) {
      windowMediator.removeListener(promptListener);
      finish();
    }
    else {
      gUseFrame = true;
      runTest();
    }

    return;
  }

  details.testNumber++;
}

var promptListener = {
  onWindowTitleChange: function () {},
  onOpenWindow: function (win) {
    details.prompts++;
    let domWin = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
    executeSoon(function () { domWin.close() });
  },
  onCloseWindow: function () {},
};
