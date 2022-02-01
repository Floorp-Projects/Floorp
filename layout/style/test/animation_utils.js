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
    if (typeof style != "string") {
      ok(false, "test author forgot to pass argument");
    }
    if (!document.getElementById("display")) {
      ok(false, "no 'display' element to append to");
    }
    gElem = document.createElement(tagname);
    gElem.setAttribute("style", style);
    gElem.classList.add("target");
    document.getElementById("display").appendChild(gElem);
    return [gElem, getComputedStyle(gElem, "")];
  }

  function listen() {
    if (!gElem) {
      ok(false, "test author forgot to call new_div before listen");
    }
    gEventsReceived = [];
    function listener(event) {
      gEventsReceived.push(event);
    }
    gElem.addEventListener("animationstart", listener);
    gElem.addEventListener("animationiteration", listener);
    gElem.addEventListener("animationend", listener);
  }

  function check_events(eventsExpected, desc) {
    // This function checks that the list of eventsExpected matches
    // the received events -- but it only checks the properties that
    // are present on eventsExpected.
    is(
      gEventsReceived.length,
      eventsExpected.length,
      "number of events received for " + desc
    );
    for (
      var i = 0,
        i_end = Math.min(eventsExpected.length, gEventsReceived.length);
      i != i_end;
      ++i
    ) {
      var exp = eventsExpected[i];
      var rec = gEventsReceived[i];
      for (var prop in exp) {
        if (prop == "elapsedTime") {
          // Allow floating point error.
          ok(
            Math.abs(rec.elapsedTime - exp.elapsedTime) < 0.000002,
            "events[" +
              i +
              "]." +
              prop +
              " for " +
              desc +
              " received=" +
              rec.elapsedTime +
              " expected=" +
              exp.elapsedTime
          );
        } else {
          is(
            rec[prop],
            exp[prop],
            "events[" + i + "]." + prop + " for " + desc
          );
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
      ok(
        false,
        "test author called done_element/done_div without matching" +
          " call to new_element/new_div"
      );
    }
    gElem.remove();
    gElem = null;
    if (gEventsReceived.length) {
      ok(false, "caller should have called check_events");
    }
  }

  [new_div, new_element, listen, check_events, done_element].forEach(function(
    fn
  ) {
    window[fn.name] = fn;
  });
  window.done_div = done_element;
})();

function px_to_num(str) {
  return Number(String(str).match(/^([\d.]+)px$/)[1]);
}

function bezier(x1, y1, x2, y2) {
  // Cubic bezier with control points (0, 0), (x1, y1), (x2, y2), and (1, 1).
  function x_for_t(t) {
    var omt = 1 - t;
    return 3 * omt * omt * t * x1 + 3 * omt * t * t * x2 + t * t * t;
  }
  function y_for_t(t) {
    var omt = 1 - t;
    return 3 * omt * omt * t * y1 + 3 * omt * t * t * y2 + t * t * t;
  }
  function t_for_x(x) {
    // Binary subdivision.
    var mint = 0,
      maxt = 1;
    for (var i = 0; i < 30; ++i) {
      var guesst = (mint + maxt) / 2;
      var guessx = x_for_t(guesst);
      if (x < guessx) {
        maxt = guesst;
      } else {
        mint = guesst;
      }
    }
    return (mint + maxt) / 2;
  }
  return function bezier_closure(x) {
    if (x == 0) {
      return 0;
    }
    if (x == 1) {
      return 1;
    }
    return y_for_t(t_for_x(x));
  };
}

function step_end(nsteps) {
  return function step_end_closure(x) {
    return Math.floor(x * nsteps) / nsteps;
  };
}

function step_start(nsteps) {
  var stepend = step_end(nsteps);
  return function step_start_closure(x) {
    return 1.0 - stepend(1.0 - x);
  };
}

var gTF = {
  ease: bezier(0.25, 0.1, 0.25, 1),
  linear: function(x) {
    return x;
  },
  ease_in: bezier(0.42, 0, 1, 1),
  ease_out: bezier(0, 0, 0.58, 1),
  ease_in_out: bezier(0.42, 0, 0.58, 1),
  step_start: step_start(1),
  step_end: step_end(1),
};

function is_approx(float1, float2, error, desc) {
  ok(
    Math.abs(float1 - float2) < error,
    desc + ": " + float1 + " and " + float2 + " should be within " + error
  );
}

function findKeyframesRule(name) {
  for (var i = 0; i < document.styleSheets.length; i++) {
    var match = [].find.call(document.styleSheets[i].cssRules, function(rule) {
      return rule.type == CSSRule.KEYFRAMES_RULE && rule.name == name;
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
//
// specialPowersForPrefs exists because some SpecialPowers objects apparently
// can get prefs and some can't; callers that would normally have one of the
// latter but can get their hands on one of the former can pass it in
// explicitly.
function runOMTATest(aTestFunction, aOnSkip, specialPowersForPrefs) {
  const OMTAPrefKey = "layers.offmainthreadcomposition.async-animations";
  var utils = SpecialPowers.DOMWindowUtils;
  if (!specialPowersForPrefs) {
    specialPowersForPrefs = SpecialPowers;
  }
  var expectOMTA =
    utils.layerManagerRemote &&
    // ^ Off-main thread animation cannot be used if off-main
    // thread composition (OMTC) is not available
    specialPowersForPrefs.getBoolPref(OMTAPrefKey);

  isOMTAWorking()
    .then(function(isWorking) {
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
        todo(
          isWorking,
          "OMTA should ideally work, though we don't expect it to work on " +
            "this platform/configuration"
        );
        aOnSkip();
      }
    })
    .catch(function(err) {
      ok(false, err);
      aOnSkip();
    });

  function isOMTAWorking() {
    // Create keyframes rule
    const animationName = "a6ce3091ed85"; // Random name to avoid clashes
    var ruleText =
      "@keyframes " +
      animationName +
      " { from { opacity: 0.5 } to { opacity: 0.5 } }";
    var style = document.createElement("style");
    style.appendChild(document.createTextNode(ruleText));
    document.head.appendChild(style);

    // Create animation target
    var div = document.createElement("div");
    document.body.appendChild(div);

    // Give the target geometry so it is eligible for layerization
    div.style.width = "100px";
    div.style.height = "100px";
    div.style.backgroundColor = "white";

    // Common clean up code
    var cleanUp = function() {
      div.remove();
      style.remove();
      if (utils.isTestControllingRefreshes) {
        utils.restoreNormalRefresh();
      }
    };

    return waitForDocumentLoad()
      .then(loadPaintListener)
      .then(function() {
        // Put refresh driver under test control and flush all pending style,
        // layout and paint to avoid the situation that waitForPaintsFlush()
        // receives unexpected MozAfterpaint event for those pending
        // notifications.
        utils.advanceTimeAndRefresh(0);
        return waitForPaintsFlushed();
      })
      .then(function() {
        div.style.animation = animationName + " 10s";

        return waitForPaintsFlushed();
      })
      .then(function() {
        var opacity = utils.getOMTAStyle(div, "opacity");
        cleanUp();
        return Promise.resolve(opacity == 0.5);
      })
      .catch(function(err) {
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

  function loadPaintListener() {
    return new Promise(function(resolve, reject) {
      if (typeof window.waitForAllPaints !== "function") {
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
      }
      return Promise.resolve(next.value).then(step, function(err) {
        throw err;
      });
    }

    // Put refresh driver under test control
    SpecialPowers.DOMWindowUtils.advanceTimeAndRefresh(0);

    // Run test
    var promise = aTestFunc();
    if (!promise.then) {
      generator = promise;
      promise = step();
    }
    return promise
      .catch(function(err) {
        ok(false, err.message);
        if (typeof aOnAbort == "function") {
          aOnAbort();
        }
      })
      .then(function() {
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
  TodoMainThread: 3,
  TodoCompositor: 4,
};

const ExpectComparisonTo = {
  Pass: 1,
  Fail: 2,
};

(function() {
  window.omta_todo_is = function(
    elem,
    property,
    expected,
    runningOn,
    desc,
    pseudo
  ) {
    return omta_is_approx(
      elem,
      property,
      expected,
      0,
      runningOn,
      desc,
      ExpectComparisonTo.Fail,
      pseudo
    );
  };

  window.omta_is = function(elem, property, expected, runningOn, desc, pseudo) {
    return omta_is_approx(
      elem,
      property,
      expected,
      0,
      runningOn,
      desc,
      ExpectComparisonTo.Pass,
      pseudo
    );
  };

  // Many callers of this method will pass 'undefined' for
  // expectedComparisonResult.
  window.omta_is_approx = function(
    elem,
    property,
    expected,
    tolerance,
    runningOn,
    desc,
    expectedComparisonResult,
    pseudo
  ) {
    // Check input
    // FIXME: Auto generate this array.
    const omtaProperties = [
      "transform",
      "translate",
      "rotate",
      "scale",
      "offset-path",
      "offset-distance",
      "offset-rotate",
      "offset-anchor",
      "opacity",
      "background-color",
    ];
    if (!omtaProperties.includes(property)) {
      ok(false, property + " is not an OMTA property");
      return;
    }
    var normalize;
    var compare;
    var normalizedToString = JSON.stringify;
    switch (property) {
      case "offset-path":
      case "offset-distance":
      case "offset-rotate":
      case "offset-anchor":
      case "translate":
      case "rotate":
      case "scale":
        if (runningOn == RunningOn.MainThread) {
          normalize = value => value;
          compare = function(a, b, error) {
            return a == b;
          };
          break;
        }
      // fall through
      case "transform":
        normalize = convertTo3dMatrix;
        compare = matricesRoughlyEqual;
        normalizedToString = convert3dMatrixToString;
        break;
      case "opacity":
        normalize = parseFloat;
        compare = function(a, b, error) {
          return Math.abs(a - b) <= error;
        };
        break;
      default:
        normalize = value => value;
        compare = function(a, b, error) {
          return a == b;
        };
        break;
    }

    if (!!expected.compositorValue) {
      const originalNormalize = normalize;
      normalize = value =>
        !!value.compositorValue
          ? originalNormalize(value.compositorValue)
          : originalNormalize(value);
    }

    // Get actual values
    var compositorStr = SpecialPowers.DOMWindowUtils.getOMTAStyle(
      elem,
      property,
      pseudo
    );
    var computedStr = window.getComputedStyle(elem, pseudo)[property];

    // Prepare expected value
    var expectedValue = normalize(expected);
    if (expectedValue === null) {
      ok(
        false,
        desc +
          ": test author should provide a valid 'expected' value" +
          " - got " +
          expected.toString()
      );
      return;
    }

    // Check expected value appears in the right place
    var actualStr;
    switch (runningOn) {
      case RunningOn.Either:
        runningOn =
          compositorStr !== "" ? RunningOn.Compositor : RunningOn.MainThread;
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
        todo(
          compositorStr === "",
          desc + ": should NOT be animating on compositor"
        );
        actualStr = compositorStr === "" ? computedStr : compositorStr;
        break;

      case RunningOn.TodoCompositor:
        todo(
          compositorStr !== "",
          desc + ": should be animating on compositor"
        );
        actualStr = compositorStr !== "" ? computedStr : compositorStr;
        break;

      default:
        if (compositorStr !== "") {
          ok(false, desc + ": should NOT be animating on compositor");
          return;
        }
        actualStr = computedStr;
        break;
    }

    var okOrTodo =
      expectedComparisonResult == ExpectComparisonTo.Fail ? todo : ok;

    // Compare animated value with expected
    var actualValue = normalize(actualStr);
    // Note: the actualStr should be empty string when using todoCompositor, so
    // actualValue is null in this case. However, compare() should handle null
    // well.
    okOrTodo(
      compare(expectedValue, actualValue, tolerance),
      desc +
        " - got " +
        actualStr +
        ", expected " +
        normalizedToString(expectedValue)
    );

    // For transform-like properties, if we have multiple transform-like
    // properties, the OMTA value and getComputedStyle() must be different,
    // so use this flag to skip the following tests.
    // FIXME: Putting this property on the expected value is a little bit odd.
    // It's not really a product of the expected value, but rather the kind of
    // test we're running. That said, the omta_is, omta_todo_is etc. methods are
    // already pretty complex and adding another parameter would probably
    // complicate things too much so this is fine for now. If we extend these
    // functions any more, though, we should probably reconsider this API.
    if (expected.usesMultipleProperties) {
      return;
    }

    if (typeof expected.computed !== "undefined") {
      // For some tests we specify a separate computed value for comparing
      // with getComputedStyle.
      //
      // In particular, we do this for the individual transform functions since
      // the form returned from getComputedStyle() reflects the individual
      // properties (e.g. 'translate: 100px') while the form we read back from
      // the compositor represents the combined result of all the transform
      // properties as a single transform matrix (e.g. [0, 0, 0, 0, 100, 0]).
      //
      // Despite the fact that we can't directly compare the OMTA value against
      // the getComputedStyle value in this case, it is still worth checking the
      // result of getComputedStyle since it will help to alert us if some
      // discrepancy arises between the way we calculate values on the main
      // thread and compositor.
      okOrTodo(
        computedStr == expected.computed,
        desc + ": Computed style should be equal to " + expected.computed
      );
    } else if (actualStr === compositorStr) {
      // For compositor animations do an additional check that they match
      // the value calculated on the main thread
      var computedValue = normalize(computedStr);
      if (computedValue === null) {
        ok(
          false,
          desc +
            ": test framework should parse computed style" +
            " - got " +
            computedStr
        );
        return;
      }
      okOrTodo(
        compare(computedValue, actualValue, 0.0),
        desc +
          ": OMTA style and computed style should be equal" +
          " - OMTA " +
          actualStr +
          ", computed " +
          computedStr
      );
    }
  };

  window.matricesRoughlyEqual = function(a, b, tolerance) {
    // Error handle if a or b is invalid.
    if (!a || !b) {
      return false;
    }

    tolerance = tolerance || 0.00011;
    for (var i = 0; i < 4; i++) {
      for (var j = 0; j < 4; j++) {
        var diff = Math.abs(a[i][j] - b[i][j]);
        if (diff > tolerance || isNaN(diff)) {
          return false;
        }
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
    if (typeof matrixLike == "string") {
      return convertStringTo3dMatrix(matrixLike);
    } else if (Array.isArray(matrixLike)) {
      return convertArrayTo3dMatrix(matrixLike);
    } else if (typeof matrixLike == "object") {
      return convertObjectTo3dMatrix(matrixLike);
    }
    return null;
  };

  // In future most of these methods should be able to be replaced
  // with DOMMatrix
  window.isInvertible = function(matrix) {
    return getDeterminant(matrix) != 0;
  };

  // Converts strings of the format "matrix(...)" and "matrix3d(...)" to a 3d
  // matrix
  function convertStringTo3dMatrix(str) {
    if (str == "none") {
      return convertArrayTo3dMatrix([1, 0, 0, 1, 0, 0]);
    }
    var result = str.match("^matrix(3d)?\\(");
    if (result === null) {
      return null;
    }

    return convertArrayTo3dMatrix(
      str
        .substring(result[0].length, str.length - 1)
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
      return convertObjectTo3dMatrix({
        a: array[0],
        b: array[1],
        c: array[2],
        d: array[3],
        e: array[4],
        f: array[5],
      });
    } else if (array.length == 16) {
      return [
        array.slice(0, 4),
        array.slice(4, 8),
        array.slice(8, 12),
        array.slice(12, 16),
      ];
    }
    return null;
  }

  // Return the first defined value in args.
  function defined(...args) {
    return args.find(arg => typeof arg !== "undefined");
  }

  // Takes an object of the form { a: 1.1, e: 23 } and builds up a 3d matrix
  // with unspecified values filled in with identity values.
  function convertObjectTo3dMatrix(obj) {
    return [
      [
        defined(obj.a, obj.sx, obj.m11, 1),
        obj.b || obj.m12 || 0,
        obj.m13 || 0,
        obj.m14 || 0,
      ],
      [
        obj.c || obj.m21 || 0,
        defined(obj.d, obj.sy, obj.m22, 1),
        obj.m23 || 0,
        obj.m24 || 0,
      ],
      [obj.m31 || 0, obj.m32 || 0, defined(obj.sz, obj.m33, 1), obj.m34 || 0],
      [
        obj.e || obj.tx || obj.m41 || 0,
        obj.f || obj.ty || obj.m42 || 0,
        obj.tz || obj.m43 || 0,
        defined(obj.m44, 1),
      ],
    ];
  }

  function convert3dMatrixToString(matrix) {
    if (is2d(matrix)) {
      return (
        "matrix(" +
        [
          matrix[0][0],
          matrix[0][1],
          matrix[1][0],
          matrix[1][1],
          matrix[3][0],
          matrix[3][1],
        ].join(", ") +
        ")"
      );
    }
    return (
      "matrix3d(" +
      matrix
        .reduce(function(outer, inner) {
          return outer.concat(inner);
        })
        .join(", ") +
      ")"
    );
  }

  function is2d(matrix) {
    return (
      matrix[0][2] === 0 &&
      matrix[0][3] === 0 &&
      matrix[1][2] === 0 &&
      matrix[1][3] === 0 &&
      matrix[2][0] === 0 &&
      matrix[2][1] === 0 &&
      matrix[2][2] === 1 &&
      matrix[2][3] === 0 &&
      matrix[3][2] === 0 &&
      matrix[3][3] === 1
    );
  }

  function getDeterminant(matrix) {
    if (is2d(matrix)) {
      return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
    }

    return (
      matrix[0][3] * matrix[1][2] * matrix[2][1] * matrix[3][0] -
      matrix[0][2] * matrix[1][3] * matrix[2][1] * matrix[3][0] -
      matrix[0][3] * matrix[1][1] * matrix[2][2] * matrix[3][0] +
      matrix[0][1] * matrix[1][3] * matrix[2][2] * matrix[3][0] +
      matrix[0][2] * matrix[1][1] * matrix[2][3] * matrix[3][0] -
      matrix[0][1] * matrix[1][2] * matrix[2][3] * matrix[3][0] -
      matrix[0][3] * matrix[1][2] * matrix[2][0] * matrix[3][1] +
      matrix[0][2] * matrix[1][3] * matrix[2][0] * matrix[3][1] +
      matrix[0][3] * matrix[1][0] * matrix[2][2] * matrix[3][1] -
      matrix[0][0] * matrix[1][3] * matrix[2][2] * matrix[3][1] -
      matrix[0][2] * matrix[1][0] * matrix[2][3] * matrix[3][1] +
      matrix[0][0] * matrix[1][2] * matrix[2][3] * matrix[3][1] +
      matrix[0][3] * matrix[1][1] * matrix[2][0] * matrix[3][2] -
      matrix[0][1] * matrix[1][3] * matrix[2][0] * matrix[3][2] -
      matrix[0][3] * matrix[1][0] * matrix[2][1] * matrix[3][2] +
      matrix[0][0] * matrix[1][3] * matrix[2][1] * matrix[3][2] +
      matrix[0][1] * matrix[1][0] * matrix[2][3] * matrix[3][2] -
      matrix[0][0] * matrix[1][1] * matrix[2][3] * matrix[3][2] -
      matrix[0][2] * matrix[1][1] * matrix[2][0] * matrix[3][3] +
      matrix[0][1] * matrix[1][2] * matrix[2][0] * matrix[3][3] +
      matrix[0][2] * matrix[1][0] * matrix[2][1] * matrix[3][3] -
      matrix[0][0] * matrix[1][2] * matrix[2][1] * matrix[3][3] -
      matrix[0][1] * matrix[1][0] * matrix[2][2] * matrix[3][3] +
      matrix[0][0] * matrix[1][1] * matrix[2][2] * matrix[3][3]
    );
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

function waitForVisitedLinkColoring(visitedLink, waitProperty, waitValue) {
  function checkLink(resolve) {
    if (
      SpecialPowers.DOMWindowUtils.getVisitedDependentComputedStyle(
        visitedLink,
        "",
        waitProperty
      ) == waitValue
    ) {
      // Our link has been styled as visited.  Resolve.
      resolve(true);
    } else {
      // Our link is not yet styled as visited.  Poll for completion.
      setTimeout(checkLink, 0, resolve);
    }
  }
  return new Promise(function(resolve, reject) {
    checkLink(resolve);
  });
}
