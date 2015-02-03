/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const hiddenFrames = require("sdk/frame/hidden-frame");
const { create: makeFrame } = require("sdk/frame/utils");
const { window } = require("sdk/addon/window");
const { Loader } = require('sdk/test/loader');
const { URL } = require("sdk/url");
const testURI = require("./fixtures").url("test.html");
const testHost = URL(testURI).scheme + '://' + URL(testURI).host;

/*
 * Utility function that allow to easily run a proxy test with a clean
 * new HTML document. See first unit test for usage.
 */
function createProxyTest(html, callback) {
  return function (assert, done) {
    let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
    let principalLoaded = false;

    let element = makeFrame(window.document, {
      nodeName: "iframe",
      type: "content",
      allowJavascript: true,
      allowPlugins: true,
      allowAuth: true,
      uri: testURI
    });

    element.addEventListener("DOMContentLoaded", onDOMReady, false);

    function onDOMReady() {
      // Reload frame after getting principal from `testURI`
      if (!principalLoaded) {
        element.setAttribute("src", url);
        principalLoaded = true;
        return;
      }

      assert.equal(element.getAttribute("src"), url, "correct URL loaded");
      element.removeEventListener("DOMContentLoaded", onDOMReady,
                                                  false);
      let xrayWindow = element.contentWindow;
      let rawWindow = xrayWindow.wrappedJSObject;

      let isDone = false;
      let helper = {
        xrayWindow: xrayWindow,
        rawWindow: rawWindow,
        createWorker: function (contentScript) {
          return createWorker(assert, xrayWindow, contentScript, helper.done);
        },
        done: function () {
          if (isDone)
            return;
          isDone = true;
          element.parentNode.removeChild(element);
          done();
        }
      };
      callback(helper, assert);
    }
  };
}

function createWorker(assert, xrayWindow, contentScript, done) {
  let loader = Loader(module);
  let Worker = loader.require("sdk/content/worker").Worker;
  let worker = Worker({
    window: xrayWindow,
    contentScript: [
      'new ' + function () {
        assert = function assert(v, msg) {
          self.port.emit("assert", {assertion:v, msg:msg});
        }
        done = function done() {
          self.port.emit("done");
        }
      },
      contentScript
    ]
  });

  worker.port.on("done", done);
  worker.port.on("assert", function (data) {
    assert.ok(data.assertion, data.msg);
  });

  return worker;
}

/* Examples for the `createProxyTest` uses */

let html = "<script>var documentGlobal = true</script>";

exports["test Create Proxy Test"] = createProxyTest(html, function (helper, assert) {
  // You can get access to regular `test` object in second argument of
  // `createProxyTest` method:
  assert.ok(helper.rawWindow.documentGlobal,
              "You have access to a raw window reference via `helper.rawWindow`");
  assert.ok(!("documentGlobal" in helper.xrayWindow),
              "You have access to an XrayWrapper reference via `helper.xrayWindow`");

  // If you do not create a Worker, you have to call helper.done(),
  // in order to say when your test is finished
  helper.done();
});

exports["test Create Proxy Test With Worker"] = createProxyTest("", function (helper) {

  helper.createWorker(
    "new " + function WorkerScope() {
      assert(true, "You can do assertions in your content script");
      // And if you create a worker, you either have to call `done`
      // from content script or helper.done()
      done();
    }
  );

});

exports["test Create Proxy Test With Events"] = createProxyTest("", function (helper, assert) {

  let worker = helper.createWorker(
    "new " + function WorkerScope() {
      self.port.emit("foo");
    }
  );

  worker.port.on("foo", function () {
    assert.pass("You can use events");
    // And terminate your test with helper.done:
    helper.done();
  });

});

/* Disabled due to bug 1038432
// Bug 714778: There was some issue around `toString` functions
//             that ended up being shared between content scripts
exports["test Shared To String Proxies"] = createProxyTest("", function(helper) {

  let worker = helper.createWorker(
    'new ' + function ContentScriptScope() {
      // We ensure that `toString` can't be modified so that nothing could
      // leak to/from the document and between content scripts
      // It only applies to JS proxies, there isn't any such issue with xrays.
      //document.location.toString = function foo() {};
      document.location.toString.foo = "bar";
      assert("foo" in document.location.toString, "document.location.toString can be modified");
      assert(document.location.toString() == "data:text/html;charset=utf-8,",
             "First document.location.toString()");
      self.postMessage("next");
    }
  );
  worker.on("message", function () {
    helper.createWorker(
      'new ' + function ContentScriptScope2() {
        assert(!("foo" in document.location.toString),
               "document.location.toString is different for each content script");
        assert(document.location.toString() == "data:text/html;charset=utf-8,",
               "Second document.location.toString()");
        done();
      }
    );
  });
});
*/

// Ensure that postMessage is working correctly across documents with an iframe
let html = '<iframe id="iframe" name="test" src="data:text/html;charset=utf-8," />';
exports["test postMessage"] = createProxyTest(html, function (helper, assert) {
  let ifWindow = helper.xrayWindow.document.getElementById("iframe").contentWindow;
  // Listen without proxies, to check that it will work in regular case
  // simulate listening from a web document.
  ifWindow.addEventListener("message", function listener(event) {
    ifWindow.removeEventListener("message", listener, false);
    // As we are in system principal, event is an XrayWrapper
    // xrays use current compartments when calling postMessage method.
    // Whereas js proxies was using postMessage method compartment,
    // not the caller one.
    assert.strictEqual(event.source, helper.xrayWindow,
                      "event.source is the top window");
    assert.equal(event.origin, testHost, "origin matches testHost");

    assert.equal(event.data, "{\"foo\":\"bar\\n \\\"escaped\\\".\"}",
                     "message data is correct");

    helper.done();
  }, false);

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      var json = JSON.stringify({foo : "bar\n \"escaped\"."});

      document.getElementById("iframe").contentWindow.postMessage(json, "*");
    }
  );
});

let html = '<input id="input2" type="checkbox" />';
exports["test Object Listener"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Test objects being given as event listener
      let input = document.getElementById("input2");
      let myClickListener = {
        called: false,
        handleEvent: function(event) {
          assert(this === myClickListener, "`this` is the original object");
          assert(!this.called, "called only once");
          this.called = true;
          assert(event.target, input, "event.target is the wrapped window");
          done();
        }
      };

      window.addEventListener("click", myClickListener, true);
      input.click();
      window.removeEventListener("click", myClickListener, true);
    }
  );

});

exports["test Object Listener 2"] = createProxyTest("", function (helper) {

  helper.createWorker(
    ('new ' + function ContentScriptScope() {
      // variable replaced with `testHost`
      let testHost = "TOKEN";
      // Verify object as DOM event listener
      let myMessageListener = {
        called: false,
        handleEvent: function(event) {
          window.removeEventListener("message", myMessageListener, true);

          assert(this == myMessageListener, "`this` is the original object");
          assert(!this.called, "called only once");
          this.called = true;
          assert(event.target == document.defaultView, "event.target is the wrapped window");
          assert(event.source == document.defaultView, "event.source is the wrapped window");
          assert(event.origin == testHost, "origin matches testHost");
          assert(event.data == "ok", "message data is correct");
          done();
        }
      };

      window.addEventListener("message", myMessageListener, true);
      document.defaultView.postMessage("ok", '*');
    }
  ).replace("TOKEN", testHost));

});

let html = '<input id="input" type="text" /><input id="input3" type="checkbox" />' +
             '<input id="input2" type="checkbox" />';

exports.testStringOverload = createProxyTest(html, function (helper, assert) {
  // Proxy - toString error
  let originalString = "string";
  let p = Proxy.create({
    get: function(receiver, name) {
      if (name == "binded")
        return originalString.toString.bind(originalString);
      return originalString[name];
    }
  });
  assert.throws(function () {
    p.toString();
  },
  /toString method called on incompatible Proxy/,
  "toString can't be called with this being the proxy");
  assert.equal(p.binded(), "string", "but it works if we bind this to the original string");

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // RightJS is hacking around String.prototype, and do similar thing:
      // Pass `this` from a String prototype method.
      // It is funny because typeof this == "object"!
      // So that when we pass `this` to a native method,
      // our proxy code can fail on another even more crazy thing.
      // See following test to see what fails around proxies.
      String.prototype.update = function () {
        assert(typeof this == "object", "in update, `this` is an object");
        assert(this.toString() == "input", "in update, `this.toString works");
        return document.querySelectorAll(this);
      };
      assert("input".update().length == 3, "String.prototype overload works");
      done();
    }
  );
});

exports["test MozMatchedSelector"] = createProxyTest("", function (helper) {
  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Check mozMatchesSelector XrayWrappers bug:
      // mozMatchesSelector returns bad results when we are not calling it from the node itself
      // SEE BUG 658909: mozMatchesSelector returns incorrect results with XrayWrappers
      assert(document.createElement( "div" ).mozMatchesSelector("div"),
             "mozMatchesSelector works while being called from the node");
      assert(document.documentElement.mozMatchesSelector.call(
               document.createElement( "div" ),
               "div"
             ),
             "mozMatchesSelector works while being called from a " +
             "function reference to " +
             "document.documentElement.mozMatchesSelector.call");
      done();
    }
  );
});

exports["test Events Overload"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // If we add a "____proxy" attribute on XrayWrappers in order to store
      // the related proxy to create an unique proxy for each wrapper;
      // we end up setting this attribute to prototype objects :x
      // And so, instances created with such prototype will be considered
      // as equal to the prototype ...
      //   // Internal method that return the proxy for a given XrayWrapper
      //   function proxify(obj) {
      //     if (obj._proxy) return obj._proxy;
      //     return obj._proxy = Proxy.create(...);
      //   }
      //
      //   // Get a proxy of an XrayWrapper prototype object
      //   let proto = proxify(xpcProto);
      //
      //   // Use this proxy as a prototype
      //   function Constr() {}
      //   Constr.proto = proto;
      //
      //   // Try to create an instance using this prototype
      //   let xpcInstance = new Constr();
      //   let wrapper = proxify(xpcInstance)
      //
      //   xpcProto._proxy = proto and as xpcInstance.__proto__ = xpcProto,
      //   xpcInstance._proxy = proto ... and profixy(xpcInstance) = proto :(
      //
      let proto = window.document.createEvent('HTMLEvents').__proto__;
      window.Event.prototype = proto;
      let event = document.createEvent('HTMLEvents');
      assert(event !== proto, "Event should not be equal to its prototype");
      event.initEvent('dataavailable', true, true);
      assert(event.type === 'dataavailable', "Events are working fine");
      done();
    }
  );

});

exports["test Nested Attributes"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // XrayWrappers has a bug when you set an attribute on it,
      // in some cases, it creates an unnecessary wrapper that introduces
      // a different object that refers to the same original object
      // Check that our wrappers don't reproduce this bug
      // SEE BUG 658560: Fix identity problem with CrossOriginWrappers
      let o = {sandboxObject:true};
      window.nested = o;
      o.foo = true;
      assert(o === window.nested, "Nested attribute to sandbox object should not be proxified");
      window.nested = document;
      assert(window.nested === document, "Nested attribute to proxy should not be double proxified");
      done();
    }
  );

});

exports["test Form nodeName"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      let body = document.body;
      // Check form[nodeName]
      let form = document.createElement("form");
      let input = document.createElement("input");
      input.setAttribute("name", "test");
      form.appendChild(input);
      body.appendChild(form);
      assert(form.test == input, "form[nodeName] is valid");
      body.removeChild(form);
      done();
    }
  );

});

exports["test localStorage"] = createProxyTest("", function (helper, assert) {

  let worker = helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Check localStorage:
      assert(window.localStorage, "has access to localStorage");
      window.localStorage.name = 1;
      assert(window.localStorage.name == 1, "localStorage appears to work");

      self.port.on("step2", function () {
        window.localStorage.clear();
        assert(window.localStorage.name == undefined, "localStorage really, really works");
        done();
      });
      self.port.emit("step1");
    }
  );

  worker.port.on("step1", function () {
    assert.equal(helper.rawWindow.localStorage.name, 1, "localStorage really works");
    worker.port.emit("step2");
  });

});

exports["test Auto Unwrap Custom Attributes"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      let body = document.body;
      // Setting a custom object to a proxy attribute is not wrapped when we get it afterward
      let object = {custom: true, enumerable: false};
      body.customAttribute = object;
      assert(object === body.customAttribute, "custom JS attributes are not wrapped");
      done();
    }
  );

});

exports["test Object Tag"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // <object>, <embed> and other tags return typeof 'function'
      let flash = document.createElement("object");
      assert(typeof flash == "function", "<object> is typeof 'function'");
      assert(flash.toString().match(/\[object HTMLObjectElement.*\]/), "<object> is HTMLObjectElement");
      assert("setAttribute" in flash, "<object> has a setAttribute method");
      done();
    }
  );

});

exports["test Highlight toString Behavior"] = createProxyTest("", function (helper, assert) {
  // We do not have any workaround this particular use of toString
  // applied on <object> elements. So disable this test until we found one!
  //assert.equal(helper.rawWindow.Object.prototype.toString.call(flash), "[object HTMLObjectElement]", "<object> is HTMLObjectElement");
  function f() {};
  let funToString = Object.prototype.toString.call(f);
  assert.ok(/\[object Function.*\]/.test(funToString), "functions are functions 1");

  // This is how jquery call toString:
  let strToString = helper.rawWindow.Object.prototype.toString.call("");
  assert.ok(/\[object String.*\]/.test(strToString), "strings are strings");

  let o = {__exposedProps__:{}};
  let objToString = helper.rawWindow.Object.prototype.toString.call(o);
  assert.ok(/\[object Object.*\]/.test(objToString), "objects are objects");

  // Make sure to pass a function from the same compartments
  // or toString will return [object Object] on FF8+
  let f2 = helper.rawWindow.eval("(function () {})");
  let funToString2 = helper.rawWindow.Object.prototype.toString.call(f2);
  assert.ok(/\[object Function.*\]/.test(funToString2), "functions are functions 2");

  helper.done();
});

exports["test Document TagName"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      let body = document.body;
      // Check document[tagName]
      let div = document.createElement("div");
      div.setAttribute("name", "test");
      body.appendChild(div);
      assert(!document.test, "document[divName] is undefined");
      body.removeChild(div);

      let form = document.createElement("form");
      form.setAttribute("name", "test");
      body.appendChild(form);
      assert(document.test == form, "document[formName] is valid");
      body.removeChild(form);

      let img = document.createElement("img");
      img.setAttribute("name", "test");
      body.appendChild(img);
      assert(document.test == img, "document[imgName] is valid");
      body.removeChild(img);
      done();
    }
  );

});

let html = '<iframe id="iframe" name="test" src="data:text/html;charset=utf-8," />';
exports["test Window Frames"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'let glob = this; new ' + function ContentScriptScope() {
      // Check window[frameName] and window.frames[i]
      let iframe = document.getElementById("iframe");
      //assert(window.frames.length == 1, "The iframe is reported in window.frames check1");
      //assert(window.frames[0] == iframe.contentWindow, "The iframe is reported in window.frames check2");
      assert(window.test == iframe.contentWindow, "window[frameName] is valid");
      done();
    }
  );

});

exports["test Collections"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Highlight XPCNativeWrapper bug with HTMLCollection
      // tds[0] is only defined on first access :o
      let body = document.body;
      let div = document.createElement("div");
      body.appendChild(div);
      div.innerHTML = "<table><tr><td style='padding:0;border:0;display:none'></td><td>t</td></tr></table>";
      let tds = div.getElementsByTagName("td");
      assert(tds[0] == tds[0], "We can get array element multiple times");
      body.removeChild(div);
      done();
    }
  );

});

let html = '<input id="input" type="text" /><input id="input3" type="checkbox" />' +
             '<input id="input2" type="checkbox" />';
exports["test Collections 2"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Verify that NodeList/HTMLCollection are working fine
      let body = document.body;
      let inputs = body.getElementsByTagName("input");
      assert(body.childNodes.length == 3, "body.childNodes length is correct");
      assert(inputs.length == 3, "inputs.length is correct");
      assert(body.childNodes[0] == inputs[0], "body.childNodes[0] is correct");
      assert(body.childNodes[1] == inputs[1], "body.childNodes[1] is correct");
      assert(body.childNodes[2] == inputs[2], "body.childNodes[2] is correct");
      let count = 0;
      for(let i in body.childNodes) {
        count++;
      }

      assert(count >= 3, "body.childNodes is iterable");
      done();
    }
  );

});

exports["test XMLHttpRequest"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // XMLHttpRequest doesn't support XMLHttpRequest.apply,
      // that may break our proxy code
      assert(new window.XMLHttpRequest(), "we are able to instantiate XMLHttpRequest object");
      done();
    }
  );

});

exports["test XPathResult"] = createProxyTest("", function (helper, assert) {
  const XPathResultTypes = ["ANY_TYPE",
                            "NUMBER_TYPE", "STRING_TYPE", "BOOLEAN_TYPE",
                            "UNORDERED_NODE_ITERATOR_TYPE",
                            "ORDERED_NODE_ITERATOR_TYPE",
                            "UNORDERED_NODE_SNAPSHOT_TYPE",
                            "ORDERED_NODE_SNAPSHOT_TYPE",
                            "ANY_UNORDERED_NODE_TYPE",
                            "FIRST_ORDERED_NODE_TYPE"];

  // Check XPathResult bug with constants being undefined on XPCNativeWrapper
  let xpcXPathResult = helper.xrayWindow.XPathResult;

  XPathResultTypes.forEach(function(type, i) {
    assert.equal(xpcXPathResult.wrappedJSObject[type],
                     helper.rawWindow.XPathResult[type],
                     "XPathResult's constants are valid on unwrapped node");

    assert.equal(xpcXPathResult[type], i,
                     "XPathResult's constants are defined on " +
                     "XPCNativeWrapper (platform bug #)");
  });

  let value = helper.rawWindow.XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE;
  let worker = helper.createWorker(
    'new ' + function ContentScriptScope() {
      self.port.on("value", function (value) {
        // Check that our work around is working:
        assert(window.XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE === value,
               "XPathResult works correctly on Proxies");
        done();
      });
    }
  );
  worker.port.emit("value", value);
});

exports["test Prototype Inheritance"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Verify that inherited prototype function like initEvent
      // are handled correctly. (e2.type will return an error if it's not the case)
      let event1 = document.createEvent( 'MouseEvents' );
      event1.initEvent( "click", true, true );
      let event2 = document.createEvent( 'MouseEvents' );
      event2.initEvent( "click", true, true );
      assert(event2.type == "click", "We are able to create an event");
      done();
    }
  );

});

exports["test Functions"] = createProxyTest("", function (helper) {

  helper.rawWindow.callFunction = function callFunction(f) f();
  helper.rawWindow.isEqual = function isEqual(a, b) a == b;
  // bug 784116: workaround in order to allow proxy code to cache proxies on
  // these functions:
  helper.rawWindow.callFunction.__exposedProps__ = {__proxy: 'rw'};
  helper.rawWindow.isEqual.__exposedProps__ = {__proxy: 'rw'};

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Check basic usage of functions
      let closure2 = function () {return "ok";};
      assert(window.wrappedJSObject.callFunction(closure2) == "ok", "Function references work");

      // Ensure that functions are cached when being wrapped to native code
      let closure = function () {};
      assert(window.wrappedJSObject.isEqual(closure, closure), "Function references are cached before being wrapped to native");
      done();
    }
  );

});

let html = '<input id="input2" type="checkbox" />';
exports["test Listeners"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Verify listeners:
      let input = document.getElementById("input2");
      assert(input, "proxy.getElementById works");

      function onclick() {};
      input.onclick = onclick;
      assert(input.onclick === onclick, "on* attributes are equal to original function set");

      let addEventListenerCalled = false;
      let expandoCalled = false;
      input.addEventListener("click", function onclick(event) {
        input.removeEventListener("click", onclick, true);

        assert(!addEventListenerCalled, "closure given to addEventListener is called once");
        if (addEventListenerCalled)
          return;
        addEventListenerCalled = true;

        assert(!event.target.ownerDocument.defaultView.documentGlobal, "event object is still wrapped and doesn't expose document globals");

        let input2 = document.getElementById("input2");

        input.onclick = function (event) {
          input.onclick = null;
          assert(!expandoCalled, "closure set to expando is called once");
          if (expandoCalled) return;
          expandoCalled = true;

          assert(!event.target.ownerDocument.defaultView.documentGlobal, "event object is still wrapped and doesn't expose document globals");

          setTimeout(function () {
            input.click();
            done();
          }, 0);

        }

        setTimeout(function () {
          input.click();
        }, 0);

      }, true);

      input.click();
    }
  );

});

exports["test MozRequestAnimationFrame"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      window.mozRequestAnimationFrame(function callback() {
        assert(callback == this, "callback is equal to `this`");
        done();
      });
    }
  );

});

exports["testGlobalScope"] = createProxyTest("", function (helper) {

  helper.createWorker(
    'let toplevelScope = true;' +
    'assert(window.toplevelScope, "variables in toplevel scope are set to `window` object");' +
    'assert(this.toplevelScope, "variables in toplevel scope are set to `this` object");' +
    'done();'
  );

});

// Bug 715755: proxy code throw an exception on COW
// Create an http server in order to simulate real cross domain documents
exports["test Cross Domain Iframe"] = createProxyTest("", function (helper) {
  let serverPort = 8099;
  let server = require("./lib/httpd").startServerAsync(serverPort);
  server.registerPathHandler("/", function handle(request, response) {
    // Returns the webpage that receive a message and forward it back to its
    // parent document by appending ' world'.
    let content = "<html><head><meta charset='utf-8'></head>\n";
    content += "<script>\n";
    content += "  window.addEventListener('message', function (event) {\n";
    content += "    parent.postMessage(event.data + ' world', '*');\n";
    content += "  }, true);\n";
    content += "</script>\n";
    content += "<body></body>\n";
    content += "</html>\n";
    response.write(content);
  });

  let worker = helper.createWorker(
    'new ' + function ContentScriptScope() {
      // Waits for the server page url
      self.on("message", function (url) {
        // Creates an iframe with this page
        let iframe = document.createElement("iframe");
        iframe.addEventListener("load", function onload() {
          iframe.removeEventListener("load", onload, true);
          try {
            // Try to communicate with iframe's content
            window.addEventListener("message", function onmessage(event) {
              window.removeEventListener("message", onmessage, true);

              assert(event.data == "hello world", "COW works properly");
              self.port.emit("end");
            }, true);
            iframe.contentWindow.postMessage("hello", "*");
          } catch(e) {
            assert(false, "COW fails : "+e.message);
          }
        }, true);
        iframe.setAttribute("src", url);
        document.body.appendChild(iframe);
      });
    }
  );

  worker.port.on("end", function () {
    server.stop(helper.done);
  });

  worker.postMessage("http://localhost:" + serverPort + "/");

});

// Bug 769006: Ensure that MutationObserver works fine with proxies
let html = '<a href="foo">link</a>';
exports["test MutationObvserver"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'new ' + function ContentScriptScope() {
      if (typeof MutationObserver == "undefined") {
        assert(true, "No MutationObserver for this FF version");
        done();
        return;
      }
      let link = document.getElementsByTagName("a")[0];

      // Register a Mutation observer
      let obs = new MutationObserver(function(mutations){
        // Ensure that mutation data are valid
        assert(mutations.length == 1, "only one attribute mutation");
        let mutation = mutations[0];
        assert(mutation.type == "attributes", "check `type`");
        assert(mutation.target == link, "check `target`");
        assert(mutation.attributeName == "href", "check `attributeName`");
        assert(mutation.oldValue == "foo", "check `oldValue`");
        obs.disconnect();
        done();
      });
      obs.observe(document, {
        subtree: true,
        attributes: true,
        attributeOldValue: true,
        attributeFilter: ["href"]
      });

      // Modify the DOM
      link.setAttribute("href", "bar");
    }
  );

});

let html = '<script>' +
  'var accessCheck = function() {' +
  '  assert(true, "exporting function works");' +
  '  try{' +
  '    exportedObj.prop;' +
  '    assert(false, "content should not have access to content-script");' +
  '  } catch(e) {' +
  '    assert(e.toString().indexOf("Permission denied") != -1,' +
  '           "content should not have access to content-script");' +
  '  }' +
  '}</script>';
exports["test nsEp for content-script"] = createProxyTest(html, function (helper) {

  helper.createWorker(
    'let glob = this; new ' + function ContentScriptScope() {

      exportFunction(assert, unsafeWindow, { defineAs: "assert" });
      window.wrappedJSObject.assert(true, "assert exported");
      window.wrappedJSObject.exportedObj = { prop: 42 };
      window.wrappedJSObject.accessCheck();
      done();
    }
  );

});

require("sdk/test").run(exports);
