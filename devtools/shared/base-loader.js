/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global __URI__ */
/* exported Loader, resolveURI, Module, Require, unload */

"use strict";

this.EXPORTED_SYMBOLS = ["Loader", "resolveURI", "Module", "Require", "unload"];

const { Constructor: CC, manager: Cm } = Components;
const systemPrincipal = CC("@mozilla.org/systemprincipal;1", "nsIPrincipal")();

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const { normalize, dirname } = ChromeUtils.import("resource://gre/modules/osfile/ospath_unix.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "resProto",
                                   "@mozilla.org/network/protocol;1?name=resource",
                                   "nsIResProtocolHandler");

ChromeUtils.defineModuleGetter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");

const { defineLazyGetter } = XPCOMUtils;

// Define some shortcuts.
const bind = Function.call.bind(Function.bind);
function* getOwnIdentifiers(x) {
  yield* Object.getOwnPropertyNames(x);
  yield* Object.getOwnPropertySymbols(x);
}

function isJSONURI(uri) {
  return uri.endsWith(".json");
}
function isJSMURI(uri) {
  return uri.endsWith(".jsm");
}
function isJSURI(uri) {
  return uri.endsWith(".js");
}
const AbsoluteRegExp = /^(resource|chrome|file|jar):/;
function isAbsoluteURI(uri) {
  return AbsoluteRegExp.test(uri);
}
function isRelative(id) {
  return id.startsWith(".");
}

function readURI(uri) {
  const nsURI = NetUtil.newURI(uri);
  if (nsURI.scheme == "resource") {
    // Resolve to a real URI, this will catch any obvious bad paths without
    // logging assertions in debug builds, see bug 1135219
    uri = resProto.resolveURI(nsURI);
  }

  const stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, "UTF-8"),
    loadUsingSystemPrincipal: true}
  ).open();
  const count = stream.available();
  const data = NetUtil.readInputStreamToString(stream, count, {
    charset: "UTF-8",
  });

  stream.close();

  return data;
}

// Combines all arguments into a resolved, normalized path
function join(base, ...paths) {
  // If this is an absolute URL, we need to normalize only the path portion,
  // or we wind up stripping too many slashes and producing invalid URLs.
  const match = /^((?:resource|file|chrome)\:\/\/[^\/]*|jar:[^!]+!)(.*)/.exec(base);
  if (match) {
    return match[1] + normalize([match[2], ...paths].join("/"));
  }

  return normalize([base, ...paths].join("/"));
}

// Function takes set of options and returns a JS sandbox. Function may be
// passed set of options:
//  - `name`: A string value which identifies the sandbox in about:memory. Will
//    throw exception if omitted.
// - `prototype`: Ancestor for the sandbox that will be created. Defaults to
//    `{}`.
// - `invisibleToDebugger`: True, if the sandbox is part of the debugger
//    implementation and should not be tracked by debugger API.
// For more details see:
// https://developer.mozilla.org/en/Components.utils.Sandbox
function Sandbox(options) {
  // Normalize options and rename to match `Cu.Sandbox` expectations.
  options = {
    // Do not expose `Components` if you really need them (bad idea!) you
    // still can expose via prototype.
    wantComponents: false,
    sandboxName: options.name,
    sandboxPrototype: "prototype" in options ? options.prototype : {},
    invisibleToDebugger: "invisibleToDebugger" in options ?
                         options.invisibleToDebugger : false,
    freshCompartment: options.freshCompartment || false,
  };

  const sandbox = Cu.Sandbox(systemPrincipal, options);

  delete sandbox.Components;

  return sandbox;
}

// Populates `exports` of the given CommonJS `module` object, in the context
// of the given `loader` by evaluating code associated with it.
function load(loader, module) {
  const { sandboxes, globals } = loader;
  const require = Require(loader, module);

  // We expose set of properties defined by `CommonJS` specification via
  // prototype of the sandbox. Also globals are deeper in the prototype
  // chain so that each module has access to them as well.
  const descriptors = {
    require: {
      configurable: true,
      enumerable: true,
      writable: true,
      value: require,
    },
    module: {
      configurable: true,
      enumerable: true,
      writable: true,
      value: module,
    },
    exports: {
      configurable: true,
      enumerable: true,
      writable: true,
      value: module.exports,
    },
  };

  let sandbox;
  if (loader.useSharedGlobalSandbox) {
    // Create a new object in this sandbox, that will be used as
    // the scope object for this particular module
    sandbox = new loader.sharedGlobalSandbox.Object();
    descriptors.lazyRequire = {
      configurable: true,
      value: lazyRequire.bind(sandbox),
    };
    descriptors.lazyRequireModule = {
      configurable: true,
      value: lazyRequireModule.bind(sandbox),
    };

    if ("console" in globals) {
      descriptors.console = {
        configurable: true,
        get() {
          return globals.console;
        },
      };
    }
    const define = Object.getOwnPropertyDescriptor(globals, "define");
    if (define && define.value) {
      descriptors.define = define;
    }
    if ("DOMParser" in globals) {
      descriptors.DOMParser = Object.getOwnPropertyDescriptor(globals, "DOMParser");
    }
    Object.defineProperties(sandbox, descriptors);
  } else {
    sandbox = Sandbox({
      name: module.uri,
      prototype: Object.create(globals, descriptors),
      invisibleToDebugger: loader.invisibleToDebugger,
    });
  }
  sandboxes[module.uri] = sandbox;

  const originalExports = module.exports;
  try {
    Services.scriptloader.loadSubScript(module.uri, sandbox);
  } catch (error) {
    // loadSubScript sometime throws string errors, which includes no stack.
    // At least provide the current stack by re-throwing a real Error object.
    if (typeof error == "string") {
      if (error.startsWith("Error creating URI") ||
          error.startsWith("Error opening input stream (invalid filename?)")) {
        throw new Error(`Module \`${module.id}\` is not found at ${module.uri}`);
      }
      throw new Error(`Error while loading module \`${module.id}\` at ${module.uri}:` +
       "\n" + error);
    }
    // Otherwise just re-throw everything else which should have a stack
    throw error;
  }

  // Only freeze the exports object if we created it ourselves. Modules
  // which completely replace the exports object and still want it
  // frozen need to freeze it themselves.
  if (module.exports === originalExports) {
    Object.freeze(module.exports);
  }

  return module;
}

// Utility function to normalize module `uri`s so they have `.js` extension.
function normalizeExt(uri) {
  if (isJSURI(uri) || isJSONURI(uri) || isJSMURI(uri)) {
    return uri;
  }
  return uri + ".js";
}

// Utility function to join paths. In common case `base` is a
// `requirer.uri` but in some cases it may be `baseURI`. In order to
// avoid complexity we require `baseURI` with a trailing `/`.
function resolve(id, base) {
  if (!isRelative(id)) {
    return id;
  }

  const baseDir = dirname(base);

  let resolved;
  if (baseDir.includes(":")) {
    resolved = join(baseDir, id);
  } else {
    resolved = normalize(`${baseDir}/${id}`);
  }

  // Joining and normalizing removes the "./" from relative files.
  // We need to ensure the resolution still has the root
  if (base.startsWith("./")) {
    resolved = "./" + resolved;
  }

  return resolved;
}

function compileMapping(paths) {
  // Make mapping array that is sorted from longest path to shortest path.
  const mapping = Object.keys(paths)
                      .sort((a, b) => b.length - a.length)
                      .map(path => [path, paths[path]]);

  const PATTERN = /([.\\?+*(){}[\]^$])/g;
  const escapeMeta = str => str.replace(PATTERN, "\\$1");

  const patterns = [];
  paths = {};

  for (let [path, uri] of mapping) {
    // Strip off any trailing slashes to make comparisons simpler
    if (path.endsWith("/")) {
      path = path.slice(0, -1);
      uri = uri.replace(/\/+$/, "");
    }

    paths[path] = uri;

    // We only want to match path segments explicitly. Examples:
    // * "foo/bar" matches for "foo/bar"
    // * "foo/bar" matches for "foo/bar/baz"
    // * "foo/bar" does not match for "foo/bar-1"
    // * "foo/bar/" does not match for "foo/bar"
    // * "foo/bar/" matches for "foo/bar/baz"
    //
    // Check for an empty path, an exact match, or a substring match
    // with the next character being a forward slash.
    if (path == "") {
      patterns.push("");
    } else {
      patterns.push(`${escapeMeta(path)}(?=$|/)`);
    }
  }

  const pattern = new RegExp(`^(${patterns.join("|")})`);

  // This will replace the longest matching path mapping at the start of
  // the ID string with its mapped value.
  return id => {
    return id.replace(pattern, (m0, m1) => paths[m1]);
  };
}

function resolveURI(id, mapping) {
  // Do not resolve if already a resource URI
  if (isAbsoluteURI(id)) {
    return normalizeExt(id);
  }

  return normalizeExt(mapping(id));
}

/**
 * Defines lazy getters on the given object, which lazily require the
 * given module the first time they are accessed, and then resolve that
 * module's exported properties.
 *
 * @param {object} obj
 *        The target object on which to define the lazy getters.
 * @param {string} moduleId
 *        The ID of the module to require, as passed to require().
 * @param {Array<string | object>} args
 *        Any number of properties to import from the module. A string
 *        will cause the property to be defined which resolves to the
 *        same property in the module's exports. An object will define a
 *        lazy getter for every value in the object which corresponds to
 *        the given key in the module's exports, as in an ordinary
 *        destructuring assignment.
 */
function lazyRequire(obj, moduleId, ...args) {
  let module;
  const getModule = () => {
    if (!module) {
      module = this.require(moduleId);
    }
    return module;
  };

  for (let props of args) {
    if (typeof props !== "object") {
      props = {[props]: props};
    }

    for (const [fromName, toName] of Object.entries(props)) {
      defineLazyGetter(obj, toName, () => getModule()[fromName]);
    }
  }
}

/**
 * Defines a lazy getter on the given object which causes a module to be
 * lazily imported the first time it is accessed.
 *
 * @param {object} obj
 *        The target object on which to define the lazy getter.
 * @param {string} moduleId
 *        The ID of the module to require, as passed to require().
 * @param {string} [prop = moduleId]
 *        The name of the lazy getter property to define.
 */
function lazyRequireModule(obj, moduleId, prop = moduleId) {
  defineLazyGetter(obj, prop, () => this.require(moduleId));
}

// Creates version of `require` that will be exposed to the given `module`
// in the context of the given `loader`. Each module gets own limited copy
// of `require` that is allowed to load only a modules that are associated
// with it during link time.
function Require(loader, requirer) {
  const {
    modules, mapping, mappingCache, requireHook,
  } = loader;

  function require(id) {
    if (!id) {
      // Throw if `id` is not passed.
      throw Error("You must provide a module name when calling require() from "
                  + requirer.id, requirer.uri);
    }

    if (requireHook) {
      return requireHook(id, _require);
    }

    return _require(id);
  }

  function _require(id) {
    let { uri, requirement } = getRequirements(id);

    let module = null;
    // If module is already cached by loader then just use it.
    if (uri in modules) {
      module = modules[uri];
    } else if (isJSMURI(uri)) {
      module = modules[uri] = Module(requirement, uri);
      module.exports = ChromeUtils.import(uri, {});
    } else if (isJSONURI(uri)) {
      let data;

      // First attempt to load and parse json uri
      // ex: `test.json`
      // If that doesn"t exist, check for `test.json.js`
      // for node parity
      try {
        data = JSON.parse(readURI(uri));
        module = modules[uri] = Module(requirement, uri);
        module.exports = data;
      } catch (err) {
        // If error thrown from JSON parsing, throw that, do not
        // attempt to find .json.js file
        if (err && /JSON\.parse/.test(err.message)) {
          throw err;
        }
        uri = uri + ".js";
      }
    }

    // If not yet cached, load and cache it.
    // We also freeze module to prevent it from further changes
    // at runtime.
    if (!(uri in modules)) {
      // Many of the loader's functionalities are dependent
      // on modules[uri] being set before loading, so we set it and
      // remove it if we have any errors.
      module = modules[uri] = Module(requirement, uri);
      try {
        Object.freeze(load(loader, module));
      } catch (e) {
        // Clear out modules cache so we can throw on a second invalid require
        delete modules[uri];
        // Also clear out the Sandbox that was created
        delete loader.sandboxes[uri];
        throw e;
      }
    }

    return module.exports;
  }

  // Resolution function taking a module name/path and
  // returning a resourceURI and a `requirement` used by the loader.
  // Used by both `require` and `require.resolve`.
  function getRequirements(id) {
    if (!id) {
      // Throw if `id` is not passed.
      throw Error("you must provide a module name when calling require() from "
                  + requirer.id, requirer.uri);
    }

    let requirement, uri;

    if (modules[id]) {
      uri = requirement = id;
    } else if (requirer) {
      // Resolve `id` to its requirer if it's relative.
      requirement = resolve(id, requirer.id);
    } else {
      requirement = id;
    }

    // Resolves `uri` of module using loaders resolve function.
    if (!uri) {
      if (mappingCache.has(requirement)) {
        uri = mappingCache.get(requirement);
      } else {
        uri = resolveURI(requirement, mapping);
        mappingCache.set(requirement, uri);
      }
    }

    // Throw if `uri` can not be resolved.
    if (!uri) {
      throw Error("Module: Can not resolve '" + id + "' module required by " +
                  requirer.id + " located at " + requirer.uri, requirer.uri);
    }

    return { uri: uri, requirement: requirement };
  }

  // Expose the `resolve` function for this `Require` instance
  require.resolve = _require.resolve = function(id) {
    const { uri } = getRequirements(id);
    return uri;
  };

  // This is like webpack's require.context.  It returns a new require
  // function that prepends the prefix to any requests.
  require.context = prefix => {
    return id => {
      return require(prefix + id);
    };
  };

  return require;
}

// Makes module object that is made available to CommonJS modules when they
// are evaluated, along with `exports` and `require`.
function Module(id, uri) {
  return Object.create(null, {
    id: { enumerable: true, value: id },
    exports: { enumerable: true, writable: true, value: Object.create(null),
               configurable: true },
    uri: { value: uri },
  });
}

// Takes `loader`, and unload `reason` string and notifies all observers that
// they should cleanup after them-self.
function unload(loader, reason) {
  // subject is a unique object created per loader instance.
  // This allows any code to cleanup on loader unload regardless of how
  // it was loaded. To handle unload for specific loader subject may be
  // asserted against loader.destructor or require("@loader/unload")
  // Note: We don not destroy loader's module cache or sandboxes map as
  // some modules may do cleanup in subsequent turns of event loop. Destroying
  // cache may cause module identity problems in such cases.
  const subject = { wrappedJSObject: loader.destructor };
  Services.obs.notifyObservers(subject, "devtools:loader:destroy", reason);
}

// Function makes new loader that can be used to load CommonJS modules.
// Loader takes following options:
// - `paths`: Mandatory dictionary of require path mapped to absolute URIs.
//   Object keys are path prefix used in require(), values are URIs where each
//   prefix should be mapped to.
// - `sharedGlobal`: Boolean, if True, loads all module in a single, shared
//   global in order to create only one global and compartment.
// - `globals`: Optional map of globals, that all module scopes will inherit
//   from. Map is also exposed under `globals` property of the returned loader
//   so it can be extended further later. Defaults to `{}`.
// - `sandboxName`: String, name of the sandbox displayed in about:memory.
// - `invisibleToDebugger`: Boolean. Should be true when loading debugger
//   modules, in order to ignore them from the Debugger API.
// - `sandboxPrototype`: Object used to define globals on all module's
//   sandboxes.
// - `requireHook`: Optional function used to replace native require function
//   from loader. This function receive the module path as first argument,
//   and native require method as second argument.
function Loader(options) {
  let { paths, sharedGlobal, globals } = options;
  if (!globals) {
    globals = {};
  }

  // We create an identity object that will be dispatched on an unload
  // event as subject. This way unload listeners will be able to assert
  // which loader is unloaded. Please note that we intentionally don"t
  // use `loader` as subject to prevent a loader access leakage through
  // observer notifications.
  const destructor = Object.create(null);

  const mapping = compileMapping(paths);

  // Define pseudo modules.
  let modules = {
    "@loader/unload": destructor,
    "@loader/options": options,
    "chrome": { Cc, Ci, Cu, Cr, Cm,
                CC: bind(CC, Components), components: Components,
                ChromeWorker,
    },
  };

  const builtinModuleExports = modules;
  modules = {};
  for (const id of Object.keys(builtinModuleExports)) {
    // We resolve `uri` from `id` since modules are cached by `uri`.
    const uri = resolveURI(id, mapping);
    const module = Module(id, uri);

    // Lazily expose built-in modules in order to
    // allow them to be loaded lazily.
    Object.defineProperty(module, "exports", {
      enumerable: true,
      get: function() {
        return builtinModuleExports[id];
      },
    });

    modules[uri] = module;
  }

  // Create the unique sandbox we will be using for all modules,
  // so that we prevent creating a new compartment per module.
  // The side effect is that all modules will share the same
  // global objects.
  const sharedGlobalSandbox = Sandbox({
    name: options.sandboxName || "DevTools",
    invisibleToDebugger: options.invisibleToDebugger || false,
    prototype: options.sandboxPrototype || globals,
    freshCompartment: options.freshCompartment,
  });

  if (options.sandboxPrototype) {
    // If we were given a sandboxPrototype, we have to define the globals on
    // the sandbox directly. Note that this will not work for callers who
    // depend on being able to add globals after the loader was created.
    for (const name of getOwnIdentifiers(globals)) {
      Object.defineProperty(sharedGlobalSandbox, name,
                            Object.getOwnPropertyDescriptor(globals, name));
    }
  }

  // Loader object is just a representation of a environment
  // state. We mark its properties non-enumerable
  // as they are pure implementation detail that no one should rely upon.
  const returnObj = {
    destructor: { enumerable: false, value: destructor },
    globals: { enumerable: false, value: globals },
    mapping: { enumerable: false, value: mapping },
    mappingCache: { enumerable: false, value: new Map() },
    // Map of module objects indexed by module URIs.
    modules: { enumerable: false, value: modules },
    useSharedGlobalSandbox: { enumerable: false, value: !!sharedGlobal },
    sharedGlobalSandbox: { enumerable: false, value: sharedGlobalSandbox },
    // Map of module sandboxes indexed by module URIs.
    sandboxes: { enumerable: false, value: {} },
    // Whether the modules loaded should be ignored by the debugger
    invisibleToDebugger: { enumerable: false,
                           value: options.invisibleToDebugger || false },
    requireHook: { enumerable: false, writable: true, value: options.requireHook },
  };

  return Object.create(null, returnObj);
}
