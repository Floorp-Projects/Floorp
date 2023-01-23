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
      obj: {
        id: "args0a",
      },
      expected: {
        value: "Zero args value",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args0b",
      },
      expected: {
        value: "Another zero args value",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args1a",
        args: { arg1: "foo1" },
      },
      expected: {
        value: "One arg value is foo1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args1a",
        args: { arg1: "foo2" },
      },
      expected: {
        value: "One arg value is foo2",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args1b",
        args: { arg1: "foo1" },
      },
      expected: {
        value: "Another one arg value is foo1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args1b",
        args: { arg1: "foo2" },
      },
      expected: {
        value: "Another one arg value is foo2",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args2a",
        args: { arg1: "foo1", arg2: "bar1" },
      },
      expected: {
        value: "Two arg values are foo1 and bar1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args2a",
        args: { arg1: "foo2", arg2: "bar2" },
      },
      expected: {
        value: "Two arg values are foo2 and bar2",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args2b",
        args: { arg1: "foo1", arg2: "bar1" },
      },
      expected: {
        value: "More two arg values are foo1 and bar1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args2b",
        args: { arg1: "foo2", arg2: "bar2" },
      },
      expected: {
        value: "More two arg values are foo2 and bar2",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args3a",
        args: { arg1: "foo1", arg2: "bar1", arg3: "baz1" },
      },
      expected: {
        value: "Three arg values are foo1, bar1, and baz1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args3a",
        args: { arg1: "foo2", arg2: "bar2", arg3: "baz2" },
      },
      expected: {
        value: "Three arg values are foo2, bar2, and baz2",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args3b",
        args: { arg1: "foo1", arg2: "bar1", arg3: "baz1" },
      },
      expected: {
        value: "More three arg values are foo1, bar1, and baz1",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args3b",
        args: { arg1: "foo2", arg2: "bar2", arg3: "baz2" },
      },
      expected: {
        value: "More three arg values are foo2, bar2, and baz2",
        attributes: null,
      },
    },

    // two instances of the same string with their args swapped
    {
      obj: {
        id: "args2a",
        args: { arg1: "arg A", arg2: "arg B" },
      },
      expected: {
        value: "Two arg values are arg A and arg B",
        attributes: null,
      },
    },
    {
      obj: {
        id: "args2a",
        args: { arg1: "arg B", arg2: "arg A" },
      },
      expected: {
        value: "Two arg values are arg B and arg A",
        attributes: null,
      },
    },

    // strings with attributes
    {
      obj: {
        id: "attrs1",
      },
      expected: {
        value: null,
        attributes: {
          label: "attrs1 label has zero args",
        },
      },
    },
    {
      obj: {
        id: "attrs2",
        args: {
          arg1: "arg A",
        },
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
      obj: {
        id: "attrs3",
        args: {
          arg1: "arg A",
          arg2: "arg B",
        },
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
  Assert.ok(!cache.get({ id: "uncached1" }), "Uncached string 1");
  Assert.ok(!cache.get({ id: "uncached2", args: "foo" }), "Uncached string 2");

  // Add each test string and get it back.
  for (let { obj, expected } of tests) {
    await cache.add(obj);
    let message = cache.get(obj);
    Assert.deepEqual(
      message,
      expected,
      "Expected message for obj: " + JSON.stringify(obj)
    );
  }

  // Get each string again to make sure each add didn't somehow mess up the
  // previously added strings.
  for (let { obj, expected } of tests) {
    Assert.deepEqual(
      cache.get(obj),
      expected,
      "Expected message for obj: " + JSON.stringify(obj)
    );
  }

  // Delete some of the strings. We'll delete every other one to mix it up.
  for (let i = 0; i < tests.length; i++) {
    if (i % 2 == 0) {
      let { obj } = tests[i];
      cache.delete(obj);
      Assert.ok(!cache.get(obj), "Deleted from cache: " + JSON.stringify(obj));
    }
  }

  // Get each remaining string.
  for (let i = 0; i < tests.length; i++) {
    if (i % 2 != 0) {
      let { obj, expected } = tests[i];
      Assert.deepEqual(
        cache.get(obj),
        expected,
        "Expected message for obj: " + JSON.stringify(obj)
      );
    }
  }

  // Clear the cache.
  cache.clear();
  for (let { obj } of tests) {
    Assert.ok(!cache.get(obj), "After cache clear: " + JSON.stringify(obj));
  }

  // `ensure` each test string and get it back.
  for (let { obj, expected } of tests) {
    await cache.ensure(obj);
    let message = cache.get(obj);
    Assert.deepEqual(
      message,
      expected,
      "Expected message for obj: " + JSON.stringify(obj)
    );

    // Call `ensure` again. This time, `add` should not be called.
    let originalAdd = cache.add;
    cache.add = () => Assert.ok(false, "add erroneously called");
    await cache.ensure(obj);
    cache.add = originalAdd;
  }

  // Clear the cache again.
  cache.clear();
  for (let { obj } of tests) {
    Assert.ok(!cache.get(obj), "After cache clear: " + JSON.stringify(obj));
  }

  // `ensureAll` the test strings and get them back.
  let objects = tests.map(({ obj }) => obj);
  await cache.ensureAll(objects);
  for (let { obj, expected } of tests) {
    let message = cache.get(obj);
    Assert.deepEqual(
      message,
      expected,
      "Expected message for obj: " + JSON.stringify(obj)
    );
  }

  // Ensure the cache is cleared after the app locale changes
  Assert.greater(cache.size(), 0, "The cache has messages in it.");
  Services.obs.notifyObservers(null, "intl:app-locales-changed");
  await l10n.ready;
  Assert.equal(cache.size(), 0, "The cache is empty on app locale change");
});

// Tests the `excludeArgsFromCacheKey` option.
add_task(async function excludeArgsFromCacheKey() {
  // Set up a mock localization.
  let l10n = initL10n({
    args0: "Zero args value",
    args1: "One arg value is { $arg1 }",
    attrs0: [".label = attrs0 label has zero args"],
    attrs1: [
      ".label = attrs1 label has zero args",
      ".tooltiptext = attrs1 tooltiptext arg value is { $arg1 }",
    ],
  });

  let cache = new L10nCache(l10n);

  let tests = [
    // string with no args and no attributes
    {
      add: {
        id: "args0",
        excludeArgsFromCacheKey: true,
      },
      gets: [
        {
          obj: { id: "args0" },
          expected: {
            value: "Zero args value",
            attributes: null,
          },
        },
        {
          obj: { id: "args0", excludeArgsFromCacheKey: true },
          expected: {
            value: "Zero args value",
            attributes: null,
          },
        },
      ],
    },

    // string with one arg and no attributes
    {
      add: {
        id: "args1",
        args: { arg1: "foo" },
        excludeArgsFromCacheKey: true,
      },
      gets: [
        {
          obj: { id: "args1" },
          expected: {
            value: "One arg value is foo",
            attributes: null,
          },
        },
        {
          obj: { id: "args1", excludeArgsFromCacheKey: true },
          expected: {
            value: "One arg value is foo",
            attributes: null,
          },
        },
        {
          obj: {
            id: "args1",
            args: { arg1: "some other value" },
            excludeArgsFromCacheKey: true,
          },
          expected: {
            value: "One arg value is foo",
            attributes: null,
          },
        },
        {
          obj: {
            id: "args1",
            args: { arg1: "some other value" },
          },
          expected: undefined,
        },
      ],
    },

    // string with no args and one attribute
    {
      add: {
        id: "attrs0",
        excludeArgsFromCacheKey: true,
      },
      gets: [
        {
          obj: { id: "attrs0" },
          expected: {
            value: null,
            attributes: {
              label: "attrs0 label has zero args",
            },
          },
        },
        {
          obj: { id: "attrs0", excludeArgsFromCacheKey: true },
          expected: {
            value: null,
            attributes: {
              label: "attrs0 label has zero args",
            },
          },
        },
      ],
    },

    // string with one arg and two attributes
    {
      add: {
        id: "attrs1",
        args: { arg1: "foo" },
        excludeArgsFromCacheKey: true,
      },
      gets: [
        {
          obj: { id: "attrs1" },
          expected: {
            value: null,
            attributes: {
              label: "attrs1 label has zero args",
              tooltiptext: "attrs1 tooltiptext arg value is foo",
            },
          },
        },
        {
          obj: { id: "attrs1", excludeArgsFromCacheKey: true },
          expected: {
            value: null,
            attributes: {
              label: "attrs1 label has zero args",
              tooltiptext: "attrs1 tooltiptext arg value is foo",
            },
          },
        },
        {
          obj: {
            id: "attrs1",
            args: { arg1: "some other value" },
            excludeArgsFromCacheKey: true,
          },
          expected: {
            value: null,
            attributes: {
              label: "attrs1 label has zero args",
              tooltiptext: "attrs1 tooltiptext arg value is foo",
            },
          },
        },
        {
          obj: {
            id: "attrs1",
            args: { arg1: "some other value" },
          },
          expected: undefined,
        },
      ],
    },
  ];

  for (let { add, gets } of tests) {
    info("Adding to cache: " + JSON.stringify(add));
    await cache.add(add);
    for (let { obj, expected } of gets) {
      Assert.deepEqual(
        cache.get(obj),
        expected,
        "Expected message for get: " + JSON.stringify(obj)
      );
    }
  }
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
