/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PageMod } = require("sdk/page-mod");
const { testPageMod, handleReadyState } = require("./pagemod-test-helpers");
const { Loader } = require('sdk/test/loader');
const tabs = require("sdk/tabs");
const { setTimeout } = require("sdk/timers");
const { Cc, Ci, Cu } = require("chrome");
const {
  open,
  getFrames,
  getMostRecentBrowserWindow,
  getInnerId
} = require('sdk/window/utils');
const { getTabContentWindow, getActiveTab, setTabURL, openTab, closeTab } = require('sdk/tabs/utils');
const xulApp = require("sdk/system/xul-app");
const { isPrivateBrowsingSupported } = require('sdk/self');
const { isPrivate } = require('sdk/private-browsing');
const { openWebpage } = require('./private-browsing/helper');
const { isTabPBSupported, isWindowPBSupported, isGlobalPBSupported } = require('sdk/private-browsing/utils');
const promise = require("sdk/core/promise");
const { pb } = require('./private-browsing/helper');
const { URL } = require("sdk/url");
const { LoaderWithHookedConsole } = require('sdk/test/loader');

const { waitUntil } = require("sdk/test/utils");
const data = require("./fixtures");

const { devtools } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const { require: devtoolsRequire } = devtools;
const contentGlobals = devtoolsRequire("devtools/server/content-globals");

const testPageURI = data.url("test.html");

// The following adds Debugger constructor to the global namespace.
const { addDebuggerToGlobal } =
  Cu.import('resource://gre/modules/jsdebugger.jsm', {});
addDebuggerToGlobal(this);

function Isolate(worker) {
  return "(" + worker + ")()";
}

/* Tests for the PageMod APIs */

exports.testPageMod1 = function(assert, done) {
  let mods = testPageMod(assert, done, "about:", [{
      include: /about:/,
      contentScriptWhen: 'end',
      contentScript: 'new ' + function WorkerScope() {
        window.document.body.setAttribute("JEP-107", "worked");
      },
      onAttach: function() {
        assert.equal(this, mods[0], "The 'this' object is the page mod.");
      }
    }],
    function(win, done) {
      assert.equal(
        win.document.body.getAttribute("JEP-107"),
        "worked",
        "PageMod.onReady test"
      );
      done();
    }
  );
};

exports.testPageMod2 = function(assert, done) {
  testPageMod(assert, done, "about:", [{
      include: "about:*",
      contentScript: [
        'new ' + function contentScript() {
          window.AUQLUE = function() { return 42; }
          try {
            window.AUQLUE()
          }
          catch(e) {
            throw new Error("PageMod scripts executed in order");
          }
          document.documentElement.setAttribute("first", "true");
        },
        'new ' + function contentScript() {
          document.documentElement.setAttribute("second", "true");
        }
      ]
    }], function(win, done) {
      assert.equal(win.document.documentElement.getAttribute("first"),
                       "true",
                       "PageMod test #2: first script has run");
      assert.equal(win.document.documentElement.getAttribute("second"),
                       "true",
                       "PageMod test #2: second script has run");
      assert.equal("AUQLUE" in win, false,
                       "PageMod test #2: scripts get a wrapped window");
      done();
    });
};

exports.testPageModIncludes = function(assert, done) {
  var asserts = [];
  function createPageModTest(include, expectedMatch) {
    // Create an 'onload' test function...
    asserts.push(function(test, win) {
      var matches = include in win.localStorage;
      assert.ok(expectedMatch ? matches : !matches,
                  "'" + include + "' match test, expected: " + expectedMatch);
    });
    // ...and corresponding PageMod options
    return {
      include: include,
      contentScript: 'new ' + function() {
        self.on("message", function(msg) {
          window.localStorage[msg] = true;
        });
      },
      // The testPageMod callback with test assertions is called on 'end',
      // and we want this page mod to be attached before it gets called,
      // so we attach it on 'start'.
      contentScriptWhen: 'start',
      onAttach: function(worker) {
        worker.postMessage(this.include[0]);
      }
    };
  }

  testPageMod(assert, done, testPageURI, [
      createPageModTest("*", false),
      createPageModTest("*.google.com", false),
      createPageModTest("resource:*", true),
      createPageModTest("resource:", false),
      createPageModTest(testPageURI, true)
    ],
    function (win, done) {
      waitUntil(() => win.localStorage[testPageURI],
          testPageURI + " page-mod to be executed")
        .then(() => {
          asserts.forEach(fn => fn(assert, win));
          win.localStorage.clear();
          done();
        });
    });
};

exports.testPageModExcludes = function(assert, done) {
  var asserts = [];
  function createPageModTest(include, exclude, expectedMatch) {
    // Create an 'onload' test function...
    asserts.push(function(test, win) {
      var matches = JSON.stringify([include, exclude]) in win.localStorage;
      assert.ok(expectedMatch ? matches : !matches,
          "[include, exclude] = [" + include + ", " + exclude +
          "] match test, expected: " + expectedMatch);
    });
    // ...and corresponding PageMod options
    return {
      include: include,
      exclude: exclude,
      contentScript: 'new ' + function() {
        self.on("message", function(msg) {
          // The key in localStorage is "[<include>, <exclude>]".
          window.localStorage[JSON.stringify(msg)] = true;
        });
      },
      // The testPageMod callback with test assertions is called on 'end',
      // and we want this page mod to be attached before it gets called,
      // so we attach it on 'start'.
      contentScriptWhen: 'start',
      onAttach: function(worker) {
        worker.postMessage([this.include[0], this.exclude[0]]);
      }
    };
  }

  testPageMod(assert, done, testPageURI, [
      createPageModTest("*", testPageURI, false),
      createPageModTest(testPageURI, testPageURI, false),
      createPageModTest(testPageURI, "resource://*", false),
      createPageModTest(testPageURI, "*.google.com", true)
    ],
    function (win, done) {
      waitUntil(() => win.localStorage[JSON.stringify([testPageURI, "*.google.com"])],
          testPageURI + " page-mod to be executed")
        .then(() => {
          asserts.forEach(fn => fn(assert, win));
          win.localStorage.clear();
          done();
        });
    });
};

exports.testPageModValidationAttachTo = function(assert) {
  [{ val: 'top', type: 'string "top"' },
   { val: 'frame', type: 'string "frame"' },
   { val: ['top', 'existing'], type: 'array with "top" and "existing"' },
   { val: ['frame', 'existing'], type: 'array with "frame" and "existing"' },
   { val: ['top'], type: 'array with "top"' },
   { val: ['frame'], type: 'array with "frame"' },
   { val: undefined, type: 'undefined' }].forEach((attachTo) => {
    new PageMod({ attachTo: attachTo.val, include: '*.validation111' });
    assert.pass("PageMod() does not throw when attachTo is " + attachTo.type);
  });

  [{ val: 'existing', type: 'string "existing"' },
   { val: ['existing'], type: 'array with "existing"' },
   { val: 'not-legit', type: 'string with "not-legit"' },
   { val: ['not-legit'], type: 'array with "not-legit"' },
   { val: {}, type: 'object' }].forEach((attachTo) => {
    assert.throws(() =>
      new PageMod({ attachTo: attachTo.val, include: '*.validation111' }),
      /The `attachTo` option/,
      "PageMod() throws when 'attachTo' option is " + attachTo.type + ".");
  });
};

exports.testPageModValidationInclude = function(assert) {
  [{ val: undefined, type: 'undefined' },
   { val: {}, type: 'object' },
   { val: [], type: 'empty array'},
   { val: [/regexp/, 1], type: 'array with non string/regexp' },
   { val: 1, type: 'number' }].forEach((include) => {
    assert.throws(() => new PageMod({ include: include.val }),
      /The `include` option must always contain atleast one rule/,
      "PageMod() throws when 'include' option is " + include.type + ".");
  });

  [{ val: '*.validation111', type: 'string' },
   { val: /validation111/, type: 'regexp' },
   { val: ['*.validation111'], type: 'array with length > 0'}].forEach((include) => {
    new PageMod({ include: include.val });
    assert.pass("PageMod() does not throw when include option is " + include.type);
  });
};

exports.testPageModValidationExclude = function(assert) {
  let includeVal = '*.validation111';

  [{ val: {}, type: 'object' },
   { val: [], type: 'empty array'},
   { val: [/regexp/, 1], type: 'array with non string/regexp' },
   { val: 1, type: 'number' }].forEach((exclude) => {
    assert.throws(() => new PageMod({ include: includeVal, exclude: exclude.val }),
      /If set, the `exclude` option must always contain at least one rule as a string, regular expression, or an array of strings and regular expressions./,
      "PageMod() throws when 'exclude' option is " + exclude.type + ".");
  });

  [{ val: undefined, type: 'undefined' },
   { val: '*.validation111', type: 'string' },
   { val: /validation111/, type: 'regexp' },
   { val: ['*.validation111'], type: 'array with length > 0'}].forEach((exclude) => {
    new PageMod({ include: includeVal, exclude: exclude.val });
    assert.pass("PageMod() does not throw when exclude option is " + exclude.type);
  });
};

/* Tests for internal functions. */
exports.testCommunication1 = function(assert, done) {
  let workerDone = false,
      callbackDone = null;

  testPageMod(assert, done, "about:", [{
      include: "about:*",
      contentScriptWhen: 'end',
      contentScript: 'new ' + function WorkerScope() {
        self.on('message', function(msg) {
          document.body.setAttribute('JEP-107', 'worked');
          self.postMessage(document.body.getAttribute('JEP-107'));
        })
      },
      onAttach: function(worker) {
        worker.on('error', function(e) {
          assert.fail('Errors where reported');
        });
        worker.on('message', function(value) {
          assert.equal(
            "worked",
            value,
            "test comunication"
          );
          workerDone = true;
          if (callbackDone)
            callbackDone();
        });
        worker.postMessage('do it!')
      }
    }],
    function(win, done) {
      (callbackDone = function() {
        if (workerDone) {
          assert.equal(
            'worked',
            win.document.body.getAttribute('JEP-107'),
            'attribute should be modified'
          );
          done();
        }
      })();
    }
  );
};

exports.testCommunication2 = function(assert, done) {
  let callbackDone = null,
      window;

  testPageMod(assert, done, "about:license", [{
      include: "about:*",
      contentScriptWhen: 'start',
      contentScript: 'new ' + function WorkerScope() {
        document.documentElement.setAttribute('AUQLUE', 42);
        window.addEventListener('load', function listener() {
          self.postMessage('onload');
        }, false);
        self.on("message", function() {
          self.postMessage(document.documentElement.getAttribute("test"))
        });
      },
      onAttach: function(worker) {
        worker.on('error', function(e) {
          assert.fail('Errors where reported');
        });
        worker.on('message', function(msg) {
          if ('onload' == msg) {
            assert.equal(
              '42',
              window.document.documentElement.getAttribute('AUQLUE'),
              'PageMod scripts executed in order'
            );
            window.document.documentElement.setAttribute('test', 'changes in window');
            worker.postMessage('get window.test')
          } else {
            assert.equal(
              'changes in window',
              msg,
              'PageMod test #2: second script has run'
            )
            callbackDone();
          }
        });
      }
    }],
    function(win, done) {
      window = win;
      callbackDone = done;
    }
  );
};

exports.testEventEmitter = function(assert, done) {
  let workerDone = false,
      callbackDone = null;

  testPageMod(assert, done, "about:", [{
      include: "about:*",
      contentScript: 'new ' + function WorkerScope() {
        self.port.on('addon-to-content', function(data) {
          self.port.emit('content-to-addon', data);
        });
      },
      onAttach: function(worker) {
        worker.on('error', function(e) {
          assert.fail('Errors were reported : '+e);
        });
        worker.port.on('content-to-addon', function(value) {
          assert.equal(
            "worked",
            value,
            "EventEmitter API works!"
          );
          if (callbackDone)
            callbackDone();
          else
            workerDone = true;
        });
        worker.port.emit('addon-to-content', 'worked');
      }
    }],
    function(win, done) {
      if (workerDone)
        done();
      else
        callbackDone = done;
    }
  );
};

// Execute two concurrent page mods on same document to ensure that their
// JS contexts are different
exports.testMixedContext = function(assert, done) {
  let doneCallback = null;
  let messages = 0;
  let modObject = {
    include: "data:text/html;charset=utf-8,",
    contentScript: 'new ' + function WorkerScope() {
      // Both scripts will execute this,
      // context is shared if one script see the other one modification.
      let isContextShared = "sharedAttribute" in document;
      self.postMessage(isContextShared);
      document.sharedAttribute = true;
    },
    onAttach: function(w) {
      w.on("message", function (isContextShared) {
        if (isContextShared) {
          assert.fail("Page mod contexts are mixed.");
          doneCallback();
        }
        else if (++messages == 2) {
          assert.pass("Page mod contexts are different.");
          doneCallback();
        }
      });
    }
  };
  testPageMod(assert, done, "data:text/html;charset=utf-8,", [modObject, modObject],
    function(win, done) {
      doneCallback = done;
    }
  );
};

exports.testHistory = function(assert, done) {
  // We need a valid url in order to have a working History API.
  // (i.e do not work on data: or about: pages)
  // Test bug 679054.
  let url = data.url("test-page-mod.html");
  let callbackDone = null;
  testPageMod(assert, done, url, [{
      include: url,
      contentScriptWhen: 'end',
      contentScript: 'new ' + function WorkerScope() {
        history.pushState({}, "", "#");
        history.replaceState({foo: "bar"}, "", "#");
        self.postMessage(history.state);
      },
      onAttach: function(worker) {
        worker.on('message', function (data) {
          assert.equal(JSON.stringify(data), JSON.stringify({foo: "bar"}),
                           "History API works!");
          callbackDone();
        });
      }
    }],
    function(win, done) {
      callbackDone = done;
    }
  );
};

exports.testRelatedTab = function(assert, done) {
  let tab;
  let pageMod = new PageMod({
    include: "about:*",
    onAttach: function(worker) {
      assert.ok(!!worker.tab, "Worker.tab exists");
      assert.equal(tab, worker.tab, "Worker.tab is valid");
      pageMod.destroy();
      tab.close(done);
    }
  });

  tabs.open({
    url: "about:",
    onOpen: function onOpen(t) {
      tab = t;
    }
  });
};

exports.testRelatedTabNoRequireTab = function(assert, done) {
  let loader = Loader(module);
  let tab;
  let url = "data:text/html;charset=utf-8," + encodeURI("Test related worker tab 2");
  let { PageMod } = loader.require("sdk/page-mod");
  let pageMod = new PageMod({
    include: url,
    onAttach: function(worker) {
      assert.equal(worker.tab.url, url, "Worker.tab.url is valid");
      worker.tab.close(function() {
        pageMod.destroy();
        loader.unload();
        done();
      });
    }
  });

  tabs.open(url);
};

exports.testRelatedTabNoOtherReqs = function(assert, done) {
  let loader = Loader(module);
  let { PageMod } = loader.require("sdk/page-mod");
  let pageMod = new PageMod({
    include: "about:blank?testRelatedTabNoOtherReqs",
    onAttach: function(worker) {
      assert.ok(!!worker.tab, "Worker.tab exists");
      pageMod.destroy();
      worker.tab.close(function() {
        worker.destroy();
        loader.unload();
        done();
      });
    }
  });

  tabs.open({
    url: "about:blank?testRelatedTabNoOtherReqs"
  });
};

exports.testWorksWithExistingTabs = function(assert, done) {
  let url = "data:text/html;charset=utf-8," + encodeURI("Test unique document");
  let { PageMod } = require("sdk/page-mod");
  tabs.open({
    url: url,
    onReady: function onReady(tab) {
      let pageModOnExisting = new PageMod({
        include: url,
        attachTo: ["existing", "top", "frame"],
        onAttach: function(worker) {
          assert.ok(!!worker.tab, "Worker.tab exists");
          assert.equal(tab, worker.tab, "A worker has been created on this existing tab");

          setTimeout(function() {
            pageModOnExisting.destroy();
            pageModOffExisting.destroy();
            tab.close(done);
          }, 0);
        }
      });

      let pageModOffExisting = new PageMod({
        include: url,
        onAttach: function(worker) {
          assert.fail("pageModOffExisting page-mod should not have attached to anything");
        }
      });
    }
  });
};

exports.testExistingFrameDoesntMatchInclude = function(assert, done) {
  let iframeURL = 'data:text/html;charset=utf-8,UNIQUE-TEST-STRING-42';
  let iframe = '<iframe src="' + iframeURL + '" />';
  let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(iframe);
  tabs.open({
    url: url,
    onReady: function onReady(tab) {
      let pagemod = new PageMod({
        include: url,
        attachTo: ['existing', 'frame'],
        onAttach: function() {
          assert.fail("Existing iframe URL doesn't match include, must not attach to anything");
        }
      });
      setTimeout(function() {
        assert.pass("PageMod didn't attach to anything")
        pagemod.destroy();
        tab.close(done);
      }, 250);
    }
  });
};

exports.testExistingOnlyFrameMatchesInclude = function(assert, done) {
  let iframeURL = 'data:text/html;charset=utf-8,UNIQUE-TEST-STRING-43';
  let iframe = '<iframe src="' + iframeURL + '" />';
  let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(iframe);
  tabs.open({
    url: url,
    onReady: function onReady(tab) {
      let pagemod = new PageMod({
        include: iframeURL,
        attachTo: ['existing', 'frame'],
        onAttach: function(worker) {
          assert.equal(iframeURL, worker.url,
              "PageMod attached to existing iframe when only it matches include rules");
          pagemod.destroy();
          tab.close(done);
        }
      });
    }
  });
};

exports.testContentScriptWhenDefault = function(assert) {
  let pagemod = PageMod({include: '*'});

  assert.equal(pagemod.contentScriptWhen, 'end', "Default contentScriptWhen is 'end'");
  pagemod.destroy();
}

// test timing for all 3 contentScriptWhen options (start, ready, end)
// for new pages, or tabs opened after PageMod is created
exports.testContentScriptWhenForNewTabs = function(assert, done) {
  const url = "data:text/html;charset=utf-8,testContentScriptWhenForNewTabs";

  let count = 0;

  handleReadyState(url, 'start', {
    onLoading: (tab) => {
      assert.pass("PageMod is attached while document is loading");
      if (++count === 3)
        tab.close(done);
    },
    onInteractive: () => assert.fail("onInteractive should not be called with 'start'."),
    onComplete: () => assert.fail("onComplete should not be called with 'start'."),
  });

  handleReadyState(url, 'ready', {
    onInteractive: (tab) => {
      assert.pass("PageMod is attached while document is interactive");
      if (++count === 3)
        tab.close(done);
    },
    onLoading: () => assert.fail("onLoading should not be called with 'ready'."),
    onComplete: () => assert.fail("onComplete should not be called with 'ready'."),
  });

  handleReadyState(url, 'end', {
    onComplete: (tab) => {
      assert.pass("PageMod is attached when document is complete");
      if (++count === 3)
        tab.close(done);
    },
    onLoading: () => assert.fail("onLoading should not be called with 'end'."),
    onInteractive: () => assert.fail("onInteractive should not be called with 'end'."),
  });

  tabs.open(url);
}

// test timing for all 3 contentScriptWhen options (start, ready, end)
// for PageMods created right as the tab is created (in tab.onOpen)
exports.testContentScriptWhenOnTabOpen = function(assert, done) {
  const url = "data:text/html;charset=utf-8,testContentScriptWhenOnTabOpen";

  tabs.open({
    url: url,
    onOpen: function(tab) {
      let count = 0;

      handleReadyState(url, 'start', {
        onLoading: () => {
          assert.pass("PageMod is attached while document is loading");
          if (++count === 3)
            tab.close(done);
        },
        onInteractive: () => assert.fail("onInteractive should not be called with 'start'."),
        onComplete: () => assert.fail("onComplete should not be called with 'start'."),
      });

      handleReadyState(url, 'ready', {
        onInteractive: () => {
          assert.pass("PageMod is attached while document is interactive");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'ready'."),
        onComplete: () => assert.fail("onComplete should not be called with 'ready'."),
      });

      handleReadyState(url, 'end', {
        onComplete: () => {
          assert.pass("PageMod is attached when document is complete");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'end'."),
        onInteractive: () => assert.fail("onInteractive should not be called with 'end'."),
      });

    }
  });
}

// test timing for all 3 contentScriptWhen options (start, ready, end)
// for PageMods created while the tab is interactive (in tab.onReady)
exports.testContentScriptWhenOnTabReady = function(assert, done) {
  // need a bit bigger document to get the right timing of events with e10s
  let iframeURL = 'data:text/html;charset=utf-8,testContentScriptWhenOnTabReady';
  let iframe = '<iframe src="' + iframeURL + '" />';
  let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(iframe);
  tabs.open({
    url: url,
    onReady: function(tab) {
      let count = 0;

      handleReadyState(url, 'start', {
        onInteractive: () => {
          assert.pass("PageMod is attached while document is interactive");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'start'."),
        onComplete: () => assert.fail("onComplete should not be called with 'start'."),
      });

      handleReadyState(url, 'ready', {
        onInteractive: () => {
          assert.pass("PageMod is attached while document is interactive");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'ready'."),
        onComplete: () => assert.fail("onComplete should not be called with 'ready'."),
      });

      handleReadyState(url, 'end', {
        onComplete: () => {
          assert.pass("PageMod is attached when document is complete");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'end'."),
        onInteractive: () => assert.fail("onInteractive should not be called with 'end'."),
      });

    }
  });
}

// test timing for all 3 contentScriptWhen options (start, ready, end)
// for PageMods created after a tab has completed loading (in tab.onLoad)
exports.testContentScriptWhenOnTabLoad = function(assert, done) {
  const url = "data:text/html;charset=utf-8,testContentScriptWhenOnTabLoad";

  tabs.open({
    url: url,
    onLoad: function(tab) {
      let count = 0;

      handleReadyState(url, 'start', {
        onComplete: () => {
          assert.pass("PageMod is attached when document is complete");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'start'."),
        onInteractive: () => assert.fail("onInteractive should not be called with 'start'."),
      });

      handleReadyState(url, 'ready', {
        onComplete: () => {
          assert.pass("PageMod is attached when document is complete");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'ready'."),
        onInteractive: () => assert.fail("onInteractive should not be called with 'ready'."),
      });

      handleReadyState(url, 'end', {
        onComplete: () => {
          assert.pass("PageMod is attached when document is complete");
          if (++count === 3)
            tab.close(done);
        },
        onLoading: () => assert.fail("onLoading should not be called with 'end'."),
        onInteractive: () => assert.fail("onInteractive should not be called with 'end'."),
      });

    }
  });
}

exports.testTabWorkerOnMessage = function(assert, done) {
  let { browserWindows } = require("sdk/windows");
  let tabs = require("sdk/tabs");
  let { PageMod } = require("sdk/page-mod");

  let url1 = "data:text/html;charset=utf-8,<title>tab1</title><h1>worker1.tab</h1>";
  let url2 = "data:text/html;charset=utf-8,<title>tab2</title><h1>worker2.tab</h1>";
  let worker1 = null;

  let mod = PageMod({
    include: "data:text/html*",
    contentScriptWhen: "ready",
    contentScript: "self.postMessage('#1');",
    onAttach: function onAttach(worker) {
      worker.on("message", function onMessage() {
        this.tab.attach({
          contentScriptWhen: "ready",
          contentScript: "self.postMessage({ url: window.location.href, title: document.title });",
          onMessage: function onMessage(data) {
            assert.equal(this.tab.url, data.url, "location is correct");
            assert.equal(this.tab.title, data.title, "title is correct");
            if (this.tab.url === url1) {
              worker1 = this;
              tabs.open({ url: url2, inBackground: true });
            }
            else if (this.tab.url === url2) {
              mod.destroy();
              worker1.tab.close(function() {
                worker1.destroy();
                worker.tab.close(function() {
                  worker.destroy();
                  done();
                });
              });
            }
          }
        });
      });
    }
  });

  tabs.open(url1);
};

exports.testAutomaticDestroy = function(assert, done) {
  let loader = Loader(module);

  let pageMod = loader.require("sdk/page-mod").PageMod({
    include: "about:*",
    contentScriptWhen: "start",
    onAttach: function(w) {
      assert.fail("Page-mod should have been detroyed during module unload");
    }
  });

  // Unload the page-mod module so that our page mod is destroyed
  loader.unload();

  // Then create a second tab to ensure that it is correctly destroyed
  let tabs = require("sdk/tabs");
  tabs.open({
    url: "about:",
    onReady: function onReady(tab) {
      assert.pass("check automatic destroy");
      tab.close(done);
    }
  });
};

exports.testAttachToTabsOnly = function(assert, done) {
  let { PageMod } = require('sdk/page-mod');
  let openedTab = null; // Tab opened in openTabWithIframe()
  let workerCount = 0;

  let mod = PageMod({
    include: 'data:text/html*',
    contentScriptWhen: 'start',
    contentScript: '',
    onAttach: function onAttach(worker) {
      if (worker.tab === openedTab) {
        if (++workerCount == 3) {
          assert.pass('Succesfully applied to tab documents and its iframe');
          worker.destroy();
          mod.destroy();
          openedTab.close(done);
        }
      }
      else {
        assert.fail('page-mod attached to a non-tab document');
      }
    }
  });

  function openHiddenFrame() {
    assert.pass('Open iframe in hidden window');
    let hiddenFrames = require('sdk/frame/hidden-frame');
    let hiddenFrame = hiddenFrames.add(hiddenFrames.HiddenFrame({
      onReady: function () {
        let element = this.element;
        element.addEventListener('DOMContentLoaded', function onload() {
          element.removeEventListener('DOMContentLoaded', onload, false);
          hiddenFrames.remove(hiddenFrame);

          if (!xulApp.is("Fennec")) {
            openToplevelWindow();
          }
          else {
            openBrowserIframe();
          }
        }, false);
        element.setAttribute('src', 'data:text/html;charset=utf-8,foo');
      }
    }));
  }

  function openToplevelWindow() {
    assert.pass('Open toplevel window');
    let win = open('data:text/html;charset=utf-8,bar');
    win.addEventListener('DOMContentLoaded', function onload() {
      win.removeEventListener('DOMContentLoaded', onload, false);
      win.close();
      openBrowserIframe();
    }, false);
  }

  function openBrowserIframe() {
    assert.pass('Open iframe in browser window');
    let window = require('sdk/deprecated/window-utils').activeBrowserWindow;
    let document = window.document;
    let iframe = document.createElement('iframe');
    iframe.setAttribute('type', 'content');
    iframe.setAttribute('src', 'data:text/html;charset=utf-8,foobar');
    iframe.addEventListener('DOMContentLoaded', function onload() {
      iframe.removeEventListener('DOMContentLoaded', onload, false);
      iframe.parentNode.removeChild(iframe);
      openTabWithIframes();
    }, false);
    document.documentElement.appendChild(iframe);
  }

  // Only these three documents will be accepted by the page-mod
  function openTabWithIframes() {
    assert.pass('Open iframes in a tab');
    let subContent = '<iframe src="data:text/html;charset=utf-8,sub frame" />'
    let content = '<iframe src="data:text/html;charset=utf-8,' +
                  encodeURIComponent(subContent) + '" />';
    require('sdk/tabs').open({
      url: 'data:text/html;charset=utf-8,' + encodeURIComponent(content),
      onOpen: function onOpen(tab) {
        openedTab = tab;
      }
    });
  }

  openHiddenFrame();
};

exports['test111 attachTo [top]'] = function(assert, done) {
  let { PageMod } = require('sdk/page-mod');

  let subContent = '<iframe src="data:text/html;charset=utf-8,sub frame" />'
  let content = '<iframe src="data:text/html;charset=utf-8,' +
                encodeURIComponent(subContent) + '" />';
  let topDocumentURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(content)

  let workerCount = 0;

  let mod = PageMod({
    include: 'data:text/html*',
    contentScriptWhen: 'start',
    contentScript: 'self.postMessage(document.location.href);',
    attachTo: ['top'],
    onAttach: function onAttach(worker) {
      if (++workerCount == 1) {
        worker.on('message', function (href) {
          assert.equal(href, topDocumentURL,
                           "worker on top level document only");
          let tab = worker.tab;
          worker.destroy();
          mod.destroy();
          tab.close(done);
        });
      }
      else {
        assert.fail('page-mod attached to a non-top document');
      }
    }
  });

  require('sdk/tabs').open(topDocumentURL);
};

exports['test111 attachTo [frame]'] = function(assert, done) {
  let { PageMod } = require('sdk/page-mod');

  let subFrameURL = 'data:text/html;charset=utf-8,subframe';
  let subContent = '<iframe src="' + subFrameURL + '" />';
  let frameURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(subContent);
  let content = '<iframe src="' + frameURL + '" />';
  let topDocumentURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(content)

  let workerCount = 0, messageCount = 0;

  function onMessage(href) {
    if (href == frameURL)
      assert.pass("worker on first frame");
    else if (href == subFrameURL)
      assert.pass("worker on second frame");
    else
      assert.fail("worker on unexpected document: " + href);
    this.destroy();
    if (++messageCount == 2) {
      mod.destroy();
      require('sdk/tabs').activeTab.close(done);
    }
  }
  let mod = PageMod({
    include: 'data:text/html*',
    contentScriptWhen: 'start',
    contentScript: 'self.postMessage(document.location.href);',
    attachTo: ['frame'],
    onAttach: function onAttach(worker) {
      if (++workerCount <= 2) {
        worker.on('message', onMessage);
      }
      else {
        assert.fail('page-mod attached to a non-frame document');
      }
    }
  });

  require('sdk/tabs').open(topDocumentURL);
};

exports.testContentScriptOptionsOption = function(assert, done) {
  let callbackDone = null;
  testPageMod(assert, done, "about:", [{
      include: "about:*",
      contentScript: "self.postMessage( [typeof self.options.d, self.options] );",
      contentScriptWhen: "end",
      contentScriptOptions: {a: true, b: [1,2,3], c: "string", d: function(){ return 'test'}},
      onAttach: function(worker) {
        worker.on('message', function(msg) {
          assert.equal( msg[0], 'undefined', 'functions are stripped from contentScriptOptions' );
          assert.equal( typeof msg[1], 'object', 'object as contentScriptOptions' );
          assert.equal( msg[1].a, true, 'boolean in contentScriptOptions' );
          assert.equal( msg[1].b.join(), '1,2,3', 'array and numbers in contentScriptOptions' );
          assert.equal( msg[1].c, 'string', 'string in contentScriptOptions' );
          callbackDone();
        });
      }
    }],
    function(win, done) {
      callbackDone = done;
    }
  );
};

exports.testPageModCss = function(assert, done) {
  let [pageMod] = testPageMod(assert, done,
    'data:text/html;charset=utf-8,<div style="background: silver">css test</div>', [{
      include: ["*", "data:*"],
      contentStyle: "div { height: 100px; }",
      contentStyleFile: data.url("css-include-file.css")
    }],
    function(win, done) {
      let div = win.document.querySelector("div");
      assert.equal(
        div.clientHeight,
        100,
        "PageMod contentStyle worked"
      );
      assert.equal(
       div.offsetHeight,
        120,
        "PageMod contentStyleFile worked"
      );
      done();
    }
  );
};

exports.testPageModCssList = function(assert, done) {
  let [pageMod] = testPageMod(assert, done,
    'data:text/html;charset=utf-8,<div style="width:320px; max-width: 480px!important">css test</div>', [{
      include: "data:*",
      contentStyleFile: [
        // Highlight evaluation order in this list
        "data:text/css;charset=utf-8,div { border: 1px solid black; }",
        "data:text/css;charset=utf-8,div { border: 10px solid black; }",
        // Highlight evaluation order between contentStylesheet & contentStylesheetFile
        "data:text/css;charset=utf-8s,div { height: 1000px; }",
        // Highlight precedence between the author and user style sheet
        "data:text/css;charset=utf-8,div { width: 200px; max-width: 640px!important}",
      ],
      contentStyle: [
        "div { height: 10px; }",
        "div { height: 100px; }"
      ]
    }],
    function(win, done) {
      let div = win.document.querySelector("div"),
          style = win.getComputedStyle(div);

      assert.equal(
       div.clientHeight,
        100,
        "PageMod contentStyle list works and is evaluated after contentStyleFile"
      );

      assert.equal(
        div.offsetHeight,
        120,
        "PageMod contentStyleFile list works"
      );

      assert.equal(
        style.width,
        "320px",
        "PageMod add-on author/page author style sheet precedence works"
      );

      assert.equal(
        style.maxWidth,
        "480px",
        "PageMod add-on author/page author style sheet precedence with !important works"
      );

      done();
    }
  );
};

exports.testPageModCssDestroy = function(assert, done) {
  let [pageMod] = testPageMod(assert, done,
    'data:text/html;charset=utf-8,<div style="width:200px">css test</div>', [{
      include: "data:*",
      contentStyle: "div { width: 100px!important; }"
    }],

    function(win, done) {
      let div = win.document.querySelector("div"),
          style = win.getComputedStyle(div);

      assert.equal(
        style.width,
        "100px",
        "PageMod contentStyle worked"
      );

      pageMod.destroy();
      assert.equal(
        style.width,
        "200px",
        "PageMod contentStyle is removed after destroy"
      );

      done();

    }
  );
};

exports.testPageModCssAutomaticDestroy = function(assert, done) {
  let loader = Loader(module);

  let pageMod = loader.require("sdk/page-mod").PageMod({
    include: "data:*",
    contentStyle: "div { width: 100px!important; }"
  });

  tabs.open({
    url: "data:text/html;charset=utf-8,<div style='width:200px'>css test</div>",

    onReady: function onReady(tab) {
      let browserWindow = getMostRecentBrowserWindow();
      let win = getTabContentWindow(getActiveTab(browserWindow));

      let div = win.document.querySelector("div");
      let style = win.getComputedStyle(div);

      assert.equal(
        style.width,
        "100px",
        "PageMod contentStyle worked"
      );

      loader.unload();

      assert.equal(
        style.width,
        "200px",
        "PageMod contentStyle is removed after loader's unload"
      );

      tab.close(done);
    }
  });
};


exports.testPageModTimeout = function(assert, done) {
  let tab = null
  let loader = Loader(module);
  let { PageMod } = loader.require("sdk/page-mod");

  let mod = PageMod({
    include: "data:*",
    contentScript: Isolate(function() {
      var id = setTimeout(function() {
        self.port.emit("fired", id)
      }, 10)
      self.port.emit("scheduled", id);
    }),
    onAttach: function(worker) {
      worker.port.on("scheduled", function(id) {
        assert.pass("timer was scheduled")
        worker.port.on("fired", function(data) {
          assert.equal(id, data, "timer was fired")
          tab.close(function() {
            worker.destroy()
            loader.unload()
            done()
          });
        })
      })
    }
  });

  tabs.open({
    url: "data:text/html;charset=utf-8,timeout",
    onReady: function($) { tab = $ }
  })
}


exports.testPageModcancelTimeout = function(assert, done) {
  let tab = null
  let loader = Loader(module);
  let { PageMod } = loader.require("sdk/page-mod");

  let mod = PageMod({
    include: "data:*",
    contentScript: Isolate(function() {
      var id1 = setTimeout(function() {
        self.port.emit("failed")
      }, 10)
      var id2 = setTimeout(function() {
        self.port.emit("timeout")
      }, 100)
      clearTimeout(id1)
    }),
    onAttach: function(worker) {
      worker.port.on("failed", function() {
        assert.fail("cancelled timeout fired")
      })
      worker.port.on("timeout", function(id) {
        assert.pass("timer was scheduled")
        tab.close(function() {
          worker.destroy();
          mod.destroy();
          loader.unload();
          done();
        });
      })
    }
  });

  tabs.open({
    url: "data:text/html;charset=utf-8,cancell timeout",
    onReady: function($) { tab = $ }
  })
}

exports.testExistingOnFrames = function(assert, done) {
  let subFrameURL = 'data:text/html;charset=utf-8,testExistingOnFrames-sub-frame';
  let subIFrame = '<iframe src="' + subFrameURL + '" />'
  let iFrameURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(subIFrame)
  let iFrame = '<iframe src="' + iFrameURL + '" />';
  let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(iFrame);

  // we want all urls related to the test here, and not just the iframe urls
  // because we need to fail if the test is applied to the top window url.
  let urls = [url, iFrameURL, subFrameURL];

  let counter = 0;
  let tab = openTab(getMostRecentBrowserWindow(), url);
  let window = getTabContentWindow(tab);

  function wait4Iframes() {
    if (window.document.readyState != "complete" ||
        getFrames(window).length != 2) {
      return;
    }

    let pagemodOnExisting = PageMod({
      include: ["*", "data:*"],
      attachTo: ["existing", "frame"],
      contentScriptWhen: 'ready',
      onAttach: function(worker) {
        // need to ignore urls that are not part of the test, because other
        // tests are not closing their tabs when they complete..
        if (urls.indexOf(worker.url) == -1)
          return;

        assert.notEqual(url,
                            worker.url,
                            'worker should not be attached to the top window');

        if (++counter < 2) {
          // we can rely on this order in this case because we are sure that
          // the frames being tested have completely loaded
          assert.equal(iFrameURL, worker.url, '1st attach is for top frame');
        }
        else if (counter > 2) {
          assert.fail('applied page mod too many times');
        }
        else {
          assert.equal(subFrameURL, worker.url, '2nd attach is for sub frame');
          // need timeout because onAttach is called before the constructor returns
          setTimeout(function() {
            pagemodOnExisting.destroy();
            pagemodOffExisting.destroy();
            closeTab(tab);
            done();
          }, 0);
        }
      }
    });

    let pagemodOffExisting = PageMod({
      include: ["*", "data:*"],
      attachTo: ["frame"],
      contentScriptWhen: 'ready',
      onAttach: function(mod) {
        assert.fail('pagemodOffExisting page-mod should not have been attached');
      }
    });
  }

  window.addEventListener("load", wait4Iframes, false);
};

exports.testIFramePostMessage = function(assert, done) {
  let count = 0;

  tabs.open({
    url: data.url("test-iframe.html"),
    onReady: function(tab) {
      var worker = tab.attach({
        contentScriptFile: data.url('test-iframe.js'),
        contentScript: 'var iframePath = \'' + data.url('test-iframe-postmessage.html') + '\'',
        onMessage: function(msg) {
          assert.equal(++count, 1);
          assert.equal(msg.first, 'a string');
          assert.ok(msg.second[1], "array");
          assert.equal(typeof msg.third, 'object');

          worker.destroy();
          tab.close(done);
        }
      });
    }
  });
};

exports.testEvents = function(assert, done) {
  let content = "<script>\n new " + function DocumentScope() {
    window.addEventListener("ContentScriptEvent", function () {
      window.receivedEvent = true;
    }, false);
  } + "\n</script>";
  let url = "data:text/html;charset=utf-8," + encodeURIComponent(content);
  testPageMod(assert, done, url, [{
      include: "data:*",
      contentScript: 'new ' + function WorkerScope() {
        let evt = document.createEvent("Event");
        evt.initEvent("ContentScriptEvent", true, true);
        document.body.dispatchEvent(evt);
      }
    }],
    function(win, done) {
      assert.ok(
        win.receivedEvent,
        "Content script sent an event and document received it"
      );
      done();
    }
  );
};

exports["test page-mod on private tab"] = function (assert, done) {
  let fail = assert.fail.bind(assert);

  let privateUri = "data:text/html;charset=utf-8," +
                   "<iframe src=\"data:text/html;charset=utf-8,frame\" />";
  let nonPrivateUri = "data:text/html;charset=utf-8,non-private";

  let pageMod = new PageMod({
    include: "data:*",
    onAttach: function(worker) {
      if (isTabPBSupported || isWindowPBSupported) {
        // When PB isn't supported, the page-mod will apply to all document
        // as all of them will be non-private
        assert.equal(worker.tab.url,
                         nonPrivateUri,
                         "page-mod should only attach to the non-private tab");
      }

      assert.ok(!isPrivate(worker),
                  "The worker is really non-private");
      assert.ok(!isPrivate(worker.tab),
                  "The document is really non-private");
      pageMod.destroy();

      page1.close().
        then(page2.close).
        then(done, fail);
    }
  });

  let page1, page2;
  page1 = openWebpage(privateUri, true);
  page1.ready.then(function() {
    page2 = openWebpage(nonPrivateUri, false);
  }, fail);
}

exports["test page-mod on private tab in global pb"] = function (assert, done) {
  if (!isGlobalPBSupported) {
    assert.pass();
    return done();
  }

  let privateUri = "data:text/html;charset=utf-8," +
                   "<iframe%20src=\"data:text/html;charset=utf-8,frame\"/>";

  let pageMod = new PageMod({
    include: privateUri,
    onAttach: function(worker) {
      assert.equal(worker.tab.url,
                       privateUri,
                       "page-mod should attach");
      assert.equal(isPrivateBrowsingSupported,
                       false,
                       "private browsing is not supported");
      assert.ok(isPrivate(worker),
                  "The worker is really non-private");
      assert.ok(isPrivate(worker.tab),
                  "The document is really non-private");
      pageMod.destroy();

      worker.tab.close(function() {
        pb.once('stop', function() {
          assert.pass('global pb stop');
          done();
        });
        pb.deactivate();
      });
    }
  });

  let page1;
  pb.once('start', function() {
    assert.pass('global pb start');
    tabs.open({ url: privateUri });
  });
  pb.activate();
}

// Bug 699450: Calling worker.tab.close() should not lead to exception
exports.testWorkerTabClose = function(assert, done) {
  let callbackDone;
  testPageMod(assert, done, "about:", [{
      include: "about:",
      contentScript: '',
      onAttach: function(worker) {
        assert.pass("The page-mod was attached");

        worker.tab.close(function () {
          // On Fennec, tab is completely destroyed right after close event is
          // dispatch, so we need to wait for the next event loop cycle to
          // check for tab nulliness.
          setTimeout(function () {
            assert.ok(!worker.tab,
                        "worker.tab should be null right after tab.close()");
            callbackDone();
          }, 0);
        });
      }
    }],
    function(win, done) {
      callbackDone = done;
    }
  );
};

exports.testDebugMetadata = function(assert, done) {
  let dbg = new Debugger;
  let globalDebuggees = [];
  dbg.onNewGlobalObject = function(global) {
    globalDebuggees.push(global);
  }

  let mods = testPageMod(assert, done, "about:", [{
      include: "about:",
      contentScriptWhen: "start",
      contentScript: "null;",
    }], function(win, done) {
      assert.ok(globalDebuggees.some(function(global) {
        try {
          let metadata = Cu.getSandboxMetadata(global.unsafeDereference());
          return metadata && metadata.addonID && metadata.SDKContentScript &&
                 metadata['inner-window-id'] == getInnerId(win);
        } catch(e) {
          // Some of the globals might not be Sandbox instances and thus
          // will cause getSandboxMetadata to fail.
          return false;
        }
      }), "one of the globals is a content script");
      done();
    }
  );
};

exports.testDevToolsExtensionsGetContentGlobals = function(assert, done) {
  let mods = testPageMod(assert, done, "about:", [{
      include: "about:",
      contentScriptWhen: "start",
      contentScript: "null;",
    }], function(win, done) {
      assert.equal(contentGlobals.getContentGlobals({ 'inner-window-id': getInnerId(win) }).length, 1);
      done();
    }
  );
};

exports.testDetachOnDestroy = function(assert, done) {
  let tab;
  const TEST_URL = 'data:text/html;charset=utf-8,detach';
  const loader = Loader(module);
  const { PageMod } = loader.require('sdk/page-mod');

  let mod1 = PageMod({
    include: TEST_URL,
    contentScript: Isolate(function() {
      self.port.on('detach', function(reason) {
        window.document.body.innerHTML += '!' + reason;
      });
    }),
    onAttach: worker => {
      assert.pass('attach[1] happened');

      worker.on('detach', _ => setTimeout(_ => {
        assert.pass('detach happened');

        let mod2 = PageMod({
          attachTo: [ 'existing', 'top' ],
          include: TEST_URL,
          contentScript: Isolate(function() {
            self.port.on('test', _ => {
              self.port.emit('result', { result: window.document.body.innerHTML});
            });
          }),
          onAttach: worker => {
            assert.pass('attach[2] happened');
            worker.port.once('result', ({ result }) => {
              assert.equal(result, 'detach!', 'the body.innerHTML is as expected');
              mod1.destroy();
              mod2.destroy();
              loader.unload();
              tab.close(done);
            });
            worker.port.emit('test');
          }
        });
      }));

      worker.destroy();
    }
  });

  tabs.open({
    url: TEST_URL,
    onOpen: t => tab = t
  })
}

exports.testDetachOnUnload = function(assert, done) {
  let tab;
  const TEST_URL = 'data:text/html;charset=utf-8,detach';
  const loader = Loader(module);
  const { PageMod } = loader.require('sdk/page-mod');

  let mod1 = PageMod({
    include: TEST_URL,
    contentScript: Isolate(function() {
      self.port.on('detach', function(reason) {
        window.document.body.innerHTML += '!' + reason;
      });
    }),
    onAttach: worker => {
      assert.pass('attach[1] happened');

      worker.on('detach', _ => setTimeout(_ => {
        assert.pass('detach happened');

        let mod2 = require('sdk/page-mod').PageMod({
          attachTo: [ 'existing', 'top' ],
          include: TEST_URL,
          contentScript: Isolate(function() {
            self.port.on('test', _ => {
              self.port.emit('result', { result: window.document.body.innerHTML});
            });
          }),
          onAttach: worker => {
            assert.pass('attach[2] happened');
            worker.port.once('result', ({ result }) => {
              assert.equal(result, 'detach!shutdown', 'the body.innerHTML is as expected');
              mod2.destroy();
              tab.close(done);
            });
            worker.port.emit('test');
          }
        });
      }));

      loader.unload('shutdown');
    }
  });

  tabs.open({
    url: TEST_URL,
    onOpen: t => tab = t
  })
}

exports.testConsole = function(assert, done) {
  let innerID;
  const TEST_URL = 'data:text/html;charset=utf-8,console';
  const { loader } = LoaderWithHookedConsole(module, onMessage);
  const { PageMod } = loader.require('sdk/page-mod');
  const system = require("sdk/system/events");

  let seenMessage = false;
  function onMessage(type, msg, msgID) {
    seenMessage = true;
    innerID = msgID;
  }

  let mod = PageMod({
    include: TEST_URL,
    contentScriptWhen: "ready",
    contentScript: Isolate(function() {
      console.log("Hello from the page mod");
      self.port.emit("done");
    }),
    onAttach: function(worker) {
      worker.port.on("done", function() {
        let window = getTabContentWindow(tab);
        let id = getInnerId(window);
        assert.ok(seenMessage, "Should have seen the console message");
        assert.equal(innerID, id, "Should have seen the right inner ID");
        closeTab(tab);
        done();
      });
    },
  });

  let tab = openTab(getMostRecentBrowserWindow(), TEST_URL);
}

exports.testSyntaxErrorInContentScript = function(assert, done) {
  const url = "data:text/html;charset=utf-8,testSyntaxErrorInContentScript";
  let hitError = null;
  let attached = false;

  testPageMod(assert, done, url, [{
      include: url,
      contentScript: 'console.log(23',

      onAttach: function() {
        attached = true;
      },

      onError: function(e) {
        hitError = e;
      }
    }],

    function(win, done) {
      assert.ok(attached, "The worker was attached.");
      assert.notStrictEqual(hitError, null, "The syntax error was reported.");
      if (hitError)
        assert.equal(hitError.name, "SyntaxError", "The error thrown should be a SyntaxError");
      done();
    }
  );
};

require('sdk/test').run(exports);
