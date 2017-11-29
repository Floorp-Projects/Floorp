/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const requireHacker = require("require-hacker");

requireHacker.global_hook("default", path => {
  switch (path) {
    // For Enzyme
    case "react-dom":
      return `const React = require('devtools/client/shared/vendor/react-dev'); module.exports = React`;
    case "react-dom/server":
      return `const React = require('devtools/client/shared/vendor/react-dev'); module.exports = React`;
    // TODO: Enzyme uses the require paths to choose which adapters are
    // needed... we need to use react-addons-test-utils instead of
    // react-dom/test-utils as the path until we upgrade to React 16+
    // https://bugzil.la/1416824
    case "react-addons-test-utils":
      return `const ReactDOM = require('devtools/client/shared/vendor/react-dom'); module.exports = ReactDOM.TestUtils`;
    // Use react-dev. This would be handled by browserLoader in Firefox.
    case "react":
    case "devtools/client/shared/vendor/react":
      return `const React = require('devtools/client/shared/vendor/react-dev'); module.exports = React`;
    // For Rep's use of AMD
    case "devtools/client/shared/vendor/react.default":
      return `const React = require('devtools/client/shared/vendor/react-dev'); module.exports = React`;
  }

  // Some modules depend on Chrome APIs which don't work in mocha. When such a module
  // is required, replace it with a mock version.
  switch (path) {
    case "devtools/shared/l10n":
      return `module.exports = require("devtools/client/netmonitor/test/fixtures/localization-helper")`;
    case "devtools/client/shared/redux/create-store":
      return `module.exports = require("devtools/client/netmonitor/test/fixtures/create-store")`;
  }

  return null;
});
