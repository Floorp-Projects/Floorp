/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: If you're adding new test harness functionality to this file -- first,
//       should you at all?  Most stuff is better in specific tests, or in
//       nested shell.js/browser.js.  Second, can you instead add it to
//       shell.js?  Our goal is to unify these two files for readability, and
//       the plan is to empty out this file into that one over time.  Third,
//       supposing you must add to this file, please add it to this IIFE for
//       better modularity/resilience against tests that must do particularly
//       bizarre things that might break the harness.

(function initializeUtilityExports(global, parent) {
  "use strict";

  /**********************************************************************
   * CACHED PRIMORDIAL FUNCTIONALITY (before a test might overwrite it) *
   **********************************************************************/

  var Error = global.Error;
  var String = global.String;
  var GlobalEval = global.eval;
  var ReflectApply = global.Reflect.apply;
  var FunctionToString = global.Function.prototype.toString;
  var ObjectDefineProperty = global.Object.defineProperty;

  // BEWARE: ObjectGetOwnPropertyDescriptor is only safe to use if its result
  //         is inspected using own-property-examining functionality.  Directly
  //         accessing properties on a returned descriptor without first
  //         verifying the property's existence can invoke user-modifiable
  //         behavior.
  var ObjectGetOwnPropertyDescriptor = global.Object.getOwnPropertyDescriptor;

  var {get: ArrayBufferByteLength} =
    ObjectGetOwnPropertyDescriptor(global.ArrayBuffer.prototype, "byteLength");

  var Worker = global.Worker;
  var Blob = global.Blob;
  var URL = global.URL;

  var document = global.document;
  var documentAll = global.document.all;
  var documentDocumentElement = global.document.documentElement;
  var DocumentCreateElement = global.document.createElement;

  var EventTargetPrototypeAddEventListener = global.EventTarget.prototype.addEventListener;
  var HTMLElementPrototypeStyleSetter =
    ObjectGetOwnPropertyDescriptor(global.HTMLElement.prototype, "style").set;
  var HTMLIFramePrototypeContentWindowGetter =
    ObjectGetOwnPropertyDescriptor(global.HTMLIFrameElement.prototype, "contentWindow").get;
  var HTMLScriptElementTextSetter =
    ObjectGetOwnPropertyDescriptor(global.HTMLScriptElement.prototype, "text").set;
  var NodePrototypeAppendChild = global.Node.prototype.appendChild;
  var NodePrototypeRemoveChild = global.Node.prototype.removeChild;
  var {get: WindowOnErrorGetter, set: WindowOnErrorSetter} =
    ObjectGetOwnPropertyDescriptor(global, "onerror");
  var WorkerPrototypePostMessage = Worker.prototype.postMessage;
  var URLCreateObjectURL = URL.createObjectURL;

  // List of saved window.onerror handlers.
  var savedGlobalOnError = [];

  // Set |newOnError| as the current window.onerror handler.
  function setGlobalOnError(newOnError) {
    var currentOnError = ReflectApply(WindowOnErrorGetter, global, []);
    ArrayPush(savedGlobalOnError, currentOnError);
    ReflectApply(WindowOnErrorSetter, global, [newOnError]);
  }

  // Restore the previous window.onerror handler.
  function restoreGlobalOnError() {
    var previousOnError = ArrayPop(savedGlobalOnError);
    ReflectApply(WindowOnErrorSetter, global, [previousOnError]);
  }

  /****************************
   * GENERAL HELPER FUNCTIONS *
   ****************************/

  function ArrayPush(array, value) {
    ReflectApply(ObjectDefineProperty, null, [
      array, array.length,
      {__proto__: null, value, writable: true, enumerable: true, configurable: true}
    ]);
  }

  function ArrayPop(array) {
    if (array.length) {
      var item = array[array.length - 1];
      array.length -= 1;
      return item;
    }
  }

  function AppendChild(elt, kid) {
    ReflectApply(NodePrototypeAppendChild, elt, [kid]);
  }

  function CreateElement(name) {
    return ReflectApply(DocumentCreateElement, document, [name]);
  }

  function RemoveChild(elt, kid) {
    ReflectApply(NodePrototypeRemoveChild, elt, [kid]);
  }

  function CreateWorker(script) {
    var blob = new Blob([script], {__proto__: null, type: "text/javascript"});
    return new Worker(URLCreateObjectURL(blob));
  }

  /****************************
   * UTILITY FUNCTION EXPORTS *
   ****************************/

  var evaluate = global.evaluate;
  if (typeof evaluate !== "function") {
    // Shim in "evaluate".
    evaluate = function evaluate(code) {
      if (typeof code !== "string")
        throw Error("Expected string argument for evaluate()");

      return GlobalEval(code);
    };

    global.evaluate = evaluate;
  }

  var evaluateScript = global.evaluateScript;
  if (typeof evaluateScript !== "function") {
    evaluateScript = function evaluateScript(code) {
      code = String(code);
      var script = CreateElement("script");

      // Temporarily install a new onerror handler to catch script errors.
      var hasUncaughtError = false;
      var uncaughtError;
      var eventOptions = {__proto__: null, once: true};
      ReflectApply(EventTargetPrototypeAddEventListener, script, [
        "beforescriptexecute", function() {
          setGlobalOnError(function(messageOrEvent, source, lineno, colno, error) {
            hasUncaughtError = true;
            uncaughtError = error;
            return true;
          });
        }, eventOptions
      ]);
      ReflectApply(EventTargetPrototypeAddEventListener, script, [
        "afterscriptexecute", function() {
          restoreGlobalOnError();
        }, eventOptions
      ]);

      ReflectApply(HTMLScriptElementTextSetter, script, [code]);
      AppendChild(documentDocumentElement, script);
      RemoveChild(documentDocumentElement, script);

      if (hasUncaughtError)
        throw uncaughtError;
    };

    global.evaluateScript = evaluateScript;
  }

  var newGlobal = global.newGlobal;
  if (typeof newGlobal !== "function") {
    // Reuse the parent's newGlobal to ensure iframes can be added to the DOM.
    newGlobal = parent ? parent.newGlobal : function newGlobal() {
      var iframe = CreateElement("iframe");
      AppendChild(documentDocumentElement, iframe);
      var win =
        ReflectApply(HTMLIFramePrototypeContentWindowGetter, iframe, []);

      // Removing the iframe breaks evaluateScript() and detachArrayBuffer().
      ReflectApply(HTMLElementPrototypeStyleSetter, iframe, ["display:none"]);

      // Create utility functions in the new global object.
      var initFunction = ReflectApply(FunctionToString, initializeUtilityExports, []);
      win.Function("parent", initFunction + "; initializeUtilityExports(this, parent);")(global);

      return win;
    };

    global.newGlobal = newGlobal;
  }

  var detachArrayBuffer = global.detachArrayBuffer;
  if (typeof detachArrayBuffer !== "function") {
    var worker = null;
    detachArrayBuffer = function detachArrayBuffer(arrayBuffer) {
      if (worker === null) {
        worker = CreateWorker("/* black hole */");
      }
      try {
        ReflectApply(WorkerPrototypePostMessage, worker, ["detach", [arrayBuffer]]);
      } catch (e) {
        // postMessage throws an error if the array buffer was already detached.
        // Test for this condition by checking if the byte length is zero.
        if (ReflectApply(ArrayBufferByteLength, arrayBuffer, []) !== 0) {
          throw e;
        }
      }
    };

    global.detachArrayBuffer = detachArrayBuffer;
  }

  var createIsHTMLDDA = global.createIsHTMLDDA;
  if (typeof createIsHTMLDDA !== "function") {
    createIsHTMLDDA = function() {
      return documentAll;
    };

    global.createIsHTMLDDA = createIsHTMLDDA;
  }
})(this);

(function(global) {
  "use strict";

  /**********************************************************************
   * CACHED PRIMORDIAL FUNCTIONALITY (before a test might overwrite it) *
   **********************************************************************/

  var undefined; // sigh

  var Error = global.Error;
  var Number = global.Number;
  var Object = global.Object;
  var String = global.String;

  var decodeURIComponent = global.decodeURIComponent;
  var ReflectApply = global.Reflect.apply;
  var ObjectDefineProperty = Object.defineProperty;
  var ObjectPrototypeHasOwnProperty = Object.prototype.hasOwnProperty;
  var ObjectPrototypeIsPrototypeOf = Object.prototype.isPrototypeOf;

  // BEWARE: ObjectGetOwnPropertyDescriptor is only safe to use if its result
  //         is inspected using own-property-examining functionality.  Directly
  //         accessing properties on a returned descriptor without first
  //         verifying the property's existence can invoke user-modifiable
  //         behavior.
  var ObjectGetOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;

  var window = global.window;
  var document = global.document;
  var documentDocumentElement = document.documentElement;
  var DocumentCreateElement = document.createElement;
  var ElementSetClassName =
    ObjectGetOwnPropertyDescriptor(global.Element.prototype, "className").set;
  var NodePrototypeAppendChild = global.Node.prototype.appendChild;
  var NodePrototypeTextContentSetter =
    ObjectGetOwnPropertyDescriptor(global.Node.prototype, "textContent").set;
  var setTimeout = global.setTimeout;

  // Saved harness functions.
  var dump = global.dump;
  var gczeal = global.gczeal;
  var print = global.print;
  var reportFailure = global.reportFailure;
  var TestCase = global.TestCase;

  var SpecialPowers = global.SpecialPowers;
  var SpecialPowersCu = SpecialPowers.Cu;
  var SpecialPowersForceGC = SpecialPowers.forceGC;
  var TestingFunctions = SpecialPowers.Cu.getJSTestingFunctions();
  var ClearKeptObjects = TestingFunctions.clearKeptObjects;

  // Cached DOM nodes used by the test harness itself.  (We assume the test
  // doesn't misbehave in a way that actively interferes with what the test
  // harness runner observes, e.g. navigating the page to a different location.
  // Short of running every test in a worker -- which has its own problems --
  // there's no way to isolate a test from the page to that extent.)
  var printOutputContainer =
    global.document.getElementById("jsreftest-print-output-container");

  /****************************
   * GENERAL HELPER FUNCTIONS *
   ****************************/

  function ArrayPush(array, value) {
    ReflectApply(ObjectDefineProperty, null, [
      array, array.length,
      {__proto__: null, value, writable: true, enumerable: true, configurable: true}
    ]);
  }

  function ArrayPop(array) {
    if (array.length) {
      var item = array[array.length - 1];
      array.length -= 1;
      return item;
    }
  }

  function HasOwnProperty(object, property) {
    return ReflectApply(ObjectPrototypeHasOwnProperty, object, [property]);
  }

  function AppendChild(elt, kid) {
    ReflectApply(NodePrototypeAppendChild, elt, [kid]);
  }

  function CreateElement(name) {
    return ReflectApply(DocumentCreateElement, document, [name]);
  }

  function SetTextContent(element, text) {
    ReflectApply(NodePrototypeTextContentSetter, element, [text]);
  }

  // Object containing the set options.
  var currentOptions;

  // browser.js version of shell.js' |shellOptionsClear| function.
  function browserOptionsClear() {
    for (var optionName in currentOptions) {
      delete currentOptions[optionName];
      SpecialPowersCu[optionName] = false;
    }
  }

  // This function is *only* used by shell.js's for-browsers |print()| function!
  // It's only defined/exported here because it needs CreateElement and friends,
  // only defined here, and we're not yet ready to move them to shell.js.
  function AddPrintOutput(s) {
    var msgDiv = CreateElement("div");
    SetTextContent(msgDiv, s);
    AppendChild(printOutputContainer, msgDiv);
  }
  global.AddPrintOutput = AddPrintOutput;

  /*************************************************************************
   * HARNESS-CENTRIC EXPORTS (we should generally work to eliminate these) *
   *************************************************************************/

  // This overwrites shell.js's version that merely prints the given string.
  function writeHeaderToLog(string) {
    string = String(string);

    // First dump to the console.
    dump(string + "\n");

    // Then output to the page.
    var h2 = CreateElement("h2");
    SetTextContent(h2, string);
    AppendChild(printOutputContainer, h2);
  }
  global.writeHeaderToLog = writeHeaderToLog;

  /*************************
   * GLOBAL ERROR HANDLING *
   *************************/

  // Possible values:
  // - "Unknown" if no error is expected,
  // - "error" if no specific error type is expected,
  // - otherwise the error name, e.g. "TypeError" or "RangeError".
  var expectedError;

  window.onerror = function (msg, page, line, column, error) {
    // Unset all options even when the test finished with an error.
    browserOptionsClear();

    if (DESCRIPTION === undefined) {
      DESCRIPTION = "Unknown";
    }

    var actual = "error";
    var expected = expectedError;
    if (expected !== "error" && expected !== "Unknown") {
      // Check the error type when an actual Error object is available.
      // NB: The |error| parameter of the onerror handler is not required to
      // be an Error instance.
      if (ReflectApply(ObjectPrototypeIsPrototypeOf, Error.prototype, [error])) {
        actual = error.constructor.name;
      } else {
        expected = "error";
      }
    }

    var reason = `${page}:${line}: ${msg}`;
    new TestCase(DESCRIPTION, expected, actual, reason);

    reportFailure(msg);
  };

  /**********************************************
   * BROWSER IMPLEMENTATION FOR SHELL FUNCTIONS *
   **********************************************/

  function gc() {
    try {
      SpecialPowersForceGC();
    } catch (ex) {
      print("gc: " + ex);
    }
  }
  global.gc = gc;

  global.clearKeptObjects = ClearKeptObjects;

  function options(aOptionName) {
    // return value of options() is a comma delimited list
    // of the previously set values

    var value = "";
    for (var optionName in currentOptions) {
      if (value)
        value += ",";
      value += optionName;
    }

    if (aOptionName) {
      if (!HasOwnProperty(SpecialPowersCu, aOptionName)) {
        // This test is trying to flip an unsupported option, so it's
        // likely no longer testing what it was supposed to.  Fail it
        // hard.
        throw "Unsupported JSContext option '" + aOptionName + "'";
      }

      if (aOptionName in currentOptions) {
        // option is set, toggle it to unset
        delete currentOptions[aOptionName];
        SpecialPowersCu[aOptionName] = false;
      } else {
        // option is not set, toggle it to set
        currentOptions[aOptionName] = true;
        SpecialPowersCu[aOptionName] = true;
      }
    }

    return value;
  }
  global.options = options;

  /****************************************
   * HARNESS SETUP AND TEARDOWN FUNCTIONS *
   ****************************************/

  function jsTestDriverBrowserInit() {
    // Initialize with an empty set, because we just turned off all options.
    currentOptions = Object.create(null);

    if (document.location.search.indexOf("?") !== 0) {
      // not called with a query string
      return;
    }

    var properties = Object.create(null);
    var fields = document.location.search.slice(1).split(";");
    for (var i = 0; i < fields.length; i++) {
      var propertycaptures = /^([^=]+)=(.*)$/.exec(fields[i]);
      if (propertycaptures === null) {
        properties[fields[i]] = true;
      } else {
        properties[propertycaptures[1]] = decodeURIComponent(propertycaptures[2]);
      }
    }

    global.gTestPath = properties.test;

    var testpathparts = properties.test.split("/");
    if (testpathparts.length < 2) {
      // must have at least suitepath/testcase.js
      return;
    }

    var testFileName = testpathparts[testpathparts.length - 1];

    if (testFileName.endsWith("-n.js")) {
      // Negative test without a specific error type.
      expectedError = "error";
    } else if (properties.error) {
      // Negative test which expects a specific error type.
      expectedError = properties.error;
    } else {
      // No error is expected.
      expectedError = "Unknown";
    }

    if (properties.gczeal) {
      gczeal(Number(properties.gczeal));
    }

    // Display the test path in the title.
    document.title = properties.test;

    // Output script tags for shell.js, then browser.js, at each level of the
    // test path hierarchy.
    var prepath = "";
    var scripts = [];
    for (var i = 0; i < testpathparts.length - 1; i++) {
      prepath += testpathparts[i] + "/";

      if (properties["test262-raw"]) {
        // Skip running test harness files (shell.js and browser.js) if the
        // test has the raw flag.
        continue;
      }

      scripts.push({src: prepath + "shell.js", module: false});
      scripts.push({src: prepath + "browser.js", module: false});
    }

    // Output the test script itself.
    var moduleTest = !!properties.module;
    scripts.push({src: prepath + testFileName, module: moduleTest});

    // Finally output the driver-end script to advance to the next test.
    scripts.push({src: "js-test-driver-end.js", module: false});

    if (properties.async) {
      gDelayTestDriverEnd = true;
    }

    if (!moduleTest) {
      for (var i = 0; i < scripts.length; i++) {
        var src = scripts[i].src;
        document.write(`<script src="${src}" charset="utf-8"><\/script>`);
      }
    } else {
      // Modules are loaded asynchronously by default, but for the test harness
      // we need to execute all scripts and modules one after the other.

      // Appends the next script element to the DOM.
      function appendScript(index) {
        var script = scriptElements[index];
        scriptElements[index] = null;
        if (script !== null) {
          ReflectApply(NodePrototypeAppendChild, documentDocumentElement, [script]);
        }
      }

      // Create all script elements upfront, so we don't need to worry about
      // modified built-ins.
      var scriptElements = [];
      for (var i = 0; i < scripts.length; i++) {
        var spec = scripts[i];

        var script = document.createElement("script");
        script.charset = "utf-8";
        if (spec.module) {
          script.type = "module";
        }
        script.src = spec.src;

        let nextScriptIndex = i + 1;
        if (nextScriptIndex < scripts.length) {
          var callNextAppend = () => appendScript(nextScriptIndex);
          script.addEventListener("afterscriptexecute", callNextAppend, {once: true});

          // Module scripts don't fire the "afterscriptexecute" event when there
          // was an error, instead the "error" event is emitted. So listen for
          // both events when creating module scripts.
          if (spec.module) {
            script.addEventListener("error", callNextAppend, {once: true});
          }
        }

        scriptElements[i] = script;
      }

      // Append the first script.
      appendScript(0);
    }
  }

  global.gDelayTestDriverEnd = false;

  function jsTestDriverEnd() {
    // gDelayTestDriverEnd is used to delay collection of the test result and
    // signal to Spider so that tests can continue to run after page load has
    // fired. They are responsible for setting gDelayTestDriverEnd = true then
    // when completed, setting gDelayTestDriverEnd = false then calling
    // jsTestDriverEnd()

    if (gDelayTestDriverEnd) {
      return;
    }

    window.onerror = null;

    // Unset all options when the test has finished.
    browserOptionsClear();

    if (window.opener && window.opener.runNextTest) {
      if (window.opener.reportCallBack) {
        window.opener.reportCallBack(window.opener.gWindow);
      }

      setTimeout("window.opener.runNextTest()", 250);
    } else {
      // tell reftest the test is complete.
      ReflectApply(ElementSetClassName, documentDocumentElement, [""]);
      // tell Spider page is complete
      gPageCompleted = true;
    }
  }
  global.jsTestDriverEnd = jsTestDriverEnd;

  /***************************************************************************
   * DIALOG CLOSER, PRESUMABLY TO CLOSE SLOW SCRIPT DIALOGS AND OTHER POPUPS *
   ***************************************************************************/

  // dialog closer from http://bclary.com/projects/spider/spider/chrome/content/spider/dialog-closer.js

  // Use an array to handle the case where multiple dialogs appear at one time.
  var dialogCloserSubjects = [];
  var dialogCloser = SpecialPowers
                     .Cc["@mozilla.org/embedcomp/window-watcher;1"]
                     .getService(SpecialPowers.Ci.nsIWindowWatcher);
  var dialogCloserObserver = {
    observe(subject, topic, data) {
      if (topic === "domwindowopened" && subject.isChromeWindow) {
        ArrayPush(dialogCloserSubjects, subject);

        // Timeout of 0 needed when running under reftest framework.
        subject.setTimeout(closeDialog, 0);
      }
    }
  };

  function closeDialog() {
    while (dialogCloserSubjects.length > 0) {
      var subject = ArrayPop(dialogCloserSubjects);
      subject.close();
    }
  }

  function unregisterDialogCloser() {
    gczeal(0);

    if (!dialogCloserObserver || !dialogCloser) {
      return;
    }

    dialogCloser.unregisterNotification(dialogCloserObserver);

    dialogCloserObserver = null;
    dialogCloser = null;
  }

  dialogCloser.registerNotification(dialogCloserObserver);
  window.addEventListener("unload", unregisterDialogCloser, true);

  /*******************************************
   * RUN ONCE CODE TO SETUP ADDITIONAL STATE *
   *******************************************/

  jsTestDriverBrowserInit();

})(this);

var gPageCompleted;
