function test() {
  waitForExplicitFinish();

  let windowsToClose = [];
  function testOnWindow(options, callback) {
    let win = OpenBrowserWindow(options);
    windowsToClose.push(win);
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      win.addEventListener("DOMContentLoaded", function onInnerLoad() {
        if (win.content.location.href != "about:blank") {
          win.gBrowser.loadURI("about:blank");
          return;
        }
        win.removeEventListener("DOMContentLoaded", onInnerLoad, true);
        executeSoon(function() callback(win));
      }, true);
      if (!options) {
        win.gBrowser.loadURI("about:blank");
      }
    }, false);
  }

  Services.prefs.setIntPref("browser.startup.page", 0);
  Services.prefs.setCharPref("browser.startup.homepage_override.mstone", "ignore");

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("browser.startup.page");
    Services.prefs.clearUserPref("browser.startup.homepage_override.mstone");
    for (var i in windowsToClose) {
      windowsToClose[i].close();
    }
  });

  var phase = 0;
  function step(data) {
      switch (++phase) {
        case 1:
          is(data, "reply", "should have got reply message");
          privWin.postMessage('query?', 'http://mochi.test:8888');
          break;
        case 2:
          is(data.split(':')[0], "data", "should have got data message");
          is(data.split(':')[1], "false", "priv window should not see event");
          privWin.postMessage('setKey', 'http://mochi.test:8888');
          break;
        case 3:
          is(data, "reply", "should have got reply message");
          pubWin.postMessage('query?', 'http://mochi.test:8888');
          break;
        case 4:
          is(data.split(':')[0], "data", "should have got data message");
          is(data.split(':')[1], "false", "pub window should not see event");
          finish();
          break;
      }
  }

  let loads = 0;
  function waitForLoad() {
    loads++;
    if (loads != 2) {
      return;
    };
    pubWin.postMessage('setKey', 'http://mochi.test:8888');
  }

  var privWin;
  testOnWindow({private: true}, function(aPrivWin) {
    let doc = aPrivWin.content.document;
    let iframe = doc.createElement('iframe');
    iframe.setAttribute('id', 'target2');
    doc.body.appendChild(iframe);
    aPrivWin.content.is = is;
    aPrivWin.content.isnot = isnot;
    aPrivWin.content.ok = ok;
    iframe.onload = function() {
      privWin = iframe.contentWindow;
      privWin.addEventListener('message', function msgHandler(ev) {
        if (ev.data == 'setKey' || ev.data == 'query?')
          return;
        step(ev.data);
      }, true);
      waitForLoad();
    };
    iframe.src = "http://mochi.test:8888/browser/dom/tests/browser/page_privatestorageevent.html";
  });

  var pubWin;
  testOnWindow(undefined, function(aWin) {
    let doc = aWin.content.document;
    let iframe = doc.createElement('iframe');
    iframe.setAttribute('id', 'target');
    doc.body.appendChild(iframe);
    aWin.content.is = is;
    aWin.content.isnot = isnot;
    aWin.content.ok = ok;
    iframe.onload = function() {
      pubWin = iframe.contentWindow;
      pubWin.addEventListener('message', function msgHandler(ev) {
        if (ev.data == 'setKey' || ev.data == 'query?')
          return;
        step(ev.data);
      }, true);
      waitForLoad();
    };
    iframe.src = "http://mochi.test:8888/browser/dom/tests/browser/page_privatestorageevent.html";
  });
}