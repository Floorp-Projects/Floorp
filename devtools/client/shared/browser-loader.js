var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const loaders = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const devtools = Cu.import("resource://gre/modules/devtools/shared/Loader.jsm", {}).devtools;
const { joinURI } = devtools.require("devtools/shared/path");
var appConstants;

// Some of the services that the system module requires is not
// available in xpcshell tests. This is ok, we can easily polyfill the
// values that we need.
try {
  const system = devtools.require("devtools/shared/shared/system");
  appConstants = system.constants;
}
catch(e) {
  // We are in a testing environment most likely. There isn't much
  // risk to this defaulting to true because the dev version of React
  // will be loaded if this is true, and that file doesn't get built
  // into the release version of Firefox, so this will only work with
  // dev environments.
  appConstants = {
    DEBUG_JS_MODULES: true
  };
}

const VENDOR_CONTENT_URL = "resource:///modules/devtools/shared/vendor";

/*
 * Create a loader to be used in a browser environment. This evaluates
 * modules in their own environment, but sets window (the normal
 * global object) as the sandbox prototype, so when a variable is not
 * defined it checks `window` before throwing an error. This makes all
 * browser APIs available to modules by default, like a normal browser
 * environment, but modules are still evaluated in their own scope.
 *
 * Another very important feature of this loader is that it *only*
 * deals with modules loaded from under `baseURI`. Anything loaded
 * outside of that path will still be loaded from the devtools loader,
 * so all system modules are still shared and cached across instances.
 * An exception to this is anything under
 * `devtools/client/shared/content`, which is where shared libraries
 * live that should be evaluated in a browser environment.
 *
 * @param string baseURI
 *        Base path to load modules from.
 * @param Object window
 *        The window instance to evaluate modules within
 * @return Object
 *         An object with two properties:
 *         - loader: the Loader instance
 *         - require: a function to require modules with
 */
function BrowserLoader(baseURI, window) {
  const loaderOptions = devtools.require('@loader/options');

  let dynamicPaths = {};
  if (appConstants.DEBUG_JS_MODULES) {
    // Load in the dev version of React
    dynamicPaths["devtools/shared/vendor/react"] =
      "resource:///modules/devtools/vendor/react-dev.js";
  }

  const opts = {
    id: "browser-loader",
    sharedGlobal: true,
    sandboxPrototype: window,
    paths: Object.assign({}, loaderOptions.paths, dynamicPaths),
    invisibleToDebugger: loaderOptions.invisibleToDebugger,
    require: (id, require) => {
      const uri = require.resolve(id);

      if (!uri.startsWith(baseURI) &&
          !uri.startsWith(VENDOR_CONTENT_URL)) {
        return devtools.require(uri);
      }
      return require(uri);
    }
  };

  // The main.js file does not have to actually exist. It just
  // represents the base environment, so requires will be relative to
  // that path when used outside of modules.
  const mainModule = loaders.Module(baseURI, joinURI(baseURI, "main.js"));
  const mainLoader = loaders.Loader(opts);

  return {
    loader: mainLoader,
    require: loaders.Require(mainLoader, mainModule)
  };
}

EXPORTED_SYMBOLS = ["BrowserLoader"];
