"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);

add_task(function* execute() {
  // Test that RecipeRunner can execute a basic recipe/action and return
  // the result of execute.
  const recipe = {
    foo: "bar",
  };
  const actionScript = `
    class TestAction {
      constructor(driver, recipe) {
        this.recipe = recipe;
      }

      execute() {
        return new Promise(resolve => {
          resolve({foo: this.recipe.foo});
        });
      }
    }

    registerAction('test-action', TestAction);
  `;

  const result = yield RecipeRunner.executeAction(recipe, {}, actionScript);
  is(result.foo, "bar", "Recipe executed correctly");
});

add_task(function* error() {
  // Test that RecipeRunner rejects with error messages from within the
  // sandbox.
  const actionScript = `
    class TestAction {
      execute() {
        return new Promise((resolve, reject) => {
          reject(new Error("ERROR MESSAGE"));
        });
      }
    }

    registerAction('test-action', TestAction);
  `;

  let gotException = false;
  try {
    yield RecipeRunner.executeAction({}, {}, actionScript);
  } catch (err) {
    gotException = true;
    is(err.message, "ERROR MESSAGE", "RecipeRunner throws errors from the sandbox correctly.");
  }
  ok(gotException, "RecipeRunner threw an error from the sandbox.");
});

add_task(function* globalObject() {
  // Test that window is an alias for the global object, and that it
  // has some expected functions available on it.
  const actionScript = `
    window.setOnWindow = "set";
    this.setOnGlobal = "set";

    class TestAction {
      execute() {
        return new Promise(resolve => {
          resolve({
            setOnWindow: setOnWindow,
            setOnGlobal: window.setOnGlobal,
            setTimeoutExists: setTimeout !== undefined,
            clearTimeoutExists: clearTimeout !== undefined,
          });
        });
      }
    }

    registerAction('test-action', TestAction);
  `;

  const result = yield RecipeRunner.executeAction({}, {}, actionScript);
  Assert.deepEqual(result, {
    setOnWindow: "set",
    setOnGlobal: "set",
    setTimeoutExists: true,
    clearTimeoutExists: true,
  }, "sandbox.window is the global object and has expected functions.");
});
