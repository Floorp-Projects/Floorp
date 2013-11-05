/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cc, Ci } = require("chrome");
const { Loader } = require('sdk/test/loader');
const { LoaderWithHookedConsole } = require("sdk/test/loader");
const timer = require("sdk/timers");
const self = require('sdk/self');
const { open, close, focus, ready } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { isWindowPBSupported, isGlobalPBSupported } = require('sdk/private-browsing/utils');
const { defer, all } = require('sdk/core/promise');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { getWindow } = require('sdk/panel/window');
const { pb } = require('./private-browsing/helper');
const { URL } = require('sdk/url');
const fixtures = require('./fixtures')

const SVG_URL = fixtures.url('mofo_logo.SVG');
const Isolate = fn => '(' + fn + ')()';

function ignorePassingDOMNodeWarning(type, message) {
  if (type !== 'warn' || !message.startsWith('Passing a DOM node'))
    console[type](message);
}

function makeEmptyPrivateBrowserWindow(options) {
  options = options || {};
  return open('chrome://browser/content/browser.xul', {
    features: {
      chrome: true,
      toolbar: true,
      private: true
    }
  });
}

exports["test Panel"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let panel = Panel({
    contentURL: "about:buildconfig",
    contentScript: "self.postMessage(1); self.on('message', function() self.postMessage(2));",
    onMessage: function (message) {
      assert.equal(this, panel, "The 'this' object is the panel.");
      switch(message) {
        case 1:
          assert.pass("The panel was loaded.");
          panel.postMessage('');
          break;
        case 2:
          assert.pass("The panel posted a message and received a response.");
          panel.destroy();
          done();
          break;
      }
    }
  });
};

exports["test Panel Emit"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let panel = Panel({
    contentURL: "about:buildconfig",
    contentScript: "self.port.emit('loaded');" +
                   "self.port.on('addon-to-content', " +
                   "             function() self.port.emit('received'));",
  });
  panel.port.on("loaded", function () {
    assert.pass("The panel was loaded and sent a first event.");
    panel.port.emit("addon-to-content");
  });
  panel.port.on("received", function () {
    assert.pass("The panel posted a message and received a response.");
    panel.destroy();
    done();
  });
};

exports["test Panel Emit Early"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let panel = Panel({
    contentURL: "about:buildconfig",
    contentScript: "self.port.on('addon-to-content', " +
                   "             function() self.port.emit('received'));",
  });
  panel.port.on("received", function () {
    assert.pass("The panel posted a message early and received a response.");
    panel.destroy();
    done();
  });
  panel.port.emit("addon-to-content");
};

exports["test Show Hide Panel"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let panel = Panel({
    contentScript: "self.postMessage('')",
    contentScriptWhen: "end",
    contentURL: "data:text/html;charset=utf-8,",
    onMessage: function (message) {
      panel.show();
    },
    onShow: function () {
      assert.pass("The panel was shown.");
      assert.equal(this, panel, "The 'this' object is the panel.");
      assert.equal(this.isShowing, true, "panel.isShowing == true.");
      panel.hide();
    },
    onHide: function () {
      assert.pass("The panel was hidden.");
      assert.equal(this, panel, "The 'this' object is the panel.");
      assert.equal(this.isShowing, false, "panel.isShowing == false.");
      panel.destroy();
      done();
    }
  });
};

exports["test Document Reload"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let url2 = "data:text/html;charset=utf-8,page2";
  let content =
    "<script>" +
    "window.addEventListener('message', function({ data }) {"+
    "  if (data == 'move') window.location = '" + url2 + "';" +
    '}, false);' +
    "</script>";
  let messageCount = 0;
  let panel = Panel({
    // using URL here is intentional, see bug 859009
    contentURL: URL("data:text/html;charset=utf-8," + encodeURIComponent(content)),
    contentScript: "self.postMessage(window.location.href);" +
                   // initiate change to url2
                   "self.port.once('move', function() document.defaultView.postMessage('move', '*'));",
    onMessage: function (message) {
      messageCount++;
      assert.notEqual(message, "about:blank", "about:blank is not a message " + messageCount);

      if (messageCount == 1) {
        assert.ok(/data:text\/html/.test(message), "First document had a content script; " + message);
        panel.port.emit('move');
        assert.pass('move message was sent');
        return;
      }
      else if (messageCount == 2) {
        assert.equal(message, url2, "Second document too; " + message);
        panel.destroy();
        done();
      }
    }
  });
  assert.pass('Panel was created');
};

// Test disabled because of bug 910230
/*
exports["test Parent Resize Hack"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let browserWindow = getMostRecentBrowserWindow();

  let previousWidth = browserWindow.outerWidth;
  let previousHeight = browserWindow.outerHeight;

  let content = "<script>" +
                "function contentResize() {" +
                "  resizeTo(200,200);" +
                "  resizeBy(200,200);" +
                "  window.postMessage('resize-attempt', '*');" +
                "}" +
                "</script>" +
                "Try to resize browser window";

  let panel = Panel({
    contentURL: "data:text/html;charset=utf-8," + encodeURIComponent(content),
    contentScriptWhen: "ready",
    contentScript: Isolate(() => {
        self.on('message', message => {
          if (message === 'resize') unsafeWindow.contentResize();
        });

        window.addEventListener('message', ({ data }) => self.postMessage(data));
      }),
    onMessage: function (message) {
      if (message !== "resize-attempt") return;

      assert.equal(browserWindow, getMostRecentBrowserWindow(),
        "The browser window is still the same");
      assert.equal(previousWidth, browserWindow.outerWidth,
        "Size doesn't change by calling resizeTo/By/...");
      assert.equal(previousHeight, browserWindow.outerHeight,
        "Size doesn't change by calling resizeTo/By/...");

      try {
        panel.destroy();
      }
      catch (e) {
        assert.fail(e);
        throw e;
      }

      done();
    },
    onShow: () => panel.postMessage('resize')
  });

  panel.show();
}
*/

exports["test Resize Panel"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  // These tests fail on Linux if the browser window in which the panel
  // is displayed is not active.  And depending on what other tests have run
  // before this one, it might not be (the untitled window in which the test
  // runner executes is often active).  So we make sure the browser window
  // is focused by focusing it before running the tests.  Then, to be the best
  // possible test citizen, we refocus whatever window was focused before we
  // started running these tests.

  let activeWindow = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                      getService(Ci.nsIWindowWatcher).
                      activeWindow;
  let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");


  function onFocus() {
    browserWindow.removeEventListener("focus", onFocus, true);

    let panel = Panel({
      contentScript: "self.postMessage('')",
      contentScriptWhen: "end",
      contentURL: "data:text/html;charset=utf-8,",
      height: 10,
      width: 10,
      onMessage: function (message) {
        panel.show();
      },
      onShow: function () {
        panel.resize(100,100);
        panel.hide();
      },
      onHide: function () {
        assert.ok((panel.width == 100) && (panel.height == 100),
          "The panel was resized.");
        if (activeWindow)
          activeWindow.focus();
        done();
      }
    });
  }

  if (browserWindow === activeWindow) {
    onFocus();
  }
  else {
    browserWindow.addEventListener("focus", onFocus, true);
    browserWindow.focus();
  }
};

exports["test Hide Before Show"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let showCalled = false;
  let panel = Panel({
    onShow: function () {
      showCalled = true;
    },
    onHide: function () {
      assert.ok(!showCalled, 'must not emit show if was hidden before');
      done();
    }
  });
  panel.show();
  panel.hide();
};

exports["test Several Show Hides"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let hideCalled = 0;
  let panel = Panel({
    contentURL: "about:buildconfig",
    onShow: function () {
      panel.hide();
    },
    onHide: function () {
      hideCalled++;
      if (hideCalled < 3)
        panel.show();
      else {
        assert.pass("onHide called three times as expected");
        done();
      }
    }
  });
  panel.on('error', function(e) {
    assert.fail('error was emitted:' + e.message + '\n' + e.stack);
  });
  panel.show();
};

exports["test Anchor And Arrow"] = function(assert, done) {
  let { loader } = LoaderWithHookedConsole(module, ignorePassingDOMNodeWarning);
  let { Panel } = loader.require('sdk/panel');

  let count = 0;
  let queue = [];
  let tab;

  function newPanel(anchor) {
    let panel = Panel({
      contentURL: "data:text/html;charset=utf-8,<html><body style='padding: 0; margin: 0; " +
                  "background: gray; text-align: center;'>Anchor: " +
                  anchor.id + "</body></html>",
      width: 200,
      height: 100,
      onShow: function () {
        panel.destroy();
        next();
      }
    });
    queue.push({ panel: panel, anchor: anchor });
  }

  function next () {
    if (!queue.length) {
      assert.pass("All anchored panel test displayed");
      tab.close(function () {
        done();
      });
      return;
    }
    let { panel, anchor } = queue.shift();
    panel.show(null, anchor);
  }

  let tabs= require("sdk/tabs");
  let url = 'data:text/html;charset=utf-8,' +
    '<html><head><title>foo</title></head><body>' +
    '<style>div {background: gray; position: absolute; width: 300px; ' +
           'border: 2px solid black;}</style>' +
    '<div id="tl" style="top: 0px; left: 0px;">Top Left</div>' +
    '<div id="tr" style="top: 0px; right: 0px;">Top Right</div>' +
    '<div id="bl" style="bottom: 0px; left: 0px;">Bottom Left</div>' +
    '<div id="br" style="bottom: 0px; right: 0px;">Bottom right</div>' +
    '</body></html>';

  tabs.open({
    url: url,
    onReady: function(_tab) {
      tab = _tab;
      let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");
      let window = browserWindow.content;
      newPanel(window.document.getElementById('tl'));
      newPanel(window.document.getElementById('tr'));
      newPanel(window.document.getElementById('bl'));
      newPanel(window.document.getElementById('br'));
      let anchor = browserWindow.document.getElementById("identity-box");
      newPanel(anchor);

      next();
    }
  });
};

exports["test Panel Focus True"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  const FM = Cc["@mozilla.org/focus-manager;1"].
                getService(Ci.nsIFocusManager);

  let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");

  // Make sure there is a focused element
  browserWindow.document.documentElement.focus();

  // Get the current focused element
  let focusedElement = FM.focusedElement;

  let panel = Panel({
    contentURL: "about:buildconfig",
    focus: true,
    onShow: function () {
      assert.ok(focusedElement !== FM.focusedElement,
        "The panel takes the focus away.");
      done();
    }
  });
  panel.show();
};

exports["test Panel Focus False"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  const FM = Cc["@mozilla.org/focus-manager;1"].
                getService(Ci.nsIFocusManager);

  let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");

  // Make sure there is a focused element
  browserWindow.document.documentElement.focus();

  // Get the current focused element
  let focusedElement = FM.focusedElement;

  let panel = Panel({
    contentURL: "about:buildconfig",
    focus: false,
    onShow: function () {
      assert.ok(focusedElement === FM.focusedElement,
        "The panel does not take the focus away.");
      done();
    }
  });
  panel.show();
};

exports["test Panel Focus Not Set"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  const FM = Cc["@mozilla.org/focus-manager;1"].
                getService(Ci.nsIFocusManager);

  let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");

  // Make sure there is a focused element
  browserWindow.document.documentElement.focus();

  // Get the current focused element
  let focusedElement = FM.focusedElement;

  let panel = Panel({
    contentURL: "about:buildconfig",
    onShow: function () {
      assert.ok(focusedElement !== FM.focusedElement,
        "The panel takes the focus away.");
      done();
    }
  });
  panel.show();
};

exports["test Panel Text Color"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let html = "<html><head><style>body {color: yellow}</style></head>" +
             "<body><p>Foo</p></body></html>";
  let panel = Panel({
    contentURL: "data:text/html;charset=utf-8," + encodeURI(html),
    contentScript: "self.port.emit('color', " +
                   "window.getComputedStyle(document.body.firstChild, null). " +
                   "       getPropertyValue('color'));"
  });
  panel.port.on("color", function (color) {
    assert.equal(color, "rgb(255, 255, 0)",
      "The panel text color style is preserved when a style exists.");
    panel.destroy();
    done();
  });
};

// Bug 866333
exports["test watch event name"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let html = "<html><head><style>body {color: yellow}</style></head>" +
             "<body><p>Foo</p></body></html>";

  let panel = Panel({
    contentURL: "data:text/html;charset=utf-8," + encodeURI(html),
    contentScript: "self.port.emit('watch', 'test');"
  });
  panel.port.on("watch", function (msg) {
    assert.equal(msg, "test", 'watch event name works');
    panel.destroy();
    done();
  });
}

// Bug 696552: Ensure panel.contentURL modification support
exports["test Change Content URL"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let panel = Panel({
    contentURL: "about:blank",
    contentScript: "self.port.emit('ready', document.location.href);"
  });

  let count = 0;
  panel.port.on("ready", function (location) {
    count++;
    if (count == 1) {
      assert.equal(location, "about:blank");
      assert.equal(panel.contentURL, "about:blank");
      panel.contentURL = "about:buildconfig";
    }
    else {
      assert.equal(location, "about:buildconfig");
      assert.equal(panel.contentURL, "about:buildconfig");
      panel.destroy();
      done();
    }
  });
};

function makeEventOrderTest(options) {
  let expectedEvents = [];

  return function(assert, done) {
    const { Panel } = require('sdk/panel');

    let panel = Panel({ contentURL: "about:buildconfig" });

    function expect(event, cb) {
      expectedEvents.push(event);
      panel.on(event, function() {
        assert.equal(event, expectedEvents.shift());
        if (cb)
          timer.setTimeout(cb, 1);
      });
      return {then: expect};
    }

    options.test(assert, done, expect, panel);
  }
}

exports["test Automatic Destroy"] = function(assert) {
  let loader = Loader(module);
  let panel = loader.require("sdk/panel").Panel({
    contentURL: "about:buildconfig",
    contentScript:
      "self.port.on('event', function() self.port.emit('event-back'));"
  });

  loader.unload();

  assert.throws(() => {
    panel.port.emit("event");
  }, /already have been unloaded/, "check automatic destroy");
};

exports["test Show Then Destroy"] = makeEventOrderTest({
  test: function(assert, done, expect, panel) {
    panel.show();
    expect('show', function() { panel.destroy(); }).
      then('hide', function() { done(); });
  }
});

exports["test Show Then Hide Then Destroy"] = makeEventOrderTest({
  test: function(assert, done, expect, panel) {
    panel.show();
    expect('show', function() { panel.hide(); }).
      then('hide', function() { panel.destroy(); done(); });
  }
});

exports["test Content URL Option"] = function(assert) {
  const { Panel } = require('sdk/panel');

  const URL_STRING = "about:buildconfig";
  const HTML_CONTENT = "<html><title>Test</title><p>This is a test.</p></html>";

  let (panel = Panel({ contentURL: URL_STRING })) {
    assert.pass("contentURL accepts a string URL.");
    assert.equal(panel.contentURL, URL_STRING,
                "contentURL is the string to which it was set.");
  }

  let dataURL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML_CONTENT);
  let (panel = Panel({ contentURL: dataURL })) {
    assert.pass("contentURL accepts a data: URL.");
  }

  let (panel = Panel({})) {
    assert.ok(panel.contentURL == null,
                "contentURL is undefined.");
  }

  assert.throws(function () Panel({ contentURL: "foo" }),
                    /The `contentURL` option must be a valid URL./,
                    "Panel throws an exception if contentURL is not a URL.");
};

exports["test SVG Document"] = function(assert) {
  let panel = require("sdk/panel").Panel({ contentURL: SVG_URL });

  panel.show();
  panel.hide();
  panel.destroy();

  assert.pass("contentURL accepts a svg document");
  assert.equal(panel.contentURL, SVG_URL,
              "contentURL is the string to which it was set.");
};

exports["test ContentScriptOptions Option"] = function(assert, done) {
  let loader = Loader(module);
  let panel = loader.require("sdk/panel").Panel({
      contentScript: "self.postMessage( [typeof self.options.d, self.options] );",
      contentScriptWhen: "end",
      contentScriptOptions: {a: true, b: [1,2,3], c: "string", d: function(){ return 'test'}},
      contentURL: "data:text/html;charset=utf-8,",
      onMessage: function(msg) {
        assert.equal( msg[0], 'undefined', 'functions are stripped from contentScriptOptions' );
        assert.equal( typeof msg[1], 'object', 'object as contentScriptOptions' );
        assert.equal( msg[1].a, true, 'boolean in contentScriptOptions' );
        assert.equal( msg[1].b.join(), '1,2,3', 'array and numbers in contentScriptOptions' );
        assert.equal( msg[1].c, 'string', 'string in contentScriptOptions' );
        done();
      }
    });
};

exports["test console.log in Panel"] = function(assert, done) {
  let text = 'console.log() in Panel works!';
  let html = '<script>onload = function log(){\
                console.log("' + text + '");\
              }</script>';

  let { loader } = LoaderWithHookedConsole(module, onMessage);
  let { Panel } = loader.require('sdk/panel');

  let panel = Panel({
    contentURL: 'data:text/html;charset=utf-8,' + encodeURIComponent(html)
  });

  panel.show();

  function onMessage(type, message) {
    assert.equal(type, 'log', 'console.log() works');
    assert.equal(message, text, 'console.log() works');
    panel.destroy();
    done();
  }
};

if (isWindowPBSupported) {
  exports.testPanelDoesNotShowInPrivateWindowNoAnchor = function(assert, done) {
    let { loader } = LoaderWithHookedConsole(module, ignorePassingDOMNodeWarning);
    let { Panel } = loader.require("sdk/panel");
    let browserWindow = getMostRecentBrowserWindow();

    assert.equal(isPrivate(browserWindow), false, 'open window is not private');

    let panel = Panel({
      contentURL: SVG_URL
    });

    testShowPanel(assert, panel).
      then(makeEmptyPrivateBrowserWindow).
      then(focus).
      then(function(window) {
        assert.equal(isPrivate(window), true, 'opened window is private');
        assert.pass('private window was focused');
        return window;
      }).
      then(function(window) {
        let { promise, resolve } = defer();
        let showTries = 0;
        let showCount = 0;

        panel.on('show', function runTests() {
          showCount++;

          if (showTries == 2) {
            panel.removeListener('show', runTests);
            assert.equal(showCount, 1, 'show count is correct - 1');
            resolve(window);
          }
        });
        showTries++;
        panel.show();
        showTries++;
        panel.show(null, browserWindow.gBrowser);

        return promise;
      }).
      then(function(window) {
        assert.equal(panel.isShowing, true, 'panel is still showing');
        panel.hide();
        assert.equal(panel.isShowing, false, 'panel is hidden');
        return window;
      }).
      then(close).
      then(function() {
        assert.pass('private window was closed');
      }).
      then(testShowPanel.bind(null, assert, panel)).
      then(done, assert.fail.bind(assert));
  }

  exports.testPanelDoesNotShowInPrivateWindowWithAnchor = function(assert, done) {
    let { loader } = LoaderWithHookedConsole(module, ignorePassingDOMNodeWarning);
    let { Panel } = loader.require("sdk/panel");
    let browserWindow = getMostRecentBrowserWindow();

    assert.equal(isPrivate(browserWindow), false, 'open window is not private');

    let panel = Panel({
      contentURL: SVG_URL
    });

    testShowPanel(assert, panel).
      then(makeEmptyPrivateBrowserWindow).
      then(focus).
      then(function(window) {
        assert.equal(isPrivate(window), true, 'opened window is private');
        assert.pass('private window was focused');
        return window;
      }).
      then(function(window) {
        let { promise, resolve } = defer();
        let showTries = 0;
        let showCount = 0;

        panel.on('show', function runTests() {
          showCount++;

          if (showTries == 2) {
            panel.removeListener('show', runTests);
            assert.equal(showCount, 1, 'show count is correct - 1');
            resolve(window);
          }
        });
        showTries++;
        panel.show(null, window.gBrowser);
        showTries++;
        panel.show(null, browserWindow.gBrowser);

        return promise;
      }).
      then(function(window) {
        assert.equal(panel.isShowing, true, 'panel is still showing');
        panel.hide();
        assert.equal(panel.isShowing, false, 'panel is hidden');
        return window;
      }).
      then(close).
      then(function() {
        assert.pass('private window was closed');
      }).
      then(testShowPanel.bind(null, assert, panel)).
      then(done, assert.fail.bind(assert));
  }
}

function testShowPanel(assert, panel) {
  let { promise, resolve } = defer();

  assert.ok(!panel.isShowing, 'the panel is not showing [1]');

  panel.once('show', function() {
    assert.ok(panel.isShowing, 'the panel is showing');

    panel.once('hide', function() {
      assert.ok(!panel.isShowing, 'the panel is not showing [2]');

      resolve(null);
    });

    panel.hide();
  })
  panel.show();

  return promise;
}

exports['test Style Applied Only Once'] = function (assert, done) {
  let loader = Loader(module);
  let panel = loader.require("sdk/panel").Panel({
    contentURL: "data:text/html;charset=utf-8,",
    contentScript:
      'self.port.on("check",function() { self.port.emit("count", document.getElementsByTagName("style").length); });' +
      'self.port.on("ping", function (count) { self.port.emit("pong", count); });'
  });

  panel.port.on('count', function (styleCount) {
    assert.equal(styleCount, 1, 'should only have one style');
    done();
  });

  panel.port.on('pong', function (counter) {
    panel[--counter % 2 ? 'hide' : 'show']();
    panel.port.emit(!counter ? 'check' : 'ping', counter);
  });

  panel.on('show', init);
  panel.show();

  function init () {
    panel.removeListener('show', init);
    panel.port.emit('ping', 10);
  }
};

exports['test Only One Panel Open Concurrently'] = function (assert, done) {
  const loader = Loader(module);
  const { Panel } = loader.require('sdk/panel')

  let panelA = Panel({
    contentURL: 'about:buildconfig'
  });

  let panelB = Panel({
    contentURL: 'about:buildconfig',
    onShow: function () {
      // When loading two panels simulataneously, only the second
      // should be shown, never showing the first
      assert.equal(panelA.isShowing, false, 'First panel is hidden');
      assert.equal(panelB.isShowing, true, 'Second panel is showing');
      panelC.show();
    }
  });

  let panelC = Panel({
    contentURL: 'about:buildconfig',
    onShow: function () {
      assert.equal(panelA.isShowing, false, 'First panel is hidden');
      assert.equal(panelB.isShowing, false, 'Second panel is hidden');
      assert.equal(panelC.isShowing, true, 'Third panel is showing');
      done();
    }
  });

  panelA.show();
  panelB.show();
};

exports['test passing DOM node as first argument'] = function (assert, done) {
  let warned = defer();
  let shown = defer();

  function onMessage(type, message) {
    let warning = 'Passing a DOM node to Panel.show() method is an unsupported ' +
                  'feature that will be soon replaced. ' +
                  'See: https://bugzilla.mozilla.org/show_bug.cgi?id=878877';

    assert.equal(type, 'warn',
      'the message logged is a warning');

    assert.equal(message, warning,
      'the warning content is correct');

    warned.resolve();
  }

  let { loader } = LoaderWithHookedConsole(module, onMessage);
  let { Panel } = loader.require('sdk/panel');
  let { Widget } = loader.require('sdk/widget');
  let { document } = getMostRecentBrowserWindow();
  let widgetId = 'widget:' + self.id + '-panel-widget';

  let panel = Panel({
    onShow: function() {
      let panelNode = document.getElementById('mainPopupSet').lastChild;

      assert.equal(panelNode.anchorNode, widgetNode,
        'the panel is properly anchored to the widget');

      shown.resolve();
    }
  });

  let widget = Widget({
    id: 'panel-widget',
    label: 'panel widget',
    content: '<i></i>',
  });

  let widgetNode = document.getElementById(widgetId);

  all(warned.promise, shown.promise).
    then(loader.unload).
    then(done, assert.fail)

  panel.show(widgetNode);
};

// This test is checking that `onpupshowing` events emitted by panel's children
// are not considered.
// See Bug 886329
exports['test nested popups'] = function (assert, done) {
  let loader = Loader(module);
  let { Panel } = loader.require('sdk/panel');
  let { getActiveView } = loader.require('sdk/view/core');
  let url = '<select><option>1<option>2<option>3</select>';

  let getContentWindow = panel => {
    return getActiveView(panel).querySelector('iframe').contentWindow;
  }

  let panel = Panel({
    contentURL: 'data:text/html;charset=utf-8,' + encodeURIComponent(url),
    onShow: () => {
      ready(getContentWindow(panel)).then(({ window, document }) => {
        let select = document.querySelector('select');
        let event = document.createEvent('UIEvent');

        event.initUIEvent('popupshowing', true, true, window, null);
        select.dispatchEvent(event);

        assert.equal(
          select,
          getContentWindow(panel).document.querySelector('select'),
          'select is still loaded in panel'
        );

        done();
      });
    }
  });

  panel.show();
};

exports['test emits on url changes'] = function (assert, done) {
  let loader = Loader(module);
  let { Panel } = loader.require('sdk/panel');
  let uriA = 'data:text/html;charset=utf-8,A';
  let uriB = 'data:text/html;charset=utf-8,B';

  let panel = Panel({
    contentURL: uriA,
    contentScript: 'new ' + function() {
      self.port.on('hi', function() {
        self.port.emit('bye', document.URL);
      });
    }
  });

  panel.contentURL = uriB;
  panel.port.emit('hi', 'hi')
  panel.port.on('bye', function(uri) {
    assert.equal(uri, uriB, 'message was delivered to new uri');
    loader.unload();
    done();
  });
};

if (isWindowPBSupported) {
  exports.testGetWindow = function(assert, done) {
    let activeWindow = getMostRecentBrowserWindow();
    open(null, { features: {
      toolbar: true,
      chrome: true,
      private: true
    } }).then(function(window) {
      assert.ok(isPrivate(window), 'window is private');
      assert.equal(getWindow(window.gBrowser), null, 'private window elements returns null');
      assert.equal(getWindow(activeWindow.gBrowser), activeWindow, 'non-private window elements returns window');
      close(window).then(done);
    })
  }
}
else if (isGlobalPBSupported) {
  exports.testGetWindow = function(assert, done) {
    let activeWindow = getMostRecentBrowserWindow();

    assert.equal(getWindow(activeWindow.gBrowser), activeWindow, 'non-private window elements returns window');
    pb.once('start', function() {
      assert.ok(isPrivate(activeWindow), 'window is private');
      assert.equal(getWindow(activeWindow.gBrowser), activeWindow, 'private window elements returns window');
      open(null, { features: {
        toolbar: true,
        chrome: true
      } }).then(function(window) {
        assert.ok(isPrivate(window), 'window is private');
        assert.equal(getWindow(window.gBrowser), window, 'private window elements returns window');
        assert.equal(getWindow(activeWindow.gBrowser), activeWindow, 'active window elements returns window');

        pb.once('stop', done);
        pb.deactivate();
      })
    });
    pb.activate();
  }
}

require("test").run(exports);
