/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-env node */

"use strict";

require("@babel/register")({
  // by default everything is ignored
  ignore: [/node_modules/],
});

const mcRoot = `${__dirname}/../../../../../`;
const getModule = mcPath =>
  `module.exports = require("${(mcRoot + mcPath).replace(/\\/gi, "/")}");`;

const { pref } = require(mcRoot +
  "devtools/client/shared/test-helpers/jest-fixtures/Services");
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
pref("devtools.webconsole.input.eagerEvaluation", true);
pref("devtools.browserconsole.contentMessages", true);
pref("devtools.browserconsole.enableNetworkMonitoring", false);
pref("devtools.webconsole.input.editorWidth", 800);
pref("devtools.webconsole.input.editorOnboarding", true);
pref("devtools.webconsole.input.context", false);

global.loader = {
  lazyServiceGetter: () => {},
  lazyGetter: (context, name, fn) => {
    try {
      global[name] = fn();
    } catch (_) {}
  },
  lazyRequireGetter: (context, names, path, destruct) => {
    const excluded = [
      "Debugger",
      "devtools/shared/event-emitter",
      "devtools/client/shared/autocomplete-popup",
      "devtools/client/framework/devtools",
      "devtools/client/shared/keycodes",
      "devtools/client/shared/sourceeditor/editor",
      "devtools/client/shared/telemetry",
      "devtools/client/shared/screenshot",
      "devtools/client/shared/focus",
      "devtools/shared/commands/target/legacy-target-watchers/legacy-processes-watcher",
      "devtools/shared/commands/target/legacy-target-watchers/legacy-workers-watcher",
      "devtools/shared/commands/target/legacy-target-watchers/legacy-sharedworkers-watcher",
      "devtools/shared/commands/target/legacy-target-watchers/legacy-serviceworkers-watcher",
    ];
    if (!excluded.includes(path)) {
      if (!Array.isArray(names)) {
        names = [names];
      }

      for (const name of names) {
        Object.defineProperty(global, name, {
          get() {
            const module = require(path);
            return destruct ? module[name] : module;
          },
          configurable: true,
        });
      }
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

global.Services = require(mcRoot +
  "devtools/client/shared/test-helpers/jest-fixtures/Services");

// Mock ChromeUtils.
global.ChromeUtils = {
  import: () => {},
  defineModuleGetter: () => {},
  addProfilerMarker: () => {},
};

global.Cu = { isInAutomation: true };
global.define = function() {};

// Used for the HTMLTooltip component.
// And set "isSystemPrincipal: false" because can't support XUL element in node.
global.document.nodePrincipal = {
  isSystemPrincipal: false,
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
    "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/stub-generator-helpers": () =>
      getModule(
        "devtools/client/webconsole/test/browser/stub-generator-helpers"
      ),

    chrome: () =>
      `module.exports = { Cc: {}, Ci: {}, Cu: { now: () => {}}, components: {stack: {caller: ""}} }`,
    // Some modules depend on Chrome APIs which don't work in mocha. When such a module
    // is required, replace it with a mock version.
    "devtools/server/devtools-server": () =>
      `module.exports = {DevToolsServer: {}}`,
    "devtools/client/shared/components/SmartTrace": () =>
      "module.exports = () => null;",
    "devtools/client/netmonitor/src/components/TabboxPanel": () => "{}",
    "devtools/client/webconsole/utils/context-menu": () => "{}",
    "devtools/client/shared/telemetry": () => `module.exports = function() {
      this.recordEvent = () => {};
      this.getKeyedHistogramById = () => ({add: () => {}});
    }`,
    "devtools/client/shared/unicode-url": () =>
      getModule(
        "devtools/client/shared/test-helpers/jest-fixtures/unicode-url"
      ),
    "devtools/shared/DevToolsUtils": () =>
      getModule("devtools/client/webconsole/test/node/fixtures/DevToolsUtils"),
    "devtools/server/actors/reflow": () => "{}",
    "devtools/shared/layout/utils": () => "{getCurrentZoom = () => {}}",
    "resource://gre/modules/AppConstants.jsm": () => "module.exports = {};",
    "devtools/client/framework/devtools": () => `module.exports = {
      gDevTools: {}
    };`,
    "devtools/shared/async-storage": () =>
      getModule("devtools/client/webconsole/test/node/fixtures/async-storage"),
    "devtools/shared/generate-uuid": () =>
      getModule(
        "devtools/client/shared/test-helpers/jest-fixtures/generate-uuid"
      ),
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

// There's an issue in React 16 that only occurs when MessageChannel is available (Node 15+),
// which prevents the nodeJS process to exit (See https://github.com/facebook/react/issues/20756)
// Due to our setup, using https://github.com/facebook/react/issues/20756#issuecomment-780927519
// does not fix the issue, so we directly replicate the fix.
// XXX: This should be removed when we update to React 17.
const MessageChannel = global.MessageChannel;
delete global.MessageChannel;

// Configure enzyme with React 16 adapter. This needs to be done after we set the
// requireHack hook so `require()` calls in Enzyme are handled as well.
const Enzyme = require("enzyme");
const Adapter = require("enzyme-adapter-react-16");
Enzyme.configure({ adapter: new Adapter() });

// Put back MessageChannel on the global, in case any of the test would use it.
global.MessageChannel = MessageChannel;
