function createSandbox() {
  const uri = Services.io.newURI("https://example.com");
  const principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    {}
  );
  return new Cu.Sandbox(principal, {});
}

add_task(async function testReactionJob() {
  const sandbox = createSandbox();

  sandbox.eval(`
var testPromise = Promise.resolve(10);
`);

  // Calling `Promise.prototype.then` from sandbox performs GetFunctionRealm
  // on wrapped `resolve` in sandbox realm, and it fails to unwrap the security
  // wrapper. The reaction job should be created with sandbox realm.
  const p = new Promise(resolve => {
    sandbox.resolve = resolve;

    sandbox.eval(`
testPromise.then(resolve);
`);
  });

  const result = await p;

  equal(result, 10);
});

add_task(async function testReactionJobNuked() {
  const sandbox = createSandbox();

  sandbox.eval(`
var testPromise = Promise.resolve(10);
`);

  // Calling `Promise.prototype.then` from sandbox performs GetFunctionRealm
  // on wrapped `resolve` in sandbox realm, and it fails to unwrap the security
  // wrapper. The reaction job should be created with sandbox realm.
  const p1 = new Promise(resolve => {
    sandbox.resolve = resolve;

    sandbox.eval(`
testPromise.then(resolve);
`);

    // Given the reaction job is created with the sandbox realm, nuking the
    // sandbox prevents the job gets executed.
    Cu.nukeSandbox(sandbox);
  });

  const p2 = Promise.resolve(11);

  // Given the p1 doesn't get resolved, p2 should win.
  const result = await Promise.race([p1, p2]);

  equal(result, 11);
});

add_task(async function testReactionJobWithXray() {
  const sandbox = createSandbox();

  sandbox.eval(`
var testPromise = Promise.resolve(10);
`);

  // Calling `Promise.prototype.then` from privileged realm via Xray uses
  // privileged `Promise.prototype.then` function, and GetFunctionRealm
  // performed there successfully gets top-level realm. The reaction job
  // should be created with top-level realm.
  const result = await new Promise(resolve => {
    sandbox.testPromise.then(resolve);

    // Given the reaction job is created with the top-level realm, nuking the
    // sandbox doesn't affect the reaction job.
    Cu.nukeSandbox(sandbox);
  });

  equal(result, 10);
});

add_task(async function testBoundReactionJob() {
  const sandbox = createSandbox();

  sandbox.eval(`
var resolve = undefined;
var callbackPromise = new Promise(r => { resolve = r; });
var callback = function (v) { resolve(v + 1); };
`);

  // Create a bound function where its realm is privileged realm, and
  // its target is from sandbox realm.
  sandbox.bound_callback = Function.prototype.bind.call(
    sandbox.callback,
    sandbox
  );

  // Calling `Promise.prototype.then` from sandbox performs GetFunctionRealm
  // and it fails. The reaction job should be created with sandbox realm.
  sandbox.eval(`
Promise.resolve(10).then(bound_callback);
`);

  const result = await sandbox.callbackPromise;
  equal(result, 11);
});

add_task(async function testThenableJob() {
  const sandbox = createSandbox();

  const p = new Promise(resolve => {
    // Create a bound function where its realm is privileged realm, and
    // its target is from sandbox realm.
    sandbox.then = function(onFulfilled, onRejected) {
      resolve(10);
    };
  });

  // Creating a promise thenable job in the following `Promise.resolve` performs
  // GetFunctionRealm on the bound thenable.then and fails. The reaction job
  // should be created with sandbox realm.
  sandbox.eval(`
var thenable = {
  then: then,
};

Promise.resolve(thenable);
`);

  const result = await p;
  equal(result, 10);
});

add_task(async function testThenableJobNuked() {
  const sandbox = createSandbox();

  let called = false;
  sandbox.then = function(onFulfilled, onRejected) {
    called = true;
  };

  // Creating a promise thenable job in the following `Promise.resolve` performs
  // GetFunctionRealm on the bound thenable.then and fails. The reaction job
  // should be created with sandbox realm.
  sandbox.eval(`
var thenable = {
  then: then,
};

Promise.resolve(thenable);
`);

  Cu.nukeSandbox(sandbox);

  // Drain the job queue, to make sure we hit dead object error inside the
  // thenable job.
  await Promise.resolve(10);

  equal(
    Services.console.getMessageArray().find(x => {
      return x.toString().includes("can't access dead object");
    }) !== undefined,
    true
  );
  equal(called, false);
});

add_task(async function testThenableJobAccessError() {
  const sandbox = createSandbox();

  let accessed = false;
  sandbox.thenable = {
    get then() {
      accessed = true;
    },
  };

  // The following operation silently fails when accessing `then` property.
  sandbox.eval(`
var x = typeof thenable.then;

Promise.resolve(thenable);
`);

  equal(accessed, false);
});

add_task(async function testBoundThenableJob() {
  const sandbox = createSandbox();

  sandbox.eval(`
var resolve = undefined;
var callbackPromise = new Promise(r => { resolve = r; });
var callback = function (v) { resolve(v + 1); };

var then = function(onFulfilled, onRejected) {
  onFulfilled(10);
};
`);

  // Create a bound function where its realm is privileged realm, and
  // its target is from sandbox realm.
  sandbox.bound_then = Function.prototype.bind.call(sandbox.then, sandbox);

  // Creating a promise thenable job in the following `Promise.resolve` performs
  // GetFunctionRealm on the bound thenable.then and fails. The reaction job
  // should be created with sandbox realm.
  sandbox.eval(`
var thenable = {
  then: bound_then,
};

Promise.resolve(thenable).then(callback);
`);

  const result = await sandbox.callbackPromise;
  equal(result, 11);
});
