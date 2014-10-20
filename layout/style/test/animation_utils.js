//----------------------------------------------------------------------
//
// Common testing functions
//
//----------------------------------------------------------------------

function advance_clock(milliseconds) {
  SpecialPowers.DOMWindowUtils.advanceTimeAndRefresh(milliseconds);
}

// Test-element creation/destruction and event checking
(function() {
  var gElem;
  var gEventsReceived = [];

  function new_div(style) {
    return new_element("div", style);
  }

  // Creates a new |tagname| element with inline style |style| and appends
  // it as a child of the element with ID 'display'.
  // The element will also be given the class 'target' which can be used
  // for additional styling.
  function new_element(tagname, style) {
    if (gElem) {
      ok(false, "test author forgot to call done_div/done_elem");
    }
    if (typeof(style) != "string") {
      ok(false, "test author forgot to pass argument");
    }
    if (!document.getElementById("display")) {
      ok(false, "no 'display' element to append to");
    }
    gElem = document.createElement(tagname);
    gElem.setAttribute("style", style);
    gElem.classList.add("target");
    document.getElementById("display").appendChild(gElem);
    return [ gElem, getComputedStyle(gElem, "") ];
  }

  function listen() {
    if (!gElem) {
      ok(false, "test author forgot to call new_div before listen");
    }
    gEventsReceived = [];
    function listener(event) {
      gEventsReceived.push(event);
    }
    gElem.addEventListener("animationstart", listener, false);
    gElem.addEventListener("animationiteration", listener, false);
    gElem.addEventListener("animationend", listener, false);
  }

  function check_events(eventsExpected, desc) {
    // This function checks that the list of eventsExpected matches
    // the received events -- but it only checks the properties that
    // are present on eventsExpected.
    is(gEventsReceived.length, gEventsReceived.length,
       "number of events received for " + desc);
    for (var i = 0,
         i_end = Math.min(eventsExpected.length, gEventsReceived.length);
         i != i_end; ++i) {
      var exp = eventsExpected[i];
      var rec = gEventsReceived[i];
      for (var prop in exp) {
        if (prop == "elapsedTime") {
          // Allow floating point error.
          ok(Math.abs(rec.elapsedTime - exp.elapsedTime) < 0.000002,
             "events[" + i + "]." + prop + " for " + desc +
             " received=" + rec.elapsedTime + " expected=" + exp.elapsedTime);
        } else {
          is(rec[prop], exp[prop],
             "events[" + i + "]." + prop + " for " + desc);
        }
      }
    }
    for (var i = eventsExpected.length; i < gEventsReceived.length; ++i) {
      ok(false, "unexpected " + gEventsReceived[i].type + " event for " + desc);
    }
    gEventsReceived = [];
  }

  function done_element() {
    if (!gElem) {
      ok(false, "test author called done_element/done_div without matching"
                + " call to new_element/new_div");
    }
    gElem.remove();
    gElem = null;
    if (gEventsReceived.length) {
      ok(false, "caller should have called check_events");
    }
  }

  [ new_div
    , new_element
    , listen
    , check_events
    , done_element ]
  .forEach(function(fn) {
    window[fn.name] = fn;
  });
  window.done_div = done_element;
})();

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

function findKeyframesRule(name) {
  for (var i = 0; i < document.styleSheets.length; i++) {
    var match = [].find.call(document.styleSheets[i].cssRules, function(rule) {
      return rule.type == CSSRule.KEYFRAMES_RULE &&
             rule.name == name;
    });
    if (match) {
      return match;
    }
  }
  return undefined;
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
        ok(isWorking, "OMTA should work");
        aOnSkip();
      }
    } else {
      todo(isWorking,
           "OMTA should ideally work, though we don't expect it to work on " +
           "this platform/configuration");
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
                   " { from { opacity: 0.5 } to { opacity: 0.5 } }";
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

//----------------------------------------------------------------------
//
// Helper functions for testing animated values on the compositor
//
//----------------------------------------------------------------------

const RunningOn = {
  MainThread: 0,
  Compositor: 1,
  Either: 2,
  TodoMainThread: 3
};

const ExpectComparisonTo = {
  Pass: 1,
  Fail: 2
};

(function() {
  window.omta_todo_is = function(elem, property, expected, runningOn, desc) {
    return omta_is_approx(elem, property, expected, 0, runningOn, desc,
                          ExpectComparisonTo.Fail);
  };

  window.omta_is = function(elem, property, expected, runningOn, desc) {
    return omta_is_approx(elem, property, expected, 0, runningOn, desc);
  };

  // Many callers of this method will pass 'undefined' for
  // expectedComparisonResult.
  window.omta_is_approx = function(elem, property, expected, tolerance,
                                   runningOn, desc, expectedComparisonResult) {
    // Check input
    const omtaProperties = [ "transform", "opacity" ];
    if (omtaProperties.indexOf(property) === -1) {
      ok(false, property + " is not an OMTA property");
      return;
    }
    var isTransform = property == "transform";
    var normalize = isTransform ? convertTo3dMatrix : parseFloat;
    var compare = isTransform ?
                  matricesRoughlyEqual :
                  function(a, b, error) { return Math.abs(a - b) <= error; };
    var normalizedToString = isTransform ?
                             convert3dMatrixToString :
                             JSON.stringify;

    // Get actual values
    var compositorStr =
      SpecialPowers.DOMWindowUtils.getOMTAStyle(elem, property);
    var computedStr = window.getComputedStyle(elem)[property];

    // Prepare expected value
    var expectedValue = normalize(expected);
    if (expectedValue === null) {
      ok(false, desc + ": test author should provide a valid 'expected' value" +
                " - got " + expected.toString());
      return;
    }

    // Check expected value appears in the right place
    var actualStr;
    switch (runningOn) {
      case RunningOn.Either:
        runningOn = compositorStr !== "" ?
                    RunningOn.Compositor :
                    RunningOn.MainThread;
        actualStr = compositorStr !== "" ? compositorStr : computedStr;
        break;

      case RunningOn.Compositor:
        if (compositorStr === "") {
          ok(false, desc + ": should be animating on compositor");
          return;
        }
        actualStr = compositorStr;
        break;

      case RunningOn.TodoMainThread:
        todo(compositorStr === "",
             desc + ": should NOT be animating on compositor");
        actualStr = compositorStr === "" ? computedStr : compositorStr;
        break;

      default:
        if (compositorStr !== "") {
          ok(false, desc + ": should NOT be animating on compositor");
          return;
        }
        actualStr = computedStr;
        break;
    }

    var okOrTodo = expectedComparisonResult == ExpectComparisonTo.Fail ?
                   todo :
                   ok;

    // Compare animated value with expected
    var actualValue = normalize(actualStr);
    if (actualValue === null) {
      ok(false, desc + ": should return a valid result - got " + actualStr);
      return;
    }
    okOrTodo(compare(expectedValue, actualValue, tolerance),
             desc + " - got " + actualStr + ", expected " +
             normalizedToString(expectedValue));

    // For compositor animations do an additional check that they match
    // the value calculated on the main thread
    if (actualStr === compositorStr) {
      var computedValue = normalize(computedStr);
      if (computedValue === null) {
        ok(false, desc + ": test framework should parse computed style" +
                  " - got " + computedStr);
        return;
      }
      okOrTodo(compare(computedValue, actualValue, 0),
               desc + ": OMTA style and computed style should be equal" +
               " - OMTA " + actualStr + ", computed " + computedStr);
    }
  };

  window.matricesRoughlyEqual = function(a, b, tolerance) {
    tolerance = tolerance || 0.00011;
    for (var i = 0; i < 4; i++) {
      for (var j = 0; j < 4; j++) {
        var diff = Math.abs(a[i][j] - b[i][j]);
        if (diff > tolerance || isNaN(diff))
          return false;
      }
    }
    return true;
  };

  // Converts something representing an transform into a 3d matrix in
  // column-major order.
  // The following are supported:
  //  "matrix(...)"
  //  "matrix3d(...)"
  //  [ 1, 0, 0, ... ]
  //  { a: 1, ty: 23 } etc.
  window.convertTo3dMatrix = function(matrixLike) {
    if (typeof(matrixLike) == "string") {
      return convertStringTo3dMatrix(matrixLike);
    } else if (Array.isArray(matrixLike)) {
      return convertArrayTo3dMatrix(matrixLike);
    } else if (typeof(matrixLike) == "object") {
      return convertObjectTo3dMatrix(matrixLike);
    } else {
      return null;
    }
  };

  // In future most of these methods should be able to be replaced
  // with DOMMatrix
  window.isInvertible = function(matrix) {
    return getDeterminant(matrix) != 0;
  };

  // Converts strings of the format "matrix(...)" and "matrix3d(...)" to a 3d
  // matrix
  function convertStringTo3dMatrix(str) {
    if (str == "none")
      return convertArrayTo3dMatrix([1, 0, 0, 1, 0, 0]);
    var result = str.match("^matrix(3d)?\\(");
    if (result === null)
      return null;

    return convertArrayTo3dMatrix(
        str.substring(result[0].length, str.length-1)
           .split(",")
           .map(function(component) {
             return Number(component);
           })
      );
  }

  // Takes an array of numbers of length 6 (2d matrix) or 16 (3d matrix)
  // representing a matrix specified in column-major order and returns a 3d
  // matrix represented as an array of arrays
  function convertArrayTo3dMatrix(array) {
    if (array.length == 6) {
      return convertObjectTo3dMatrix(
        { a: array[0], b: array[1],
          c: array[2], d: array[3],
          e: array[4], f: array[5] } );
    } else if (array.length == 16) {
      return [
        array.slice(0, 4),
        array.slice(4, 8),
        array.slice(8, 12),
        array.slice(12, 16)
      ];
    } else {
      return null;
    }
  }

  // Takes an object of the form { a: 1.1, e: 23 } and builds up a 3d matrix
  // with unspecified values filled in with identity values.
  function convertObjectTo3dMatrix(obj) {
    return [
      [
        obj.a || obj.sx || obj.m11 || 1,
        obj.b || obj.m12 || 0,
        obj.m13 || 0,
        obj.m14 || 0
      ], [
        obj.c || obj.m21 || 0,
        obj.d || obj.sy || obj.m22 || 1,
        obj.m23 || 0,
        obj.m24 || 0
      ], [
        obj.m31 || 0,
        obj.m32 || 0,
        obj.sz || obj.m33 || 1,
        obj.m34 || 0
      ], [
        obj.e || obj.tx || obj.m41 || 0,
        obj.f || obj.ty || obj.m42 || 0,
        obj.tz || obj.m43 || 0,
        obj.m44 || 1
      ]
    ];
  }

  function convert3dMatrixToString(matrix) {
    if (is2d(matrix)) {
      return "matrix(" +
             [ matrix[0][0], matrix[0][1],
               matrix[1][0], matrix[1][1],
               matrix[3][0], matrix[3][1] ].join(", ") + ")";
    } else {
      return "matrix3d(" +
              matrix.reduce(function(outer, inner) {
                  return outer.concat(inner);
              }).join(", ") + ")";
    }
  }

  function is2d(matrix) {
    return matrix[0][2] === 0 && matrix[0][3] === 0 &&
           matrix[1][2] === 0 && matrix[1][3] === 0 &&
           matrix[2][0] === 0 && matrix[2][1] === 0 &&
           matrix[2][2] === 1 && matrix[2][3] === 0 &&
           matrix[3][2] === 0 && matrix[3][3] === 1;
  }

  function getDeterminant(matrix) {
    if (is2d(matrix)) {
      return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    }

    return   matrix[0][3] * matrix[1][2] * matrix[2][1] * matrix[3][0]
           - matrix[0][2] * matrix[1][3] * matrix[2][1] * matrix[3][0]
           - matrix[0][3] * matrix[1][1] * matrix[2][2] * matrix[3][0]
           + matrix[0][1] * matrix[1][3] * matrix[2][2] * matrix[3][0]
           + matrix[0][2] * matrix[1][1] * matrix[2][3] * matrix[3][0]
           - matrix[0][1] * matrix[1][2] * matrix[2][3] * matrix[3][0]
           - matrix[0][3] * matrix[1][2] * matrix[2][0] * matrix[3][1]
           + matrix[0][2] * matrix[1][3] * matrix[2][0] * matrix[3][1]
           + matrix[0][3] * matrix[1][0] * matrix[2][2] * matrix[3][1]
           - matrix[0][0] * matrix[1][3] * matrix[2][2] * matrix[3][1]
           - matrix[0][2] * matrix[1][0] * matrix[2][3] * matrix[3][1]
           + matrix[0][0] * matrix[1][2] * matrix[2][3] * matrix[3][1]
           + matrix[0][3] * matrix[1][1] * matrix[2][0] * matrix[3][2]
           - matrix[0][1] * matrix[1][3] * matrix[2][0] * matrix[3][2]
           - matrix[0][3] * matrix[1][0] * matrix[2][1] * matrix[3][2]
           + matrix[0][0] * matrix[1][3] * matrix[2][1] * matrix[3][2]
           + matrix[0][1] * matrix[1][0] * matrix[2][3] * matrix[3][2]
           - matrix[0][0] * matrix[1][1] * matrix[2][3] * matrix[3][2]
           - matrix[0][2] * matrix[1][1] * matrix[2][0] * matrix[3][3]
           + matrix[0][1] * matrix[1][2] * matrix[2][0] * matrix[3][3]
           + matrix[0][2] * matrix[1][0] * matrix[2][1] * matrix[3][3]
           - matrix[0][0] * matrix[1][2] * matrix[2][1] * matrix[3][3]
           - matrix[0][1] * matrix[1][0] * matrix[2][2] * matrix[3][3]
           + matrix[0][0] * matrix[1][1] * matrix[2][2] * matrix[3][3];
  }
})();

//----------------------------------------------------------------------
//
// Promise wrappers for paint_listener.js
//
//----------------------------------------------------------------------

// Returns a Promise that resolves once all paints have completed
function waitForPaints() {
  return new Promise(function(resolve, reject) {
    waitForAllPaints(resolve);
  });
}

// As with waitForPaints but also flushes pending style changes before waiting
function waitForPaintsFlushed() {
  return new Promise(function(resolve, reject) {
    waitForAllPaintsFlushed(resolve);
  });
}
