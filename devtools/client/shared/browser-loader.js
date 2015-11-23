var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const loaders = Cu.import("resource://gre/modules/commonjs/toolkit/loader.js", {});
const { devtools, DevToolsLoader } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { joinURI } = devtools.require("devtools/shared/path");
const VENDOR_CONTENT_URL = "resource://devtools/client/shared/vendor";
const COMPONENTS_URL = "resource://devtools/client/shared/components";

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
 * `devtools/client/shared/{vendor/components}`, which is where shared libraries
 * and React components live that should be evaluated in a browser environment.
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
  const loaderOptions = devtools.require("@loader/options");

  const opts = {
    id: "browser-loader",
    sharedGlobal: true,
    sandboxPrototype: window,
    paths: Object.assign({}, loaderOptions.paths),
    invisibleToDebugger: loaderOptions.invisibleToDebugger,
    require: (id, require) => {
      let uri = require.resolve(id);

      if (!uri.startsWith(baseURI) &&
          !uri.startsWith(COMPONENTS_URL) &&
          !uri.startsWith(VENDOR_CONTENT_URL)) {
        return devtools.require(uri);
      }
      return require(uri);
    }
  };

  const mainModule = loaders.Module(baseURI, joinURI(baseURI, "main.js"));
  const mainLoader = loaders.Loader(opts);

  return {
    loader: mainLoader,
    require: loaders.Require(mainLoader, mainModule)
  };
}

EXPORTED_SYMBOLS = ["BrowserLoader"];
