/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { shallow, mount } = require("enzyme");
const {
  ELLIPSIS,
} = require("resource://devtools/client/shared/components/reps/reps/rep-utils.js");
const {
  REPS,
} = require("resource://devtools/client/shared/components/reps/reps/rep.js");
const { Rep } = REPS;

const renderRep = (string, props) =>
  mount(
    Rep({
      object: string,
      ...props,
    })
  );

const testCases = [
  {
    name: "testMultiline",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbbbbbb\ncccccccccccccccc\n",
    },
    result:
      '"aaaaaaaaaaaaaaaaaaaaa\\nbbbbbbbbbbbbbbbbbbb\\ncccccccccccccccc\\n"',
  },
  {
    name: "testMultilineLimit",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbbbbbb\ncccccccccccccccc\n",
      cropLimit: 20,
    },
    result: `\"aaaaaaaaa${ELLIPSIS}cccccc\\n\"`,
  },
  {
    name: "testMultilineOpen",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaa\nbbbbbbbbbbbbbbbbbbb\ncccccccccccccccc\n",
      member: { open: true },
    },
    result:
      '"aaaaaaaaaaaaaaaaaaaaa\\nbbbbbbbbbbbbbbbbbbb\\ncccccccccccccccc\\n"',
  },
  {
    name: "testUseQuotes",
    props: {
      object: "abc",
      useQuotes: false,
    },
    result: "abc",
  },
  {
    name: "testNonPrintableCharacters",
    props: {
      object: "a\x01b",
      useQuotes: false,
    },
    result: "a\ufffdb",
  },
  {
    name: "testQuoting",
    props: {
      object:
        "\t\n\r\"'\\\x1f\x9f\ufeff\ufffe\ud8000\u2063\ufffc\u2028\ueeee\ufffd",
      useQuotes: true,
    },
    result:
      "`\\t\\n\\r\"'\\\\\\u001f\\u009f\\ufeff\\ufffe\\ud8000\\u2063" +
      "\\ufffc\\u2028\\ueeee\ufffd`",
  },
  {
    name: "testUnpairedSurrogate",
    props: {
      object: "\uDC23",
      useQuotes: true,
    },
    result: '"\\udc23"',
  },
  {
    name: "testValidSurrogate",
    props: {
      object: "\ud83d\udeec",
      useQuotes: true,
    },
    result: '"\ud83d\udeec"',
  },
  {
    name: "testNoEscapeWhitespace",
    props: {
      object: "line 1\r\nline 2\n\tline 3",
      useQuotes: true,
      escapeWhitespace: false,
    },
    result: '"line 1\r\nline 2\n\tline 3"',
  },
  {
    name: "testIgnoreFullTextWhenOpen",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      fullText:
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
        "aaaaaaaaaaaaa",
      member: { open: true },
    },
    result: '"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"',
  },
  {
    name: "testIgnoreFullTextWithLimit",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      fullText:
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
        "aaaaaaaaaaaaa",
      cropLimit: 20,
    },
    result: `\"aaaaaaaaa${ELLIPSIS}aaaaaaaa\"`,
  },
  {
    name: "testIgnoreFullTextWhenOpenWithLimit",
    props: {
      object: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
      fullText:
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" +
        "aaaaaaaaaaaaa",
      member: { open: true },
      cropLimit: 20,
    },
    result: '"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"',
  },
  {
    name: "testEmptyStringWithoutQuotes",
    props: {
      object: "",
      transformEmptyString: true,
      useQuotes: false,
    },
    result: "<empty string>",
  },
  {
    name: "testEmptyStringWithoutQuotesAndNoTransform",
    props: {
      object: "",
      useQuotes: false,
      transformEmptyString: false,
    },
    result: "",
  },
  {
    name: "testEmptyStringWithQuotes",
    props: {
      object: "",
      useQuotes: true,
      transformEmptyString: true,
    },
    result: `""`,
  },
  {
    name: "testEmptyStringWithQuotesAndNoTransforms",
    props: {
      object: "",
      useQuotes: true,
      transformEmptyString: false,
    },
    result: `""`,
  },
  {
    name: "testQuotingSingleQuote",
    props: {
      object: "'",
      useQuotes: true,
    },
    result: `"'"`,
  },
  {
    name: "testQuotingDoubleQuote",
    props: {
      object: '"',
      useQuotes: true,
    },
    result: `'"'`,
  },
  {
    name: "testQuotingBacktick",
    props: {
      object: "`",
      useQuotes: true,
    },
    result: '"`"',
  },
  {
    name: "testQuotingSingleAndDoubleQuotes",
    props: {
      object: "'\"",
      useQuotes: true,
    },
    result: "`'\"`",
  },
  {
    name: "testQuotingSingleAndDoubleQuotesAnd${",
    props: {
      object: "'\"${",
      useQuotes: true,
    },
    result: '"\'\\"${"',
  },
  {
    name: "testQuotingSingleQuoteAndBacktick",
    props: {
      object: "'`",
      useQuotes: true,
    },
    result: '"\'`"',
  },
  {
    name: "testQuotingDoubleQuoteAndBacktick",
    props: {
      object: '"`',
      useQuotes: true,
    },
    result: "'\"`'",
  },
  {
    name: "testQuotingSingleAndDoubleQuotesAndBacktick",
    props: {
      object: "'\"`",
      useQuotes: true,
    },
    result: '"\'\\"`"',
  },
];

describe("test String", () => {
  for (const testCase of testCases) {
    it(`String rep ${testCase.name}`, () => {
      const renderedComponent = shallow(Rep(testCase.props));
      expect(renderedComponent.text()).toEqual(testCase.result);
    });
  }

  it("If shouldRenderTooltip, StringRep displays a tooltip title on the span element.", () => {
    const tooltipText = "This is a tooltip";
    const element = renderRep(tooltipText, { shouldRenderTooltip: true });
    expect(element.prop("title")).toBe('"This is a tooltip"');
  });

  it("If !shouldRenderTooltip, StringRep doesn't display a tooltip title.", () => {
    const noTooltip = "There is no tooltip";
    const element = renderRep(noTooltip, { shouldRenderTooltip: false });
    expect(element.prop("title")).toBe(undefined);
  });
});
