// Promise created with default resolution functions (without instances) should
// behave in the same way as normal case.

class MockPromise {
  constructor(...args) {
    return new Promise(...args);
  }
}

for (const expected of ["resolve", "reject"]) {
  for (const unexpected of ["resolve", "reject"]) {
    let resolve, reject;
    const p = new Promise((a, b) => {
      resolve = a;
      reject = b;
    });

    // To prevent fast path in Promise.resolve that returns the passed promise,
    // modify constructor property.
    p.constructor = MockPromise;

    // Given `p` has custom constructor, Promise.resolve creates a new promise
    // instead of returning `p` itself.
    //
    // Newly created promise `optimized` does not have resolution function
    // instances.
    const optimized = Promise.resolve(p);

    // Newly created promise `normal` has resolution functions.
    const normal = new Promise(r => r(p));

    // These calls should be ignored because [[AlreadyResolved]] == true,
    if (unexpected === "resolve") {
      resolvePromise(optimized, "unexpected resolve optimized");
      resolvePromise(normal, "unexpected resolve normal");
    } else {
      rejectPromise(optimized, "unexpected reject optimized");
      rejectPromise(normal, "unexpected reject normal");
    }

    if (expected === "resolve") {
      resolve("resolve");
    } else {
      reject("reject");
    }

    let optimized_resolutionValue, optimized_rejectionValue;
    optimized.then(
      x => { optimized_resolutionValue = x; },
      x => { optimized_rejectionValue = x; }
    );

    let normal_resolutionValue, normal_rejectionValue;
    normal.then(
      x => { normal_resolutionValue = x; },
      x => { normal_rejectionValue = x; }
    );

    drainJobQueue();

    if (expected === "resolve") {
      assertEq(optimized_resolutionValue, "resolve",
               `${expected} + ${unexpected}`);
      assertEq(normal_resolutionValue, "resolve",
               `${expected} + ${unexpected}`);
    } else {
      assertEq(optimized_rejectionValue, "reject",
               `${expected} + ${unexpected}`);
      assertEq(normal_rejectionValue, "reject",
               `${expected} + ${unexpected}`);
    }
  }
}
