/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

;((factory) => { // Module boilerplate :(
  if (typeof(require) === 'function') { // CommonJS
    require("chrome").Cu.import(module.uri, exports);
  }
  else if (~String(this).indexOf('BackstagePass')) { // JSM
    let module = { uri: __URI__, id: "toolkit/loader", exports: Object.create(null) }
    factory(module);
    Object.assign(this, module.exports);
    this.EXPORTED_SYMBOLS = Object.getOwnPropertyNames(module.exports);
  }
  else {
    throw Error("Loading environment is not supported");
  }
})(module => {

'use strict';

module.metadata = {
  "stability": "unstable"
};

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu,
        results: Cr, manager: Cm } = Components;
const systemPrincipal = CC('@mozilla.org/systemprincipal;1', 'nsIPrincipal')();
const { loadSubScript } = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                     getService(Ci.mozIJSSubScriptLoader);
const { notifyObservers } = Cc['@mozilla.org/observer-service;1'].
                        getService(Ci.nsIObserverService);
const { XPCOMUtils } = Cu.import("resource://gre/modules/XPCOMUtils.jsm", {});
const { NetUtil } = Cu.import("resource://gre/modules/NetUtil.jsm", {});
const { join: pathJoin, normalize, dirname } = Cu.import("resource://gre/modules/osfile/ospath_unix.jsm");

XPCOMUtils.defineLazyGetter(this, "XulApp", () => {
  let xulappURI = module.uri.replace("toolkit/loader.js",
                                       "sdk/system/xul-app.jsm");
  return Cu.import(xulappURI, {});
});

// Define some shortcuts.
const bind = Function.call.bind(Function.bind);
const getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
const define = Object.defineProperties;
const prototypeOf = Object.getPrototypeOf;
const create = Object.create;
const keys = Object.keys;
const getOwnIdentifiers = x => [...Object.getOwnPropertyNames(x),
                                ...Object.getOwnPropertySymbols(x)];

const NODE_MODULES = ["assert", "buffer_ieee754", "buffer", "child_process", "cluster", "console", "constants", "crypto", "_debugger", "dgram", "dns", "domain", "events", "freelist", "fs", "http", "https", "_linklist", "module", "net", "os", "path", "punycode", "querystring", "readline", "repl", "stream", "string_decoder", "sys", "timers", "tls", "tty", "url", "util", "vm", "zlib"];

const COMPONENT_ERROR = '`Components` is not available in this context.\n' +
  'Functionality provided by Components may be available in an SDK\n' +
  'module: https://developer.mozilla.org/en-US/Add-ons/SDK \n\n' +
  'However, if you still need to import Components, you may use the\n' +
  '`chrome` module\'s properties for shortcuts to Component properties:\n\n' +
  'Shortcuts: \n' +
  '    Cc = Components' + '.classes \n' +
  '    Ci = Components' + '.interfaces \n' +
  '    Cu = Components' + '.utils \n' +
  '    CC = Components' + '.Constructor \n' +
  'Example: \n' +
  '    let { Cc, Ci } = require(\'chrome\');\n';

// Workaround for bug 674195. Freezing objects from other compartments fail,
// so we use `Object.freeze` from the same component instead.
function freeze(object) {
  if (prototypeOf(object) === null) {
      Object.freeze(object);
  }
  else {
    prototypeOf(prototypeOf(object.isPrototypeOf)).
      constructor. // `Object` from the owner compartment.
      freeze(object);
  }
  return object;
}

// Returns map of given `object`-s own property descriptors.
const descriptor = iced(function descriptor(object) {
  let value = {};
  getOwnIdentifiers(object).forEach(function(name) {
    value[name] = getOwnPropertyDescriptor(object, name)
  });
  return value;
});
Loader.descriptor = descriptor;

// Freeze important built-ins so they can't be used by untrusted code as a
// message passing channel.
freeze(Object);
freeze(Object.prototype);
freeze(Function);
freeze(Function.prototype);
freeze(Array);
freeze(Array.prototype);
freeze(String);
freeze(String.prototype);

// This function takes `f` function sets it's `prototype` to undefined and
// freezes it. We need to do this kind of deep freeze with all the exposed
// functions so that untrusted code won't be able to use them a message
// passing channel.
function iced(f) {
  if (!Object.isFrozen(f)) {
    f.prototype = undefined;
  }
  return freeze(f);
}

// Defines own properties of given `properties` object on the given
// target object overriding any existing property with a conflicting name.
// Returns `target` object. Note we only export this function because it's
// useful during loader bootstrap when other util modules can't be used &
// thats only case where this export should be used.
const override = iced(function override(target, source) {
  let properties = descriptor(target)
  let extension = descriptor(source || {})
  getOwnIdentifiers(extension).forEach(function(name) {
    properties[name] = extension[name];
  });
  return define({}, properties);
});
Loader.override = override;

function sourceURI(uri) { return String(uri).split(" -> ").pop(); }
Loader.sourceURI = iced(sourceURI);

function isntLoaderFrame(frame) { return frame.fileName !== module.uri }

function parseURI(uri) { return String(uri).split(" -> ").pop(); }
Loader.parseURI = parseURI;

function parseStack(stack) {
  let lines = String(stack).split("\n");
  return lines.reduce(function(frames, line) {
    if (line) {
      let atIndex = line.indexOf("@");
      let columnIndex = line.lastIndexOf(":");
      let lineIndex = line.lastIndexOf(":", columnIndex - 1);
      let fileName = parseURI(line.slice(atIndex + 1, lineIndex));
      let lineNumber = parseInt(line.slice(lineIndex + 1, columnIndex));
      let columnNumber = parseInt(line.slice(columnIndex + 1));
      let name = line.slice(0, atIndex).split("(").shift();
      frames.unshift({
        fileName: fileName,
        name: name,
        lineNumber: lineNumber,
        columnNumber: columnNumber
      });
    }
    return frames;
  }, []);
}
Loader.parseStack = parseStack;

function serializeStack(frames) {
  return frames.reduce(function(stack, frame) {
    return frame.name + "@" +
           frame.fileName + ":" +
           frame.lineNumber + ":" +
           frame.columnNumber + "\n" +
           stack;
  }, "");
}
Loader.serializeStack = serializeStack;

function readURI(uri) {
  let nsURI = NetUtil.newURI(uri);
  if (nsURI.scheme == "resource") {
    // Resolve to a real URI, this will catch any obvious bad paths without
    // logging assertions in debug builds, see bug 1135219
    let proto = Cc["@mozilla.org/network/protocol;1?name=resource"].
                getService(Ci.nsIResProtocolHandler);
    uri = proto.resolveURI(nsURI);
  }

  let stream = NetUtil.newChannel({
    uri: NetUtil.newURI(uri, 'UTF-8'),
    loadUsingSystemPrincipal: true}
  ).open();
  let count = stream.available();
  let data = NetUtil.readInputStreamToString(stream, count, {
    charset: 'UTF-8'
  });

  stream.close();

  return data;
}

// Combines all arguments into a resolved, normalized path
function join (...paths) {
  let joined = pathJoin(...paths);
  let resolved = normalize(joined);

  // OS.File `normalize` strips out any additional slashes breaking URIs like
  // `resource://`, `resource:///`, `chrome://` or `file:///`, so we work
  // around this putting back the slashes originally given, for such schemes.
  let re = /^(resource|file|chrome)(\:\/{1,3})([^\/])/;
  let matches = joined.match(re);

  return resolved.replace(re, (...args) => args[1] + matches[2] + args[3]);
}
Loader.join = join;

// Function takes set of options and returns a JS sandbox. Function may be
// passed set of options:
//  - `name`: A string value which identifies the sandbox in about:memory. Will
//    throw exception if omitted.
// - `principal`: String URI or `nsIPrincipal` for the sandbox. Defaults to
//    system principal.
// - `prototype`: Ancestor for the sandbox that will be created. Defaults to
//    `{}`.
// - `wantXrays`: A Boolean value indicating whether code outside the sandbox
//    wants X-ray vision with respect to objects inside the sandbox. Defaults
//    to `true`.
// - `sandbox`: A sandbox to share JS compartment with. If omitted new
//    compartment will be created.
// - `metadata`: A metadata object associated with the sandbox. It should
//    be JSON-serializable.
// For more details see:
// https://developer.mozilla.org/en/Components.utils.Sandbox
const Sandbox = iced(function Sandbox(options) {
  // Normalize options and rename to match `Cu.Sandbox` expectations.
  options = {
    // Do not expose `Components` if you really need them (bad idea!) you
    // still can expose via prototype.
    wantComponents: false,
    sandboxName: options.name,
    principal: 'principal' in options ? options.principal : systemPrincipal,
    wantXrays: 'wantXrays' in options ? options.wantXrays : true,
    wantGlobalProperties: 'wantGlobalProperties' in options ?
                          options.wantGlobalProperties : [],
    sandboxPrototype: 'prototype' in options ? options.prototype : {},
    invisibleToDebugger: 'invisibleToDebugger' in options ?
                         options.invisibleToDebugger : false,
    metadata: 'metadata' in options ? options.metadata : {},
    waiveIntereposition: !!options.waiveIntereposition
  };

  if (options.metadata && options.metadata.addonID) {
    options.addonId = options.metadata.addonID;
  }

  let sandbox = Cu.Sandbox(options.principal, options);

  // Each sandbox at creation gets set of own properties that will be shadowing
  // ones from it's prototype. We override delete such `sandbox` properties
  // to avoid shadowing.
  delete sandbox.Iterator;
  delete sandbox.Components;
  delete sandbox.importFunction;
  delete sandbox.debug;

  return sandbox;
});
Loader.Sandbox = Sandbox;

// Evaluates code from the given `uri` into given `sandbox`. If
// `options.source` is passed, then that code is evaluated instead.
// Optionally following options may be given:
// - `options.encoding`: Source encoding, defaults to 'UTF-8'.
// - `options.line`: Line number to start count from for stack traces.
//    Defaults to 1.
// - `options.version`: Version of JS used, defaults to '1.8'.
const evaluate = iced(function evaluate(sandbox, uri, options) {
  let { source, line, version, encoding } = override({
    encoding: 'UTF-8',
    line: 1,
    version: '1.8',
    source: null
  }, options);

  return source ? Cu.evalInSandbox(source, sandbox, version, uri, line)
                : loadSubScript(uri, sandbox, encoding);
});
Loader.evaluate = evaluate;

// Populates `exports` of the given CommonJS `module` object, in the context
// of the given `loader` by evaluating code associated with it.
const load = iced(function load(loader, module) {
  let { sandboxes, globals, loadModuleHook } = loader;
  let require = Require(loader, module);

  // We expose set of properties defined by `CommonJS` specification via
  // prototype of the sandbox. Also globals are deeper in the prototype
  // chain so that each module has access to them as well.
  let descriptors = descriptor({
    require: require,
    module: module,
    exports: module.exports,
    get Components() {
      // Expose `Components` property to throw error on usage with
      // additional information
      throw new ReferenceError(COMPONENT_ERROR);
    }
  });

  let sandbox;
  if (loader.sharedGlobalSandbox &&
      loader.sharedGlobalBlocklist.indexOf(module.id) == -1) {
    // Create a new object in this sandbox, that will be used as
    // the scope object for this particular module
    sandbox = new loader.sharedGlobalSandbox.Object();
    // Inject all expected globals in the scope object
    getOwnIdentifiers(globals).forEach(function(name) {
      descriptors[name] = getOwnPropertyDescriptor(globals, name)
    });
    define(sandbox, descriptors);
  }
  else {
    sandbox = Sandbox({
      name: module.uri,
      prototype: create(globals, descriptors),
      wantXrays: false,
      wantGlobalProperties: module.id == "sdk/indexed-db" ? ["indexedDB"] : [],
      invisibleToDebugger: loader.invisibleToDebugger,
      metadata: {
        addonID: loader.id,
        URI: module.uri
      }
    });
  }
  sandboxes[module.uri] = sandbox;

  try {
    evaluate(sandbox, module.uri);
  }
  catch (error) {
    let { message, fileName, lineNumber } = error;
    let stack = error.stack || Error().stack;
    let frames = parseStack(stack).filter(isntLoaderFrame);
    let toString = String(error);
    let file = sourceURI(fileName);

    // Note that `String(error)` where error is from subscript loader does
    // not puts `:` after `"Error"` unlike regular errors thrown by JS code.
    // If there is a JS stack then this error has already been handled by an
    // inner module load.
    if (/^Error opening input stream/.test(String(error))) {
      let caller = frames.slice(0).pop();
      fileName = caller.fileName;
      lineNumber = caller.lineNumber;
      message = "Module `" + module.id + "` is not found at " + module.uri;
      toString = message;
    }
    // Workaround for a Bug 910653. Errors thrown by subscript loader
    // do not include `stack` field and above created error won't have
    // fileName or lineNumber of the module being loaded, so we ensure
    // it does.
    else if (frames[frames.length - 1].fileName !== file) {
      frames.push({ fileName: file, lineNumber: lineNumber, name: "" });
    }

    let prototype = typeof(error) === "object" ? error.constructor.prototype :
                    Error.prototype;

    throw create(prototype, {
      message: { value: message, writable: true, configurable: true },
      fileName: { value: fileName, writable: true, configurable: true },
      lineNumber: { value: lineNumber, writable: true, configurable: true },
      stack: { value: serializeStack(frames), writable: true, configurable: true },
      toString: { value: () => toString, writable: true, configurable: true },
    });
  }

  if(loadModuleHook) {
    module = loadModuleHook(module, require);
  }

  if (loader.checkCompatibility) {
    let err = XulApp.incompatibility(module);
    if (err) {
      throw err;
    }
  }

  if (module.exports && typeof(module.exports) === 'object')
    freeze(module.exports);

  return module;
});
Loader.load = load;

// Utility function to normalize module `uri`s so they have `.js` extension.
function normalizeExt (uri) {
  return isJSURI(uri) ? uri :
         isJSONURI(uri) ? uri :
         isJSMURI(uri) ? uri :
         uri + '.js';
}

// Strips `rootURI` from `string` -- used to remove absolute resourceURI
// from a relative path
function stripBase (rootURI, string) {
  return string.replace(rootURI, './');
}

// Utility function to join paths. In common case `base` is a
// `requirer.uri` but in some cases it may be `baseURI`. In order to
// avoid complexity we require `baseURI` with a trailing `/`.
const resolve = iced(function resolve(id, base) {
  if (!isRelative(id)) return id;
  let basePaths = base.split('/');
  // Pop the last element in the `base`, because it is from a
  // relative file
  // '../mod.js' from '/path/to/file.js' should resolve to '/path/mod.js'
  basePaths.pop();
  if (!basePaths.length)
    return normalize(id);
  let resolved = join(basePaths.join('/'), id);

  // Joining and normalizing removes the './' from relative files.
  // We need to ensure the resolution still has the root
  if (isRelative(base))
    resolved = './' + resolved;

  return resolved;
});
Loader.resolve = resolve;

// Node-style module lookup
// Takes an id and path and attempts to load a file using node's resolving
// algorithm.
// `id` should already be resolved relatively at this point.
// http://nodejs.org/api/modules.html#modules_all_together
const nodeResolve = iced(function nodeResolve(id, requirer, { rootURI }) {
  // Resolve again
  id = Loader.resolve(id, requirer);

  // If this is already an absolute URI then there is no resolution to do
  if (isAbsoluteURI(id))
    return void 0;

  // we assume that extensions are correct, i.e., a directory doesnt't have '.js'
  // and a js file isn't named 'file.json.js'
  let fullId = join(rootURI, id);
  let resolvedPath;

  if ((resolvedPath = loadAsFile(fullId)))
    return stripBase(rootURI, resolvedPath);

  if ((resolvedPath = loadAsDirectory(fullId)))
    return stripBase(rootURI, resolvedPath);

  // If the requirer is an absolute URI then the node module resolution below
  // won't work correctly as we prefix everything with rootURI
  if (isAbsoluteURI(requirer))
    return void 0;

  // If manifest has dependencies, attempt to look up node modules
  // in the `dependencies` list
  let dirs = getNodeModulePaths(dirname(requirer)).map(dir => join(rootURI, dir, id));
  for (let i = 0; i < dirs.length; i++) {
    if ((resolvedPath = loadAsFile(dirs[i])))
      return stripBase(rootURI, resolvedPath);

    if ((resolvedPath = loadAsDirectory(dirs[i])))
      return stripBase(rootURI, resolvedPath);
  }

  // We would not find lookup for things like `sdk/tabs`, as that's part of
  // the alias mapping. If during `generateMap`, the runtime lookup resolves
  // with `resolveURI` -- if during runtime, then `resolve` will throw.
  return void 0;
});

// String (`${rootURI}:${requirer}:${id}`) -> resolvedPath
Loader.nodeResolverCache = new Map();

const nodeResolveWithCache = iced(function cacheNodeResolutions(id, requirer, { rootURI }) {
  // Compute the cache key based on current arguments.
  let cacheKey = `${rootURI || ""}:${requirer}:${id}`;

  // Try to get the result from the cache.
  if (Loader.nodeResolverCache.has(cacheKey)) {
    return Loader.nodeResolverCache.get(cacheKey);
  }

  // Resolve and cache if it is not in the cache yet.
  let result = nodeResolve(id, requirer, { rootURI });
  Loader.nodeResolverCache.set(cacheKey, result);
  return result;
});
Loader.nodeResolve = nodeResolveWithCache;

// Attempts to load `path` and then `path.js`
// Returns `path` with valid file, or `undefined` otherwise
function loadAsFile (path) {
  let found;

  // As per node's loader spec,
  // we first should try and load 'path' (with no extension)
  // before trying 'path.js'. We will not support this feature
  // due to performance, but may add it if necessary for adoption.
  try {
    // Append '.js' to path name unless it's another support filetype
    path = normalizeExt(path);
    readURI(path);
    found = path;
  } catch (e) {}

  return found;
}

// Attempts to load `path/package.json`'s `main` entry,
// followed by `path/index.js`, or `undefined` otherwise
function loadAsDirectory (path) {
  try {
    // If `path/package.json` exists, parse the `main` entry
    // and attempt to load that
    let main = getManifestMain(JSON.parse(readURI(path + '/package.json')));
    if (main != null) {
      let tmpPath = join(path, main);
      let found = loadAsFile(tmpPath);
      if (found)
        return found
    }
    try {
      let tmpPath = path + '/index.js';
      readURI(tmpPath);
      return tmpPath;
    } catch (e) {}
  } catch (e) {
    try {
      let tmpPath = path + '/index.js';
      readURI(tmpPath);
      return tmpPath;
    } catch (e) {}
  }
  return void 0;
}

// From `resolve` module
// https://github.com/substack/node-resolve/blob/master/lib/node-modules-paths.js
function getNodeModulePaths (start) {
  // Configurable in node -- do we need this to be configurable?
  let moduleDir = 'node_modules';

  let parts = start.split('/');
  let dirs = [];
  for (let i = parts.length - 1; i >= 0; i--) {
    if (parts[i] === moduleDir) continue;
    let dir = join(parts.slice(0, i + 1).join('/'), moduleDir);
    dirs.push(dir);
  }
  dirs.push(moduleDir);
  return dirs;
}


function addTrailingSlash (path) {
  return !path ? null : !path.endsWith('/') ? path + '/' : path;
}

// Utility function to determine of module id `name` is a built in
// module in node (fs, path, etc.);
function isNodeModule (name) {
  return !!~NODE_MODULES.indexOf(name);
}

// Make mapping array that is sorted from longest path to shortest path
// to allow overlays. Used by `resolveURI`, returns an array
function sortPaths (paths) {
  return keys(paths).
    sort((a, b) => (b.length - a.length)).
    map((path) => [ path, paths[path] ]);
}

const resolveURI = iced(function resolveURI(id, mapping) {
  let count = mapping.length, index = 0;

  // Do not resolve if already a resource URI
  if (isAbsoluteURI(id)) return normalizeExt(id);

  while (index < count) {
    let [ path, uri ] = mapping[index++];

    // Strip off any trailing slashes to make comparisons simpler
    let stripped = path.endsWith('/') ? path.slice(0, -1) : path;

    // We only want to match path segments explicitly. Examples:
    // * "foo/bar" matches for "foo/bar"
    // * "foo/bar" matches for "foo/bar/baz"
    // * "foo/bar" does not match for "foo/bar-1"
    // * "foo/bar/" does not match for "foo/bar"
    // * "foo/bar/" matches for "foo/bar/baz"
    //
    // Check for an empty path, an exact match, or a substring match
    // with the next character being a forward slash.
    if(stripped === "" ||
       (id.indexOf(stripped) === 0 &&
        (id.length === path.length || id[stripped.length] === '/'))) {
      return normalizeExt(id.replace(path, uri));
    }
  }
  return void 0; // otherwise we raise a warning, see bug 910304
});
Loader.resolveURI = resolveURI;

// Creates version of `require` that will be exposed to the given `module`
// in the context of the given `loader`. Each module gets own limited copy
// of `require` that is allowed to load only a modules that are associated
// with it during link time.
const Require = iced(function Require(loader, requirer) {
  let {
    modules, mapping, resolve: loaderResolve, load,
    manifest, rootURI, isNative, requireMap,
    requireHook
  } = loader;

  function require(id) {
    if (!id) // Throw if `id` is not passed.
      throw Error('You must provide a module name when calling require() from '
                  + requirer.id, requirer.uri);

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
    }
    else if (isJSMURI(uri)) {
      module = modules[uri] = Module(requirement, uri);
      module.exports = Cu.import(uri, {});
      freeze(module);
    }
    else if (isJSONURI(uri)) {
      let data;

      // First attempt to load and parse json uri
      // ex: `test.json`
      // If that doesn't exist, check for `test.json.js`
      // for node parity
      try {
        data = JSON.parse(readURI(uri));
        module = modules[uri] = Module(requirement, uri);
        module.exports = data;
        freeze(module);
      }
      catch (err) {
        // If error thrown from JSON parsing, throw that, do not
        // attempt to find .json.js file
        if (err && /JSON\.parse/.test(err.message))
          throw err;
        uri = uri + '.js';
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
        freeze(load(loader, module));
      }
      catch (e) {
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
    if (!id) // Throw if `id` is not passed.
      throw Error('you must provide a module name when calling require() from '
                  + requirer.id, requirer.uri);

    let requirement, uri;

    // TODO should get native Firefox modules before doing node-style lookups
    // to save on loading time
    if (isNative) {
      // If a requireMap is available from `generateMap`, use that to
      // immediately resolve the node-style mapping.
      // TODO: write more tests for this use case
      if (requireMap && requireMap[requirer.id])
        requirement = requireMap[requirer.id][id];

      let { overrides } = manifest.jetpack;
      for (let key in overrides) {
        // ignore any overrides using relative keys
        if (/^[\.\/]/.test(key)) {
          continue;
        }

        // If the override is for x -> y,
        // then using require("x/lib/z") to get reqire("y/lib/z")
        // should also work
        if (id == key || (id.substr(0, key.length + 1) == (key + "/"))) {
          id = overrides[key] + id.substr(key.length);
          id = id.replace(/^[\.\/]+/, "./");
          if (id.substr(0, 2) == "./") {
            id = "" + id.substr(2);
          }
        }
      }

      // For native modules, we want to check if it's a module specified
      // in 'modules', like `chrome`, or `@loader` -- if it exists,
      // just set the uri to skip resolution
      if (!requirement && modules[id])
        uri = requirement = id;

      // If no requireMap was provided, or resolution not found in
      // the requireMap, and not a npm dependency, attempt a runtime lookup
      if (!requirement && !isNodeModule(id)) {
        // If `isNative` defined, this is using the new, native-style
        // loader, not cuddlefish, so lets resolve using node's algorithm
        // and get back a path that needs to be resolved via paths mapping
        // in `resolveURI`
        requirement = loaderResolve(id, requirer.id, {
          manifest: manifest,
          rootURI: rootURI
        });
      }

      // If not found in the map, not a node module, and wasn't able to be
      // looked up, it's something
      // found in the paths most likely, like `sdk/tabs`, which should
      // be resolved relatively if needed using traditional resolve
      if (!requirement) {
        requirement = isRelative(id) ? Loader.resolve(id, requirer.id) : id;
      }
    }
    else {
      // Resolve `id` to its requirer if it's relative.
      requirement = requirer ? loaderResolve(id, requirer.id) : id;
    }

    // Resolves `uri` of module using loaders resolve function.
    uri = uri || resolveURI(requirement, mapping);

    // Throw if `uri` can not be resolved.
    if (!uri) {
      throw Error('Module: Can not resolve "' + id + '" module required by ' +
                  requirer.id + ' located at ' + requirer.uri, requirer.uri);
    }

    return { uri: uri, requirement: requirement };
  }

  // Expose the `resolve` function for this `Require` instance
  require.resolve = _require.resolve = function resolve(id) {
    let { uri } = getRequirements(id);
    return uri;
  }

  // Make `require.main === module` evaluate to true in main module scope.
  require.main = loader.main === requirer ? requirer : undefined;
  return iced(require);
});
Loader.Require = Require;

const main = iced(function main(loader, id) {
  // If no main entry provided, and native loader is used,
  // read the entry in the manifest
  if (!id && loader.isNative)
    id = getManifestMain(loader.manifest);
  let uri = resolveURI(id, loader.mapping);
  let module = loader.main = loader.modules[uri] = Module(id, uri);
  return loader.load(loader, module).exports;
});
Loader.main = main;

// Makes module object that is made available to CommonJS modules when they
// are evaluated, along with `exports` and `require`.
const Module = iced(function Module(id, uri) {
  return create(null, {
    id: { enumerable: true, value: id },
    exports: { enumerable: true, writable: true, value: create(null),
               configurable: true },
    uri: { value: uri }
  });
});
Loader.Module = Module;

// Takes `loader`, and unload `reason` string and notifies all observers that
// they should cleanup after them-self.
const unload = iced(function unload(loader, reason) {
  // Clear the nodeResolverCache when the loader is unloaded.
  Loader.nodeResolverCache.clear();

  // subject is a unique object created per loader instance.
  // This allows any code to cleanup on loader unload regardless of how
  // it was loaded. To handle unload for specific loader subject may be
  // asserted against loader.destructor or require('@loader/unload')
  // Note: We don not destroy loader's module cache or sandboxes map as
  // some modules may do cleanup in subsequent turns of event loop. Destroying
  // cache may cause module identity problems in such cases.
  let subject = { wrappedJSObject: loader.destructor };
  notifyObservers(subject, 'sdk:loader:destroy', reason);
});
Loader.unload = unload;

// Function makes new loader that can be used to load CommonJS modules
// described by a given `options.manifest`. Loader takes following options:
// - `globals`: Optional map of globals, that all module scopes will inherit
//   from. Map is also exposed under `globals` property of the returned loader
//   so it can be extended further later. Defaults to `{}`.
// - `modules` Optional map of built-in module exports mapped by module id.
//   These modules will incorporated into module cache. Each module will be
//   frozen.
// - `resolve` Optional module `id` resolution function. If given it will be
//   used to resolve module URIs, by calling it with require term, requirer
//   module object (that has `uri` property) and `baseURI` of the loader.
//   If `resolve` does not returns `uri` string exception will be thrown by
//   an associated `require` call.
function Loader(options) {
  if (options.sharedGlobalBlacklist && !options.sharedGlobalBlocklist) {
    options.sharedGlobalBlocklist = options.sharedGlobalBlacklist;
  }
  let {
    modules, globals, resolve, paths, rootURI, manifest, requireMap, isNative,
    metadata, sharedGlobal, sharedGlobalBlocklist, checkCompatibility, waiveIntereposition
  } = override({
    paths: {},
    modules: {},
    globals: {
      get console() {
        // Import Console.jsm from here to prevent loading it until someone uses it
        let { ConsoleAPI } = Cu.import("resource://gre/modules/Console.jsm");
        let console = new ConsoleAPI({
          consoleID: options.id ? "addon/" + options.id : ""
        });
        Object.defineProperty(this, "console", { value: console });
        return this.console;
      }
    },
    checkCompatibility: false,
    resolve: options.isNative ?
      // Make the returned resolve function have the same signature
      (id, requirer) => Loader.nodeResolve(id, requirer, { rootURI: rootURI }) :
      Loader.resolve,
    sharedGlobalBlocklist: ["sdk/indexed-db"],
    waiveIntereposition: false
  }, options);

  // Create overrides defaults, none at the moment
  if (typeof manifest != "object" || !manifest) {
    manifest = {};
  }
  if (typeof manifest.jetpack != "object" || !manifest.jetpack) {
    manifest.jetpack = {
      overrides: {}
    };
  }
  if (typeof manifest.jetpack.overrides != "object" || !manifest.jetpack.overrides) {
    manifest.jetpack.overrides = {};
  }

  // We create an identity object that will be dispatched on an unload
  // event as subject. This way unload listeners will be able to assert
  // which loader is unloaded. Please note that we intentionally don't
  // use `loader` as subject to prevent a loader access leakage through
  // observer notifications.
  let destructor = freeze(create(null));

  let mapping = sortPaths(paths);

  // Define pseudo modules.
  modules = override({
    '@loader/unload': destructor,
    '@loader/options': options,
    'chrome': { Cc: Cc, Ci: Ci, Cu: Cu, Cr: Cr, Cm: Cm,
                CC: bind(CC, Components), components: Components,
                // `ChromeWorker` has to be inject in loader global scope.
                // It is done by bootstrap.js:loadSandbox for the SDK.
                ChromeWorker: ChromeWorker
    }
  }, modules);

  const builtinModuleExports = modules;
  modules = keys(modules).reduce(function(result, id) {
    // We resolve `uri` from `id` since modules are cached by `uri`.
    let uri = resolveURI(id, mapping);
    // In native loader, the mapping will not contain values for
    // pseudomodules -- store them as their ID rather than the URI
    if (isNative && !uri)
      uri = id;
    let module = Module(id, uri);

    // Lazily expose built-in modules in order to
    // allow them to be loaded lazily.
    Object.defineProperty(module, "exports", {
      enumerable: true,
      get: function() {
        return builtinModuleExports[id];
      }
    });

    result[uri] = freeze(module);
    return result;
  }, {});

  let sharedGlobalSandbox;
  if (sharedGlobal) {
    // Create the unique sandbox we will be using for all modules,
    // so that we prevent creating a new comportment per module.
    // The side effect is that all modules will share the same
    // global objects.
    sharedGlobalSandbox = Sandbox({
      name: "Addon-SDK",
      wantXrays: false,
      wantGlobalProperties: [],
      invisibleToDebugger: options.invisibleToDebugger || false,
      metadata: {
        addonID: options.id,
        URI: "Addon-SDK"
      },
      prototype: options.sandboxPrototype || {}
    });
  }

  // Loader object is just a representation of a environment
  // state. We freeze it and mark make it's properties non-enumerable
  // as they are pure implementation detail that no one should rely upon.
  let returnObj = {
    destructor: { enumerable: false, value: destructor },
    globals: { enumerable: false, value: globals },
    mapping: { enumerable: false, value: mapping },
    // Map of module objects indexed by module URIs.
    modules: { enumerable: false, value: modules },
    metadata: { enumerable: false, value: metadata },
    sharedGlobalSandbox: { enumerable: false, value: sharedGlobalSandbox },
    sharedGlobalBlocklist: { enumerable: false, value: sharedGlobalBlocklist },
    sharedGlobalBlacklist: { enumerable: false, value: sharedGlobalBlocklist },
    // Map of module sandboxes indexed by module URIs.
    sandboxes: { enumerable: false, value: {} },
    resolve: { enumerable: false, value: resolve },
    // ID of the addon, if provided.
    id: { enumerable: false, value: options.id },
    // Whether the modules loaded should be ignored by the debugger
    invisibleToDebugger: { enumerable: false,
                           value: options.invisibleToDebugger || false },
    load: { enumerable: false, value: options.load || load },
    checkCompatibility: { enumerable: false, value: checkCompatibility },
    requireHook: { enumerable: false, value: options.requireHook },
    loadModuleHook: { enumerable: false, value: options.loadModuleHook },
    // Main (entry point) module, it can be set only once, since loader
    // instance can have only one main module.
    main: new function() {
      let main;
      return {
        enumerable: false,
        get: function() { return main; },
        // Only set main if it has not being set yet!
        set: function(module) { main = main || module; }
      }
    }
  };

  if (isNative) {
    returnObj.isNative = { enumerable: false, value: true };
    returnObj.manifest = { enumerable: false, value: manifest };
    returnObj.requireMap = { enumerable: false, value: requireMap };
    returnObj.rootURI = { enumerable: false, value: addTrailingSlash(rootURI) };
  }

  return freeze(create(null, returnObj));
};
Loader.Loader = Loader;

var isJSONURI = uri => uri.substr(-5) === '.json';
var isJSMURI = uri => uri.substr(-4) === '.jsm';
var isJSURI = uri => uri.substr(-3) === '.js';
var isAbsoluteURI = uri => uri.indexOf("resource://") >= 0 ||
                           uri.indexOf("chrome://") >= 0 ||
                           uri.indexOf("file://") >= 0
var isRelative = id => id[0] === '.'

const generateMap = iced(function generateMap(options, callback) {
  let { rootURI, resolve, paths } = override({
    paths: {},
    resolve: Loader.nodeResolve
  }, options);

  rootURI = addTrailingSlash(rootURI);

  let manifest;
  let manifestURI = join(rootURI, 'package.json');

  if (rootURI)
    manifest = JSON.parse(readURI(manifestURI));
  else
    throw new Error('No `rootURI` given to generate map');

  let main = getManifestMain(manifest);

  findAllModuleIncludes(main, {
    resolve: resolve,
    manifest: manifest,
    rootURI: rootURI
  }, {}, callback);

});
Loader.generateMap = generateMap;

// Default `main` entry to './index.js' and ensure is relative,
// since node allows 'lib/index.js' without relative `./`
function getManifestMain (manifest) {
  let main = manifest.main || './index.js';
  return isRelative(main) ? main : './' + main;
}

function findAllModuleIncludes (uri, options, results, callback) {
  let { resolve, manifest, rootURI } = options;
  results = results || {};

  // Abort if JSON or JSM
  if (isJSONURI(uri) || isJSMURI(uri)) {
    callback(results);
    return;
  }

  findModuleIncludes(join(rootURI, uri), modules => {
    // If no modules are included in the file, just call callback immediately
    if (!modules.length) {
      callback(results);
      return;
    }

    results[uri] = modules.reduce((agg, mod) => {
      let resolved = resolve(mod, uri, { manifest: manifest, rootURI: rootURI });

      // If resolution found, store the resolution; otherwise,
      // skip storing it as runtime lookup will handle this
      if (!resolved)
        return agg;
      agg[mod] = resolved;
      return agg;
    }, {});

    let includes = keys(results[uri]);
    let count = 0;
    let subcallback = () => { if (++count >= includes.length) callback(results) };
    includes.map(id => {
      let moduleURI = results[uri][id];
      if (!results[moduleURI])
        findAllModuleIncludes(moduleURI, options, results, subcallback);
      else
        subcallback();
    });
  });
}

// From Substack's detector
// https://github.com/substack/node-detective
//
// Given a resource URI or source, return an array of strings passed into
// the require statements from the source
function findModuleIncludes (uri, callback) {
  let src = isAbsoluteURI(uri) ? readURI(uri) : uri;
  let modules = [];

  walk(src, function (node) {
    if (isRequire(node))
      modules.push(node.arguments[0].value);
  });

  callback(modules);
}

function walk (src, callback) {
  // Import Reflect.jsm from here to prevent loading it until someone uses it
  let { Reflect } = Cu.import("resource://gre/modules/reflect.jsm", {});
  let nodes = Reflect.parse(src);
  traverse(nodes, callback);
}

function traverse (node, cb) {
  if (Array.isArray(node)) {
    node.map(x => {
      if (x != null) {
        x.parent = node;
        traverse(x, cb);
      }
    });
  }
  else if (node && typeof node === 'object') {
    cb(node);
    keys(node).map(key => {
      if (key === 'parent' || !node[key]) return;
      if (typeof node[key] === "object")
        node[key].parent = node;
      traverse(node[key], cb);
    });
  }
}

// From Substack's detector
// https://github.com/substack/node-detective
// Check an AST node to see if its a require statement.
// A modification added to only evaluate to true if it actually
// has a value being passed in as an argument
function isRequire (node) {
  var c = node.callee;
  return c
    && node.type === 'CallExpression'
    && c.type === 'Identifier'
    && c.name === 'require'
    && node.arguments.length
   && node.arguments[0].type === 'Literal';
}

module.exports = iced(Loader);
});
