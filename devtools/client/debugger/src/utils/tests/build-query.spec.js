/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { escapeRegExp } from "lodash";
import buildQuery from "../build-query";

describe("build-query", () => {
  it("case-sensitive, whole-word, regex search", () => {
    const query = buildQuery(
      "hi.*",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: true,
      },
      {}
    );

    expect(query.source).toBe("\\bhi.*\\b");
    expect(query.flags).toBe("");
    expect(query.ignoreCase).toBe(false);
  });

  it("case-sensitive, whole-word, regex search, global", () => {
    const query = buildQuery(
      "hi.*",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: true,
      },
      { isGlobal: true }
    );

    expect(query.source).toBe("\\bhi.*\\b");
    expect(query.flags).toBe("g");
    expect(query.ignoreCase).toBe(false);
  });

  it("case-insensitive, non-whole, string search", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: false,
        wholeWord: false,
        regexMatch: false,
      },
      {}
    );

    expect(query.source).toBe("hi");
    expect(query.flags).toBe("i");
    expect(query.ignoreCase).toBe(true);
  });

  it("case-insensitive, non-whole, string search, global", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: false,
        wholeWord: false,
        regexMatch: false,
      },
      { isGlobal: true }
    );

    expect(query.source).toBe("hi");
    expect(query.flags).toBe("gi");
    expect(query.ignoreCase).toBe(true);
  });

  it("case-sensitive string search", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      },
      {}
    );

    expect(query.source).toBe("hi");
    expect(query.flags).toBe("");
    expect(query.ignoreCase).toBe(false);
  });

  it("string search with wholeWord", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: false,
        wholeWord: true,
        regexMatch: false,
      },
      {}
    );

    expect(query.source).toBe("\\bhi\\b");
    expect(query.flags).toBe("i");
    expect(query.ignoreCase).toBe(true);
  });

  it("case-insensitive, regex search", () => {
    const query = buildQuery(
      "hi.*",
      {
        caseSensitive: false,
        wholeWord: false,
        regexMatch: true,
      },
      {}
    );

    expect(query.source).toBe("hi.*");
    expect(query.flags).toBe("i");
    expect(query.global).toBe(false);
    expect(query.ignoreCase).toBe(true);
  });

  it("string search with wholeWord and case sensitivity", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: false,
      },
      {}
    );

    expect(query.source).toBe("\\bhi\\b");
    expect(query.flags).toBe("");
    expect(query.global).toBe(false);
    expect(query.ignoreCase).toBe(false);
  });

  it("string search with wholeWord and case sensitivity, global", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: false,
      },
      { isGlobal: true }
    );

    expect(query.source).toBe("\\bhi\\b");
    expect(query.flags).toBe("g");
    expect(query.global).toBe(true);
    expect(query.ignoreCase).toBe(false);
  });

  it("string search with regex chars escaped", () => {
    const query = buildQuery(
      "hi.*",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: false,
      },
      {}
    );

    expect(query.source).toBe("\\bhi\\.\\*\\b");
    expect(query.flags).toBe("");
    expect(query.global).toBe(false);
    expect(query.ignoreCase).toBe(false);
  });

  it("string search with regex chars escaped, global", () => {
    const query = buildQuery(
      "hi.*",
      {
        caseSensitive: true,
        wholeWord: true,
        regexMatch: false,
      },
      { isGlobal: true }
    );

    expect(query.source).toBe("\\bhi\\.\\*\\b");
    expect(query.flags).toBe("g");
    expect(query.global).toBe(true);
    expect(query.ignoreCase).toBe(false);
  });

  it("ignore spaces w/o spaces", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      },
      { ignoreSpaces: true }
    );

    expect(query.source).toBe("hi");
    expect(query.flags).toBe("");
    expect(query.global).toBe(false);
    expect(query.ignoreCase).toBe(false);
  });

  it("ignore spaces w/o spaces, global", () => {
    const query = buildQuery(
      "hi",
      {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      },
      { isGlobal: true, ignoreSpaces: true }
    );

    expect(query.source).toBe("hi");
    expect(query.flags).toBe("g");
    expect(query.global).toBe(true);
    expect(query.ignoreCase).toBe(false);
  });

  it("ignore spaces w/ spaces", () => {
    const query = buildQuery(
      "  ",
      {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      },
      { ignoreSpaces: true }
    );

    expect(query.source).toBe(escapeRegExp("(?!\\s*.*)"));
    expect(query.flags).toBe("");
    expect(query.global).toBe(false);
    expect(query.ignoreCase).toBe(false);
  });

  it("ignore spaces w/ spaces, global", () => {
    const query = buildQuery(
      "  ",
      {
        caseSensitive: true,
        wholeWord: false,
        regexMatch: false,
      },
      { isGlobal: true, ignoreSpaces: true }
    );

    expect(query.source).toBe(escapeRegExp("(?!\\s*.*)"));
    expect(query.flags).toBe("g");
    expect(query.global).toBe(true);
    expect(query.ignoreCase).toBe(false);
  });
});
