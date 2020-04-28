function neverCalled() {
  assertEq(true, false, "unexpected call");
}

const g = newGlobal();

assertEq(typeof String.prototype.replaceAll, "function");
assertEq(String.prototype.replaceAll.length, 2);
assertEq(String.prototype.replaceAll.name, "replaceAll");

// Throws if called with undefined or null.
assertThrowsInstanceOf(() => String.prototype.replaceAll.call(undefined), TypeError);
assertThrowsInstanceOf(() => String.prototype.replaceAll.call(null), TypeError);

// Throws if called with a non-global RegExp.
assertThrowsInstanceOf(() => "".replaceAll(/a/, ""), TypeError);
assertThrowsInstanceOf(() => "".replaceAll(g.RegExp(""), ""), TypeError);

// Also throws with RegExp-like objects.
assertThrowsInstanceOf(() => {
  "".replaceAll({[Symbol.match]: neverCalled, flags: ""}, "");
}, TypeError);

// |flags| property mustn't be undefined or null.
assertThrowsInstanceOf(() => {
  "".replaceAll({[Symbol.match]: neverCalled, flags: undefined}, "");
}, TypeError);
assertThrowsInstanceOf(() => {
  "".replaceAll({[Symbol.match]: neverCalled, flags: null}, "");
}, TypeError);

// Global RegExp (or RegExp-like) simply redirect to @@replace.
assertEq("aba".replace(/a/g, "c"), "cbc");
assertEq("aba".replace(g.RegExp("a", "g"), "c"), "cbc");
assertEq("aba".replace({
  [Symbol.match]: true,
  [Symbol.replace]: () => "ok",
  flags: "flags has 'g' character",
}, ""), "ok");

// Applies ToString on the replace-function return value.
assertEq("aa".replaceAll("a", () => ({toString(){ return 1; }})), "11");
assertEq("aa".replaceAll("a", () => ({valueOf(){ return 1; }})), "[object Object][object Object]");

const replacer = {
  "$$": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return "$";
  },
  "$$-$$": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return "$-$";
  },
  "$&": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return string.substring(position, position + searchString.length);
  },
  "$&-$&": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    var s = string.substring(position, position + searchString.length);
    return `${s}-${s}`;
  },
  "$`": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return string.substring(0, position);
  },
  "$`-$`": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    var s = string.substring(0, position);
    return `${s}-${s}`;
  },
  "$'": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return string.substring(position + searchString.length);
  },
  "$'-$'": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    var s = string.substring(position + searchString.length);
    return `${s}-${s}`;
  },
  "A": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return "A";
  },
  "A-B": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return "A-B";
  },
  "": function(searchString, position, string) {
    "use strict";
    assertEq(this, undefined);

    return "";
  },
};

// Tests when |pattern| is longer than |string|.
{
  const tests = [
    { string: "", pattern: "a" },
    { string: "a", pattern: "ab" },
    { string: "", pattern: "α" },
    { string: "α", pattern: "αβ" },
  ];

  for (let [replacementString, replacementFunction] of Object.entries(replacer)) {
    for (let {string, pattern} of tests) {
      let a = string.replaceAll(pattern, replacementString);
      let b = string.replaceAll(pattern, replacementFunction);
      let expected = string.replace(RegExp(pattern, "g"), replacementString);
      assertEq(a, expected);
      assertEq(b, expected);
      assertEq(expected, string);
    }
  }
}

// Tests when |pattern| doesn't match once.
 {
   const tests = [
    { string: "a", pattern: "A" },
    { string: "ab", pattern: "A" },
    { string: "ab", pattern: "AB" },

    { string: "α", pattern: "Γ" },
    { string: "αβ", pattern: "Γ" },
    { string: "αβ", pattern: "ΓΔ" },
  ];

  for (let [replacementString, replacementFunction] of Object.entries(replacer)) {
    for (let {string, pattern} of tests) {
      let a = string.replaceAll(pattern, replacementString);
      let b = string.replaceAll(pattern, replacementFunction);
      let expected = string.replace(RegExp(pattern, "g"), replacementString);
      assertEq(a, expected);
      assertEq(b, expected);
      assertEq(expected, string);
    }
  }
}

// Tests when |pattern| is the empty string.
{
  const strings = ["", "a", "ab", "α", "αβ"];
  const pattern = "";
  const re = /(?:)/g;

  for (let [replacementString, replacementFunction] of Object.entries(replacer)) {
    for (let string of strings) {
      let a = string.replaceAll(pattern, replacementString);
      let b = string.replaceAll(pattern, replacementFunction);
      let expected = string.replace(re, replacementString);
      assertEq(a, expected);
      assertEq(b, expected);
    }
  }
}

// Tests when |pattern| isn't the empty string.
{
  const tests = [
    {
      strings: [
        "a", "b",
        "aa", "ab", "ba", "bb",
        "aaa", "aab", "aba", "abb", "baa", "bab", "bba", "bbb",
      ],
      pattern: "a",
    },
    {
      strings: [
        "α", "β",
        "αα", "αβ", "βα", "ββ",
        "ααα", "ααβ", "αβα", "αββ", "βαα", "βαβ", "ββα", "βββ",
      ],
      pattern: "α",
    },
  ];

  for (let {strings, pattern} of tests) {
    let re = RegExp(pattern, "g");
    for (let [replacementString, replacementFunction] of Object.entries(replacer)) {
      for (let string of strings) {
        let a = string.replaceAll(pattern, replacementString);
        let b = string.replaceAll(pattern, replacementFunction);
        let expected = string.replace(re, replacementString);
        assertEq(a, expected);
        assertEq(b, expected);
      }
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
