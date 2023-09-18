// `debugGetQueuedJobs` is available only in debug build.
if (!getBuildConfiguration("debug")) {
  quit();
}

function testOne(func) {
  assertEq(debugGetQueuedJobs().length, 0);

  func();

  drainJobQueue();

  assertEq(debugGetQueuedJobs().length, 0);

  if (func.length == 1) {
    func({sameCompartmentAs: globalThis});

    drainJobQueue();

    assertEq(debugGetQueuedJobs().length, 0);
  }
}

function assertGlobal(obj, expectedGlobal) {
  const global = objectGlobal(obj);
  if (global) {
    assertEq(global === expectedGlobal, true);
  } else {
    // obj is a wrapper.
    // expectedGlobal should be other global than this.
    assertEq(expectedGlobal !== globalThis, true);
  }
}

testOne(() => {
  // Just creating a promise shouldn't enqueue any jobs.
  Promise.resolve(10);
  assertEq(debugGetQueuedJobs().length, 0);
});

testOne(() => {
  // Calling then should create a job for each.
  Promise.resolve(10).then(() => {});
  Promise.resolve(10).then(() => {});
  Promise.resolve(10).then(() => {});

  assertEq(debugGetQueuedJobs().length, 3);
});

testOne(() => {
  // The reaction job should use the function's realm.
  Promise.resolve(10).then(() => {});

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the function's realm.
  var g = newGlobal(newGlobalOptions);
  g.eval(`
Promise.resolve(10).then(() => {});
`);

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The reaction job should use the function's realm.
  var g = newGlobal(newGlobalOptions);
  g.Promise.resolve(10).then(g.eval(`() => {}`));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The reaction job should use the function's realm.
  var g = newGlobal(newGlobalOptions);
  g.Promise.resolve(10).then(() => {});

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the bound function's target function's realm.
  var g = newGlobal(newGlobalOptions);
  g.Promise.resolve(10)
    .then(Function.prototype.bind.call(g.eval(`() => {}`), this));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The reaction job should use the bound function's target function's realm.
  var g = newGlobal(newGlobalOptions);
  g.Promise.resolve(10)
    .then(g.Function.prototype.bind.call(() => {}, g));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the bound function's target function's realm,
  // recursively
  var g = newGlobal(newGlobalOptions);
  g.Promise.resolve(10)
    .then(
      g.Function.prototype.bind.call(
        Function.prototype.bind.call(
          g.Function.prototype.bind.call(
            () => {},
            g),
          this),
        g)
    );

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the bound function's target function's realm,
  // recursively
  var g = newGlobal(newGlobalOptions);
  Promise.resolve(10)
    .then(
      g.Function.prototype.bind.call(
        Function.prototype.bind.call(
          g.Function.prototype.bind.call(
            Function.prototype.bind.call(
              g.eval(`() => {}`),
              this),
            g),
          this),
        g)
    );

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The reaction job should use the proxy's target function's realm.
  var g = newGlobal(newGlobalOptions);
  g.handler = () => {};
  g.eval(`
Promise.resolve(10).then(new Proxy(handler, {}));
`);

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the proxy's target function's realm.
  var g = newGlobal(newGlobalOptions);
  g.eval(`
var handler = () => {};
`);
  Promise.resolve(10).then(new Proxy(g.handler, {}));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});


testOne(newGlobalOptions => {
  // The reaction job should use the proxy's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  g.handler = () => {};
  g.outerProxy = Proxy;
  g.eval(`
Promise.resolve(10).then(
  new outerProxy(new Proxy(new outerProxy(new Proxy(handler, {}), {}), {}), {})
);
`);

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The reaction job should use the proxy's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  g.eval(`
var handler = () => {};
`);
  Promise.resolve(10)
    .then(new Proxy(new g.Proxy(new Proxy(g.handler, {}), {}), {}));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(() => {
  // The thenable job should use the `then` function's realm.
  Promise.resolve({
    then: () => {}
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The thenable job should use the `then` function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve(g.eval(`
({
  then: () => {}
});
`));

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the `then` function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: g.eval(`() => {}`),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the bound function's target function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: Function.prototype.bind.call(g.eval(`() => {}`), this),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the bound function's target function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: g.Function.prototype.bind.call(() => {}, g),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The thenable job should use the bound function's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: Function.prototype.bind.call(
      g.Function.prototype.bind.call(
        Function.prototype.bind.call(
          g.eval(`() => {}`),
          this),
        g),
      this)
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the bound function's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: g.Function.prototype.bind.call(
      Function.prototype.bind.call(
        g.Function.prototype.bind.call(
          () => {},
          g),
        this),
      g),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The thenable job should use the proxy's target function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: new Proxy(g.eval(`() => {}`), {}),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the proxy's target function's realm.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: new g.Proxy(() => {}, {}),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

testOne(newGlobalOptions => {
  // The thenable job should use the proxy's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: new Proxy(new g.Proxy(new Proxy(g.eval(`() => {}`), {}), {}), {}),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], g);
});

testOne(newGlobalOptions => {
  // The thenable job should use the proxy's target function's realm,
  // recursively.
  var g = newGlobal(newGlobalOptions);
  Promise.resolve({
    then: new g.Proxy(new Proxy(new g.Proxy(() => {}, {}), {}), {}),
  });

  var jobs = debugGetQueuedJobs();
  assertEq(jobs.length, 1);
  assertGlobal(jobs[0], globalThis);
});

print("ok");
