/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Loader } = require('sdk/test/loader');
const Pages = require("sdk/page-worker");
const Page = Pages.Page;

const ERR_DESTROYED =
  "Couldn't find the worker to receive this message. " +
  "The script may not be initialized yet, or may already have been unloaded.";

exports.testSimplePageCreation = function(assert, done) {
  let page = new Page({
    contentScript: "self.postMessage(window.location.href)",
    contentScriptWhen: "end",
    onMessage: function (message) {
      assert.equal(message, "about:blank",
                       "Page Worker should start with a blank page by default");
      assert.equal(this, page, "The 'this' object is the page itself.");
      done();
    }
  });
}

/*
 * Tests that we can't be tricked by document overloads as we have access
 * to wrapped nodes
 */
exports.testWrappedDOM = function(assert, done) {
  let page = Page({
    allow: { script: true },
    contentURL: "data:text/html;charset=utf-8,<script>document.getElementById=3;window.scrollTo=3;</script>",
    contentScript: "window.addEventListener('load', function () " +
                   "self.postMessage([typeof(document.getElementById), " +
                   "typeof(window.scrollTo)]), true)",
    onMessage: function (message) {
      assert.equal(message[0],
                       "function",
                       "getElementById from content script is the native one");

      assert.equal(message[1],
                       "function",
                       "scrollTo from content script is the native one");

      done();
    }
  });
}

/*
// We do not offer unwrapped access to DOM since bug 601295 landed
// See 660780 to track progress of unwrap feature
exports.testUnwrappedDOM = function(assert, done) {
  let page = Page({
    allow: { script: true },
    contentURL: "data:text/html;charset=utf-8,<script>document.getElementById=3;window.scrollTo=3;</script>",
    contentScript: "window.addEventListener('load', function () " +
                   "self.postMessage([typeof(unsafeWindow.document.getElementById), " +
                   "typeof(unsafeWindow.scrollTo)]), true)",
    onMessage: function (message) {
      assert.equal(message[0],
                       "number",
                       "document inside page is free to be changed");

      assert.equal(message[1],
                       "number",
                       "window inside page is free to be changed");

      done();
    }
  });
}
*/

exports.testPageProperties = function(assert) {
  let page = new Page();

  for each (let prop in ['contentURL', 'allow', 'contentScriptFile',
                         'contentScript', 'contentScriptWhen', 'on',
                         'postMessage', 'removeListener']) {
    assert.ok(prop in page, prop + " property is defined on page.");
  }

  assert.ok(function () page.postMessage("foo") || true,
              "postMessage doesn't throw exception on page.");
}

exports.testConstructorAndDestructor = function(assert, done) {
  let loader = Loader(module);
  let Pages = loader.require("sdk/page-worker");
  let global = loader.sandbox("sdk/page-worker");

  let pagesReady = 0;

  let page1 = Pages.Page({
    contentScript:      "self.postMessage('')",
    contentScriptWhen:  "end",
    onMessage:          pageReady
  });
  let page2 = Pages.Page({
    contentScript:      "self.postMessage('')",
    contentScriptWhen:  "end",
    onMessage:          pageReady
  });

  assert.notEqual(page1, page2,
                      "Page 1 and page 2 should be different objects.");

  function pageReady() {
    if (++pagesReady == 2) {
      page1.destroy();
      page2.destroy();

      assert.ok(isDestroyed(page1), "page1 correctly unloaded.");
      assert.ok(isDestroyed(page2), "page2 correctly unloaded.");

      loader.unload();
      done();
    }
  }
}

exports.testAutoDestructor = function(assert, done) {
  let loader = Loader(module);
  let Pages = loader.require("sdk/page-worker");

  let page = Pages.Page({
    contentScript: "self.postMessage('')",
    contentScriptWhen: "end",
    onMessage: function() {
      loader.unload();
      assert.ok(isDestroyed(page), "Page correctly unloaded.");
      done();
    }
  });
}

exports.testValidateOptions = function(assert) {
  assert.throws(
    function () Page({ contentURL: 'home' }),
    /The `contentURL` option must be a valid URL\./,
    "Validation correctly denied a non-URL contentURL"
  );

  assert.throws(
    function () Page({ onMessage: "This is not a function."}),
    /The event listener must be a function\./,
    "Validation correctly denied a non-function onMessage."
  );

  assert.pass("Options validation is working.");
}

exports.testContentAndAllowGettersAndSetters = function(assert, done) {
  let content = "data:text/html;charset=utf-8,<script>window.localStorage.allowScript=3;</script>";
  let page = Page({
    contentURL: content,
    contentScript: "self.postMessage(window.localStorage.allowScript)",
    contentScriptWhen: "end",
    onMessage: step0
  });

  function step0(message) {
    assert.equal(message, "3",
                     "Correct value expected for allowScript - 3");
    assert.equal(page.contentURL, content,
                     "Correct content expected");
    page.removeListener('message', step0);
    page.on('message', step1);
    page.allow = { script: false };
    page.contentURL = content =
      "data:text/html;charset=utf-8,<script>window.localStorage.allowScript='f'</script>";
  }

  function step1(message) {
    assert.equal(message, "3",
                     "Correct value expected for allowScript - 3");
    assert.equal(page.contentURL, content, "Correct content expected");
    page.removeListener('message', step1);
    page.on('message', step2);
    page.allow = { script: true };
    page.contentURL = content =
      "data:text/html;charset=utf-8,<script>window.localStorage.allowScript='g'</script>";
  }

  function step2(message) {
    assert.equal(message, "g",
                     "Correct value expected for allowScript - g");
    assert.equal(page.contentURL, content, "Correct content expected");
    page.removeListener('message', step2);
    page.on('message', step3);
    page.allow.script = false;
    page.contentURL = content =
      "data:text/html;charset=utf-8,<script>window.localStorage.allowScript=3</script>";
  }

  function step3(message) {
    assert.equal(message, "g",
                     "Correct value expected for allowScript - g");
    assert.equal(page.contentURL, content, "Correct content expected");
    page.removeListener('message', step3);
    page.on('message', step4);
    page.allow.script = true;
    page.contentURL = content =
      "data:text/html;charset=utf-8,<script>window.localStorage.allowScript=4</script>";
  }

  function step4(message) {
    assert.equal(message, "4",
                     "Correct value expected for allowScript - 4");
    assert.equal(page.contentURL, content, "Correct content expected");
    done();
  }

}

exports.testOnMessageCallback = function(assert, done) {
  Page({
    contentScript: "self.postMessage('')",
    contentScriptWhen: "end",
    onMessage: function() {
      assert.pass("onMessage callback called");
      done();
    }
  });
}

exports.testMultipleOnMessageCallbacks = function(assert, done) {
  let count = 0;
  let page = Page({
    contentScript: "self.postMessage('')",
    contentScriptWhen: "end",
    onMessage: function() count += 1
  });
  page.on('message', function() count += 2);
  page.on('message', function() count *= 3);
  page.on('message', function()
    assert.equal(count, 9, "All callbacks were called, in order."));
  page.on('message', function() done());

}

exports.testLoadContentPage = function(assert, done) {
  let page = Page({
    onMessage: function(message) {
      // The message is an array whose first item is the test method to call
      // and the rest of whose items are arguments to pass it.
      let msg = message.shift();
      if (msg == "done")
        return done();
      assert[msg].apply(assert, message);
    },
    contentURL: require("sdk/self").data.url("test-page-worker.html"),
    contentScriptFile: require("sdk/self").data.url("test-page-worker.js"),
    contentScriptWhen: "ready"
  });
}

exports.testAllowScriptDefault = function(assert, done) {
  let page = Page({
    onMessage: function(message) {
      assert.ok(message, "Script is allowed to run by default.");
      done();
    },
    contentURL: "data:text/html;charset=utf-8,<script>document.documentElement.setAttribute('foo', 3);</script>",
    contentScript: "self.postMessage(document.documentElement.getAttribute('foo'))",
    contentScriptWhen: "ready"
  });
}

exports.testAllowScript = function(assert, done) {
  let page = Page({
    onMessage: function(message) {
      assert.ok(message, "Script runs when allowed to do so.");
      done();
    },
    allow: { script: true },
    contentURL: "data:text/html;charset=utf-8,<script>document.documentElement.setAttribute('foo', 3);</script>",
    contentScript: "self.postMessage(document.documentElement.hasAttribute('foo') && " +
                   "                 document.documentElement.getAttribute('foo') == 3)",
    contentScriptWhen: "ready"
  });
}

exports.testPingPong = function(assert, done) {
  let page = Page({
    contentURL: 'data:text/html;charset=utf-8,ping-pong',
    contentScript: 'self.on("message", function(message) self.postMessage("pong"));'
      + 'self.postMessage("ready");',
    onMessage: function(message) {
      if ('ready' == message) {
        page.postMessage('ping');
      }
      else {
        assert.ok(message, 'pong', 'Callback from contentScript');
        done();
      }
    }
  });
};

exports.testMultipleDestroys = function(assert) {
  let page = Page();
  page.destroy();
  page.destroy();
  assert.pass("Multiple destroys should not cause an error");
};

exports.testContentScriptOptionsOption = function(assert, done) {
  let page = new Page({
    contentScript: "self.postMessage( [typeof self.options.d, self.options] );",
    contentScriptWhen: "end",
    contentScriptOptions: {a: true, b: [1,2,3], c: "string", d: function(){ return 'test'}},
    onMessage: function(msg) {
      assert.equal(msg[0], 'undefined', 'functions are stripped from contentScriptOptions');
      assert.equal(typeof msg[1], 'object', 'object as contentScriptOptions');
      assert.equal(msg[1].a, true, 'boolean in contentScriptOptions');
      assert.equal(msg[1].b.join(), '1,2,3', 'array and numbers in contentScriptOptions');
      assert.equal(msg[1].c, 'string', 'string in contentScriptOptions');
      done();
    }
  });
};

exports.testMessageQueue = function (assert, done) {
  let page = new Page({
    contentScript: 'self.on("message", function (m) {' +
      'self.postMessage(m);' +
      '});',
    contentURL: 'data:text/html;charset=utf-8,',
  });
  page.postMessage('ping');
  page.on('message', function (m) {
    assert.equal(m, 'ping', 'postMessage should queue messages');
    done();
  });
};

function isDestroyed(page) {
  try {
    page.postMessage("foo");
  }
  catch (err if err.message == ERR_DESTROYED) {
    return true;
  }
  return false;
}

require("test").run(exports);
