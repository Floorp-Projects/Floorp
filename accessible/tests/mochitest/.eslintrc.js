"use strict";

module.exports = {
  rules: {
    // XXX These are rules that are enabled in the recommended configuration, but
    // disabled here due to failures when initially implemented. They should be
    // removed (and hence enabled) at some stage.
    "no-nested-ternary": "off",
  },

  overrides: [
    {
      files: [
        // Bug 1602061 TODO: These tests access DOM elements via
        // id-as-variable-name, which eslint doesn't have support for yet.
        "attributes/test_listbox.html",
        "treeupdate/test_ariaowns.html",
      ],
      rules: {
        "no-undef": "off",
      },
    },
  ],
};
