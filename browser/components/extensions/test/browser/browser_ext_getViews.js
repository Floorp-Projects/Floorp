let {CustomizableUI} = Cu.import("resource:///modules/CustomizableUI.jsm", {});

function makeWidgetId(id)
{
  id = id.toLowerCase();
  return id.replace(/[^a-z0-9_-]/g, "_");
}

function genericChecker()
{
  var kind = "background";
  var path = window.location.pathname;
  if (path.indexOf("popup") != -1) {
    kind = "popup";
  } else if (path.indexOf("tab") != -1) {
    kind = "tab";
  }
  window.kind = kind;

  browser.test.onMessage.addListener((msg, ...args) => {
    if (msg == kind + "-check-views") {
      var views = browser.extension.getViews();
      var counts = {
        "background": 0,
        "tab": 0,
        "popup": 0
      };
      for (var i = 0; i < views.length; i++) {
        var view = views[i];
        browser.test.assertTrue(view.kind in counts, "view type is valid");
        counts[view.kind]++;
        if (view.kind == "background") {
          browser.test.assertTrue(view === browser.extension.getBackgroundPage(),
                                  "background page is correct");
        }
      }
      browser.test.sendMessage("counts", counts);
    } else if (msg == kind + "-open-tab") {
      browser.tabs.create({windowId: args[0], url: chrome.runtime.getURL("tab.html")});
    } else if (msg == kind + "-close-tab") {
      browser.tabs.query({
        windowId: args[0],
      }, tabs => {
        var tab = tabs.find(tab => tab.url.indexOf("tab.html") != -1);
        browser.tabs.remove(tab.id, () => {
          browser.test.sendMessage("closed");
        });
      });
    }
  });
  browser.test.sendMessage(kind + "-ready");
}

add_task(function* () {
  let win1 = yield BrowserTestUtils.openNewBrowserWindow();
  let win2 = yield BrowserTestUtils.openNewBrowserWindow();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "browser_action": {
        "default_popup": "popup.html"
      },
    },

    files: {
      "tab.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="tab.js"></script>
      </body></html>
      `,

      "tab.js": genericChecker,

      "popup.html": `
      <!DOCTYPE html>
      <html><body>
      <script src="popup.js"></script>
      </body></html>
      `,

      "popup.js": genericChecker,
    },

    background: genericChecker,
  });

  yield Promise.all([extension.startup(), extension.awaitMessage("background-ready")]);

  info("started");

  let {TabManager, WindowManager} = Cu.import("resource://gre/modules/Extension.jsm", {});

  let winId1 = WindowManager.getId(win1);
  let winId2 = WindowManager.getId(win2);

  function* openTab(winId) {
    extension.sendMessage("background-open-tab", winId);
    yield extension.awaitMessage("tab-ready");
  }

  function* checkViews(kind, tabCount, popupCount) {
    extension.sendMessage(kind + "-check-views");
    let counts = yield extension.awaitMessage("counts");
    is(counts.background, 1, "background count correct");
    is(counts.tab, tabCount, "tab count correct");
    is(counts.popup, popupCount, "popup count correct");
  }

  yield checkViews("background", 0, 0);

  yield openTab(winId1);

  yield checkViews("background", 1, 0);
  yield checkViews("tab", 1, 0);

  yield openTab(winId2);

  yield checkViews("background", 2, 0);

  function* triggerPopup(win, callback) {
    let widgetId = makeWidgetId(extension.id) + "-browser-action";
    let node = CustomizableUI.getWidget(widgetId).forWindow(win).node;

    let evt = new CustomEvent("command", {
      bubbles: true,
      cancelable: true
    });
    node.dispatchEvent(evt);

    yield extension.awaitMessage("popup-ready");

    yield callback();

    let panel = node.querySelector("panel");
    if (panel) {
      panel.hidePopup();
    }
  }

  yield triggerPopup(win1, function*() {
    yield checkViews("background", 2, 1);
    yield checkViews("popup", 2, 1);
  });

  yield triggerPopup(win2, function*() {
    yield checkViews("background", 2, 1);
    yield checkViews("popup", 2, 1);
  });

  info("checking counts after popups");

  yield checkViews("background", 2, 0);

  info("closing one tab");

  extension.sendMessage("background-close-tab", winId1);
  yield extension.awaitMessage("closed");

  info("one tab closed, one remains");

  yield checkViews("background", 1, 0);

  info("opening win1 popup");

  yield triggerPopup(win1, function*() {
    yield checkViews("background", 1, 1);
    yield checkViews("tab", 1, 1);
    yield checkViews("popup", 1, 1);
  });

  info("opening win2 popup");

  yield triggerPopup(win2, function*() {
    yield checkViews("background", 1, 1);
    yield checkViews("tab", 1, 1);
    yield checkViews("popup", 1, 1);
  });

  yield extension.unload();

  yield BrowserTestUtils.closeWindow(win1);
  yield BrowserTestUtils.closeWindow(win2);
});
