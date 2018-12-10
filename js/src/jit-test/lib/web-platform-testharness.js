// Emulate W3C testharness.js in the JS shell.
//
// This file loads ./w3c-testharness.js, which consists of code copied from the
// actual W3C testharness.js file (/dom/imptests/testharness.js).  But that
// code won't work in the shell without considerable help provided by this
// file.


// *** Hacky versions of DOM builtins *****************************************

(function (self) {
    "use strict";

    self.self = self;
    self.window = self;

    // Our stub version of addEventListener never actually fires events.
    // This is sufficient for the tests we've copied so far.
    self.addEventListener = function (event, callback) {};

    let nextId = 1;
    let now = 0;  // bogus clock used by our stub version of setTimeout
    let timeouts = [];

    function enqueue(id, action, dt) {
        let t = now + dt;
        timeouts.push({id, action, t});
        timeouts.sort((a, b) => a.t - b.t);
    }

    self.setTimeout = function (action, dt) {
        let id = nextId++;
        enqueue(id, action, dt);
        return id;
    }

    self.clearTimeout = function (id) {
        timeouts = timeouts.filter(timeout => timeout.id !== id);
    }

    self.setInterval = function (action, dt) {
        let id = nextId++;
        function scheduleNext() {
            enqueue(id, () => { scheduleNext(); action(); }, dt);
        }
        scheduleNext();
        return id;
    }

    self.clearInterval = self.clearTimeout;

    self.drainTimeoutQueue = function drainTimeoutQueue() {
        drainJobQueue();
        while (timeouts.length > 0) {
            let target = timeouts.shift();
            let f = target.action;
            let dt = target.t - now;
            now = target.t;
            f();
            drainJobQueue();
        }
    };

    // *** importScripts ******************************************************
    // Many wpt tests use this; for sanity's sake we allow loading only a few
    // files used by the particular tests we run as jit-tests.

    let allowed_scripts = [
        "../resources/test-utils.js",
        "../resources/rs-utils.js",
        "../resources/constructor-ordering.js",
        "../resources/recording-streams.js",
        "../resources/rs-test-templates.js",
    ];

    self.importScripts = function (url) {
        // Don't load the real test harness, as this would clobber our
        // carefully constructed fake test harness that doesn't require the DOM.
        if (url === "/resources/testharness.js") {
            return;  // this is fine
        }

        if (!allowed_scripts.includes(url)) {
            throw new Error("can't import script: " + uneval(url));
        }

        // Load the file relative to the caller's filename.
        let stack = new Error().stack.split("\n");
        let match = /^\s*[^@]*@(.*):\d+:\d+$/.exec(stack[1]);
        let callerFile = match[1];
        let dir = callerFile.replace(/[^/\\]*$/, "");

        load(dir + url);
    };

}(this));


loadRelativeToScript("w3c-testharness.js");


// *** Hand-coded reimplementation of some parts of testharness.js ************

let all_tests = Promise.resolve();

function fail() {
    print("fail() was called");
    quit(1);
}

function promise_rejects(test, expected, promise) {
    return promise.then(fail).catch(function(e) {
        assert_throws(expected, function() { throw e });
    });
}

function test(fn, description) {
    promise_test(fn, description);
}

function promise_test(fn, description) {
    let test = {
        unreached_func(message) {
            return () => { assert_unreached(description); };
        },

        cleanups: [],
        add_cleanup(fn) {
            this.cleanups.push(fn);
        },

        step_func(func, this_obj = this) {
            return (...args) => this.step(func, this_obj, ...args);
        },

        step(func, this_obj, ...args) {
            return func.apply(this_obj, args);
        },
    };

    all_tests = all_tests.then(async () => {
        print("*   " + description);
        try {
            await fn(test);
            print("    passed\n");
        } catch (exc) {
            print("    failed: " + exc + "\n");
            throw exc;
        } finally {
            for (const cleanup of test.cleanups) {
                cleanup();
            }
        }
    });
}

function done() {
    let passed;
    let problem;
    all_tests
        .then(() => { passed = true; })
        .catch(exc => { passed = false; problem = exc; });
    drainTimeoutQueue();
    if (passed === undefined) {
        throw new Error("test didn't finish");
    }
    if (!passed) {
        throw problem;
    }
    print("all tests passed");
}


// *** Test runner ********************************************************************************

function load_web_platform_test(path) {
  let stabs = [
    libdir + "../../../../testing/web-platform/tests/" + path,
    libdir + "../../../web-platform/tests/" + path,
  ];

  for (let filename of stabs) {
    let code;
    try {
      code = read(filename);
    } catch (exc) {
      if (!(exc instanceof Error && /^can't open/.exec(exc.message))) {
        throw exc;
      }
      continue;
    }

    // Successfully read the file! Now evaluate the code.
    return evaluate(code, {fileName: filename, lineNumber: 1});
  }

  throw new Error(`can't open ${uneval(path)}: tried ${uneval(stabs)}`);
}
