function px_to_num(str)
{
    return Number(String(str).match(/^([\d.]+)px$/)[1]);
}

function bezier(x1, y1, x2, y2) {
    // Cubic bezier with control points (0, 0), (x1, y1), (x2, y2), and (1, 1).
    function x_for_t(t) {
        var omt = 1-t;
        return 3 * omt * omt * t * x1 + 3 * omt * t * t * x2 + t * t * t;
    }
    function y_for_t(t) {
        var omt = 1-t;
        return 3 * omt * omt * t * y1 + 3 * omt * t * t * y2 + t * t * t;
    }
    function t_for_x(x) {
        // Binary subdivision.
        var mint = 0, maxt = 1;
        for (var i = 0; i < 30; ++i) {
            var guesst = (mint + maxt) / 2;
            var guessx = x_for_t(guesst);
            if (x < guessx)
                maxt = guesst;
            else
                mint = guesst;
        }
        return (mint + maxt) / 2;
    }
    return function bezier_closure(x) {
        if (x == 0) return 0;
        if (x == 1) return 1;
        return y_for_t(t_for_x(x));
    }
}

function step_end(nsteps) {
    return function step_end_closure(x) {
        return Math.floor(x * nsteps) / nsteps;
    }
}

function step_start(nsteps) {
    var stepend = step_end(nsteps);
    return function step_start_closure(x) {
        return 1.0 - stepend(1.0 - x);
    }
}

var gTF = {
  "ease": bezier(0.25, 0.1, 0.25, 1),
  "linear": function(x) { return x; },
  "ease_in": bezier(0.42, 0, 1, 1),
  "ease_out": bezier(0, 0, 0.58, 1),
  "ease_in_out": bezier(0.42, 0, 0.58, 1),
  "step_start": step_start(1),
  "step_end": step_end(1),
};

function is_approx(float1, float2, error, desc) {
  ok(Math.abs(float1 - float2) < error,
     desc + ": " + float1 + " and " + float2 + " should be within " + error);
}

// Checks if off-main thread animation (OMTA) is available, and if it is, runs
// the provided callback function. If OMTA is not available or is not
// functioning correctly, the second callback, aOnSkip, is run instead.
//
// This function also does an internal test to verify that OMTA is working at
// all so that if OMTA is not functioning correctly when it is expected to
// function only a single failure is produced.
//
// Since this function relies on various asynchronous operations, the caller is
// responsible for calling SimpleTest.waitForExplicitFinish() before calling
// this and SimpleTest.finish() within aTestFunction and aOnSkip.
function runOMTATest(aTestFunction, aOnSkip) {
  const OMTAPrefKey = "layers.offmainthreadcomposition.async-animations";
  var utils      = SpecialPowers.DOMWindowUtils;
  var expectOMTA = utils.layerManagerRemote &&
                   // ^ Off-main thread animation cannot be used if off-main
                   // thread composition (OMTC) is not available
                   SpecialPowers.getBoolPref(OMTAPrefKey);

  isOMTAWorking().then(function(isWorking) {
    if (expectOMTA) {
      if (isWorking) {
        aTestFunction();
      } else {
        // We only call this when we know it will fail as otherwise in the
        // regular success case we will end up inflating the "passed tests"
        // count by 1
        ok(isWorking, "OMTA is working as expected");
        aOnSkip();
      }
    } else {
      todo(isWorking, "OMTA is working");
      aOnSkip();
    }
  }).catch(function(err) {
    ok(false, err);
    aOnSkip();
  });

  function isOMTAWorking() {
    // Create keyframes rule
    const animationName = "a6ce3091ed85"; // Random name to avoid clashes
    var ruleText = "@keyframes " + animationName +
                   " { from { opacity: 0.5 } to { opacity 0.5 } }";
    var style = document.createElement("style");
    style.appendChild(document.createTextNode(ruleText));
    document.head.appendChild(style);

    // Create animation target
    var div = document.createElement("div");
    document.body.appendChild(div);

    // Give the target geometry so it is eligible for layerization
    div.style.width  = "100px";
    div.style.height = "100px";
    div.style.backgroundColor = "white";

    // Common clean up code
    var cleanUp = function() {
      div.parentNode.removeChild(div);
      style.parentNode.removeChild(style);
      if (utils.isTestControllingRefreshes) {
        utils.restoreNormalRefresh();
      }
    };

    return waitForDocumentLoad()
      .then(loadPaintListener)
      .then(function() {
        // Put refresh driver under test control and trigger animation
        utils.advanceTimeAndRefresh(0);
        div.style.animation = animationName + " 10s";

        // Trigger style flush
        div.clientTop;
        return waitForPaints();
      }).then(function() {
        var opacity = utils.getOMTAStyle(div, "opacity");
        cleanUp();
        return Promise.resolve(opacity == 0.5);
      }).catch(function(err) {
        cleanUp();
        return Promise.reject(err);
      });
  }

  function waitForDocumentLoad() {
    return new Promise(function(resolve, reject) {
      if (document.readyState === "complete") {
        resolve();
      } else {
        window.addEventListener("load", resolve);
      }
    });
  }

  function waitForPaints() {
    return new Promise(function(resolve, reject) {
      waitForAllPaintsFlushed(resolve);
    });
  }

  function loadPaintListener() {
    return new Promise(function(resolve, reject) {
      if (typeof(window.waitForAllPaints) !== "function") {
        var script = document.createElement("script");
        script.onload = resolve;
        script.onerror = function() {
          reject(new Error("Failed to load paint listener"));
        };
        script.src = "/tests/SimpleTest/paint_listener.js";
        var firstScript = document.scripts[0];
        firstScript.parentNode.insertBefore(script, firstScript);
      } else {
        resolve();
      }
    });
  }
}

// Common architecture for setting up a series of asynchronous animation tests
//
// Usage example:
//
//    addAsyncAnimTest(function *() {
//       .. do work ..
//       yield functionThatReturnsAPromise();
//       .. do work ..
//    });
//    runAllAsyncAnimTests().then(SimpleTest.finish());
//
(function() {
  var tests = [];

  window.addAsyncAnimTest = function(generator) {
    tests.push(generator);
  };

  // Returns a promise when all tests have run
  window.runAllAsyncAnimTests = function(aOnAbort) {
    // runAsyncAnimTest returns a Promise that is resolved when the
    // test is finished so we can chain them together
    return tests.reduce(function(sequence, test) {
        return sequence.then(function() {
          return runAsyncAnimTest(test, aOnAbort);
        });
      }, Promise.resolve() /* the start of the sequence */);
  };

  // Takes a generator function that represents a test case. Each point in the
  // test case that waits asynchronously for some result yields a Promise that
  // is resolved when the asynchronous action has completed. By chaining these
  // intermediate results together we run the test to completion.
  //
  // This method itself returns a Promise that is resolved when the generator
  // function has completed.
  //
  // This arrangement is based on add_task() which is currently only available
  // in mochitest-chrome (bug 872229). If add_task becomes available in
  // mochitest-plain, we can remove this function and use add_task instead.
  function runAsyncAnimTest(aTestFunc, aOnAbort) {
    var generator;

    function step(arg) {
      var next;
      try {
        next = generator.next(arg);
      } catch (e) {
        return Promise.reject(e);
      }
      if (next.done) {
        return Promise.resolve(next.value);
      } else {
        return Promise.resolve(next.value)
               .then(step, function(err) { throw err; });
      }
    }

    // Put refresh driver under test control
    SpecialPowers.DOMWindowUtils.advanceTimeAndRefresh(0);

    // Run test
    generator = aTestFunc();
    return step()
    .catch(function(err) {
      ok(false, err.message);
      if (typeof aOnAbort == "function") {
        aOnAbort();
      }
    }).then(function() {
      // Restore clock
      SpecialPowers.DOMWindowUtils.restoreNormalRefresh();
    });
  }
})();
