import { ASRouterTargeting } from "lib/ASRouterTargeting.jsm";
import docs from "content-src/asrouter/docs/targeting-attributes.md";

// The following targeting parameters are either deprecated or should not be included in the docs for some reason.
const SKIP_DOCS = [];
// These are extra message context attributes via ASRouter.jsm
const MESSAGE_CONTEXT_ATTRIBUTES = [
  "previousSessionEnd",
  "trailheadInterrupt",
  "trailheadTriplet",
];

function getHeadingsFromDocs() {
  const re = /### `(\w+)`/g;
  const found = [];
  let match = 1;
  while (match) {
    match = re.exec(docs);
    if (match) {
      found.push(match[1]);
    }
  }
  return found;
}

function getTOCFromDocs() {
  const re = /## Available attributes\n+([^]+)\n+## Detailed usage/;
  const sectionMatch = docs.match(re);
  if (!sectionMatch) {
    return [];
  }
  const [, listText] = sectionMatch;
  const re2 = /\[(\w+)\]/g;
  const found = [];
  let match = 1;
  while (match) {
    match = re2.exec(listText);
    if (match) {
      found.push(match[1]);
    }
  }
  return found;
}

describe("ASRTargeting docs", () => {
  const DOCS_TARGETING_HEADINGS = getHeadingsFromDocs();
  const DOCS_TOC = getTOCFromDocs();
  const ASRTargetingAttributes = [
    ...Object.keys(ASRouterTargeting.Environment).filter(
      attribute => !SKIP_DOCS.includes(attribute)
    ),
    ...MESSAGE_CONTEXT_ATTRIBUTES,
  ];

  describe("All targeting params documented in targeting-attributes.md", () => {
    for (const targetingParam of ASRTargetingAttributes) {
      // If this test is failing, you probably forgot to add docs to content-src/asrouter/targeting-attributes.md
      // for a new targeting attribute, or you forgot to put it in the table of contents up top.
      it(`should have docs and table of contents entry for ${targetingParam}`, () => {
        assert.include(
          DOCS_TARGETING_HEADINGS,
          targetingParam,
          `Didn't find the heading: ### \`${targetingParam}\``
        );
        assert.include(
          DOCS_TOC,
          targetingParam,
          `Didn't find a table of contents entry for ${targetingParam}`
        );
      });
    }
  });
  describe("No extra attributes in targeting-attributes.md", () => {
    for (const targetingParam of DOCS_TARGETING_HEADINGS) {
      // If this test is failing, you might has spelled something wrong or removed a targeting param without
      // removing its docs.
      it(`should have an implementation for ${targetingParam} in ASRouterTargeting.Environment`, () => {
        assert.include(
          ASRTargetingAttributes,
          targetingParam,
          `Didn't find an implementation for ${targetingParam}`
        );
      });
    }
  });
});
