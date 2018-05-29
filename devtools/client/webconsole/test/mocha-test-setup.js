/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

"use strict";

const mcRoot = `${__dirname}/../../../../`;
const getModule = mcPath => `module.exports = require("${mcRoot}${mcPath}");`;

const { Services: { pref } } = require("devtools-modules");
pref("devtools.debugger.remote-timeout", 10000);
pref("devtools.hud.loglimit", 10000);
pref("devtools.webconsole.filter.error", true);
pref("devtools.webconsole.filter.warn", true);
pref("devtools.webconsole.filter.info", true);
pref("devtools.webconsole.filter.log", true);
pref("devtools.webconsole.filter.debug", true);
pref("devtools.webconsole.filter.css", false);
pref("devtools.webconsole.filter.net", false);
pref("devtools.webconsole.filter.netxhr", false);
pref("devtools.webconsole.ui.filterbar", false);
pref("devtools.webconsole.inputHistoryCount", 50);
pref("devtools.webconsole.persistlog", false);
pref("devtools.webconsole.timestampMessages", false);
pref("devtools.webconsole.sidebarToggle", true);

global.loader = {
  lazyServiceGetter: () => {},
  lazyRequireGetter: () => {}
};

// Point to vendored-in files and mocks when needed.
const requireHacker = require("require-hacker");
requireHacker.global_hook("default", (path, module) => {
  switch (path) {
    // For Enzyme
    case "react-dom":
      return getModule("devtools/client/shared/vendor/react-dom");
    case "react-dom/server":
      return getModule("devtools/client/shared/vendor/react-dom-server");
    case "react-dom/test-utils":
      return getModule("devtools/client/shared/vendor/react-dom-test-utils-dev");
    case "react-redux":
      return getModule("devtools/client/shared/vendor/react-redux");
    // Use react-dev. This would be handled by browserLoader in Firefox.
    case "react":
    case "devtools/client/shared/vendor/react":
      return getModule("devtools/client/shared/vendor/react-dev");
    case "chrome":
      return `module.exports = { Cc: {}, Ci: {}, Cu: {} }`;
  }

  // Some modules depend on Chrome APIs which don't work in mocha. When such a module
  // is required, replace it with a mock version.
  switch (path) {
    case "devtools/shared/l10n":
      return getModule(
        "devtools/client/webconsole/test/fixtures/LocalizationHelper");
    case "devtools/shared/plural-form":
      return getModule(
        "devtools/client/webconsole/test/fixtures/PluralForm");
    case "Services":
    case "Services.default":
      return `module.exports = require("devtools-modules/src/Services")`;
    case "devtools/shared/client/object-client":
    case "devtools/shared/client/long-string-client":
      return `() => {}`;
    case "devtools/client/netmonitor/src/components/TabboxPanel":
    case "devtools/client/webconsole/utils/context-menu":
      return "{}";
    case "devtools/client/shared/telemetry":
      return `module.exports = function() {}`;
    case "devtools/shared/event-emitter":
      return `module.exports = require("devtools-modules/src/utils/event-emitter")`;
    case "devtools/client/shared/unicode-url":
      return `module.exports = require("devtools-modules/src/unicode-url")`;
  }

  // We need to rewrite all the modules assuming the root is mozilla-central and give them
  // an absolute path.
  if (path.startsWith("devtools/")) {
    return getModule(path);
  }

  return undefined;
});

// Configure enzyme with React 16 adapter. This needs to be done after we set the
// requireHack hook so `require()` calls in Enzyme are handled as well.
const Enzyme = require("enzyme");
const Adapter = require("enzyme-adapter-react-16");
Enzyme.configure({ adapter: new Adapter() });
