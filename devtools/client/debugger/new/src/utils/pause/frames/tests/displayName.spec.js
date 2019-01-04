/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import {
  formatCopyName,
  formatDisplayName,
  simplifyDisplayName
} from "../displayName";

describe("formatCopyName", () => {
  it("simple", () => {
    const frame = {
      displayName: "child",
      location: {
        line: 12
      },
      source: {
        url: "todo-view.js"
      }
    };

    expect(formatCopyName(frame)).toEqual("child (todo-view.js#12)");
  });
});

describe("formatting display names", () => {
  it("uses a library description", () => {
    const frame = {
      library: "Backbone",
      displayName: "extend/child",
      source: {
        url: "assets/backbone.js"
      }
    };

    expect(formatDisplayName(frame)).toEqual("Create Class");
  });

  it("shortens an anonymous function", () => {
    const frame = {
      displayName: "extend/child/bar/baz",
      source: {
        url: "assets/bar.js"
      }
    };

    expect(formatDisplayName(frame)).toEqual("baz");
  });

  it("does not truncates long function names", () => {
    const frame = {
      displayName: "bazbazbazbazbazbazbazbazbazbazbazbazbaz",
      source: {
        url: "assets/bar.js"
      }
    };

    expect(formatDisplayName(frame)).toEqual(
      "bazbazbazbazbazbazbazbazbazbazbazbazbaz"
    );
  });

  it("returns the original function name when present", () => {
    const frame = {
      originalDisplayName: "originalFn",
      displayName: "fn",
      source: {
        url: "entry.js"
      }
    };

    expect(formatDisplayName(frame)).toEqual("originalFn");
  });

  it("returns anonymous when displayName is undefined", () => {
    const frame = {};
    expect(formatDisplayName(frame, undefined, L10N)).toEqual("<anonymous>");
  });

  it("returns anonymous when displayName is null", () => {
    const frame = {
      displayName: null
    };
    expect(formatDisplayName(frame, undefined, L10N)).toEqual("<anonymous>");
  });

  it("returns anonymous when displayName is an empty string", () => {
    const frame = {
      displayName: ""
    };
    expect(formatDisplayName(frame, undefined, L10N)).toEqual("<anonymous>");
  });
});

describe("simplifying display names", () => {
  const cases = {
    defaultCase: [["define", "define"]],

    objectProperty: [
      ["z.foz", "foz"],
      ["z.foz/baz", "baz"],
      ["z.foz/baz/y.bay", "bay"],
      ["outer/x.fox.bax.nx", "nx"],
      ["outer/fow.baw", "baw"],
      ["fromYUI._attach", "_attach"],
      ["Y.ClassNameManager</getClassName", "getClassName"],
      ["orion.textview.TextView</addHandler", "addHandler"],
      ["this.eventPool_.createObject", "createObject"]
    ],

    arrayProperty: [
      ["this.eventPool_[createObject]", "createObject"],
      ["jQuery.each(^)/jQuery.fn[o]", "o"],
      ["viewport[get+D]", "get+D"],
      ["arr[0]", "0"]
    ],

    functionProperty: [
      ["fromYUI._attach/<.", "_attach"],
      ["Y.ClassNameManager<", "ClassNameManager"],
      ["fromExtJS.setVisible/cb<", "cb"],
      ["fromDojo.registerWin/<", "registerWin"]
    ],

    annonymousProperty: [["jQuery.each(^)", "each"]]
  };

  Object.keys(cases).forEach(type => {
    cases[type].forEach(([kase, expected]) => {
      it(`${type} - ${kase}`, () =>
        expect(simplifyDisplayName(kase)).toEqual(expected));
    });
  });
});
