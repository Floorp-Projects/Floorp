/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Skipping due to window creation being unsupported in Fennec
module.metadata = {
  engines: {
    'Firefox': '*'
  }
};

const { Cc, Ci } = require("chrome");
const { on } = require("sdk/event/core");
const { setTimeout } = require("sdk/timers");
const { LoaderWithHookedConsole } = require("sdk/test/loader");
const { Worker } = require("sdk/content/worker");
const { close } = require("sdk/window/helpers");
const { set: setPref } = require("sdk/preferences/service");
const { isArray } = require("sdk/lang/type");
const { URL } = require('sdk/url');
const fixtures = require("./fixtures");

const DEPRECATE_PREF = "devtools.errorconsole.deprecation_warnings";

const DEFAULT_CONTENT_URL = "data:text/html;charset=utf-8,foo";

const WINDOW_SCRIPT_URL = "data:text/html;charset=utf-8," +
                          "<script>window.addEventListener('message', function (e) {" +
                          "  if (e.data === 'from -> content-script')" +
                          "    window.postMessage('from -> window', '*');" +
                          "});</script>";

function makeWindow() {
  let content =
    "<?xml version=\"1.0\"?>" +
    "<window " +
    "xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\">" +
    "<script>var documentValue=true;</script>" +
    "</window>";
  var url = "data:application/vnd.mozilla.xul+xml;charset=utf-8," +
            encodeURIComponent(content);
  var features = ["chrome", "width=10", "height=10"];

  return Cc["@mozilla.org/embedcomp/window-watcher;1"].
         getService(Ci.nsIWindowWatcher).
         openWindow(null, url, null, features.join(","), null);
}

// Listen for only first one occurence of DOM event
function listenOnce(node, eventName, callback) {
  node.addEventListener(eventName, function onevent(event) {
    node.removeEventListener(eventName, onevent, true);
    callback(node);
  }, true);
}

// Load a given url in a given browser and fires the callback when it is loaded
function loadAndWait(browser, url, callback) {
  listenOnce(browser, "load", callback);
  // We have to wait before calling `loadURI` otherwise, if we call
  // `loadAndWait` during browser load event, the history will be broken
  setTimeout(function () {
    browser.loadURI(url);
  }, 0);
}

// Returns a test function that will automatically open a new chrome window
// with a <browser> element loaded on a given content URL
// The callback receive 3 arguments:
// - test: reference to the jetpack test object
// - browser: a reference to the <browser> xul node
// - done: a callback to call when test is over
function WorkerTest(url, callback) {
  return function testFunction(assert, done) {
    let chromeWindow = makeWindow();
    chromeWindow.addEventListener("load", function onload() {
      chromeWindow.removeEventListener("load", onload, true);
      let browser = chromeWindow.document.createElement("browser");
      browser.setAttribute("type", "content");
      chromeWindow.document.documentElement.appendChild(browser);
      // Wait for about:blank load event ...
      listenOnce(browser, "load", function onAboutBlankLoad() {
        // ... before loading the expected doc and waiting for its load event
        loadAndWait(browser, url, function onDocumentLoaded() {
          callback(assert, browser, function onTestDone() {

            close(chromeWindow).then(done);
          });
        });
      });
    }, true);
  };
}

exports["test:sample"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    assert.notEqual(browser.contentWindow.location.href, "about:blank",
                        "window is now on the right document");

    let window = browser.contentWindow
    let worker =  Worker({
      window: window,
      contentScript: "new " + function WorkerScope() {
        // window is accessible
        let myLocation = window.location.toString();
        self.on("message", function(data) {
          if (data == "hi!")
            self.postMessage("bye!");
        });
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        assert.equal("bye!", msg);
        assert.equal(worker.url, window.location.href,
                         "worker.url still works");
        done();
      }
    });

    assert.equal(worker.url, window.location.href,
                     "worker.url works");
    assert.equal(worker.contentURL, window.location.href,
                     "worker.contentURL works");
    worker.postMessage("hi!");
  }
);

exports["test:emit"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          // Validate self.on and self.emit
          self.port.on("addon-to-content", function (data) {
            self.port.emit("content-to-addon", data);
          });

          // Check for global pollution
          //if (typeof on != "undefined")
          //  self.postMessage("`on` is in globals");
          if (typeof once != "undefined")
            self.postMessage("`once` is in globals");
          if (typeof emit != "undefined")
            self.postMessage("`emit` is in globals");

        },
        onMessage: function(msg) {
          assert.fail("Got an unexpected message : "+msg);
        }
      });

    // Validate worker.port
    worker.port.on("content-to-addon", function (data) {
      assert.equal(data, "event data");
      done();
    });
    worker.port.emit("addon-to-content", "event data");
  }
);

exports["test:emit hack message"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          // Validate self.port
          self.port.on("message", function (data) {
            self.port.emit("message", data);
          });
          // We should not receive message on self, but only on self.port
          self.on("message", function (data) {
            self.postMessage("message", data);
          });
        },
        onError: function(e) {
          assert.fail("Got exception: "+e);
        }
      });

    worker.port.on("message", function (data) {
      assert.equal(data, "event data");
      done();
    });
    worker.on("message", function (data) {
      assert.fail("Got an unexpected message : "+msg);
    });
    worker.port.emit("message", "event data");
  }
);

exports["test:n-arguments emit"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let repeat = 0;
    let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          // Validate self.on and self.emit
          self.port.on("addon-to-content", function (a1, a2, a3) {
            self.port.emit("content-to-addon", a1, a2, a3);
          });
        }
      });

    // Validate worker.port
    worker.port.on("content-to-addon", function (arg1, arg2, arg3) {
      if (!repeat++) {
        this.emit("addon-to-content", "first argument", "second", "third");
      } else {
        assert.equal(arg1, "first argument");
        assert.equal(arg2, "second");
        assert.equal(arg3, "third");
        done();
      }
    });
    worker.port.emit("addon-to-content", "first argument", "second", "third");
  }
);

exports["test:post-json-values-only"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          self.on("message", function (message) {
            self.postMessage([ message.fun === undefined,
                               typeof message.w,
                               message.w && "port" in message.w,
                               message.w._url,
                               Array.isArray(message.array),
                               JSON.stringify(message.array)]);
          });
        }
      });

    // Validate worker.onMessage
    let array = [1, 2, 3];
    worker.on("message", function (message) {
      assert.ok(message[0], "function becomes undefined");
      assert.equal(message[1], "object", "object stays object");
      assert.ok(message[2], "object's attributes are enumerable");
      assert.equal(message[3], DEFAULT_CONTENT_URL,
                       "jsonable attributes are accessible");
      // See bug 714891, Arrays may be broken over compartements:
      assert.ok(message[4], "Array keeps being an array");
      assert.equal(message[5], JSON.stringify(array),
                       "Array is correctly serialized");
      done();
    });
    // Add a new url property sa the Class function used by
    // Worker doesn't set enumerables to true for non-functions
    worker._url = DEFAULT_CONTENT_URL;

    worker.postMessage({ fun: function () {}, w: worker, array: array });
  }
);

exports["test:emit-json-values-only"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          // Validate self.on and self.emit
          self.port.on("addon-to-content", function (fun, w, obj, array) {
            self.port.emit("content-to-addon", [
                            fun === null,
                            typeof w,
                            "port" in w,
                            w._url,
                            "fun" in obj,
                            Object.keys(obj.dom).length,
                            Array.isArray(array),
                            JSON.stringify(array)
                          ]);
          });
        }
      });

    // Validate worker.port
    let array = [1, 2, 3];
    worker.port.on("content-to-addon", function (result) {
      assert.ok(result[0], "functions become null");
      assert.equal(result[1], "object", "objects stay objects");
      assert.ok(result[2], "object's attributes are enumerable");
      assert.equal(result[3], DEFAULT_CONTENT_URL,
                       "json attribute is accessible");
      assert.ok(!result[4], "function as object attribute is removed");
      assert.equal(result[5], 0, "DOM nodes are converted into empty object");
      // See bug 714891, Arrays may be broken over compartments:
      assert.ok(result[6], "Array keeps being an array");
      assert.equal(result[7], JSON.stringify(array),
                       "Array is correctly serialized");
      done();
    });

    let obj = {
      fun: function () {},
      dom: browser.contentWindow.document.createElement("div")
    };
    // Add a new url property sa the Class function used by
    // Worker doesn't set enumerables to true for non-functions
    worker._url = DEFAULT_CONTENT_URL;
    worker.port.emit("addon-to-content", function () {}, worker, obj, array);
  }
);

exports["test:content is wrapped"] = WorkerTest(
  "data:text/html;charset=utf-8,<script>var documentValue=true;</script>",
  function(assert, browser, done) {

    let worker =  Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        self.postMessage(!window.documentValue);
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        assert.ok(msg,
          "content script has a wrapped access to content document");
        done();
      }
    });
  }
);

exports["test:chrome is unwrapped"] = function(assert, done) {
  let window = makeWindow();

  listenOnce(window, "load", function onload() {

    let worker =  Worker({
      window: window,
      contentScript: "new " + function WorkerScope() {
        self.postMessage(window.documentValue);
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        assert.ok(msg,
          "content script has an unwrapped access to chrome document");
        close(window).then(done);
      }
    });

  });
}

exports["test:nothing is leaked to content script"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let worker =  Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        self.postMessage([
          "ContentWorker" in window,
          "UNWRAP_ACCESS_KEY" in window,
          "getProxyForObject" in window
        ]);
      },
      contentScriptWhen: "ready",
      onMessage: function(list) {
        assert.ok(!list[0], "worker API contrustor isn't leaked");
        assert.ok(!list[1], "Proxy API stuff isn't leaked 1/2");
        assert.ok(!list[2], "Proxy API stuff isn't leaked 2/2");
        done();
      }
    });
  }
);

exports["test:ensure console.xxx works in cs"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let { loader } = LoaderWithHookedConsole(module, onMessage);

    // Intercept all console method calls
    let calls = [];
    function onMessage(type, msg) {
      assert.equal(type, msg,
        "console.xxx(\"xxx\"), i.e. message is equal to the " +
        "console method name we are calling");
      calls.push(msg);
    }

    // Finally, create a worker that will call all console methods
    let worker =  loader.require("sdk/content/worker").Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        console.time("time");
        console.log("log");
        console.info("info");
        console.warn("warn");
        console.error("error");
        console.debug("debug");
        console.exception("exception");
        console.timeEnd("timeEnd");
        self.postMessage();
      },
      onMessage: function() {
        // Ensure that console methods are called in the same execution order
        const EXPECTED_CALLS = ["time", "log", "info", "warn", "error",
          "debug", "exception", "timeEnd"];
        assert.equal(JSON.stringify(calls),
          JSON.stringify(EXPECTED_CALLS),
          "console methods have been called successfully, in expected order");
        done();
      }
    });
  }
);

exports["test:setTimeout works with string argument"] = WorkerTest(
  "data:text/html;charset=utf-8,<script>var docVal=5;</script>",
  function(assert, browser, done) {
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "new " + function ContentScriptScope() {
        // must use "window.scVal" instead of "var csVal"
        // since we are inside ContentScriptScope function.
        // i'm NOT putting code-in-string inside code-in-string </YO DAWG>
        window.csVal = 13;
        setTimeout("self.postMessage([" +
                      "csVal, " +
                      "window.docVal, " +
                      "'ContentWorker' in window, " +
                      "'UNWRAP_ACCESS_KEY' in window, " +
                      "'getProxyForObject' in window, " +
                    "])", 1);
      },
      contentScriptWhen: "ready",
      onMessage: function([csVal, docVal, chrome1, chrome2, chrome3]) {
        // test timer code is executed in the correct context
        assert.equal(csVal, 13, "accessing content-script values");
        assert.notEqual(docVal, 5, "can't access document values (directly)");
        assert.ok(!chrome1 && !chrome2 && !chrome3, "nothing is leaked from chrome");
        done();
      }
    });
  }
);

exports["test:setInterval works with string argument"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let count = 0;
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "setInterval('self.postMessage(1)', 50)",
      contentScriptWhen: "ready",
      onMessage: function(one) {
        count++;
        assert.equal(one, 1, "got " + count + " message(s) from setInterval");
        if (count >= 3) done();
      }
    });
  }
);

exports["test:setInterval async Errors passed to .onError"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let count = 0;
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "setInterval(() => { throw Error('ubik') }, 50)",
      contentScriptWhen: "ready",
      onError: function(err) {
        count++;
        assert.equal(err.message, "ubik",
            "error (corectly) propagated  " + count + " time(s)");
        if (count >= 3) done();
      }
    });
  }
);

exports["test:setTimeout throws array, passed to .onError"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "setTimeout(function() { throw ['array', 42] }, 1)",
      contentScriptWhen: "ready",
      onError: function(arr) {
        assert.ok(isArray(arr),
            "the type of thrown/propagated object is array");
        assert.ok(arr.length==2,
            "the propagated thrown array is the right length");
        assert.equal(arr[1], 42,
            "element inside the thrown array correctly propagated");
        done();
      }
    });
  }
);

exports["test:setTimeout string arg with SyntaxError to .onError"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "setTimeout('syntax 123 error', 1)",
      contentScriptWhen: "ready",
      onError: function(err) {
        assert.equal(err.name, "SyntaxError",
            "received SyntaxError thrown from bad code in string argument to setTimeout");
        assert.ok('fileName' in err,
            "propagated SyntaxError contains a fileName property");
        assert.ok('stack' in err,
            "propagated SyntaxError contains a stack property");
        assert.equal(err.message, "missing ; before statement",
            "propagated SyntaxError has the correct (helpful) message");
        assert.equal(err.lineNumber, 1,
            "propagated SyntaxError was thrown on the right lineNumber");
        done();
      }
    });
  }
);

exports["test:setTimeout can't be cancelled by content"] = WorkerTest(
  "data:text/html;charset=utf-8,<script>var documentValue=true;</script>",
  function(assert, browser, done) {

    let worker =  Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        let id = setTimeout(function () {
          self.postMessage("timeout");
        }, 100);
        unsafeWindow.eval("clearTimeout("+id+");");
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        assert.ok(msg,
          "content didn't managed to cancel our setTimeout");
        done();
      }
    });
  }
);

exports["test:clearTimeout"] = WorkerTest(
  "data:text/html;charset=utf-8,clear timeout",
  function(assert, browser, done) {
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        let id1 = setTimeout(function() {
          self.postMessage("failed");
        }, 10);
        let id2 = setTimeout(function() {
          self.postMessage("done");
        }, 100);
        clearTimeout(id1);
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        if (msg === "failed") {
          assert.fail("failed to cancel timer");
        } else {
          assert.pass("timer cancelled");
          done();
        }
      }
    });
  }
);

exports["test:clearInterval"] = WorkerTest(
  "data:text/html;charset=utf-8,clear timeout",
  function(assert, browser, done) {
    let called = 0;
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        let id = setInterval(function() {
          self.postMessage("intreval")
          clearInterval(id)
          setTimeout(function() {
            self.postMessage("done")
          }, 100)
        }, 10);
      },
      contentScriptWhen: "ready",
      onMessage: function(msg) {
        if (msg === "intreval") {
          called = called + 1;
          if (called > 1) assert.fail("failed to cancel timer");
        } else {
          assert.pass("interval cancelled");
          done();
        }
      }
    });
  }
)

exports["test:setTimeout are unregistered on content unload"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let originalWindow = browser.contentWindow;
    let worker =  Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        document.title = "ok";
        let i = 0;
        setInterval(function () {
          document.title = i++;
        }, 10);
      },
      contentScriptWhen: "ready"
    });

    // Change location so that content script is destroyed,
    // and all setTimeout/setInterval should be unregistered.
    // Wait some cycles in order to execute some intervals.
    setTimeout(function () {
      // Bug 689621: Wait for the new document load so that we are sure that
      // previous document cancelled its intervals
      let url2 = "data:text/html;charset=utf-8,<title>final</title>";
      loadAndWait(browser, url2, function onload() {
        let titleAfterLoad = originalWindow.document.title;
        // Wait additional cycles to verify that intervals are really cancelled
        setTimeout(function () {
          assert.equal(browser.contentDocument.title, "final",
                           "New document has not been modified");
          assert.equal(originalWindow.document.title, titleAfterLoad,
                           "Nor previous one");

          done();
        }, 100);
      });
    }, 100);
  }
);

exports['test:check window attribute in iframes'] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    // Create a first iframe and wait for its loading
    let contentWin = browser.contentWindow;
    let contentDoc = contentWin.document;
    let iframe = contentDoc.createElement("iframe");
    contentDoc.body.appendChild(iframe);

    listenOnce(iframe, "load", function onload() {

      // Create a second iframe inside the first one and wait for its loading
      let iframeDoc = iframe.contentWindow.document;
      let subIframe = iframeDoc.createElement("iframe");
      iframeDoc.body.appendChild(subIframe);

      listenOnce(subIframe, "load", function onload() {
        subIframe.removeEventListener("load", onload, true);

        // And finally create a worker against this second iframe
        let worker =  Worker({
          window: subIframe.contentWindow,
          contentScript: 'new ' + function WorkerScope() {
            self.postMessage([
              window.top !== window,
              frameElement,
              window.parent !== window,
              top.location.href,
              parent.location.href,
            ]);
          },
          onMessage: function(msg) {
            assert.ok(msg[0], "window.top != window");
            assert.ok(msg[1], "window.frameElement is defined");
            assert.ok(msg[2], "window.parent != window");
            assert.equal(msg[3], contentWin.location.href,
                             "top.location refers to the toplevel content doc");
            assert.equal(msg[4], iframe.contentWindow.location.href,
                             "parent.location refers to the first iframe doc");
            done();
          }
        });

      });
      subIframe.setAttribute("src", "data:text/html;charset=utf-8,bar");

    });
    iframe.setAttribute("src", "data:text/html;charset=utf-8,foo");
  }
);

exports['test:check window attribute in toplevel documents'] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let worker =  Worker({
      window: browser.contentWindow,
      contentScript: 'new ' + function WorkerScope() {
        self.postMessage([
          window.top === window,
          frameElement,
          window.parent === window
        ]);
      },
      onMessage: function(msg) {
        assert.ok(msg[0], "window.top == window");
        assert.ok(!msg[1], "window.frameElement is null");
        assert.ok(msg[2], "window.parent == window");
        done();
      }
    });
  }
);

exports["test:check worker API with page history"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let url2 = "data:text/html;charset=utf-8,bar";

    loadAndWait(browser, url2, function () {
      let worker =  Worker({
        window: browser.contentWindow,
        contentScript: "new " + function WorkerScope() {
          // Just before the content script is disable, we register a timeout
          // that will be disable until the page gets visible again
          self.on("pagehide", function () {
            setTimeout(function () {
              self.postMessage("timeout restored");
            }, 0);
          });
        },
        contentScriptWhen: "start"
      });

      // postMessage works correctly when the page is visible
      worker.postMessage("ok");

      // We have to wait before going back into history,
      // otherwise `goBack` won't do anything.
      setTimeout(function () {
        browser.goBack();
      }, 0);

      // Wait for the document to be hidden
      browser.addEventListener("pagehide", function onpagehide() {
        browser.removeEventListener("pagehide", onpagehide, false);
        // Now any event sent to this worker should throw

        assert.throws(
            function () { worker.postMessage("data"); },
            /The page is currently hidden and can no longer be used/,
            "postMessage should throw when the page is hidden in history"
            );

        assert.throws(
            function () { worker.port.emit("event"); },
            /The page is currently hidden and can no longer be used/,
            "port.emit should throw when the page is hidden in history"
            );

        // Display the page with attached content script back in order to resume
        // its timeout and receive the expected message.
        // We have to delay this in order to not break the history.
        // We delay for a non-zero amount of time in order to ensure that we
        // do not receive the message immediatly, so that the timeout is
        // actually disabled
        setTimeout(function () {
          worker.on("message", function (data) {
            assert.ok(data, "timeout restored");
            done();
          });
          browser.goForward();
        }, 500);

      }, false);
    });

  }
);

exports['test:conentScriptFile as URL instance'] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {

    let url = new URL(fixtures.url("test-contentScriptFile.js"));
    let worker =  Worker({
      window: browser.contentWindow,
      contentScriptFile: url,
      onMessage: function(msg) {
        assert.equal(msg, "msg from contentScriptFile",
            "received a wrong message from contentScriptFile");
        done();
      }
    });
  }
);

exports["test:worker events"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function (assert, browser, done) {
    let window = browser.contentWindow;
    let events = [];
    let worker = Worker({
      window: window,
      contentScript: 'new ' + function WorkerScope() {
        self.postMessage('start');
      },
      onAttach: win => {
        events.push('attach');
        assert.pass('attach event called when attached');
        assert.equal(window, win, 'attach event passes in attached window');
      },
      onError: err => {
        assert.equal(err.message, 'Custom',
          'Error passed into error event');
        worker.detach();
      },
      onMessage: msg => {
        assert.pass('`onMessage` handles postMessage')
        throw new Error('Custom');
      },
      onDetach: _ => {
        assert.pass('`onDetach` called when worker detached');
        done();
      }
    });
    // `attach` event is called synchronously during instantiation,
    // so we can't listen to that, TODO FIX?
    //  worker.on('attach', obj => console.log('attach', obj));
  }
);

exports["test:onDetach in contentScript on destroy"] = WorkerTest(
  "data:text/html;charset=utf-8,foo#detach",
  function(assert, browser, done) {
    let worker = Worker({
      window: browser.contentWindow,
      contentScript: 'new ' + function WorkerScope() {
        self.port.on('detach', function(reason) {
          window.location.hash += '!' + reason;
        })
      },
    });
    browser.contentWindow.addEventListener('hashchange', _ => {
      assert.equal(browser.contentWindow.location.hash, '#detach!',
                   "location.href is as expected");
      done();
    })
    worker.destroy();
  }
);

exports["test:onDetach in contentScript on unload"] = WorkerTest(
  "data:text/html;charset=utf-8,foo#detach",
  function(assert, browser, done) {
    let { loader } = LoaderWithHookedConsole(module);
    let worker = loader.require("sdk/content/worker").Worker({
      window: browser.contentWindow,
      contentScript: 'new ' + function WorkerScope() {
        self.port.on('detach', function(reason) {
          window.location.hash += '!' + reason;
        })
      },
    });
    browser.contentWindow.addEventListener('hashchange', _ => {
      assert.equal(browser.contentWindow.location.hash, '#detach!shutdown',
                   "location.href is as expected");
      done();
    })
    loader.unload('shutdown');
  }
);

exports["test:console method log functions properly"] = WorkerTest(
  DEFAULT_CONTENT_URL,
  function(assert, browser, done) {
    let logs = [];

    let clean = message =>
          message.trim().
          replace(/[\r\n]/g, " ").
          replace(/ +/g, " ");

    let onMessage = (type, message) => logs.push(clean(message));
    let { loader } = LoaderWithHookedConsole(module, onMessage);

    let worker =  loader.require("sdk/content/worker").Worker({
      window: browser.contentWindow,
      contentScript: "new " + function WorkerScope() {
        console.log(Function);
        console.log((foo) => foo * foo);
        console.log(function foo(bar) { return bar + bar });

        self.postMessage();
      },
      onMessage: () => {
        assert.deepEqual(logs, [
          "function Function() { [native code] }",
          "(foo) => foo * foo",
          "function foo(bar) { \"use strict\"; return bar + bar }"
        ]);

        done();
      }
    });
  }
);

exports["test:global postMessage"] = WorkerTest(
  WINDOW_SCRIPT_URL,
  function(assert, browser, done) {
    let contentScript = "window.addEventListener('message', function (e) {" +
                        "  if (e.data === 'from -> window')" +
                        "    self.port.emit('response', e.data, e.origin);" +
                        "});" +
                        "postMessage('from -> content-script', '*');";
    let { loader } = LoaderWithHookedConsole(module);
    let worker =  loader.require("sdk/content/worker").Worker({
      window: browser.contentWindow,
      contentScriptWhen: "ready",
      contentScript: contentScript
    });

    worker.port.on("response", (data, origin) => {
      assert.equal(data, "from -> window", "Communication from content-script to window completed");
      done();
    });
});

require("test").run(exports);
