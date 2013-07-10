/* vim:set ts=2 sw=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
;(function(id, factory) { // Module boilerplate :(
  if (typeof(define) === 'function') { // RequireJS
    define(factory);
  } else if (typeof(require) === 'function') { // CommonJS
    factory.call(this, require, exports, module);
  } else if (~String(this).indexOf('BackstagePass')) { // JSM
    this[factory.name] = {};
    factory(function require(uri) {
      var imports = {};
      this['Components'].utils.import(uri, imports);
      return imports;
    }, this[factory.name], { uri: __URI__, id: id });
    this.EXPORTED_SYMBOLS = [factory.name];
  } else if (~String(this).indexOf('Sandbox')) { // Sandbox
    factory(function require(uri) {}, this, { uri: __URI__, id: id });
  } else {  // Browser or alike
    var globals = this
    factory(function require(id) {
      return globals[id];
    }, (globals[id] = {}), { uri: document.location.href + '#' + id, id: id });
  }
}).call(this, 'loader', function Loader(require, exports, module) {

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

// Define some shortcuts.
const bind = Function.call.bind(Function.bind);
const getOwnPropertyNames = Object.getOwnPropertyNames;
const getOwnPropertyDescriptor = Object.getOwnPropertyDescriptor;
const define = Object.defineProperties;
const prototypeOf = Object.getPrototypeOf;
const create = Object.create;
const keys = Object.keys;

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
  getOwnPropertyNames(object).forEach(function(name) {
    value[name] = getOwnPropertyDescriptor(object, name)
  });
  return value;
});
exports.descriptor = descriptor;

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
  f.prototype = undefined;
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
  getOwnPropertyNames(extension).forEach(function(name) {
    properties[name] = extension[name];
  });
  return define({}, properties);
});
exports.override = override;


function sourceURI(uri) { return String(uri).split(" -> ").pop(); }
exports.sourceURI = iced(sourceURI);

function isntLoaderFrame(frame) { return frame.fileName !== module.uri }

var parseStack = iced(function parseStack(stack) {
  let lines = String(stack).split("\n");
  return lines.reduce(function(frames, line) {
    if (line) {
      let atIndex = line.indexOf("@");
      let columnIndex = line.lastIndexOf(":");
      let fileName = sourceURI(line.slice(atIndex + 1, columnIndex));
      let lineNumber = parseInt(line.slice(columnIndex + 1));
      let name = line.slice(0, atIndex).split("(").shift();
      frames.unshift({
        fileName: fileName,
        name: name,
        lineNumber: lineNumber
      });
    }
    return frames;
  }, []);
})
exports.parseStack = parseStack

var serializeStack = iced(function serializeStack(frames) {
  return frames.reduce(function(stack, frame) {
    return frame.name + "@" +
           frame.fileName + ":" +
           frame.lineNumber + "\n" +
           stack;
  }, "");
})
exports.serializeStack = serializeStack

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
    sandboxPrototype: 'prototype' in options ? options.prototype : {},
    sameGroupAs: 'sandbox' in options ? options.sandbox : null
  };

  // Make `options.sameGroupAs` only if `sandbox` property is passed,
  // otherwise `Cu.Sandbox` will throw.
  if (!options.sameGroupAs)
    delete options.sameGroupAs;

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
exports.Sandbox = Sandbox;

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
exports.evaluate = evaluate;

// Populates `exports` of the given CommonJS `module` object, in the context
// of the given `loader` by evaluating code associated with it.
const load = iced(function load(loader, module) {
  let { sandboxes, globals } = loader;
  let require = Require(loader, module);

  let sandbox = sandboxes[module.uri] = Sandbox({
    name: module.uri,
    // Get an existing module sandbox, if any, so we can reuse its compartment
    // when creating the new one to reduce memory consumption.
    sandbox: sandboxes[keys(sandboxes).shift()],
    // We expose set of properties defined by `CommonJS` specification via
    // prototype of the sandbox. Also globals are deeper in the prototype
    // chain so that each module has access to them as well.
    prototype: create(globals, descriptor({
      require: require,
      module: module,
      exports: module.exports
    })),
    wantXrays: false
  });

  try {
    evaluate(sandbox, module.uri);
  } catch (error) {
    let { message, fileName, lineNumber } = error;
    let stack = error.stack || Error().stack;
    let frames = parseStack(stack).filter(isntLoaderFrame);
    let toString = String(error);

    // Note that `String(error)` where error is from subscript loader does
    // not puts `:` after `"Error"` unlike regular errors thrown by JS code.
    // If there is a JS stack then this error has already been handled by an
    // inner module load.
    if (String(error) === "Error opening input stream (invalid filename?)") {
      let caller = frames.slice(0).pop();
      fileName = caller.fileName;
      lineNumber = caller.lineNumber;
      message = "Module `" + module.id + "` is not found at " + module.uri;
      toString = message;
    }

    let prototype = typeof(error) === "object" ? error.constructor.prototype :
                    Error.prototype;

    throw create(prototype, {
      message: { value: message, writable: true, configurable: true },
      fileName: { value: fileName, writable: true, configurable: true },
      lineNumber: { value: lineNumber, writable: true, configurable: true },
      stack: { value: serializeStack(frames), writable: true, configurable: true },
      toString: { value: function() toString, writable: true, configurable: true },
    });
  }

  if (module.exports && typeof(module.exports) === 'object')
    freeze(module.exports);

  return module;
});
exports.load = load;

// Utility function to check if id is relative.
function isRelative(id) { return id[0] === '.'; }
// Utility function to normalize module `uri`s so they have `.js` extension.
function normalize(uri) { return uri.substr(-3) === '.js' ? uri : uri + '.js'; }
// Utility function to join paths. In common case `base` is a
// `requirer.uri` but in some cases it may be `baseURI`. In order to
// avoid complexity we require `baseURI` with a trailing `/`.
const resolve = iced(function resolve(id, base) {
  if (!isRelative(id)) return id;
  let paths = id.split('/');
  let result = base.split('/');
  result.pop();
  while (paths.length) {
    let path = paths.shift();
    if (path === '..')
      result.pop();
    else if (path !== '.')
      result.push(path);
  }
  return result.join('/');
});
exports.resolve = resolve;

const resolveURI = iced(function resolveURI(id, mapping) {
  let count = mapping.length, index = 0;
  while (index < count) {
    let [ path, uri ] = mapping[index ++];
    if (id.indexOf(path) === 0)
      return normalize(id.replace(path, uri));
  }
});
exports.resolveURI = resolveURI;

// Creates version of `require` that will be exposed to the given `module`
// in the context of the given `loader`. Each module gets own limited copy
// of `require` that is allowed to load only a modules that are associated
// with it during link time.
const Require = iced(function Require(loader, requirer) {
  let { modules, mapping, resolve, load } = loader;

  function require(id) {
    if (!id) // Throw if `id` is not passed.
      throw Error('you must provide a module name when calling require() from '
                  + requirer.id, requirer.uri);

    // Resolve `id` to its requirer if it's relative.
    let requirement = requirer ? resolve(id, requirer.id) : id;

    // Resolves `uri` of module using loaders resolve function.
    let uri = resolveURI(requirement, mapping);

    if (!uri) // Throw if `uri` can not be resolved.
      throw Error('Module: Can not resolve "' + id + '" module required by ' +
                  requirer.id + ' located at ' + requirer.uri, requirer.uri);

    let module = null;
    // If module is already cached by loader then just use it.
    if (uri in modules) {
      module = modules[uri];
    }
    // Otherwise load and cache it. We also freeze module to prevent it from
    // further changes at runtime.
    else {
      module = modules[uri] = Module(requirement, uri);
      freeze(load(loader, module));
    }

    return module.exports;
  }
  // Make `require.main === module` evaluate to true in main module scope.
  require.main = loader.main === requirer ? requirer : undefined;
  return iced(require);
});
exports.Require = Require;

const main = iced(function main(loader, id) {
  let uri = resolveURI(id, loader.mapping)
  let module = loader.main = loader.modules[uri] = Module(id, uri);
  return loader.load(loader, module).exports;
});
exports.main = main;

// Makes module object that is made available to CommonJS modules when they
// are evaluated, along with `exports` and `require`.
const Module = iced(function Module(id, uri) {
  return create(null, {
    id: { enumerable: true, value: id },
    exports: { enumerable: true, writable: true, value: create(null) },
    uri: { value: uri }
  });
});
exports.Module = Module;

// Takes `loader`, and unload `reason` string and notifies all observers that
// they should cleanup after them-self.
const unload = iced(function unload(loader, reason) {
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
exports.unload = unload;

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
const Loader = iced(function Loader(options) {
  let { modules, globals, resolve, paths } = override({
    paths: {},
    modules: {},
    globals: {},
    resolve: exports.resolve
  }, options);

  // We create an identity object that will be dispatched on an unload
  // event as subject. This way unload listeners will be able to assert
  // which loader is unloaded. Please note that we intentionally don't
  // use `loader` as subject to prevent a loader access leakage through
  // observer notifications.
  let destructor = freeze(create(null));

  // Make mapping array that is sorted from longest path to shortest path
  // to allow overlays.
  let mapping = keys(paths).
    sort(function(a, b) { return b.length - a.length }).
    map(function(path) { return [ path, paths[path] ] });

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

  modules = keys(modules).reduce(function(result, id) {
    // We resolve `uri` from `id` since modules are cached by `uri`.
    let uri = resolveURI(id, mapping);
    let module = Module(id, uri);
    module.exports = freeze(modules[id]);
    result[uri] = freeze(module);
    return result;
  }, {});

  // Loader object is just a representation of a environment
  // state. We freeze it and mark make it's properties non-enumerable
  // as they are pure implementation detail that no one should rely upon.
  return freeze(create(null, {
    destructor: { enumerable: false, value: destructor },
    globals: { enumerable: false, value: globals },
    mapping: { enumerable: false, value: mapping },
    // Map of module objects indexed by module URIs.
    modules: { enumerable: false, value: modules },
    // Map of module sandboxes indexed by module URIs.
    sandboxes: { enumerable: false, value: {} },
    resolve: { enumerable: false, value: resolve },
    load: { enumerable: false, value: options.load || load },
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
  }));
});
exports.Loader = Loader;

});

