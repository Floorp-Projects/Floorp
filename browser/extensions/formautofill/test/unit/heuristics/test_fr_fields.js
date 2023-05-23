/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "heuristics_fr_fields.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp" },
            { fieldName: "cc-name" },
          ],
        },
      ],
    },
  ],
  "../../fixtures/"
);
