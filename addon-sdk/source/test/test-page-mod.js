/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PageMod } = require("sdk/page-mod");
const testPageMod = require("./pagemod-test-helpers").testPageMod;
const { Loader } = require('sdk/test/loader');
const tabs = require("sdk/tabs");
const timer = require("sdk/timers");
const { Cc, Ci } = require("chrome");
const { open, getFrames, getMostRecentBrowserWindow } = require('sdk/window/utils');
const windowUtils = require('sdk/deprecated/window-utils');
const { getTabContentWindow, getActiveTab, setTabURL, openTab, closeTab } = require('sdk/tabs/utils');
const xulApp = require("sdk/system/xul-app");
const { data, isPrivateBrowsingSupported } = require('sdk/self');
const { isPrivate } = require('sdk/private-browsing');
const { openWebpage } = require('./private-browsing/helper');
const { isTabPBSupported, isWindowPBSupported, isGlobalPBSupported } = require('sdk/private-browsing/utils');
const promise = require("sdk/core/promise");
const { pb } = require('./private-browsing/helper');

/* XXX This can be used to delay closing the test Firefox instance for interactive
 * testing or visual inspection. This test is registered first so that it runs
 * the last. */
exports.delay = function(test) {
  if (false) {
    test.waitUntilDone(60000);
    timer.setTimeout(function() {test.done();}, 4000);
  }
  else {
    test.pass();
  }
}

function Isolate(worker) {
  return "(" + worker + ")()";
}

/* Tests for the PageMod APIs */

exports.testPageMod1 = function(test) {
  let mods = testPageMod(test, "about:", [{
      include: /about:/,
      contentScriptWhen: 'end',
      contentScript: 'new ' + function WorkerScope() {
        window.document.body.setAttribute("JEP-107", "worked");
      },
      onAttach: function() {
        test.assertEqual(this, mods[0], "The 'this' object is the page mod.");
      }
    }],
    function(win, done) {
      test.assertEqual(
        win.document.body.getAttribute("JEP-107"),
        "worked",
        "PageMod.onReady test"
      );
      done();
    }
  );
};

exports.testPageMod2 = function(test) {
  testPageMod(test, "about:", [{
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
      test.assertEqual(win.document.documentElement.getAttribute("first"),
                       "true",
                       "PageMod test #2: first script has run");
      test.assertEqual(win.document.documentElement.getAttribute("second"),
                       "true",
                       "PageMod test #2: second script has run");
      test.assertEqual("AUQLUE" in win, false,
                       "PageMod test #2: scripts get a wrapped window");
      done();
    });
};

exports.testPageModIncludes = function(test) {
  var asserts = [];
  function createPageModTest(include, expectedMatch) {
    // Create an 'onload' test function...
    asserts.push(function(test, win) {
      var matches = include in win.localStorage;
      test.assert(expectedMatch ? matches : !matches,
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

  testPageMod(test, "about:buildconfig", [
      createPageModTest("*", false),
      createPageModTest("*.google.com", false),
      createPageModTest("about:*", true),
      createPageModTest("about:", false),
      createPageModTest("about:buildconfig", true)
    ],
    function (win, done) {
      test.waitUntil(function () win.localStorage["about:buildconfig"],
                     "about:buildconfig page-mod to be executed")
          .then(function () {
            asserts.forEach(function(fn) {
              fn(test, win);
            });
            done();
          });
    }
    );
};

exports.testPageModErrorHandling = function(test) {
  test.assertRaises(function() {
      new PageMod();
    },
    'The `include` option must always contain atleast one rule',
    "PageMod() throws when 'include' option is not specified.");
};

/* Tests for internal functions. */
exports.testCommunication1 = function(test) {
  let workerDone = false,
      callbackDone = null;

  testPageMod(test, "about:", [{
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
          test.fail('Errors where reported');
        });
        worker.on('message', function(value) {
          test.assertEqual(
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
          test.assertEqual(
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

exports.testCommunication2 = function(test) {
  let callbackDone = null,
      window;

  testPageMod(test, "about:license", [{
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
          test.fail('Errors where reported');
        });
        worker.on('message', function(msg) {
          if ('onload' == msg) {
            test.assertEqual(
              '42',
              window.document.documentElement.getAttribute('AUQLUE'),
              'PageMod scripts executed in order'
            );
            window.document.documentElement.setAttribute('test', 'changes in window');
            worker.postMessage('get window.test')
          } else {
            test.assertEqual(
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

exports.testEventEmitter = function(test) {
  let workerDone = false,
      callbackDone = null;

  testPageMod(test, "about:", [{
      include: "about:*",
      contentScript: 'new ' + function WorkerScope() {
        self.port.on('addon-to-content', function(data) {
          self.port.emit('content-to-addon', data);
        });
      },
      onAttach: function(worker) {
        worker.on('error', function(e) {
          test.fail('Errors were reported : '+e);
        });
        worker.port.on('content-to-addon', function(value) {
          test.assertEqual(
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
exports.testMixedContext = function(test) {
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
          test.fail("Page mod contexts are mixed.");
          doneCallback();
        }
        else if (++messages == 2) {
          test.pass("Page mod contexts are different.");
          doneCallback();
        }
      });
    }
  };
  testPageMod(test, "data:text/html;charset=utf-8,", [modObject, modObject],
    function(win, done) {
      doneCallback = done;
    }
  );
};

exports.testHistory = function(test) {
  // We need a valid url in order to have a working History API.
  // (i.e do not work on data: or about: pages)
  // Test bug 679054.
  let url = require("sdk/self").data.url("test-page-mod.html");
  let callbackDone = null;
  testPageMod(test, url, [{
      include: url,
      contentScriptWhen: 'end',
      contentScript: 'new ' + function WorkerScope() {
        history.pushState({}, "", "#");
        history.replaceState({foo: "bar"}, "", "#");
        self.postMessage(history.state);
      },
      onAttach: function(worker) {
        worker.on('message', function (data) {
          test.assertEqual(JSON.stringify(data), JSON.stringify({foo: "bar"}),
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

exports.testRelatedTab = function(test) {
  test.waitUntilDone();

  let tab;
  let pageMod = new PageMod({
    include: "about:*",
    onAttach: function(worker) {
      test.assert(!!worker.tab, "Worker.tab exists");
      test.assertEqual(tab, worker.tab, "Worker.tab is valid");
      pageMod.destroy();
      tab.close(function() {
        test.done();
      });
    }
  });

  tabs.open({
    url: "about:",
    onOpen: function onOpen(t) {
      tab = t;
    }
  });
};

exports.testRelatedTabNoRequireTab = function(test) {
  test.waitUntilDone();

  let loader = Loader(module);
  let tab;
  let url = "data:text/html;charset=utf-8," + encodeURI("Test related worker tab 2");
  let { PageMod } = loader.require("sdk/page-mod");
  let pageMod = new PageMod({
    include: url,
    onAttach: function(worker) {
      test.assertEqual(worker.tab.url, url, "Worker.tab.url is valid");
      worker.tab.close(function() {
        pageMod.destroy();
        loader.unload();
        test.done();
      });
    }
  });

  tabs.open(url);
};

exports.testRelatedTabNoOtherReqs = function(test) {
  test.waitUntilDone();

  let loader = Loader(module);
  let { PageMod } = loader.require("sdk/page-mod");
  let pageMod = new PageMod({
    include: "about:blank?testRelatedTabNoOtherReqs",
    onAttach: function(worker) {
      test.assert(!!worker.tab, "Worker.tab exists");
      pageMod.destroy();
      worker.tab.close(function() {
        worker.destroy();
        loader.unload();
        test.done();
      });
    }
  });

  tabs.open({
    url: "about:blank?testRelatedTabNoOtherReqs"
  });
};

exports.testWorksWithExistingTabs = function(test) {
  test.waitUntilDone();

  let url = "data:text/html;charset=utf-8," + encodeURI("Test unique document");
  let { PageMod } = require("sdk/page-mod");
  tabs.open({
    url: url,
    onReady: function onReady(tab) {
      let pageModOnExisting = new PageMod({
        include: url,
        attachTo: ["existing", "top", "frame"],
        onAttach: function(worker) {
          test.assert(!!worker.tab, "Worker.tab exists");
          test.assertEqual(tab, worker.tab, "A worker has been created on this existing tab");

          timer.setTimeout(function() {
            pageModOnExisting.destroy();
            pageModOffExisting.destroy();
            tab.close(test.done.bind(test));
          }, 0);
        }
      });

      let pageModOffExisting = new PageMod({
        include: url,
        onAttach: function(worker) {
          test.fail("pageModOffExisting page-mod should not have attached to anything");
        }
      });
    }
  });
};

exports.testTabWorkerOnMessage = function(test) {
  test.waitUntilDone();

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
            test.assertEqual(this.tab.url, data.url, "location is correct");
            test.assertEqual(this.tab.title, data.title, "title is correct");
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
                  test.done();
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

exports.testAutomaticDestroy = function(test) {
  test.waitUntilDone();
  let loader = Loader(module);

  let pageMod = loader.require("sdk/page-mod").PageMod({
    include: "about:*",
    contentScriptWhen: "start",
    onAttach: function(w) {
      test.fail("Page-mod should have been detroyed during module unload");
    }
  });

  // Unload the page-mod module so that our page mod is destroyed
  loader.unload();

  // Then create a second tab to ensure that it is correctly destroyed
  let tabs = require("sdk/tabs");
  tabs.open({
    url: "about:",
    onReady: function onReady(tab) {
      test.pass("check automatic destroy");
      tab.close(test.done.bind(test));
    }
  });
}

exports.testAttachToTabsOnly = function(test) {
  test.waitUntilDone();

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
          test.pass('Succesfully applied to tab documents and its iframe');
          worker.destroy();
          mod.destroy();
          openedTab.close(function() {
            test.done();
          });
        }
      }
      else {
        test.fail('page-mod attached to a non-tab document');
      }
    }
  });

  function openHiddenFrame() {
    console.info('Open iframe in hidden window');
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
    console.info('Open toplevel window');
    let win = open('data:text/html;charset=utf-8,bar');
    win.addEventListener('DOMContentLoaded', function onload() {
      win.removeEventListener('DOMContentLoaded', onload, false);
      win.close();
      openBrowserIframe();
    }, false);
  }

  function openBrowserIframe() {
    console.info('Open iframe in browser window');
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
    console.info('Open iframes in a tab');
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

exports['test111 attachTo [top]'] = function(test) {
  test.waitUntilDone();

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
          test.assertEqual(href, topDocumentURL,
                           "worker on top level document only");
          let tab = worker.tab;
          worker.destroy();
          mod.destroy();
          tab.close(function() {
            test.done();
          });
        });
      }
      else {
        test.fail('page-mod attached to a non-top document');
      }
    }
  });

  require('sdk/tabs').open(topDocumentURL);
};

exports['test111 attachTo [frame]'] = function(test) {
  test.waitUntilDone();

  let { PageMod } = require('sdk/page-mod');

  let subFrameURL = 'data:text/html;charset=utf-8,subframe';
  let subContent = '<iframe src="' + subFrameURL + '" />';
  let frameURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(subContent);
  let content = '<iframe src="' + frameURL + '" />';
  let topDocumentURL = 'data:text/html;charset=utf-8,' + encodeURIComponent(content)

  let workerCount = 0, messageCount = 0;

  function onMessage(href) {
    if (href == frameURL)
      test.pass("worker on first frame");
    else if (href == subFrameURL)
      test.pass("worker on second frame");
    else
      test.fail("worker on unexpected document: " + href);
    this.destroy();
    if (++messageCount == 2) {
      mod.destroy();
      require('sdk/tabs').activeTab.close(function() {
        test.done();
      });
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
        test.fail('page-mod attached to a non-frame document');
      }
    }
  });

  require('sdk/tabs').open(topDocumentURL);
};

exports.testContentScriptOptionsOption = function(test) {
  test.waitUntilDone();

  let callbackDone = null;
  testPageMod(test, "about:", [{
      include: "about:*",
      contentScript: "self.postMessage( [typeof self.options.d, self.options] );",
      contentScriptWhen: "end",
      contentScriptOptions: {a: true, b: [1,2,3], c: "string", d: function(){ return 'test'}},
      onAttach: function(worker) {
        worker.on('message', function(msg) {
          test.assertEqual( msg[0], 'undefined', 'functions are stripped from contentScriptOptions' );
          test.assertEqual( typeof msg[1], 'object', 'object as contentScriptOptions' );
          test.assertEqual( msg[1].a, true, 'boolean in contentScriptOptions' );
          test.assertEqual( msg[1].b.join(), '1,2,3', 'array and numbers in contentScriptOptions' );
          test.assertEqual( msg[1].c, 'string', 'string in contentScriptOptions' );
          callbackDone();
        });
      }
    }],
    function(win, done) {
      callbackDone = done;
    }
  );
};

exports.testPageModCss = function(test) {
  let [pageMod] = testPageMod(test,
    'data:text/html;charset=utf-8,<div style="background: silver">css test</div>', [{
      include: ["*", "data:*"],
      contentStyle: "div { height: 100px; }",
      contentStyleFile:
        require("sdk/self").data.url("pagemod-css-include-file.css")
    }],
    function(win, done) {
      let div = win.document.querySelector("div");
      test.assertEqual(
        div.clientHeight,
        100,
        "PageMod contentStyle worked"
      );
      test.assertEqual(
       div.offsetHeight,
        120,
        "PageMod contentStyleFile worked"
      );
      done();
    }
  );
};

exports.testPageModCssList = function(test) {
  let [pageMod] = testPageMod(test,
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

      test.assertEqual(
       div.clientHeight,
        100,
        "PageMod contentStyle list works and is evaluated after contentStyleFile"
      );

      test.assertEqual(
        div.offsetHeight,
        120,
        "PageMod contentStyleFile list works"
      );

      test.assertEqual(
        style.width,
        "320px",
        "PageMod add-on author/page author style sheet precedence works"
      );

      test.assertEqual(
        style.maxWidth,
        "480px",
        "PageMod add-on author/page author style sheet precedence with !important works"
      );

      done();
    }
  );
};

exports.testPageModCssDestroy = function(test) {
  let [pageMod] = testPageMod(test,
    'data:text/html;charset=utf-8,<div style="width:200px">css test</div>', [{
      include: "data:*",
      contentStyle: "div { width: 100px!important; }"
    }],

    function(win, done) {
      let div = win.document.querySelector("div"),
          style = win.getComputedStyle(div);

      test.assertEqual(
        style.width,
        "100px",
        "PageMod contentStyle worked"
      );

      pageMod.destroy();
      test.assertEqual(
        style.width,
        "200px",
        "PageMod contentStyle is removed after destroy"
      );

      done();

    }
  );
};

exports.testPageModCssAutomaticDestroy = function(test) {
  test.waitUntilDone();
  let loader = Loader(module);

  let pageMod = loader.require("sdk/page-mod").PageMod({
    include: "data:*",
    contentStyle: "div { width: 100px!important; }"
  });

  tabs.open({
    url: "data:text/html;charset=utf-8,<div style='width:200px'>css test</div>",

    onReady: function onReady(tab) {
      let browserWindow = windowUtils.activeBrowserWindow;
      let win = getTabContentWindow(getActiveTab(browserWindow));

      let div = win.document.querySelector("div");
      let style = win.getComputedStyle(div);

      test.assertEqual(
        style.width,
        "100px",
        "PageMod contentStyle worked"
      );

      loader.unload();

      test.assertEqual(
        style.width,
        "200px",
        "PageMod contentStyle is removed after loader's unload"
      );

      tab.close(test.done.bind(test));
    }
  });
};


exports.testPageModTimeout = function(test) {
  test.waitUntilDone();
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
        test.pass("timer was scheduled")
        worker.port.on("fired", function(data) {
          test.assertEqual(id, data, "timer was fired")
          tab.close(function() {
            worker.destroy()
            loader.unload()
            test.done()
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


exports.testPageModcancelTimeout = function(test) {
  test.waitUntilDone();
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
        test.fail("cancelled timeout fired")
      })
      worker.port.on("timeout", function(id) {
        test.pass("timer was scheduled")
        tab.close(function() {
          worker.destroy();
          mod.destroy();
          loader.unload();
          test.done();
        });
      })
    }
  });

  tabs.open({
    url: "data:text/html;charset=utf-8,cancell timeout",
    onReady: function($) { tab = $ }
  })
}

exports.testExistingOnFrames = function(test) {
  test.waitUntilDone();

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

        test.assertNotEqual(url,
                            worker.url,
                            'worker should not be attached to the top window');

        if (++counter < 2) {
          // we can rely on this order in this case because we are sure that
          // the frames being tested have completely loaded
          test.assertEqual(iFrameURL, worker.url, '1st attach is for top frame');
        }
        else if (counter > 2) {
          test.fail('applied page mod too many times');
        }
        else {
          test.assertEqual(subFrameURL, worker.url, '2nd attach is for sub frame');
          // need timeout because onAttach is called before the constructor returns
          timer.setTimeout(function() {
            pagemodOnExisting.destroy();
            pagemodOffExisting.destroy();
            closeTab(tab);
            test.done();
          }, 0);
        }
      }
    });

    let pagemodOffExisting = PageMod({
      include: ["*", "data:*"],
      attachTo: ["frame"],
      contentScriptWhen: 'ready',
      onAttach: function(mod) {
        test.fail('pagemodOffExisting page-mod should not have been attached');
      }
    });
  }

  window.addEventListener("load", wait4Iframes, false);
};

exports.testIFramePostMessage = function(test) {
  test.waitUntilDone();
  let count = 0;

  tabs.open({
    url: data.url("test-iframe.html"),
    onReady: function(tab) {
      var worker = tab.attach({
        contentScriptFile: data.url('test-iframe.js'),
        contentScript: 'var iframePath = \'' + data.url('test-iframe-postmessage.html') + '\'',
        onMessage: function(msg) {
          test.assertEqual(++count, 1);
          test.assertEqual(msg.first, 'a string');
          test.assert(msg.second[1], "array");
          test.assertEqual(typeof msg.third, 'object');

          worker.destroy();
          tab.close(function() test.done());
        }
      });
    }
  });
};

exports.testEvents = function(test) {
  let content = "<script>\n new " + function DocumentScope() {
    window.addEventListener("ContentScriptEvent", function () {
      window.receivedEvent = true;
    }, false);
  } + "\n</script>";
  let url = "data:text/html;charset=utf-8," + encodeURIComponent(content);
  testPageMod(test, url, [{
      include: "data:*",
      contentScript: 'new ' + function WorkerScope() {
        let evt = document.createEvent("Event");
        evt.initEvent("ContentScriptEvent", true, true);
        document.body.dispatchEvent(evt);
      }
    }],
    function(win, done) {
      test.assert(
        win.receivedEvent,
        "Content script sent an event and document received it"
      );
      done();
    }
  );
};

exports["test page-mod on private tab"] = function (test) {
  test.waitUntilDone();
  let fail = test.fail.bind(test);

  let privateUri = "data:text/html;charset=utf-8," +
                   "<iframe src=\"data:text/html;charset=utf-8,frame\" />";
  let nonPrivateUri = "data:text/html;charset=utf-8,non-private";

  let pageMod = new PageMod({
    include: "data:*",
    onAttach: function(worker) {
      if (isTabPBSupported || isWindowPBSupported) {
        // When PB isn't supported, the page-mod will apply to all document
        // as all of them will be non-private
        test.assertEqual(worker.tab.url,
                         nonPrivateUri,
                         "page-mod should only attach to the non-private tab");
      }

      test.assert(!isPrivate(worker),
                  "The worker is really non-private");
      test.assert(!isPrivate(worker.tab),
                  "The document is really non-private");
      pageMod.destroy();

      page1.close().
        then(page2.close).
        then(test.done.bind(test), fail);
    }
  });

  let page1, page2;
  page1 = openWebpage(privateUri, true);
  page1.ready.then(function() {
    page2 = openWebpage(nonPrivateUri, false);
  }, fail);
}

exports["test page-mod on private tab in global pb"] = function (test) {
  test.waitUntilDone();
  if (!isGlobalPBSupported) {
    test.pass();
    return test.done();
  }

  let privateUri = "data:text/html;charset=utf-8," +
                   "<iframe%20src=\"data:text/html;charset=utf-8,frame\"/>";

  let pageMod = new PageMod({
    include: privateUri,
    onAttach: function(worker) {
      test.assertEqual(worker.tab.url,
                       privateUri,
                       "page-mod should attach");
      test.assertEqual(isPrivateBrowsingSupported,
                       false,
                       "private browsing is not supported");
      test.assert(isPrivate(worker),
                  "The worker is really non-private");
      test.assert(isPrivate(worker.tab),
                  "The document is really non-private");
      pageMod.destroy();

      worker.tab.close(function() {
        pb.once('stop', function() {
          test.pass('global pb stop');
          test.done();
        });
        pb.deactivate();
      });
    }
  });

  let page1;
  pb.once('start', function() {
    test.pass('global pb start');
    tabs.open({ url: privateUri });
  });
  pb.activate();
}

// Bug 699450: Calling worker.tab.close() should not lead to exception
exports.testWorkerTabClose = function(test) {
  let callbackDone;
  testPageMod(test, "about:", [{
      include: "about:",
      contentScript: '',
      onAttach: function(worker) {
        console.log("call close");
        worker.tab.close(function () {
          // On Fennec, tab is completely destroyed right after close event is
          // dispatch, so we need to wait for the next event loop cycle to
          // check for tab nulliness.
          timer.setTimeout(function () {
            test.assert(!worker.tab,
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
