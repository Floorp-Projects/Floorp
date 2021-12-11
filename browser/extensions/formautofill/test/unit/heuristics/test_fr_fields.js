/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "heuristics_fr_fields.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-name",
            },
          ],
        ],
      ],
    },
  ],
  "../../fixtures/"
);
