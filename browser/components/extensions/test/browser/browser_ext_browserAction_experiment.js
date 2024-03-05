/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let fooExperimentAPIs = {
  foo: {
    schema: "schema.json",
    parent: {
      scopes: ["addon_parent"],
      script: "parent.js",
      paths: [["experiments", "foo", "parent"]],
    },
    child: {
      scopes: ["addon_child"],
      script: "child.js",
      paths: [["experiments", "foo", "child"]],
    },
  },
};

let fooExperimentFiles = {
  "schema.json": JSON.stringify([
    {
      namespace: "experiments.foo",
      types: [
        {
          id: "Meh",
          type: "object",
          properties: {},
        },
      ],
      functions: [
        {
          name: "parent",
          type: "function",
          async: true,
          parameters: [],
        },
        {
          name: "child",
          type: "function",
          parameters: [],
          returns: { type: "string" },
        },
      ],
    },
  ]),

  /* globals ExtensionAPI */
  "parent.js": () => {
    this.foo = class extends ExtensionAPI {
      getAPI() {
        return {
          experiments: {
            foo: {
              parent() {
                return Promise.resolve("parent");
              },
            },
          },
        };
      }
    };
  },

  "child.js": () => {
    this.foo = class extends ExtensionAPI {
      getAPI() {
        return {
          experiments: {
            foo: {
              child() {
                return "child";
              },
            },
          },
        };
      }
    };
  },
};

async function testFooExperiment() {
  browser.test.assertEq(
    "object",
    typeof browser.experiments,
    "typeof browser.experiments"
  );

  browser.test.assertEq(
    "object",
    typeof browser.experiments.foo,
    "typeof browser.experiments.foo"
  );

  browser.test.assertEq(
    "function",
    typeof browser.experiments.foo.child,
    "typeof browser.experiments.foo.child"
  );

  browser.test.assertEq(
    "function",
    typeof browser.experiments.foo.parent,
    "typeof browser.experiments.foo.parent"
  );

  browser.test.assertEq(
    "child",
    browser.experiments.foo.child(),
    "foo.child()"
  );

  browser.test.assertEq(
    "parent",
    await browser.experiments.foo.parent(),
    "await foo.parent()"
  );
}

add_task(async function test_browseraction_with_experiment() {
  async function background() {
    await new Promise(resolve =>
      browser.browserAction.onClicked.addListener(resolve)
    );
    browser.test.log("Got browserAction.onClicked");

    await testFooExperiment();

    browser.test.notifyPass("background-browserAction-experiments.foo");
  }

  let extension = ExtensionTestUtils.loadExtension({
    isPrivileged: true,

    manifest: {
      browser_action: {
        default_area: "navbar",
      },

      experiment_apis: fooExperimentAPIs,
    },

    background: `
      ${testFooExperiment}
      (${background})();
    `,

    files: fooExperimentFiles,
  });

  await extension.startup();

  await clickBrowserAction(extension, window);

  await extension.awaitFinish("background-browserAction-experiments.foo");

  await extension.unload();
});
