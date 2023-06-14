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
        // Bug 1836256: The 'securityCode' field is being recognized as both an 'email'
        // and 'csc' field. Given that we match 'csc' before the 'email' field, this
        // field is currently recognized as a 'csc' field. We need to implement a heuristic
        // to accurately determine the field type when a field aligns with multiple field types
        // instead of depending on the order of we perform the matching
        //{
          //invalid: true,
          //fields: [
            //{ fieldName: "email", reason: "regex-heuristic"},
          //],
        //},
      ],
    },
  ],
  "fixtures/third_party/Macys/"
);
