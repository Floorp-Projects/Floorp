/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* This file will test the parameters parsing and validation directly through
   the PoliciesValidator API.
 */

const { PoliciesValidator } = Cu.import("resource:///modules/policies/PoliciesValidator.jsm", {});

add_task(async function test_boolean_values() {
  let schema = {
    type: "boolean"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters(true, schema);
  ok(valid && parsed === true, "Parsed boolean value correctly");

  [valid, parsed] = PoliciesValidator.validateAndParseParameters(false, schema);
  ok(valid && parsed === false, "Parsed boolean value correctly");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters("0", schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters("true", schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters(undefined, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_number_values() {
  let schema = {
    type: "number"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed === 1, "Parsed number value correctly");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters("1", schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_integer_values() {
  // Integer is an alias for number
  let schema = {
    type: "integer"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed == 1, "Parsed integer value correctly");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters("1", schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_string_values() {
  let schema = {
    type: "string"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters("foobar", schema);
  ok(valid && parsed == "foobar", "Parsed string value correctly");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters(1, schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters(true, schema)[0], "No type coercion");
  ok(!PoliciesValidator.validateAndParseParameters(undefined, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!PoliciesValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_URL_values() {
  let schema = {
    type: "URL"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters("https://www.example.com/foo#bar", schema);
  ok(valid, "URL is valid");
  ok(parsed instanceof Ci.nsIURI, "parsed is a nsIURI");
  is(parsed.prePath, "https://www.example.com", "prePath is correct");
  is(parsed.pathQueryRef, "/foo#bar", "pathQueryRef is correct");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters("www.example.com", schema)[0], "Scheme is required for URL");
  ok(!PoliciesValidator.validateAndParseParameters("https://:!$%", schema)[0], "Invalid URL");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
});

add_task(async function test_origin_values() {
  // Origin is a URL that doesn't contain a path/query string (i.e., it's only scheme + host + port)
  let schema = {
    type: "origin"
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters("https://www.example.com", schema);
  ok(valid, "Origin is valid");
  ok(parsed instanceof Ci.nsIURI, "parsed is a nsIURI");
  is(parsed.prePath, "https://www.example.com", "prePath is correct");
  is(parsed.pathQueryRef, "/", "pathQueryRef is corect");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters("https://www.example.com/foobar", schema)[0], "Origin cannot contain a path part");
  ok(!PoliciesValidator.validateAndParseParameters("https://:!$%", schema)[0], "Invalid origin");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
});

add_task(async function test_array_values() {
  // The types inside an array object must all be the same
  let schema = {
    type: "array",
    items: {
      type: "number"
    }
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters([1, 2, 3], schema);
  ok(valid, "Array is valid");
  ok(Array.isArray(parsed), "parsed is an array");
  is(parsed.length, 3, "array is correct");

  // An empty array is also valid
  [valid, parsed] = PoliciesValidator.validateAndParseParameters([], schema);
  ok(valid, "Array is valid");
  ok(Array.isArray(parsed), "parsed is an array");
  is(parsed.length, 0, "array is correct");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters([1, true, 3], schema)[0], "Mixed types");
  ok(!PoliciesValidator.validateAndParseParameters(2, schema)[0], "Type is correct but not in an array");
  ok(!PoliciesValidator.validateAndParseParameters({}, schema)[0], "Object is not an array");
});

add_task(async function test_object_values() {
  let schema = {
    type: "object",
    properties: {
      url: {
        type: "URL"
      },
      title: {
        type: "string"
      }
    }
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters(
    {
      url: "https://www.example.com/foo#bar",
      title: "Foo",
      alias: "Bar"
    },
    schema);

  ok(valid, "Object is valid");
  ok(typeof(parsed) == "object", "parsed in an object");
  ok(parsed.url instanceof Ci.nsIURI, "types inside the object are also parsed");
  is(parsed.url.spec, "https://www.example.com/foo#bar", "URL was correctly parsed");
  is(parsed.title, "Foo", "title was correctly parsed");
  is(parsed.alias, undefined, "property not described in the schema is not present in the parsed object");

  // Invalid values:
  ok(!PoliciesValidator.validateAndParseParameters(
    {
      url: "https://www.example.com/foo#bar",
      title: 3,
    },
    schema)[0], "Mismatched type for title");

  ok(!PoliciesValidator.validateAndParseParameters(
    {
      url: "www.example.com",
      title: 3,
    },
    schema)[0], "Invalid URL inside the object");
});

add_task(async function test_array_of_objects() {
  // This schema is used, for example, for bookmarks
  let schema = {
    type: "array",
    items: {
      type: "object",
      properties: {
        url: {
          type: "URL",
        },
        title: {
          type: "string"
        }
      }
    }
  };

  let valid, parsed;
  [valid, parsed] = PoliciesValidator.validateAndParseParameters(
    [{
      url: "https://www.example.com/bookmark1",
      title: "Foo",
    },
    {
      url: "https://www.example.com/bookmark2",
      title: "Bar",
    }],
    schema);

  ok(valid, "Array is valid");
  is(parsed.length, 2, "Correct number of items");

  ok(typeof(parsed[0]) == "object" && typeof(parsed[1]) == "object", "Correct objects inside array");

  is(parsed[0].url.spec, "https://www.example.com/bookmark1", "Correct URL for bookmark 1");
  is(parsed[1].url.spec, "https://www.example.com/bookmark2", "Correct URL for bookmark 2");

  is(parsed[0].title, "Foo", "Correct title for bookmark 1");
  is(parsed[1].title, "Bar", "Correct title for bookmark 2");
});
