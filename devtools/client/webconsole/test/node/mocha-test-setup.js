/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

"use strict";

const mcRoot = `${__dirname}/../../../../../`;
const getModule = mcPath => `module.exports = require("${mcRoot}${mcPath}");`;

const {
  Services: { pref },
} = require("devtools-modules");
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
pref("devtools.webconsole.inputHistoryCount", 300);
pref("devtools.webconsole.persistlog", false);
pref("devtools.webconsole.timestampMessages", false);
pref("devtools.webconsole.sidebarToggle", true);
pref("devtools.webconsole.groupWarningMessages", false);
pref("devtools.webconsole.input.editor", false);
pref("devtools.webconsole.input.autocomplete", true);
pref("devtools.browserconsole.contentMessages", true);
pref("devtools.webconsole.features.editor", true);
pref("devtools.webconsole.input.editorWidth", 800);
pref("devtools.webconsole.input.editorOnboarding", true);

global.loader = {
  lazyServiceGetter: () => {},
  lazyGetter: (context, name, fn) => {},
  lazyRequireGetter: (context, name, path, destruct) => {
    if (path === "devtools/shared/async-storage") {
      global[
        name
      ] = require("devtools/client/webconsole/test/node/fixtures/async-storage");
    }
    const excluded = [
      "Debugger",
      "devtools/shared/event-emitter",
      "devtools/client/shared/autocomplete-popup",
      "devtools/client/framework/devtools",
      "devtools/client/shared/keycodes",
      "devtools/client/shared/sourceeditor/editor",
      "devtools/client/shared/telemetry",
      "devtools/shared/screenshot/save",
      "devtools/client/shared/focus",
    ];
    if (!excluded.includes(path)) {
      const module = require(path);
      global[name] = destruct ? module[name] : module;
    }
  },
};

// Setting up globals used in some modules.
global.isWorker = false;
global.indexedDB = { open: () => ({}) };

// URLSearchParams was added to the global object in Node 10.0.0. To not cause any issue
// with prior versions, we add it to the global object if it is not defined there.
if (!global.URLSearchParams) {
  global.URLSearchParams = require("url").URLSearchParams;
}

if (!global.ResizeObserver) {
  global.ResizeObserver = class ResizeObserver {
    observe() {}
    unobserve() {}
    disconnect() {}
  };
}

// Mock ChromeUtils.
global.ChromeUtils = {
  import: () => {},
  defineModuleGetter: () => {},
};

// Point to vendored-in files and mocks when needed.
const requireHacker = require("require-hacker");
requireHacker.global_hook("default", (path, module) => {
  const paths = {
    // For Enzyme
    "react-dom": () => getModule("devtools/client/shared/vendor/react-dom"),
    "react-dom/server": () =>
      getModule("devtools/client/shared/vendor/react-dom-server"),
    "react-dom/test-utils": () =>
      getModule("devtools/client/shared/vendor/react-dom-test-utils-dev"),
    "react-redux": () => getModule("devtools/client/shared/vendor/react-redux"),
    // Use react-dev. This would be handled by browserLoader in Firefox.
    react: () => getModule("devtools/client/shared/vendor/react-dev"),
    "devtools/client/shared/vendor/react": () =>
      getModule("devtools/client/shared/vendor/react-dev"),
    chrome: () => `module.exports = { Cc: {}, Ci: {}, Cu: {} }`,
    // Some modules depend on Chrome APIs which don't work in mocha. When such a module
    // is required, replace it with a mock version.
    "devtools/shared/l10n": () =>
      getModule(
        "devtools/client/webconsole/test/node/fixtures/LocalizationHelper"
      ),
    "devtools/shared/plural-form": () =>
      getModule("devtools/client/webconsole/test/node/fixtures/PluralForm"),
    Services: () => `module.exports = require("devtools-modules/src/Services")`,
    "Services.default": () =>
      `module.exports = require("devtools-modules/src/Services")`,
    "devtools/shared/client/object-client": () => `() => {}`,
    "devtools/shared/client/long-string-client": () => `() => {}`,
    "devtools/client/shared/components/SmartTrace": () => "{}",
    "devtools/client/netmonitor/src/components/TabboxPanel": () => "{}",
    "devtools/client/webconsole/utils/context-menu": () => "{}",
    "devtools/client/shared/telemetry": () => `module.exports = function() {
      this.recordEvent = () => {};
      this.getKeyedHistogramById = () => ({add: () => {}});
    }`,
    "devtools/shared/event-emitter": () =>
      `module.exports = require("devtools-modules/src/utils/event-emitter")`,
    "devtools/client/shared/unicode-url": () =>
      `module.exports = require("devtools-modules/src/unicode-url")`,
    "devtools/shared/DevToolsUtils": () => "{}",
    "devtools/server/actors/reflow": () => "{}",
    "devtools/shared/layout/utils": () => "{getCurrentZoom = () => {}}",
  };

  if (paths.hasOwnProperty(path)) {
    return paths[path]();
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
