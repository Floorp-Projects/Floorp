/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const requireHacker = require("require-hacker");

requireHacker.global_hook("default", path => {
  switch (path) {
    // For Enzyme
    case "react-dom/server":
      return `const React = require('react-dev'); module.exports = React`;
    case "react-addons-test-utils":
      return `const React = require('react-dev'); module.exports = React.addons.TestUtils`;
    // Use react-dev. This would be handled by browserLoader in Firefox.
    case "react":
    case "devtools/client/shared/vendor/react":
      return `const React = require('react-dev'); module.exports = React`;
    // For Rep's use of AMD
    case "devtools/client/shared/vendor/react.default":
      return `const React = require('react-dev'); module.exports = React`;
  }

  // Some modules depend on Chrome APIs which don't work in mocha. When such a module
  // is required, replace it with a mock version.
  switch (path) {
    case "devtools/client/webconsole/utils":
      return `module.exports = require("devtools/client/webconsole/new-console-output/test/fixtures/WebConsoleUtils")`;
    case "Services":
    case "Services.default":
      return `module.exports = require("devtools/client/webconsole/new-console-output/test/fixtures/Services")`;
  }
});
