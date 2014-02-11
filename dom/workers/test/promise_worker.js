function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function isnot(a, b, msg) {
  dump("ISNOT: " + (a!==b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a !== b, msg: a + " !== " + b + ": " + msg });
}

function promiseResolve() {
  ok(Promise, "Promise object should exist");

  var promise = new Promise(function(resolve, reject) {
    ok(resolve, "Promise.resolve exists");
    ok(reject, "Promise.reject exists");

    resolve(42);
  }).then(function(what) {
    ok(true, "Then - resolveCb has been called");
    is(what, 42, "ResolveCb received 42");
    runTest();
  }, function() {
    ok(false, "Then - rejectCb has been called");
    runTest();
  });
}

function promiseResolveNoArg() {
  var promise = new Promise(function(resolve, reject) {
    ok(resolve, "Promise.resolve exists");
    ok(reject, "Promise.reject exists");

    resolve();
  }).then(function(what) {
    ok(true, "Then - resolveCb has been called");
    is(what, undefined, "ResolveCb received undefined");
    runTest();
  }, function() {
    ok(false, "Then - rejectCb has been called");
    runTest();
  });
}

function promiseRejectNoHandler() {
  // This test only checks that the code that reports unhandled errors in the
  // Promises implementation does not crash or leak.
  var promise = new Promise(function(res, rej) {
    noSuchMethod();
  });
  runTest();
}

function promiseReject() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  }).then(function(what) {
    ok(false, "Then - resolveCb has been called");
    runTest();
  }, function(what) {
    ok(true, "Then - rejectCb has been called");
    is(what, 42, "RejectCb received 42");
    runTest();
  });
}

function promiseRejectNoArg() {
  var promise = new Promise(function(resolve, reject) {
    reject();
  }).then(function(what) {
    ok(false, "Then - resolveCb has been called");
    runTest();
  }, function(what) {
    ok(true, "Then - rejectCb has been called");
    is(what, undefined, "RejectCb received undefined");
    runTest();
  });
}

function promiseException() {
  var promise = new Promise(function(resolve, reject) {
    throw 42;
  }).then(function(what) {
    ok(false, "Then - resolveCb has been called");
    runTest();
  }, function(what) {
    ok(true, "Then - rejectCb has been called");
    is(what, 42, "RejectCb received 42");
    runTest();
  });
}

function promiseAsync() {
  var global = "foo";
  var f = new Promise(function(r1, r2) {
    is(global, "foo", "Global should be foo");
    r1(42);
    is(global, "foo", "Global should still be foo");
    setTimeout(function() {
      is(global, "bar", "Global should still be bar!");
      runTest();
    }, 0);
  }).then(function() {
    global = "bar";
  });
  is(global, "foo", "Global should still be foo (2)");
}

function promiseDoubleThen() {
  var steps = 0;
  var promise = new Promise(function(r1, r2) {
    r1(42);
  });

  promise.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 42, "Value == 42");
    steps++;
  }, function(what) {
    ok(false, "Then.reject has been called");
  });

  promise.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(steps, 1, "Then.resolve - step == 1");
    is(what, 42, "Value == 42");
    runTest();
  }, function(what) {
    ok(false, "Then.reject has been called");
  });
}

function promiseThenException() {
  var promise = new Promise(function(resolve, reject) {
    resolve(42);
  });

  promise.then(function(what) {
    ok(true, "Then.resolve has been called");
    throw "booh";
  }).catch(function(e) {
    ok(true, "Catch has been called!");
    runTest();
  });
}

function promiseThenCatchThen() {
  var promise = new Promise(function(resolve, reject) {
    resolve(42);
  });

  var promise2 = promise.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 42, "Value == 42");
    return what + 1;
  }, function(what) {
    ok(false, "Then.reject has been called");
  });

  isnot(promise, promise2, "These 2 promise objs are different");

  promise2.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 43, "Value == 43");
    return what + 1;
  }, function(what) {
    ok(false, "Then.reject has been called");
  }).catch(function() {
    ok(false, "Catch has been called");
  }).then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 44, "Value == 44");
    runTest();
  }, function(what) {
    ok(false, "Then.reject has been called");
  });
}

function promiseRejectThenCatchThen() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  var promise2 = promise.then(function(what) {
    ok(false, "Then.resolve has been called");
  }, function(what) {
    ok(true, "Then.reject has been called");
    is(what, 42, "Value == 42");
    return what + 1;
  });

  isnot(promise, promise2, "These 2 promise objs are different");

  promise2.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 43, "Value == 43");
    return what+1;
  }).catch(function(what) {
    ok(false, "Catch has been called");
  }).then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 44, "Value == 44");
    runTest();
  });
}

function promiseRejectThenCatchThen2() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  promise.then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 42, "Value == 42");
    return what+1;
  }).catch(function(what) {
    is(what, 42, "Value == 42");
    ok(true, "Catch has been called");
    return what+1;
  }).then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 43, "Value == 43");
    runTest();
  });
}

function promiseRejectThenCatchExceptionThen() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  promise.then(function(what) {
    ok(false, "Then.resolve has been called");
  }, function(what) {
    ok(true, "Then.reject has been called");
    is(what, 42, "Value == 42");
    throw(what + 1);
  }).catch(function(what) {
    ok(true, "Catch has been called");
    is(what, 43, "Value == 43");
    return what + 1;
  }).then(function(what) {
    ok(true, "Then.resolve has been called");
    is(what, 44, "Value == 44");
    runTest();
  });
}

function promiseThenCatchOrderingResolve() {
  var global = 0;
  var f = new Promise(function(r1, r2) {
    r1(42);
  });

  f.then(function() {
    f.then(function() {
      global++;
    });
    f.catch(function() {
      global++;
    });
    f.then(function() {
      global++;
    });
    setTimeout(function() {
      is(global, 2, "Many steps... should return 2");
      runTest();
    }, 0);
  });
}

function promiseThenCatchOrderingReject() {
  var global = 0;
  var f = new Promise(function(r1, r2) {
    r2(42);
  })

  f.then(function() {}, function() {
    f.then(function() {
      global++;
    });
    f.catch(function() {
      global++;
    });
    f.then(function() {}, function() {
      global++;
    });
    setTimeout(function() {
      is(global, 2, "Many steps... should return 2");
      runTest();
    }, 0);
  });
}

function promiseThenNoArg() {
  var promise = new Promise(function(resolve, reject) {
    resolve(42);
  });

  var clone = promise.then();
  isnot(promise, clone, "These 2 promise objs are different");
  promise.then(function(v) {
    clone.then(function(cv) {
      is(v, cv, "Both resolve to the same value");
      runTest();
    });
  });
}

function promiseThenUndefinedResolveFunction() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  try {
    promise.then(undefined, function(v) {
      is(v, 42, "Promise rejected with 42");
      runTest();
    });
  } catch (e) {
    ok(false, "then should not throw on undefined resolve function");
  }
}

function promiseThenNullResolveFunction() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  try {
    promise.then(null, function(v) {
      is(v, 42, "Promise rejected with 42");
      runTest();
    });
  } catch (e) {
    ok(false, "then should not throw on null resolve function");
  }
}

function promiseCatchNoArg() {
  var promise = new Promise(function(resolve, reject) {
    reject(42);
  });

  var clone = promise.catch();
  isnot(promise, clone, "These 2 promise objs are different");
  promise.catch(function(v) {
    clone.catch(function(cv) {
      is(v, cv, "Both reject to the same value");
      runTest();
    });
  });
}

function promiseNestedPromise() {
  new Promise(function(resolve, reject) {
    resolve(new Promise(function(resolve, reject) {
      ok(true, "Nested promise is executed");
      resolve(42);
    }));
  }).then(function(value) {
    is(value, 42, "Nested promise is executed and then == 42");
    runTest();
  });
}

function promiseNestedNestedPromise() {
  new Promise(function(resolve, reject) {
    resolve(new Promise(function(resolve, reject) {
      ok(true, "Nested promise is executed");
      resolve(42);
    }).then(function(what) { return what+1; }));
  }).then(function(value) {
    is(value, 43, "Nested promise is executed and then == 43");
    runTest();
  });
}

function promiseWrongNestedPromise() {
  new Promise(function(resolve, reject) {
    resolve(new Promise(function(r, r2) {
      ok(true, "Nested promise is executed");
      r(42);
    }));
    reject(42);
  }).then(function(value) {
    is(value, 42, "Nested promise is executed and then == 42");
    runTest();
  }, function(value) {
     ok(false, "This is wrong");
  });
}

function promiseLoop() {
  new Promise(function(resolve, reject) {
    resolve(new Promise(function(r1, r2) {
      ok(true, "Nested promise is executed");
      r1(new Promise(function(r1, r2) {
        ok(true, "Nested nested promise is executed");
        r1(42);
      }));
    }));
  }).then(function(value) {
    is(value, 42, "Nested nested promise is executed and then == 42");
    runTest();
  }, function(value) {
     ok(false, "This is wrong");
  });
}

function promiseStaticReject() {
  var promise = Promise.reject(42).then(function(what) {
    ok(false, "This should not be called");
  }, function(what) {
    is(what, 42, "Value == 42");
    runTest();
  });
}

function promiseStaticResolve() {
  var promise = Promise.resolve(42).then(function(what) {
    is(what, 42, "Value == 42");
    runTest();
  }, function() {
    ok(false, "This should not be called");
  });
}

function promiseResolveNestedPromise() {
  var promise = Promise.resolve(new Promise(function(r, r2) {
    ok(true, "Nested promise is executed");
    r(42);
  }, function() {
    ok(false, "This should not be called");
  })).then(function(what) {
    is(what, 42, "Value == 42");
    runTest();
  }, function() {
    ok(false, "This should not be called");
  });
}

function promiseRejectNoHandler() {
  // This test only checks that the code that reports unhandled errors in the
  // Promises implementation does not crash or leak.
  var promise = new Promise(function(res, rej) {
    noSuchMethod();
  });
  runTest();
}

function promiseUtilitiesDefined() {
  ok(Promise.all, "Promise.all must be defined when Promise is enabled.");
  ok(Promise.race, "Promise.race must be defined when Promise is enabled.");
  runTest();
}

function promiseAllArray() {
  var p = Promise.all([1, new Date(), Promise.resolve("firefox")]);
  ok(p instanceof Promise, "Return value of Promise.all should be a Promise.");
  p.then(function(values) {
    ok(Array.isArray(values), "Resolved value should be an array.");
    is(values.length, 3, "Resolved array length should match iterable's length.");
    is(values[0], 1, "Array values should match.");
    ok(values[1] instanceof Date, "Array values should match.");
    is(values[2], "firefox", "Array values should match.");
    runTest();
  }, function() {
    ok(false, "Promise.all shouldn't fail when iterable has no rejected Promises.");
    runTest();
  });
}

function promiseAllWaitsForAllPromises() {
  var arr = [
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 1), 50);
    }),
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 2), 10);
    }),
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, new Promise(function(resolve2) {
        resolve2(3);
      })), 10);
    }),
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 4), 20);
    })
  ];

  var p = Promise.all(arr);
  p.then(function(values) {
    ok(Array.isArray(values), "Resolved value should be an array.");
    is(values.length, 4, "Resolved array length should match iterable's length.");
    is(values[0], 1, "Array values should match.");
    is(values[1], 2, "Array values should match.");
    is(values[2], 3, "Array values should match.");
    is(values[3], 4, "Array values should match.");
    runTest();
  }, function() {
    ok(false, "Promise.all shouldn't fail when iterable has no rejected Promises.");
    runTest();
  });
}

function promiseAllRejectFails() {
  var arr = [
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 1), 50);
    }),
    new Promise(function(resolve, reject) {
      setTimeout(reject.bind(undefined, 2), 10);
    }),
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 3), 10);
    }),
    new Promise(function(resolve) {
      setTimeout(resolve.bind(undefined, 4), 20);
    })
  ];

  var p = Promise.all(arr);
  p.then(function(values) {
    ok(false, "Promise.all shouldn't resolve when iterable has rejected Promises.");
    runTest();
  }, function(e) {
    ok(true, "Promise.all should reject when iterable has rejected Promises.");
    is(e, 2, "Rejection value should match.");
    runTest();
  });
}

function promiseRaceEmpty() {
  var p = Promise.race([]);
  ok(p instanceof Promise, "Should return a Promise.");
  // An empty race never resolves!
  runTest();
}

function promiseRaceValuesArray() {
  var p = Promise.race([true, new Date(), 3]);
  ok(p instanceof Promise, "Should return a Promise.");
  p.then(function(winner) {
    is(winner, true, "First value should win.");
    runTest();
  }, function(err) {
    ok(false, "Should not fail " + err + ".");
    runTest();
  });
}

function promiseRacePromiseArray() {
  function timeoutPromise(n) {
    return new Promise(function(resolve) {
      setTimeout(function() {
        resolve(n);
      }, n);
    });
  }

  var arr = [
    timeoutPromise(50),
    timeoutPromise(20),
    timeoutPromise(30),
    timeoutPromise(100)
  ];

  var p = Promise.race(arr);
  p.then(function(winner) {
    is(winner, 20, "Fastest timeout should win.");
    runTest();
  });
}

function promiseRaceReject() {
  var p = Promise.race([
    Promise.reject(new Error("Fail bad!")),
    new Promise(function(resolve) {
      setTimeout(resolve, 0);
    })
  ]);

  p.then(function() {
    ok(false, "Should not resolve when winning Promise rejected.");
    runTest();
  }, function(e) {
    ok(true, "Should be rejected");
    ok(e instanceof Error, "Should reject with Error.");
    ok(e.message == "Fail bad!", "Message should match.");
    runTest();
  });
}

function promiseRaceThrow() {
  var p = Promise.race([
    new Promise(function(resolve) {
      nonExistent();
    }),
    new Promise(function(resolve) {
      setTimeout(resolve, 0);
    })
  ]);

  p.then(function() {
    ok(false, "Should not resolve when winning Promise had an error.");
    runTest();
  }, function(e) {
    ok(true, "Should be rejected");
    ok(e instanceof ReferenceError, "Should reject with ReferenceError for function nonExistent().");
    runTest();
  });
}

function promiseResolveArray() {
  var p = Promise.resolve([1,2,3]);
  ok(p instanceof Promise, "Should return a Promise.");
  p.then(function(v) {
    ok(Array.isArray(v), "Resolved value should be an Array");
    is(v.length, 3, "Length should match");
    is(v[0], 1, "Resolved value should match original");
    is(v[1], 2, "Resolved value should match original");
    is(v[2], 3, "Resolved value should match original");
    runTest();
  });
}

function promiseResolveThenable() {
  var p = Promise.resolve({ then: function(onFulfill, onReject) { onFulfill(2); } });
  ok(p instanceof Promise, "Should cast to a Promise.");
  p.then(function(v) {
    is(v, 2, "Should resolve to 2.");
    runTest();
  }, function(e) {
    ok(false, "promiseResolveThenable should've resolved");
    runTest();
  });
}

function promiseResolvePromise() {
  var original = Promise.resolve(true);
  var cast = Promise.resolve(original);

  ok(cast instanceof Promise, "Should cast to a Promise.");
  is(cast, original, "Should return original Promise.");
  cast.then(function(v) {
    is(v, true, "Should resolve to true.");
    runTest();
  });
}

var tests = [
    promiseResolve,
    promiseReject,
    promiseException,
    promiseAsync,
    promiseDoubleThen,
    promiseThenException,
    promiseThenCatchThen,
    promiseRejectThenCatchThen,
    promiseRejectThenCatchThen2,
    promiseRejectThenCatchExceptionThen,
    promiseThenCatchOrderingResolve,
    promiseThenCatchOrderingReject,
    promiseNestedPromise,
    promiseNestedNestedPromise,
    promiseWrongNestedPromise,
    promiseLoop,
    promiseStaticReject,
    promiseStaticResolve,
    promiseResolveNestedPromise,
    promiseResolveNoArg,
    promiseRejectNoArg,

    promiseThenNoArg,
    promiseThenUndefinedResolveFunction,
    promiseThenNullResolveFunction,
    promiseCatchNoArg,
    promiseRejectNoHandler,

    promiseUtilitiesDefined,

    promiseAllArray,
    promiseAllWaitsForAllPromises,
    promiseAllRejectFails,

    promiseRaceEmpty,
    promiseRaceValuesArray,
    promiseRacePromiseArray,
    promiseRaceReject,
    promiseRaceThrow,

    promiseResolveArray,
    promiseResolveThenable,
    promiseResolvePromise,
];

function runTest() {
  if (!tests.length) {
    postMessage({ type: 'finish' });
    return;
  }

  var test = tests.shift();
  test();
}

onmessage = function() {
  runTest();
}
