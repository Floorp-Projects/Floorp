"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/RecipeRunner.jsm", this);

add_task(function*() {
  // Test that RecipeRunner can execute a basic recipe/action.
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
          resolve(this.recipe.foo);
        });
      }
    }

    registerAction('test-action', TestAction);
  `;

  const result = yield RecipeRunner.executeAction(recipe, {}, actionScript);
  is(result, "bar", "Recipe executed correctly");
});
