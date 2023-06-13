/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            // Sign in
            { fieldName: "email", reason: "regex-heuristic"},
            // {fieldName: "password"},
          ],
        },
        {
          invalid: true,
          fields: [
            // Forgot password
            { fieldName: "email", reason: "regex-heuristic"},
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic"},
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Macys/"
);
