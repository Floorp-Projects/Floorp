/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests L10nCache in UrlbarUtils.jsm.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  L10nCache: "resource:///modules/UrlbarUtils.sys.mjs",
});

add_task(async function comprehensive() {
  // Set up a mock localization.
  let l10n = initL10n({
    args0a: "Zero args value",
    args0b: "Another zero args value",
    args1a: "One arg value is { $arg1 }",
    args1b: "Another one arg value is { $arg1 }",
    args2a: "Two arg values are { $arg1 } and { $arg2 }",
    args2b: "More two arg values are { $arg1 } and { $arg2 }",
    args3a: "Three arg values are { $arg1 }, { $arg2 }, and { $arg3 }",
    args3b: "More three arg values are { $arg1 }, { $arg2 }, and { $arg3 }",
    attrs1: [".label = attrs1 label has zero args"],
    attrs2: [
      ".label = attrs2 label has zero args",
      ".tooltiptext = attrs2 tooltiptext arg value is { $arg1 }",
    ],
    attrs3: [
      ".label = attrs3 label has zero args",
      ".tooltiptext = attrs3 tooltiptext arg value is { $arg1 }",
      ".alt = attrs3 alt arg values are { $arg1 } and { $arg2 }",
    ],
  });

  let tests = [
    // different strings with the same number of args and also the same strings
    // with different args
    {
      id: "args0a",
      expected: {
        value: "Zero args value",
        attributes: null,
      },
    },
    {
      id: "args0b",
      expected: {
        value: "Another zero args value",
        attributes: null,
      },
    },
    {
      id: "args1a",
      args: { arg1: "foo1" },
      expected: {
        value: "One arg value is foo1",
        attributes: null,
      },
    },
    {
      id: "args1a",
      args: { arg1: "foo2" },
      expected: {
        value: "One arg value is foo2",
        attributes: null,
      },
    },
    {
      id: "args1b",
      args: { arg1: "foo1" },
      expected: {
        value: "Another one arg value is foo1",
        attributes: null,
      },
    },
    {
      id: "args1b",
      args: { arg1: "foo2" },
      expected: {
        value: "Another one arg value is foo2",
        attributes: null,
      },
    },
    {
      id: "args2a",
      args: { arg1: "foo1", arg2: "bar1" },
      expected: {
        value: "Two arg values are foo1 and bar1",
        attributes: null,
      },
    },
    {
      id: "args2a",
      args: { arg1: "foo2", arg2: "bar2" },
      expected: {
        value: "Two arg values are foo2 and bar2",
        attributes: null,
      },
    },
    {
      id: "args2b",
      args: { arg1: "foo1", arg2: "bar1" },
      expected: {
        value: "More two arg values are foo1 and bar1",
        attributes: null,
      },
    },
    {
      id: "args2b",
      args: { arg1: "foo2", arg2: "bar2" },
      expected: {
        value: "More two arg values are foo2 and bar2",
        attributes: null,
      },
    },
    {
      id: "args3a",
      args: { arg1: "foo1", arg2: "bar1", arg3: "baz1" },
      expected: {
        value: "Three arg values are foo1, bar1, and baz1",
        attributes: null,
      },
    },
    {
      id: "args3a",
      args: { arg1: "foo2", arg2: "bar2", arg3: "baz2" },
      expected: {
        value: "Three arg values are foo2, bar2, and baz2",
        attributes: null,
      },
    },
    {
      id: "args3b",
      args: { arg1: "foo1", arg2: "bar1", arg3: "baz1" },
      expected: {
        value: "More three arg values are foo1, bar1, and baz1",
        attributes: null,
      },
    },
    {
      id: "args3b",
      args: { arg1: "foo2", arg2: "bar2", arg3: "baz2" },
      expected: {
        value: "More three arg values are foo2, bar2, and baz2",
        attributes: null,
      },
    },

    // two instances of the same string with their args swapped
    {
      id: "args2a",
      args: { arg1: "arg A", arg2: "arg B" },
      expected: {
        value: "Two arg values are arg A and arg B",
        attributes: null,
      },
    },
    {
      id: "args2a",
      args: { arg1: "arg B", arg2: "arg A" },
      expected: {
        value: "Two arg values are arg B and arg A",
        attributes: null,
      },
    },

    // strings with attributes
    {
      id: "attrs1",
      expected: {
        value: null,
        attributes: {
          label: "attrs1 label has zero args",
        },
      },
    },
    {
      id: "attrs2",
      expected: {
        value: null,
        attributes: {
          label: "attrs2 label has zero args",
          tooltiptext: "attrs2 tooltiptext arg value is {$arg1}",
        },
      },
    },
    {
      id: "attrs2",
      args: {
        arg1: "arg A",
      },
      expected: {
        value: null,
        attributes: {
          label: "attrs2 label has zero args",
          tooltiptext: "attrs2 tooltiptext arg value is arg A",
        },
      },
    },
    {
      id: "attrs3",
      args: {
        arg1: "arg A",
      },
      expected: {
        value: null,
        attributes: {
          label: "attrs3 label has zero args",
          tooltiptext: "attrs3 tooltiptext arg value is arg A",
          alt: "attrs3 alt arg values are arg A and {$arg2}",
        },
      },
    },
    {
      id: "attrs3",
      args: {
        arg1: "arg A",
        arg2: "arg B",
      },
      expected: {
        value: null,
        attributes: {
          label: "attrs3 label has zero args",
          tooltiptext: "attrs3 tooltiptext arg value is arg A",
          alt: "attrs3 alt arg values are arg A and arg B",
        },
      },
    },
  ];

  let cache = new L10nCache(l10n);

  // Get some non-cached strings.
  Assert.ok(!cache.get("uncached1"), "Uncached string 1");
  Assert.ok(!cache.get("uncached2", { args: "foo" }), "Uncached string 2");

  // Add each test string and get it back.
  for (let { id, args, expected } of tests) {
    await cache.add(id, args);
    let message = cache.get(id, args);
    Assert.deepEqual(
      message,
      expected,
      `Expected message for id=${id} args=${JSON.stringify(args)}`
    );
  }

  // Get each string again to make sure each add didn't somehow mess up the
  // previously added strings.
  for (let { id, args, expected } of tests) {
    Assert.deepEqual(
      cache.get(id, args),
      expected,
      `Expected message for id=${id} args=${JSON.stringify(args)}`
    );
  }

  // Delete some of the strings. We'll delete every other one to mix it up.
  for (let i = 0; i < tests.length; i++) {
    if (i % 2 == 0) {
      let { id, args } = tests[i];
      cache.delete(id, args);
      Assert.ok(
        !cache.get(id, args),
        `Deleted from cache: id=${id} args=${JSON.stringify(args)}`
      );
    }
  }

  // Get each remaining string.
  for (let i = 0; i < tests.length; i++) {
    if (i % 2 != 0) {
      let { id, args, expected } = tests[i];
      Assert.deepEqual(
        cache.get(id, args),
        expected,
        `Expected message for id=${id} args=${JSON.stringify(args)}`
      );
    }
  }

  // Clear the cache.
  cache.clear();
  for (let { id, args } of tests) {
    Assert.ok(
      !cache.get(id, args),
      `After cache clear: id=${id} args=${JSON.stringify(args)}`
    );
  }

  // `ensure` each test string and get it back.
  for (let { id, args, expected } of tests) {
    await cache.ensure(id, args);
    let message = cache.get(id, args);
    Assert.deepEqual(
      message,
      expected,
      `Expected message for id=${id} args=${JSON.stringify(args)}`
    );

    // Call `ensure` again. This time, `add` should not be called.
    let originalAdd = cache.add;
    cache.add = () => Assert.ok(false, "add erroneously called");
    await cache.ensure(id, args);
    cache.add = originalAdd;
  }

  // Clear the cache again.
  cache.clear();
  for (let { id, args } of tests) {
    Assert.ok(
      !cache.get(id, args),
      `After cache clear: id=${id} args=${JSON.stringify(args)}`
    );
  }

  // `ensureAll` the test strings and get them back.
  let idArgs = tests.map(({ id, args }) => ({ id, args }));
  await cache.ensureAll(idArgs);
  for (let { id, args, expected } of tests) {
    let message = cache.get(id, args);
    Assert.deepEqual(
      message,
      expected,
      `Expected message for id=${id} args=${JSON.stringify(args)}`
    );
  }

  // Ensure the cache is cleared after the app locale changes
  Assert.greater(cache.size(), 0, "The cache has messages in it.");
  Services.obs.notifyObservers(null, "intl:app-locales-changed");
  await l10n.ready;
  Assert.equal(cache.size(), 0, "The cache is empty on app locale change");
});

/**
 * Sets up a mock localization.
 *
 * @param {object} pairs
 *   Fluent strings as key-value pairs.
 * @returns {Localization}
 *   The mock Localization object.
 */
function initL10n(pairs) {
  let source = Object.entries(pairs)
    .map(([key, value]) => {
      if (Array.isArray(value)) {
        value = value.map(s => "  \n" + s).join("");
      }
      return `${key} = ${value}`;
    })
    .join("\n");
  let registry = new L10nRegistry();
  registry.registerSources([
    L10nFileSource.createMock(
      "test",
      "app",
      ["en-US"],
      "/localization/{locale}",
      [{ source, path: "/localization/en-US/test.ftl" }]
    ),
  ]);
  return new Localization(["/test.ftl"], true, registry, ["en-US"]);
}
