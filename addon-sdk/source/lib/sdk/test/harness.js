/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, Cu } = require("chrome");
const { Loader } = require('./loader');
const { serializeStack, parseStack  } = require("toolkit/loader");
const { setTimeout } = require('../timers');
const { PlainTextConsole } = require("../console/plain-text");
const { when: unload } = require("../system/unload");
const { format, fromException }  = require("../console/traceback");
const system = require("../system");
const memory = require('../deprecated/memory');
const { gc: gcPromise } = require('./memory');
const { defer } = require('../core/promise');

// Trick manifest builder to make it think we need these modules ?
const unit = require("../deprecated/unit-test");
const test = require("../../test");
const url = require("../url");

function emptyPromise() {
  let { promise, resolve } = defer();
  resolve();
  return promise;
}

var cService = Cc['@mozilla.org/consoleservice;1'].getService()
               .QueryInterface(Ci.nsIConsoleService);

// The console used to log messages
var testConsole;

// Cuddlefish loader in which we load and execute tests.
var loader;

// Function to call when we're done running tests.
var onDone;

// Function to print text to a console, w/o CR at the end.
var print;

// How many more times to run all tests.
var iterationsLeft;

// Whether to report memory profiling information.
var profileMemory;

// Whether we should stop as soon as a test reports a failure.
var stopOnError;

// Function to call to retrieve a list of tests to execute
var findAndRunTests;

// Combined information from all test runs.
var results = {
  passed: 0,
  failed: 0,
  testRuns: []
};

// A list of the compartments and windows loaded after startup
var startLeaks;

// JSON serialization of last memory usage stats; we keep it stringified
// so we don't actually change the memory usage stats (in terms of objects)
// of the JSRuntime we're profiling.
var lastMemoryUsage;

function analyzeRawProfilingData(data) {
  var graph = data.graph;
  var shapes = {};

  // Convert keys in the graph from strings to ints.
  // TODO: Can we get rid of this ridiculousness?
  var newGraph = {};
  for (id in graph) {
    newGraph[parseInt(id)] = graph[id];
  }
  graph = newGraph;

  var modules = 0;
  var moduleIds = [];
  var moduleObjs = {UNKNOWN: 0};
  for (let name in data.namedObjects) {
    moduleObjs[name] = 0;
    moduleIds[data.namedObjects[name]] = name;
    modules++;
  }

  var count = 0;
  for (id in graph) {
    var parent = graph[id].parent;
    while (parent) {
      if (parent in moduleIds) {
        var name = moduleIds[parent];
        moduleObjs[name]++;
        break;
      }
      if (!(parent in graph)) {
        moduleObjs.UNKNOWN++;
        break;
      }
      parent = graph[parent].parent;
    }
    count++;
  }

  print("\nobject count is " + count + " in " + modules + " modules" +
        " (" + data.totalObjectCount + " across entire JS runtime)\n");
  if (lastMemoryUsage) {
    var last = JSON.parse(lastMemoryUsage);
    var diff = {
      moduleObjs: dictDiff(last.moduleObjs, moduleObjs),
      totalObjectClasses: dictDiff(last.totalObjectClasses,
                                   data.totalObjectClasses)
    };

    for (let name in diff.moduleObjs)
      print("  " + diff.moduleObjs[name] + " in " + name + "\n");
    for (let name in diff.totalObjectClasses)
      print("  " + diff.totalObjectClasses[name] + " instances of " +
            name + "\n");
  }
  lastMemoryUsage = JSON.stringify(
    {moduleObjs: moduleObjs,
     totalObjectClasses: data.totalObjectClasses}
  );
}

function dictDiff(last, curr) {
  var diff = {};

  for (let name in last) {
    var result = (curr[name] || 0) - last[name];
    if (result)
      diff[name] = (result > 0 ? "+" : "") + result;
  }
  for (let name in curr) {
    var result = curr[name] - (last[name] || 0);
    if (result)
      diff[name] = (result > 0 ? "+" : "") + result;
  }
  return diff;
}

function reportMemoryUsage() {
  if (!profileMemory) {
    return emptyPromise();
  }

  return gcPromise().then((function () {
    var mgr = Cc["@mozilla.org/memory-reporter-manager;1"]
              .getService(Ci.nsIMemoryReporterManager);
    let count = 0;
    function logReporter(process, path, kind, units, amount, description) {
      print(((++count == 1) ? "\n" : "") + description + ": " + amount + "\n");
    }
    mgr.getReportsForThisProcess(logReporter, null);

    var weakrefs = [info.weakref.get()
                    for each (info in memory.getObjects())];
    weakrefs = [weakref for each (weakref in weakrefs) if (weakref)];
    print("Tracked memory objects in testing sandbox: " + weakrefs.length + "\n");
  }));
}

var gWeakrefInfo;

function checkMemory() {
  return gcPromise().then(_ => {
    let leaks = getPotentialLeaks();

    let compartmentURLs = Object.keys(leaks.compartments).filter(function(url) {
      return !(url in startLeaks.compartments);
    });

    let windowURLs = Object.keys(leaks.windows).filter(function(url) {
      return !(url in startLeaks.windows);
    });

    for (let url of compartmentURLs)
      console.warn("LEAKED", leaks.compartments[url]);

    for (let url of windowURLs)
      console.warn("LEAKED", leaks.windows[url]);
  }).then(showResults);
}

function showResults() {
  let { promise, resolve } = defer();

  if (gWeakrefInfo) {
    gWeakrefInfo.forEach(
      function(info) {
        var ref = info.weakref.get();
        if (ref !== null) {
          var data = ref.__url__ ? ref.__url__ : ref;
          var warning = data == "[object Object]"
            ? "[object " + data.constructor.name + "(" +
              [p for (p in data)].join(", ") + ")]"
            : data;
          console.warn("LEAK", warning, info.bin);
        }
      }
    );
  }

  onDone(results);

  resolve();
  return promise;
}

function cleanup() {
  let coverObject = {};
  try {
    for (let name in loader.modules)
      memory.track(loader.modules[name],
                           "module global scope: " + name);
      memory.track(loader, "Cuddlefish Loader");

    if (profileMemory) {
      gWeakrefInfo = [{ weakref: info.weakref, bin: info.bin }
                      for each (info in memory.getObjects())];
    }

    loader.unload();

    if (loader.globals.console.errorsLogged && !results.failed) {
      results.failed++;
      console.error("warnings and/or errors were logged.");
    }

    if (consoleListener.errorsLogged && !results.failed) {
      console.warn(consoleListener.errorsLogged + " " +
                   "warnings or errors were logged to the " +
                   "platform's nsIConsoleService, which could " +
                   "be of no consequence; however, they could also " +
                   "be indicative of aberrant behavior.");
    }

    // read the code coverage object, if it exists, from CoverJS-moz
    if (typeof loader.globals.global == "object") {
      coverObject = loader.globals.global['__$coverObject'] || {};
    }

    consoleListener.errorsLogged = 0;
    loader = null;

    memory.gc();
  }
  catch (e) {
    results.failed++;
    console.error("unload.send() threw an exception.");
    console.exception(e);
  };

  setTimeout(require('@test/options').checkMemory ? checkMemory : showResults, 1);

  // dump the coverobject
  if (Object.keys(coverObject).length){
    const self = require('sdk/self');
    const {pathFor} = require("sdk/system");
    let file = require('sdk/io/file');
    const {env} = require('sdk/system/environment');
    console.log("CWD:", env.PWD);
    let out = file.join(env.PWD,'coverstats-'+self.id+'.json');
    console.log('coverstats:', out);
    let outfh = file.open(out,'w');
    outfh.write(JSON.stringify(coverObject,null,2));
    outfh.flush();
    outfh.close();
  }
}

function getPotentialLeaks() {
  memory.gc();

  // Things we can assume are part of the platform and so aren't leaks
  let WHITELIST_BASE_URLS = [
    "chrome://",
    "resource:///",
    "resource://app/",
    "resource://gre/",
    "resource://gre-resources/",
    "resource://pdf.js/",
    "resource://pdf.js.components/",
    "resource://services-common/",
    "resource://services-crypto/",
    "resource://services-sync/"
  ];

  let ioService = Cc["@mozilla.org/network/io-service;1"].
                 getService(Ci.nsIIOService);
  let uri = ioService.newURI("chrome://global/content/", "UTF-8", null);
  let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].
                  getService(Ci.nsIChromeRegistry);
  uri = chromeReg.convertChromeURL(uri);
  let spec = uri.spec;
  let pos = spec.indexOf("!/");
  WHITELIST_BASE_URLS.push(spec.substring(0, pos + 2));

  let zoneRegExp = new RegExp("^explicit/js-non-window/zones/zone[^/]+/compartment\\((.+)\\)");
  let compartmentRegexp = new RegExp("^explicit/js-non-window/compartments/non-window-global/compartment\\((.+)\\)/");
  let compartmentDetails = new RegExp("^([^,]+)(?:, (.+?))?(?: \\(from: (.*)\\))?$");
  let windowRegexp = new RegExp("^explicit/window-objects/top\\((.*)\\)/active");
  let windowDetails = new RegExp("^(.*), id=.*$");

  function isPossibleLeak(item) {
    if (!item.location)
      return false;

    for (let whitelist of WHITELIST_BASE_URLS) {
      if (item.location.substring(0, whitelist.length) == whitelist)
        return false;
    }

    return true;
  }

  let compartments = {};
  let windows = {};
  function logReporter(process, path, kind, units, amount, description) {
    let matches;

    if ((matches = compartmentRegexp.exec(path)) || (matches = zoneRegExp.exec(path))) {
      if (matches[1] in compartments)
        return;

      let details = compartmentDetails.exec(matches[1]);
      if (!details) {
        console.error("Unable to parse compartment detail " + matches[1]);
        return;
      }

      let item = {
        path: matches[1],
        principal: details[1],
        location: details[2] ? details[2].replace("\\", "/", "g") : undefined,
        source: details[3] ? details[3].split(" -> ").reverse() : undefined,
        toString: function() this.location
      };

      if (!isPossibleLeak(item))
        return;

      compartments[matches[1]] = item;
      return;
    }

    if (matches = windowRegexp.exec(path)) {
      if (matches[1] in windows)
        return;

      let details = windowDetails.exec(matches[1]);
      if (!details) {
        console.error("Unable to parse window detail " + matches[1]);
        return;
      }

      let item = {
        path: matches[1],
        location: details[1].replace("\\", "/", "g"),
        source: [details[1].replace("\\", "/", "g")],
        toString: function() this.location
      };

      if (!isPossibleLeak(item))
        return;

      windows[matches[1]] = item;
    }
  }

  Cc["@mozilla.org/memory-reporter-manager;1"]
    .getService(Ci.nsIMemoryReporterManager)
    .getReportsForThisProcess(logReporter, null);

  return { compartments: compartments, windows: windows };
}

function nextIteration(tests) {
  if (tests) {
    results.passed += tests.passed;
    results.failed += tests.failed;

    reportMemoryUsage().then(_ => {
      let testRun = [];
      for each (let test in tests.testRunSummary) {
        let testCopy = {};
        for (let info in test) {
          testCopy[info] = test[info];
        }
        testRun.push(testCopy);
      }

      results.testRuns.push(testRun);
      iterationsLeft--;

      checkForEnd();
    })
  }
  else {
    checkForEnd();
  }
}

function checkForEnd() {
  if (iterationsLeft && (!stopOnError || results.failed == 0)) {
    // Pass the loader which has a hooked console that doesn't dispatch
    // errors to the JS console and avoid firing false alarm in our
    // console listener
    findAndRunTests(loader, nextIteration);
  }
  else {
    setTimeout(cleanup, 0);
  }
}

var POINTLESS_ERRORS = [
  'Invalid chrome URI:',
  'OpenGL LayerManager Initialized Succesfully.',
  '[JavaScript Error: "TelemetryStopwatch:',
  'reference to undefined property',
  '[JavaScript Error: "The character encoding of the HTML document was ' +
    'not declared.',
  '[Javascript Warning: "Error: Failed to preserve wrapper of wrapped ' +
    'native weak map key',
  '[JavaScript Warning: "Duplicate resource declaration for',
  'file: "chrome://browser/content/',
  'file: "chrome://global/content/',
  '[JavaScript Warning: "The character encoding of a framed document was ' +
    'not declared.'
];

var consoleListener = {
  errorsLogged: 0,
  observe: function(object) {
    if (!(object instanceof Ci.nsIScriptError))
      return;
    this.errorsLogged++;
    var message = object.QueryInterface(Ci.nsIConsoleMessage).message;
    var pointless = [err for each (err in POINTLESS_ERRORS)
                         if (message.indexOf(err) >= 0)];
    if (pointless.length == 0 && message)
      testConsole.log(message);
  }
};

function TestRunnerConsole(base, options) {
  this.__proto__ = {
    errorsLogged: 0,
    warn: function warn() {
      this.errorsLogged++;
      base.warn.apply(base, arguments);
    },
    error: function error() {
      this.errorsLogged++;
      base.error.apply(base, arguments);
    },
    info: function info(first) {
      if (options.verbose)
        base.info.apply(base, arguments);
      else
        if (first == "pass:")
          print(".");
    },
    __proto__: base
  };
}

function stringify(arg) {
  try {
    return String(arg);
  }
  catch(ex) {
    return "<toString() error>";
  }
}

function stringifyArgs(args) {
  return Array.map(args, stringify).join(" ");
}

function TestRunnerTinderboxConsole(base, options) {
  this.base = base;
  this.print = options.print;
  this.verbose = options.verbose;
  this.errorsLogged = 0;

  // Binding all the public methods to an instance so that they can be used
  // as callback / listener functions straightaway.
  this.log = this.log.bind(this);
  this.info = this.info.bind(this);
  this.warn = this.warn.bind(this);
  this.error = this.error.bind(this);
  this.debug = this.debug.bind(this);
  this.exception = this.exception.bind(this);
  this.trace = this.trace.bind(this);
};

TestRunnerTinderboxConsole.prototype = {
  testMessage: function testMessage(pass, expected, test, message) {
    let type = "TEST-";
    if (expected) {
      if (pass)
        type += "PASS";
      else
        type += "KNOWN-FAIL";
    }
    else {
      this.errorsLogged++;
      if (pass)
        type += "UNEXPECTED-PASS";
      else
        type += "UNEXPECTED-FAIL";
    }

    this.print(type + " | " + test + " | " + message + "\n");
    if (!expected)
      this.trace();
  },

  log: function log() {
    this.print("TEST-INFO | " + stringifyArgs(arguments) + "\n");
  },

  info: function info(first) {
    this.print("TEST-INFO | " + stringifyArgs(arguments) + "\n");
  },

  warn: function warn() {
    this.errorsLogged++;
    this.print("TEST-UNEXPECTED-FAIL | " + stringifyArgs(arguments) + "\n");
  },

  error: function error() {
    this.errorsLogged++;
    this.print("TEST-UNEXPECTED-FAIL | " + stringifyArgs(arguments) + "\n");
    this.base.error.apply(this.base, arguments);
  },

  debug: function debug() {
    this.print("TEST-INFO | " + stringifyArgs(arguments) + "\n");
  },

  exception: function exception(e) {
    this.print("An exception occurred.\n" +
               require("../console/traceback").format(e) + "\n" + e + "\n");
  },

  trace: function trace() {
    var traceback = require("../console/traceback");
    var stack = traceback.get();
    stack.splice(-1, 1);
    this.print("TEST-INFO | " + stringify(traceback.format(stack)) + "\n");
  }
};

var runTests = exports.runTests = function runTests(options) {
  iterationsLeft = options.iterations;
  profileMemory = options.profileMemory;
  stopOnError = options.stopOnError;
  onDone = options.onDone;
  print = options.print;
  findAndRunTests = options.findAndRunTests;

  try {
    cService.registerListener(consoleListener);
    print("Running tests on " + system.name + " " + system.version +
          "/Gecko " + system.platformVersion + " (" +
          system.id + ") under " +
          system.platform + "/" + system.architecture + ".\n");

    if (options.parseable)
      testConsole = new TestRunnerTinderboxConsole(new PlainTextConsole(), options);
    else
      testConsole = new TestRunnerConsole(new PlainTextConsole(), options);

    loader = Loader(module, {
      console: testConsole,
      global: {} // useful for storing things like coverage testing.
    });

    // Load these before getting initial leak stats as they will still be in
    // memory when we check later
    require("../deprecated/unit-test");
    require("../deprecated/unit-test-finder");
    startLeaks = getPotentialLeaks();

    nextIteration();
  } catch (e) {
    let frames = fromException(e).reverse().reduce(function(frames, frame) {
      if (frame.fileName.split("/").pop() === "unit-test-finder.js")
        frames.done = true
      if (!frames.done) frames.push(frame)

      return frames
    }, [])

    let prototype = typeof(e) === "object" ? e.constructor.prototype :
                    Error.prototype;
    let stack = serializeStack(frames.reverse());

    let error = Object.create(prototype, {
      message: { value: e.message, writable: true, configurable: true },
      fileName: { value: e.fileName, writable: true, configurable: true },
      lineNumber: { value: e.lineNumber, writable: true, configurable: true },
      stack: { value: stack, writable: true, configurable: true },
      toString: { value: function() String(e), writable: true, configurable: true },
    });

    print("Error: " + error + " \n " + format(error));
    onDone({passed: 0, failed: 1});
  }
};

unload(function() {
  cService.unregisterListener(consoleListener);
});

