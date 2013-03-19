/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 'use strict';

const { Cc, Ci } = require("chrome");
const { Loader } = require('sdk/test/loader');
const { LoaderWithHookedConsole } = require("sdk/test/loader");
const timer = require("sdk/timers");
const self = require('sdk/self');
const { open, close, focus } = require('sdk/window/helpers');
const { isPrivate } = require('sdk/private-browsing');
const { isWindowPBSupported } = require('sdk/private-browsing/utils');
const { defer } = require('sdk/core/promise');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');

const SVG_URL = self.data.url('mofo_logo.SVG');

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

  let content =
    "<script>" +
    "setTimeout(function () {" +
    "  window.location = 'about:blank';" +
    "}, 250);" +
    "</script>";
  let messageCount = 0;
  let panel = Panel({
    contentURL: "data:text/html;charset=utf-8," + encodeURIComponent(content),
    contentScript: "self.postMessage(window.location.href)",
    onMessage: function (message) {
      messageCount++;
      if (messageCount == 1) {
        assert.ok(/data:text\/html/.test(message), "First document had a content script");
      }
      else if (messageCount == 2) {
        assert.equal(message, "about:blank", "Second document too");
        panel.destroy();
        done();
      }
    }
  });
};

exports["test Parent Resize Hack"] = function(assert, done) {
  const { Panel } = require('sdk/panel');

  let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");
  let docShell = browserWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation)
                  .QueryInterface(Ci.nsIDocShell);
  if (!("allowWindowControl" in docShell)) {
    // bug 635673 is not fixed in this firefox build
    assert.pass("allowWindowControl attribute that allow to fix browser window " +
              "resize is not available on this build.");
    return;
  }

  let previousWidth = browserWindow.outerWidth, previousHeight = browserWindow.outerHeight;

  let content = "<script>" +
                "function contentResize() {" +
                "  resizeTo(200,200);" +
                "  resizeBy(200,200);" +
                "}" +
                "</script>" +
                "Try to resize browser window";
  let panel = Panel({
    contentURL: "data:text/html;charset=utf-8," + encodeURIComponent(content),
    contentScript: "self.on('message', function(message){" +
                   "  if (message=='resize') " +
                   "    unsafeWindow.contentResize();" +
                   "});",
    contentScriptWhen: "ready",
    onMessage: function (message) {

    },
    onShow: function () {
      panel.postMessage('resize');
      timer.setTimeout(function () {
        assert.equal(previousWidth,browserWindow.outerWidth,"Size doesn't change by calling resizeTo/By/...");
        assert.equal(previousHeight,browserWindow.outerHeight,"Size doesn't change by calling resizeTo/By/...");
        panel.destroy();
        done();
      },0);
    }
  });
  panel.show();
}

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
  const { Panel } = require('sdk/panel');

  let count = 0;
  function newPanel(tab, anchor) {
    let panel = Panel({
      contentURL: "data:text/html;charset=utf-8,<html><body style='padding: 0; margin: 0; " +
                  "background: gray; text-align: center;'>Anchor: " +
                  anchor.id + "</body></html>",
      width: 200,
      height: 100,
      onShow: function () {
        count++;
        panel.destroy();
        if (count==5) {
          assert.pass("All anchored panel test displayed");
          tab.close(function () {
            done();
          });
        }
      }
    });
    panel.show(anchor);
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
    onReady: function(tab) {
      let browserWindow = Cc["@mozilla.org/appshell/window-mediator;1"].
                      getService(Ci.nsIWindowMediator).
                      getMostRecentWindow("navigator:browser");
      let window = browserWindow.content;
      newPanel(tab, window.document.getElementById('tl'));
      newPanel(tab, window.document.getElementById('tr'));
      newPanel(tab, window.document.getElementById('bl'));
      newPanel(tab, window.document.getElementById('br'));
      let anchor = browserWindow.document.getElementById("identity-box");
      newPanel(tab, anchor);
    }
  });



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

  panel.port.on("event-back", function () {
    assert.fail("Panel should have been destroyed on module unload");
  });
  panel.port.emit("event");
  assert.pass("check automatic destroy");
};

exports["test Wait For Init Then Show Then Destroy"] = makeEventOrderTest({
  test: function(assert, done, expect, panel) {
    expect('inited', function() { panel.show(); }).
      then('show', function() { panel.destroy(); }).
      then('hide', function() { done(); });
  }
});

exports["test Show Then Wait For Init Then Destroy"] = makeEventOrderTest({
  test: function(assert, done, expect, panel) {
    panel.show();
    expect('inited').
      then('show', function() { panel.destroy(); }).
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
    let loader = Loader(module);
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
        panel.show(browserWindow.gBrowser);

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
    let loader = Loader(module);
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
        panel.show(window.gBrowser);
        showTries++;
        panel.show(browserWindow.gBrowser);

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

try {
  require("sdk/panel");
}
catch (e) {
  if (!/^Unsupported Application/.test(e.message))
    throw e;

  module.exports = {
    "test Unsupported Application": function Unsupported (assert) {
      assert.pass(e.message);
    }
  }
}

require("test").run(exports);
