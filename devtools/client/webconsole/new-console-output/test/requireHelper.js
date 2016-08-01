/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const requireHacker = require("require-hacker");

module.exports = () => {
  try {
    requireHacker.global_hook("default", path => {
      switch (path) {
        case "react-dom/server":
          return `const React = require('react-dev'); module.exports = React`;
        case "react-addons-test-utils":
          return `const React = require('react-dev'); module.exports = React.addons.TestUtils`;
        case "react":
          return `const React = require('react-dev'); module.exports = React`;
        case "devtools/client/shared/vendor/react":
          return `const React = require('react-dev'); module.exports = React`;
        case "devtools/client/shared/vendor/react.default":
          return `const React = require('react-dev'); module.exports = React`;
      }
    });
  } catch (e) {
    // Do nothing. This means the hook is already registered.
  }
};
