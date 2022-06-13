/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test for adaptive history autofill.

testEngine_setup();

const TEST_DATA = [
  {
    description: "Basic behavior for adaptive history autofill",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "URL that has www",
    pref: true,
    visitHistory: ["http://www.example.com/test"],
    inputHistory: [{ uri: "http://www.example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://www.example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://www.example.com/test",
            title: "test visit for http://www.example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Case differences for input are ignored",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "EXA" }],
    userInput: "eXA",
    expected: {
      autofilled: "eXAmple.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Mutiple case difference input history",
    pref: true,
    visitHistory: ["http://example.com/yes", "http://example.com/no"],
    inputHistory: [
      { uri: "http://example.com/yes", input: "exa" },
      { uri: "http://example.com/yes", input: "EXA" },
      { uri: "http://example.com/yes", input: "EXa" },
      { uri: "http://example.com/yes", input: "eXa" },
      { uri: "http://example.com/yes", input: "eXA" },
      { uri: "http://example.com/no", input: "exa" },
      { uri: "http://example.com/no", input: "exa" },
      { uri: "http://example.com/no", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/yes",
      completed: "http://example.com/yes",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/yes",
            title: "test visit for http://example.com/yes",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/no",
            title: "test visit for http://example.com/no",
          }),
      ],
    },
  },
  {
    description: "Multiple input history count",
    pref: true,
    visitHistory: ["http://example.com/few", "http://example.com/many"],
    inputHistory: [
      { uri: "http://example.com/many", input: "exa" },
      { uri: "http://example.com/few", input: "exa" },
      { uri: "http://example.com/many", input: "examp" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/many",
      completed: "http://example.com/many",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/many",
            title: "test visit for http://example.com/many",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/few",
            title: "test visit for http://example.com/few",
          }),
      ],
    },
  },
  {
    description: "Multiple input history count with same input",
    pref: true,
    visitHistory: ["http://example.com/few", "http://example.com/many"],
    inputHistory: [
      { uri: "http://example.com/many", input: "exa" },
      { uri: "http://example.com/few", input: "exa" },
      { uri: "http://example.com/many", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/many",
      completed: "http://example.com/many",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/many",
            title: "test visit for http://example.com/many",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/few",
            title: "test visit for http://example.com/few",
          }),
      ],
    },
  },
  {
    description:
      "Multiple input history count with same input but different frecency",
    pref: true,
    visitHistory: [
      "http://example.com/few",
      "http://example.com/many",
      "http://example.com/many",
    ],
    inputHistory: [
      { uri: "http://example.com/many", input: "exa" },
      { uri: "http://example.com/few", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/many",
      completed: "http://example.com/many",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/many",
            title: "test visit for http://example.com/many",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/few",
            title: "test visit for http://example.com/few",
          }),
      ],
    },
  },
  {
    description: "User input is shorter than the input history",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "e",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "User input is longer than the input history",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "example",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description:
      "User input starts with input history and includes path of the url",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "example.com/te",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "User input starts with input history and but another url",
    pref: true,
    visitHistory: ["http://example.com/test", "http://example.org/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "example.o",
    expected: {
      autofilled: "example.org/",
      completed: "http://example.org/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.org/",
            title: "example.org",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.org/test",
            title: "test visit for http://example.org/test",
          }),
      ],
    },
  },
  {
    description: "User input does not start with input history",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "notmatch" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description:
      "User input does not start with input history, but it includes as part of URL",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "test",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "User input does not start with visited URL",
    pref: true,
    visitHistory: ["http://mozilla.com/test"],
    inputHistory: [{ uri: "http://mozilla.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://mozilla.com/test",
            title: "test visit for http://mozilla.com/test",
          }),
      ],
    },
  },
  {
    description: "Visited page is bookmarked",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    bookmarks: [{ uri: "http://example.com/test", title: "test bookmark" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test bookmark",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Visit history and no bookamrk with HISTORY source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Visit history and no bookamrk with BOOKMARK source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Bookmarked visit history with HISTORY source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    visitHistory: ["http://example.com/test", "http://example.com/bookmarked"],
    bookmarks: [
      { uri: "http://example.com/bookmarked", title: "test bookmark" },
    ],
    inputHistory: [
      {
        uri: "http://example.com/test",
        input: "exa",
      },
      {
        uri: "http://example.com/bookmarked",
        input: "exa",
      },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/bookmarked",
      completed: "http://example.com/bookmarked",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/bookmarked",
            title: "test bookmark",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Bookmarked visit history with BOOKMARK source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    visitHistory: ["http://example.com/test", "http://example.com/bookmarked"],
    bookmarks: [
      { uri: "http://example.com/bookmarked", title: "test bookmark" },
    ],
    inputHistory: [
      {
        uri: "http://example.com/test",
        input: "exa",
      },
      {
        uri: "http://example.com/bookmarked",
        input: "exa",
      },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/bookmarked",
      completed: "http://example.com/bookmarked",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/bookmarked",
            title: "test bookmark",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "No visit history with HISTORY source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "No visit history with BOOKMARK source",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    bookmarks: [{ uri: "http://example.com/bookmarked", title: "test" }],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Match with path expression",
    pref: true,
    visitHistory: [
      "http://example.com/testMany",
      "http://example.com/testMany",
      "http://example.com/test",
    ],
    inputHistory: [{ uri: "http://example.com/test", input: "example.com/te" }],
    userInput: "example.com/te",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/testMany",
            title: "test visit for http://example.com/testMany",
          }),
      ],
    },
  },
  {
    description:
      "Prefixed URL for input history and the same string for user input",
    pref: true,
    visitHistory: [
      "http://example.com/testMany",
      "http://example.com/testMany",
      "http://example.com/test",
    ],
    inputHistory: [
      { uri: "http://example.com/test", input: "http://example.com/test" },
    ],
    userInput: "http://example.com/test",
    expected: {
      autofilled: "http://example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/testMany",
            title: "test visit for http://example.com/testMany",
          }),
      ],
    },
  },
  {
    description:
      "Prefixed URL for input history and URL expression for user input",
    pref: true,
    visitHistory: [
      "http://example.com/testMany",
      "http://example.com/testMany",
      "http://example.com/test",
    ],
    inputHistory: [
      { uri: "http://example.com/test", input: "http://example.com/te" },
    ],
    userInput: "http://example.com/te",
    expected: {
      autofilled: "http://example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/testMany",
            title: "test visit for http://example.com/testMany",
          }),
      ],
    },
  },
  {
    description:
      "Prefixed URL for input history and path expression for user input",
    pref: true,
    visitHistory: [
      "http://example.com/testMany",
      "http://example.com/testMany",
      "http://example.com/test",
    ],
    inputHistory: [
      { uri: "http://example.com/test", input: "http://example.com/te" },
    ],
    userInput: "example.com/te",
    expected: {
      autofilled: "example.com/testMany",
      completed: "http://example.com/testMany",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/testMany",
            title: "test visit for http://example.com/testMany",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Prefixed URL for input history and 'http' for user input",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "http" }],
    userInput: "http",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Prefixed URL for input history and 'http:' for user input",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "http:" }],
    userInput: "http:",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Prefixed URL for input history and 'http:/' for user input",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "http:/" }],
    userInput: "http:/",
    expected: {
      autofilled: "http://example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Prefixed URL for input history and 'http://' for user input",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "http://" }],
    userInput: "http://",
    expected: {
      autofilled: "http://example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Prefixed URL for input history and 'http://e' for user input",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "http://e" }],
    userInput: "http://e",
    expected: {
      autofilled: "http://example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description:
      "Those that match with fixed URL take precedence over those that match prefixed URL",
    pref: true,
    visitHistory: ["http://http.example.com/test", "http://example.com/test"],
    inputHistory: [
      { uri: "http://http.example.com/test", input: "http" },
      { uri: "http://example.com/test", input: "http://example.com/test" },
    ],
    userInput: "http",
    expected: {
      autofilled: "http.example.com/test",
      completed: "http://http.example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://http.example.com/test",
            title: "test visit for http://http.example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Input history is totally different string from the URL",
    pref: true,
    visitHistory: [
      "http://example.com/testMany",
      "http://example.com/testMany",
      "http://example.com/test",
    ],
    inputHistory: [
      { uri: "http://example.com/test", input: "totally-different-string" },
    ],
    userInput: "totally",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description:
      "Input history is totally different string from the URL and there is a visit history whose URL starts with the input",
    pref: true,
    visitHistory: ["http://example.com/test", "http://totally.example.com"],
    inputHistory: [
      { uri: "http://example.com/test", input: "totally-different-string" },
    ],
    userInput: "totally",
    expected: {
      autofilled: "totally.example.com/",
      completed: "http://totally.example.com/",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://totally.example.com/",
            title: "test visit for http://totally.example.com/",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "Use count threshold is as same as use count of input history",
    pref: true,
    useCountThreshold: 1 * 0.9 + 1,
    visitHistory: ["http://example.com/test"],
    inputHistory: [
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Use count threshold is less than use count of input history",
    pref: true,
    useCountThreshold: 3,
    visitHistory: ["http://example.com/test"],
    inputHistory: [
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "Use count threshold is more than use count of input history",
    pref: true,
    useCountThreshold: 10,
    visitHistory: ["http://example.com/test"],
    inputHistory: [
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
      { uri: "http://example.com/test", input: "exa" },
    ],
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description: "minCharsThreshold pref equals to the user input length",
    pref: true,
    minCharsThreshold: 3,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "minCharsThreshold pref is smaller than the user input length",
    pref: true,
    minCharsThreshold: 2,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description: "minCharsThreshold pref is larger than the user input length",
    pref: true,
    minCharsThreshold: 4,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description:
      "Prioritize path component with case-sensitive and that is visited",
    pref: true,
    visitHistory: [
      "http://example.com/TEST",
      "http://example.com/TEST",
      "http://example.com/test",
    ],
    inputHistory: [
      { uri: "http://example.com/TEST", input: "example.com/test" },
      { uri: "http://example.com/test", input: "example.com/test" },
    ],
    userInput: "example.com/test",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: true,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/TEST",
            title: "test visit for http://example.com/TEST",
          }),
      ],
    },
  },
  {
    description:
      "Prioritize path component with case-sensitive but no visited data",
    pref: true,
    visitHistory: ["http://example.com/TEST"],
    inputHistory: [
      { uri: "http://example.com/TEST", input: "example.com/test" },
    ],
    userInput: "example.com/test",
    expected: {
      autofilled: "example.com/test",
      completed: "http://example.com/test",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "example.com/test",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/TEST",
            title: "test visit for http://example.com/TEST",
          }),
      ],
    },
  },
  {
    description: "Turn the pref off",
    pref: false,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description:
      "With history and bookmarks sources, foreign_count == 0, frecency <= 0: No adaptive history autofill",
    pref: true,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    frecency: 0,
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
  {
    description:
      "With history source, visit_count == 0, foreign_count != 0: No adaptive history autofill",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    bookmarks: [{ uri: "http://example.com/test", title: "test bookmark" }],
    userInput: "exa",
    expected: {
      results: [
        context =>
          makeSearchResult(context, {
            engineName: "Suggestions",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description:
      "With history source, visit_count > 0, foreign_count != 0, frecency <= 20: No adaptive history autofill",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    bookmarks: [{ uri: "http://example.com/test", title: "test bookmark" }],
    frecency: 0,
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            heuristic: true,
          }),
      ],
    },
  },
  {
    description:
      "With history source, visit_count > 0, foreign_count == 0, frecency <= 20: No adaptive history autofill",
    pref: true,
    source: UrlbarUtils.RESULT_SOURCE.HISTORY,
    visitHistory: ["http://example.com/test"],
    inputHistory: [{ uri: "http://example.com/test", input: "exa" }],
    frecency: 0,
    userInput: "exa",
    expected: {
      autofilled: "example.com/",
      completed: "http://example.com/",
      hasAutofillTitle: false,
      results: [
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/",
            heuristic: true,
          }),
        context =>
          makeVisitResult(context, {
            uri: "http://example.com/test",
            title: "test visit for http://example.com/test",
          }),
      ],
    },
  },
];

add_task(async function inputTest() {
  for (const {
    description,
    pref,
    minCharsThreshold,
    useCountThreshold,
    source,
    visitHistory,
    inputHistory,
    bookmarks,
    frecency,
    userInput,
    expected,
  } of TEST_DATA) {
    info(description);

    UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", pref);

    if (!isNaN(minCharsThreshold)) {
      UrlbarPrefs.set(
        "autoFill.adaptiveHistory.minCharsThreshold",
        minCharsThreshold
      );
    }

    if (!isNaN(useCountThreshold)) {
      UrlbarPrefs.set(
        "autoFill.adaptiveHistory.useCountThreshold",
        useCountThreshold
      );
    }

    if (visitHistory && visitHistory.length) {
      await PlacesTestUtils.addVisits(visitHistory);
    }
    for (const { uri, input } of inputHistory) {
      await UrlbarUtils.addToInputHistory(uri, input);
    }
    for (const bookmark of bookmarks || []) {
      await PlacesTestUtils.addBookmarkWithDetails(bookmark);
    }

    if (typeof frecency == "number") {
      await PlacesUtils.withConnectionWrapper("test::setFrecency", db =>
        db.execute(
          `UPDATE moz_places SET frecency = :frecency WHERE url = :url`,
          {
            frecency,
            url: visitHistory[0],
          }
        )
      );
    }

    const sources = source
      ? [source]
      : [
          UrlbarUtils.RESULT_SOURCE.HISTORY,
          UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
        ];

    const context = createContext(userInput, {
      sources,
      isPrivate: false,
    });

    await check_results({
      context,
      autofilled: expected.autofilled,
      completed: expected.completed,
      hasAutofillTitle: expected.hasAutofillTitle,
      matches: expected.results.map(f => f(context)),
    });

    await cleanupPlaces();
    UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
    UrlbarPrefs.clear("autoFill.adaptiveHistory.minCharsThreshold");
    UrlbarPrefs.clear("autoFill.adaptiveHistory.useCountThreshold");
  }
});

add_task(async function urlCase() {
  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  const testVisitFixed = "example.com/ABC/DEF";
  const testVisitURL = `http://${testVisitFixed}`;
  const testInput = "example";
  await PlacesTestUtils.addVisits([testVisitURL]);
  await UrlbarUtils.addToInputHistory(testVisitURL, testInput);

  const userInput = "example.COM/abc/def";
  for (let i = 1; i <= userInput.length; i++) {
    const currentUserInput = userInput.substring(0, i);
    const context = createContext(currentUserInput, { isPrivate: false });

    if (currentUserInput.length < testInput.length) {
      // Not autofill.
      await check_results({
        context,
        matches: [
          makeVisitResult(context, {
            uri: "http://example.com/",
            title: "example.com",
            heuristic: true,
          }),
          makeVisitResult(context, {
            uri: "http://example.com/ABC/DEF",
            title: "test visit for http://example.com/ABC/DEF",
          }),
        ],
      });
    } else if (currentUserInput.length !== testVisitFixed.length) {
      // Autofill using input history.
      const autofilled = currentUserInput + testVisitFixed.substring(i);
      await check_results({
        context,
        autofilled,
        completed: "http://example.com/ABC/DEF",
        hasAutofillTitle: true,
        matches: [
          makeVisitResult(context, {
            uri: "http://example.com/ABC/DEF",
            title: "test visit for http://example.com/ABC/DEF",
            heuristic: true,
          }),
        ],
      });
    } else {
      // Autofill using user's input.
      await check_results({
        context,
        autofilled: "example.COM/abc/def",
        completed: "http://example.com/abc/def",
        hasAutofillTitle: false,
        matches: [
          makeVisitResult(context, {
            uri: "http://example.com/abc/def",
            title: "example.com/abc/def",
            heuristic: true,
          }),
          makeVisitResult(context, {
            uri: "http://example.com/ABC/DEF",
            title: "test visit for http://example.com/ABC/DEF",
          }),
        ],
      });
    }
  }

  await cleanupPlaces();
  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
});

add_task(async function decayTest() {
  UrlbarPrefs.set("autoFill.adaptiveHistory.enabled", true);

  await PlacesTestUtils.addVisits(["http://example.com/test"]);
  await UrlbarUtils.addToInputHistory("http://example.com/test", "exa");

  const initContext = createContext("exa", { isPrivate: false });
  await check_results({
    context: initContext,
    autofilled: "example.com/test",
    completed: "http://example.com/test",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(initContext, {
        uri: "http://example.com/test",
        title: "test visit for http://example.com/test",
        heuristic: true,
      }),
    ],
  });

  // The decay rate for a day is 0.975 defined in
  // nsNavHistory::PREF_FREC_DECAY_RATE_DEF. Therefore, after 30 days, as
  // use_count will be 0.975^30 = 0.468, we set the useCountThreshold 0.47 to not
  // take the input history passed 30 days.
  UrlbarPrefs.set("autoFill.adaptiveHistory.useCountThreshold", 0.47);

  // Make 29 days later.
  for (let i = 0; i < 29; i++) {
    PlacesUtils.history
      .QueryInterface(Ci.nsIObserver)
      .observe(null, "idle-daily", "");
    await PlacesTestUtils.waitForNotification(
      "pages-rank-changed",
      () => true,
      "places"
    );
  }
  const midContext = createContext("exa", { isPrivate: false });
  await check_results({
    context: midContext,
    autofilled: "example.com/test",
    completed: "http://example.com/test",
    hasAutofillTitle: true,
    matches: [
      makeVisitResult(midContext, {
        uri: "http://example.com/test",
        title: "test visit for http://example.com/test",
        heuristic: true,
      }),
    ],
  });

  // Total 30 days later.
  PlacesUtils.history
    .QueryInterface(Ci.nsIObserver)
    .observe(null, "idle-daily", "");
  await PlacesTestUtils.waitForNotification(
    "pages-rank-changed",
    () => true,
    "places"
  );
  const context = createContext("exa", { isPrivate: false });
  await check_results({
    context,
    autofilled: "example.com/",
    completed: "http://example.com/",
    hasAutofillTitle: false,
    matches: [
      makeVisitResult(context, {
        uri: "http://example.com/",
        title: "example.com",
        heuristic: true,
      }),
      makeVisitResult(context, {
        uri: "http://example.com/test",
        title: "test visit for http://example.com/test",
      }),
    ],
  });

  await cleanupPlaces();
  UrlbarPrefs.clear("autoFill.adaptiveHistory.enabled");
  UrlbarPrefs.clear("autoFill.adaptiveHistory.useCountThreshold");
});
