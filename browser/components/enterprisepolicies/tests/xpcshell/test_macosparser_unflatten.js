/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let { macOSPoliciesParser } = ChromeUtils.importESModule(
  "resource://gre/modules/policies/macOSPoliciesParser.sys.mjs"
);

add_task(async function test_object_unflatten() {
  // Note: these policies are just examples and they won't actually
  // run through the policy engine on this test. We're just testing
  // that the unflattening algorithm produces the correct output.
  let input = {
    DisplayBookmarksToolbar: true,

    Homepage__URL: "https://www.mozilla.org",
    Homepage__Locked: "true",
    Homepage__Additional__0: "https://extra-homepage-1.example.com",
    Homepage__Additional__1: "https://extra-homepage-2.example.com",

    WebsiteFilter__Block__0: "*://*.example.org/*",
    WebsiteFilter__Block__1: "*://*.example.net/*",
    WebsiteFilter__Exceptions__0: "*://*.example.org/*exception*",

    Permissions__Camera__Allow__0: "https://www.example.com",

    Permissions__Notifications__Allow__0: "https://www.example.com",
    Permissions__Notifications__Allow__1: "https://www.example.org",
    Permissions__Notifications__Block__0: "https://www.example.net",

    Permissions__Notifications__BlockNewRequests: true,
    Permissions__Notifications__Locked: true,

    Bookmarks__0__Title: "Bookmark 1",
    Bookmarks__0__URL: "https://bookmark1.example.com",

    Bookmarks__1__Title: "Bookmark 2",
    Bookmarks__1__URL: "https://bookmark2.example.com",
    Bookmarks__1__Folder: "Folder",
  };

  let expected = {
    DisplayBookmarksToolbar: true,

    Homepage: {
      URL: "https://www.mozilla.org",
      Locked: "true",
      Additional: [
        "https://extra-homepage-1.example.com",
        "https://extra-homepage-2.example.com",
      ],
    },

    WebsiteFilter: {
      Block: ["*://*.example.org/*", "*://*.example.net/*"],
      Exceptions: ["*://*.example.org/*exception*"],
    },

    Permissions: {
      Camera: {
        Allow: ["https://www.example.com"],
      },

      Notifications: {
        Allow: ["https://www.example.com", "https://www.example.org"],
        Block: ["https://www.example.net"],
        BlockNewRequests: true,
        Locked: true,
      },
    },

    Bookmarks: [
      {
        Title: "Bookmark 1",
        URL: "https://bookmark1.example.com",
      },
      {
        Title: "Bookmark 2",
        URL: "https://bookmark2.example.com",
        Folder: "Folder",
      },
    ],
  };

  let unflattened = macOSPoliciesParser.unflatten(input);

  deepEqual(unflattened, expected, "Input was unflattened correctly.");
});

add_task(async function test_array_unflatten() {
  let input = {
    Foo__1: 1,
    Foo__5: 5,
    Foo__10: 10,
    Foo__30: 30,
    Foo__51: 51, // This one should not be included as the limit is 50
  };

  let unflattened = macOSPoliciesParser.unflatten(input);
  equal(unflattened.Foo.length, 31, "Array size is correct");

  let expected = {
    Foo: [, 1, , , , 5], // eslint-disable-line no-sparse-arrays
  };
  expected.Foo[10] = 10;
  expected.Foo[30] = 30;

  deepEqual(unflattened, expected, "Array was unflattened correctly.");
});
