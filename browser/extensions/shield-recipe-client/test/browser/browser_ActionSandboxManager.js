"use strict";

Cu.import("resource://shield-recipe-client/lib/ActionSandboxManager.jsm", this);
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm", this);

async function withManager(script, testFunction) {
  const manager = new ActionSandboxManager(script);
  manager.addHold("testing");
  await testFunction(manager);
  manager.removeHold("testing");
}

add_task(async function testMissingCallbackName() {
  await withManager("1 + 1", async manager => {
    is(
      await manager.runAsyncCallback("missingCallback"),
      undefined,
      "runAsyncCallback returns undefined when given a missing callback name",
    );
  });
});

add_task(async function testCallback() {
  const script = `
    registerAsyncCallback("testCallback", async function(normandy) {
      return 5;
    });
  `;

  await withManager(script, async manager => {
    const result = await manager.runAsyncCallback("testCallback");
    is(result, 5, "runAsyncCallback executes the named callback inside the sandbox");
  });
});

add_task(async function testArguments() {
  const script = `
    registerAsyncCallback("testCallback", async function(normandy, a, b) {
      return a + b;
    });
  `;

  await withManager(script, async manager => {
    const result = await manager.runAsyncCallback("testCallback", 4, 6);
    is(result, 10, "runAsyncCallback passes arguments to the callback");
  });
});

add_task(async function testCloning() {
  const script = `
    registerAsyncCallback("testCallback", async function(normandy, obj) {
      return {foo: "bar", baz: obj.baz};
    });
  `;

  await withManager(script, async manager => {
    const result = await manager.runAsyncCallback("testCallback", {baz: "biff"});

    Assert.deepEqual(
      result,
      {foo: "bar", baz: "biff"},
      (
        "runAsyncCallback clones arguments into the sandbox and return values into the " +
        "context it was called from"
      ),
    );
  });
});

add_task(async function testError() {
  const script = `
    registerAsyncCallback("testCallback", async function(normandy) {
      throw new Error("WHY")
    });
  `;

  await withManager(script, async manager => {
    try {
      await manager.runAsyncCallback("testCallback");
      ok(false, "runAsnycCallbackFromScript throws errors when raised by the sandbox");
    } catch (err) {
      is(err.message, "WHY", "runAsnycCallbackFromScript throws errors when raised by the sandbox");
    }
  });
});

add_task(async function testDriver() {
  // The value returned by runAsyncCallback is cloned without the cloneFunctions
  // option, so we can't inspect the driver itself since its methods will not be
  // present. Instead, we inspect the properties on it available to the sandbox.
  const script = `
    registerAsyncCallback("testCallback", async function(normandy) {
      return Object.keys(normandy);
    });
  `;

  await withManager(script, async manager => {
    const sandboxDriverKeys = await manager.runAsyncCallback("testCallback");
    const referenceDriver = new NormandyDriver(manager);
    for (const prop of Object.keys(referenceDriver)) {
      ok(sandboxDriverKeys.includes(prop), `runAsyncCallback's driver has the "${prop}" property.`);
    }
  });
});

add_task(async function testGlobalObject() {
  // Test that window is an alias for the global object, and that it
  // has some expected functions available on it.
  const script = `
    window.setOnWindow = "set";
    this.setOnGlobal = "set";

    registerAsyncCallback("testCallback", async function(normandy) {
      return {
        setOnWindow: setOnWindow,
        setOnGlobal: window.setOnGlobal,
        setTimeoutExists: setTimeout !== undefined,
        clearTimeoutExists: clearTimeout !== undefined,
      };
    });
  `;

  await withManager(script, async manager => {
    const result = await manager.runAsyncCallback("testCallback");
    Assert.deepEqual(result, {
      setOnWindow: "set",
      setOnGlobal: "set",
      setTimeoutExists: true,
      clearTimeoutExists: true,
    }, "sandbox.window is the global object and has expected functions.");
  });
});

add_task(async function testRegisterActionShim() {
  const recipe = {
    foo: "bar",
  };
  const script = `
    class TestAction {
      constructor(driver, recipe) {
        this.driver = driver;
        this.recipe = recipe;
      }

      execute() {
        return new Promise(resolve => {
          resolve({
            foo: this.recipe.foo,
            isDriver: "log" in this.driver,
          });
        });
      }
    }

    registerAction('test-action', TestAction);
  `;

  await withManager(script, async manager => {
    const result = await manager.runAsyncCallback("action", recipe);
    is(result.foo, "bar", "registerAction registers an async callback for actions");
    is(
      result.isDriver,
      true,
      "registerAction passes the driver to the action class constructor",
    );
  });
});
