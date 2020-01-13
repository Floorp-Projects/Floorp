/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global BigInt */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

loader.lazyRequireGetter(
  this,
  "Reflect",
  "resource://gre/modules/reflect.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "formatCommand",
  "devtools/server/actors/webconsole/commands",
  true
);
loader.lazyRequireGetter(
  this,
  "isCommand",
  "devtools/server/actors/webconsole/commands",
  true
);
loader.lazyRequireGetter(
  this,
  "WebConsoleCommands",
  "devtools/server/actors/webconsole/utils",
  true
);

loader.lazyRequireGetter(
  this,
  "LongStringActor",
  "devtools/server/actors/string",
  true
);

function isObject(value) {
  return Object(value) === value;
}

/**
 * Evaluates a string using the debugger API.
 *
 * To allow the variables view to update properties from the Web Console we
 * provide the "selectedObjectActor" mechanism: the Web Console tells the
 * ObjectActor ID for which it desires to evaluate an expression. The
 * Debugger.Object pointed at by the actor ID is bound such that it is
 * available during expression evaluation (executeInGlobalWithBindings()).
 *
 * Example:
 *   _self['foobar'] = 'test'
 * where |_self| refers to the desired object.
 *
 * The |frameActor| property allows the Web Console client to provide the
 * frame actor ID, such that the expression can be evaluated in the
 * user-selected stack frame.
 *
 * For the above to work we need the debugger and the Web Console to share
 * a connection, otherwise the Web Console actor will not find the frame
 * actor.
 *
 * The Debugger.Frame comes from the jsdebugger's Debugger instance, which
 * is different from the Web Console's Debugger instance. This means that
 * for evaluation to work, we need to create a new instance for the Web
 * Console Commands helpers - they need to be Debugger.Objects coming from the
 * jsdebugger's Debugger instance.
 *
 * When |selectedObjectActor| is used objects can come from different iframes,
 * from different domains. To avoid permission-related errors when objects
 * come from a different window, we also determine the object's own global,
 * such that evaluation happens in the context of that global. This means that
 * evaluation will happen in the object's iframe, rather than the top level
 * window.
 *
 * @param string string
 *        String to evaluate.
 * @param object [options]
 *        Options for evaluation:
 *        - selectedObjectActor: the ObjectActor ID to use for evaluation.
 *          |evalWithBindings()| will be called with one additional binding:
 *          |_self| which will point to the Debugger.Object of the given
 *          ObjectActor. Executes with the top level window as the global.
 *        - frameActor: the FrameActor ID to use for evaluation. The given
 *        debugger frame is used for evaluation, instead of the global window.
 *        - selectedNodeActor: the NodeActor ID of the currently selected node
 *        in the Inspector (or null, if there is no selection). This is used
 *        for helper functions that make reference to the currently selected
 *        node, like $0.
 *         - url: the url to evaluate the script as. Defaults to
 *         "debugger eval code".
 * @return object
 *         An object that holds the following properties:
 *         - dbg: the debugger where the string was evaluated.
 *         - frame: (optional) the frame where the string was evaluated.
 *         - window: the Debugger.Object for the global where the string was
 *         evaluated.
 *         - result: the result of the evaluation.
 *         - helperResult: any result coming from a Web Console commands
 *         function.
 */
exports.evalWithDebugger = function(string, options = {}, webConsole) {
  const evalString = getEvalInput(string);
  const { frame, dbg } = getFrameDbg(options, webConsole);

  // early return for replay
  if (dbg.replaying) {
    if (options.eager) {
      throw new Error("Eager evaluations are not supported while replaying");
    }
    return evalReplay(frame, dbg, evalString);
  }

  const { dbgWindow, bindSelf } = getDbgWindow(options, dbg, webConsole);
  const helpers = getHelpers(dbgWindow, options, webConsole);
  const { bindings, helperCache } = bindCommands(
    isCommand(string),
    dbgWindow,
    bindSelf,
    frame,
    helpers
  );

  // Ready to evaluate the string.
  helpers.evalInput = string;
  const evalOptions =
    typeof options.url === "string" ? { url: options.url } : null;

  updateConsoleInputEvaluation(dbg, dbgWindow, webConsole);

  let sideEffectData = null;
  if (options.eager) {
    sideEffectData = preventSideEffects(dbg);
  }

  const result = getEvalResult(
    evalString,
    evalOptions,
    bindings,
    frame,
    dbgWindow
  );

  if (options.eager) {
    allowSideEffects(dbg, sideEffectData);
  }

  // Attempt to initialize any declarations found in the evaluated string
  // since they may now be stuck in an "initializing" state due to the
  // error. Already-initialized bindings will be ignored.
  if (!frame && result && "throw" in result) {
    parseErrorOutput(dbgWindow, string);
  }

  const { helperResult } = helpers;

  // Clean up helpers helpers and bindings
  delete helpers.evalInput;
  delete helpers.helperResult;
  delete helpers.selectedNode;
  cleanupBindings(bindings, helperCache);

  return {
    result,
    helperResult,
    dbg,
    frame,
    window: dbgWindow,
  };
};

function getEvalResult(string, evalOptions, bindings, frame, dbgWindow) {
  if (frame) {
    return frame.evalWithBindings(string, bindings, evalOptions);
  }
  return dbgWindow.executeInGlobalWithBindings(
    string,
    bindings,
    evalOptions
  );
}

function parseErrorOutput(dbgWindow, string) {
  // Reflect is not usable in workers, so return early to avoid logging an error
  // to the console when loading it.
  if (isWorker) {
    return;
  }

  let ast;
  // Parse errors will raise an exception. We can/should ignore the error
  // since it's already being handled elsewhere and we are only interested
  // in initializing bindings.
  try {
    ast = Reflect.parse(string);
  } catch (ex) {
    return;
  }
  for (const line of ast.body) {
    // Only let and const declarations put bindings into an
    // "initializing" state.
    if (!(line.kind == "let" || line.kind == "const")) {
      continue;
    }

    const identifiers = [];
    for (const decl of line.declarations) {
      switch (decl.id.type) {
        case "Identifier":
          // let foo = bar;
          identifiers.push(decl.id.name);
          break;
        case "ArrayPattern":
          // let [foo, bar]    = [1, 2];
          // let [foo=99, bar] = [1, 2];
          for (const e of decl.id.elements) {
            if (e.type == "Identifier") {
              identifiers.push(e.name);
            } else if (e.type == "AssignmentExpression") {
              identifiers.push(e.left.name);
            }
          }
          break;
        case "ObjectPattern":
          // let {bilbo, my}    = {bilbo: "baggins", my: "precious"};
          // let {blah: foo}    = {blah: yabba()}
          // let {blah: foo=99} = {blah: yabba()}
          for (const prop of decl.id.properties) {
            // key
            if (prop.key.type == "Identifier") {
              identifiers.push(prop.key.name);
            }
            // value
            if (prop.value.type == "Identifier") {
              identifiers.push(prop.value.name);
            } else if (prop.value.type == "AssignmentExpression") {
              identifiers.push(prop.value.left.name);
            }
          }
          break;
      }
    }

    for (const name of identifiers) {
      dbgWindow.forceLexicalInitializationByName(name);
    }
  }
}

function preventSideEffects(dbg) {
  if (dbg.onEnterFrame || dbg.onNativeCall) {
    throw new Error("Debugger has hook installed");
  }

  const data = {
    executedScripts: new Set(),
    debuggees: dbg.getDebuggees(),

    handler: {
      hit: () => null,
    },
  };

  dbg.addAllGlobalsAsDebuggees();

  dbg.onEnterFrame = frame => {
    const script = frame.script;

    if (data.executedScripts.has(script)) {
      return;
    }
    data.executedScripts.add(script);

    const offsets = script.getEffectfulOffsets();
    for (const offset of offsets) {
      script.setBreakpoint(offset, data.handler);
    }
  };

  dbg.onNativeCall = (callee, reason) => {
    // Getters are never considered effectful, and setters are always effectful.
    // Natives called normally are handled with a whitelist.
    if (
      reason == "get" ||
      (reason == "call" && nativeHasNoSideEffects(callee))
    ) {
      // Returning undefined causes execution to continue normally.
      return undefined;
    }
    // Returning null terminates the current evaluation.
    return null;
  };

  return data;
}

function allowSideEffects(dbg, data) {
  for (const script of data.executedScripts) {
    script.clearBreakpoint(data.handler);
  }

  for (const global of dbg.getDebuggees()) {
    if (!data.debuggees.includes(global)) {
      dbg.removeDebuggee(global);
    }
  }

  dbg.onEnterFrame = undefined;
  dbg.onNativeCall = undefined;
}

// Native functions which are considered to be side effect free.
let gSideEffectFreeNatives; // string => Array(Function)

function ensureSideEffectFreeNatives() {
  if (gSideEffectFreeNatives) {
    return;
  }

  const natives = [
    Array,
    Array.from,
    Array.isArray,
    Array.of,
    Array.prototype.concat,
    Array.prototype.entries,
    Array.prototype.every,
    Array.prototype.fill,
    Array.prototype.filter,
    Array.prototype.find,
    Array.prototype.findIndex,
    Array.prototype.flat,
    Array.prototype.flatMap,
    Array.prototype.forEach,
    Array.prototype.includes,
    Array.prototype.indexOf,
    Array.prototype.join,
    Array.prototype.keys,
    Array.prototype.lastIndexOf,
    Array.prototype.map,
    Array.prototype.reduce,
    Array.prototype.reduceRight,
    Array.prototype.slice,
    Array.prototype.some,
    Array.prototype.values,
    ArrayBuffer,
    ArrayBuffer.isView,
    ArrayBuffer.prototype.slice,
    BigInt,
    ...allProperties(BigInt),
    Boolean,
    DataView,
    Date,
    Date.now,
    Date.parse,
    Date.UTC,
    ...matchingProperties(Date.prototype, /^get/),
    ...matchingProperties(Date.prototype, /^to.*?String$/),
    Error,
    Function,
    Function.prototype.apply,
    Function.prototype.bind,
    Function.prototype.call,
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    // These will apply to other typed array prototypes.
    Int8Array.prototype.entries,
    Int8Array.prototype.every,
    Int8Array.prototype.filter,
    Int8Array.prototype.find,
    Int8Array.prototype.findIndex,
    Int8Array.prototype.forEach,
    Int8Array.prototype.indexOf,
    Int8Array.prototype.includes,
    Int8Array.prototype.join,
    Int8Array.prototype.keys,
    Int8Array.prototype.lastIndexOf,
    Int8Array.prototype.map,
    Int8Array.prototype.reduce,
    Int8Array.prototype.reduceRight,
    Int8Array.prototype.slice,
    Int8Array.prototype.some,
    Int8Array.prototype.subarray,
    Int8Array.prototype.values,
    ...allProperties(JSON),
    Map,
    Map.prototype.forEach,
    Map.prototype.get,
    Map.prototype.has,
    Map.prototype.entries,
    Map.prototype.keys,
    Map.prototype.values,
    ...allProperties(Math),
    Number,
    ...allProperties(Number),
    ...allProperties(Number.prototype),
    Object,
    Object.create,
    Object.keys,
    Object.entries,
    Object.getOwnPropertyDescriptor,
    Object.getOwnPropertyDescriptors,
    Object.getOwnPropertyNames,
    Object.getOwnPropertySymbols,
    Object.getPrototypeOf,
    Object.is,
    Object.isExtensible,
    Object.isFrozen,
    Object.isSealed,
    Object.values,
    Object.prototype.hasOwnProperty,
    Object.prototype.isPrototypeOf,
    RegExp,
    RegExp.prototype.exec,
    RegExp.prototype.test,
    Set,
    Set.prototype.entries,
    Set.prototype.forEach,
    Set.prototype.has,
    Set.prototype.values,
    String,
    ...allProperties(String),
    ...allProperties(String.prototype),
    Symbol,
    Symbol.keyFor,
    WeakMap,
    WeakMap.prototype.get,
    WeakMap.prototype.has,
    WeakSet,
    WeakSet.prototype.has,
    decodeURI,
    decodeURIComponent,
    encodeURI,
    encodeURIComponent,
    escape,
    isFinite,
    isNaN,
    unescape,
  ];

  const map = new Map();
  for (const n of natives) {
    if (!map.has(n.name)) {
      map.set(n.name, []);
    }
    map.get(n.name).push(n);
  }

  gSideEffectFreeNatives = map;

  function matchingProperties(obj, regexp) {
    return Object.getOwnPropertyNames(obj)
      .filter(n => regexp.test(n))
      .map(n => obj[n])
      .filter(v => typeof v == "function");
  }

  function allProperties(obj) {
    return matchingProperties(obj, /./);
  }
}

function nativeHasNoSideEffects(fn) {
  // Natives with certain names are always considered side effect free.
  switch (fn.name) {
    case "toString":
    case "toLocaleString":
    case "valueOf":
      return true;
  }

  ensureSideEffectFreeNatives();

  const natives = gSideEffectFreeNatives.get(fn.name);
  return natives && natives.some(n => fn.isSameNative(n));
}

function updateConsoleInputEvaluation(dbg, dbgWindow, webConsole) {
  // Adopt webConsole._lastConsoleInputEvaluation value in the new debugger,
  // to prevent "Debugger.Object belongs to a different Debugger" exceptions
  // related to the $_ bindings if the debugger object is changed from the
  // last evaluation.
  if (webConsole._lastConsoleInputEvaluation) {
    webConsole._lastConsoleInputEvaluation = dbg.adoptDebuggeeValue(
      webConsole._lastConsoleInputEvaluation
    );
  }
}

function getEvalInput(string) {
  const trimmedString = string.trim();
  // The help function needs to be easy to guess, so we make the () optional.
  if (trimmedString === "help" || trimmedString === "?") {
    return "help()";
  }
  // we support Unix like syntax for commands if it is preceeded by `:`
  if (isCommand(string)) {
    try {
      return formatCommand(string);
    } catch (e) {
      console.log(e);
      return `throw "${e}"`;
    }
  }

  // Add easter egg for console.mihai().
  if (
    trimmedString == "console.mihai()" ||
    trimmedString == "console.mihai();"
  ) {
    return '"http://incompleteness.me/blog/2015/02/09/console-dot-mihai/"';
  }
  return string;
}

function getFrameDbg(options, webConsole) {
  if (!options.frameActor) {
    return { frame: null, dbg: webConsole.dbg };
  }
  // Find the Debugger.Frame of the given FrameActor.
  const frameActor = webConsole.conn.getActor(options.frameActor);
  if (frameActor) {
    // If we've been given a frame actor in whose scope we should evaluate the
    // expression, be sure to use that frame's Debugger (that is, the JavaScript
    // debugger's Debugger) for the whole operation, not the console's Debugger.
    // (One Debugger will treat a different Debugger's Debugger.Object instances
    // as ordinary objects, not as references to be followed, so mixing
    // debuggers causes strange behaviors.)
    return { frame: frameActor.frame, dbg: frameActor.threadActor.dbg };
  }
  return DevToolsUtils.reportException(
    "evalWithDebugger",
    Error("The frame actor was not found: " + options.frameActor)
  );
}

function evalReplay(frame, dbg, string) {
  // If the debugger is replaying then we can't yet introduce new bindings
  // for the eval, so compute the result now.
  let result;
  if (frame) {
    try {
      result = frame.eval(string);
    } catch (e) {
      result = { throw: e };
    }
  } else {
    result = { throw: "Cannot evaluate while replaying without a frame" };
  }
  return {
    result: result,
    helperResult: null,
    dbg: dbg,
    frame: frame,
    window: null,
  };
}

function getDbgWindow(options, dbg, webConsole) {
  const dbgWindow = dbg.makeGlobalObjectReference(webConsole.evalWindow);

  // If we have an object to bind to |_self|, create a Debugger.Object
  // referring to that object, belonging to dbg.
  if (!options.selectedObjectActor) {
    return { bindSelf: null, dbgWindow };
  }

  const actor = webConsole.getActorByID(options.selectedObjectActor);

  if (!actor) {
    return { bindSelf: null, dbgWindow };
  }

  const jsVal = actor instanceof LongStringActor ? actor.str : actor.rawValue();
  if (!isObject(jsVal)) {
    return { bindSelf: jsVal, dbgWindow };
  }

  // If we use the makeDebuggeeValue method of jsVal's own global, then
  // we'll get a D.O that sees jsVal as viewed from its own compartment -
  // that is, without wrappers. The evalWithBindings call will then wrap
  // jsVal appropriately for the evaluation compartment.
  const bindSelf = dbgWindow.makeDebuggeeValue(jsVal);
  return { bindSelf, dbgWindow };
}

function getHelpers(dbgWindow, options, webConsole) {
  // Get the Web Console commands for the given debugger window.
  const helpers = webConsole._getWebConsoleCommands(dbgWindow);
  if (options.selectedNodeActor) {
    const actor = webConsole.conn.getActor(options.selectedNodeActor);
    if (actor) {
      helpers.selectedNode = actor.rawNode;
    }
  }

  return helpers;
}

function cleanupBindings(bindings, helperCache) {
  // Replaces bindings that were overwritten with commands saved in the helperCache
  for (const [helperName, helper] of Object.entries(helperCache)) {
    bindings[helperName] = helper;
  }

  if (bindings._self) {
    delete bindings._self;
  }
}

function bindCommands(isCmd, dbgWindow, bindSelf, frame, helpers) {
  const bindings = helpers.sandbox;
  if (bindSelf) {
    bindings._self = bindSelf;
  }
  // Check if the Debugger.Frame or Debugger.Object for the global include any of the
  // helper function we set. We will not overwrite these functions with the Web Console
  // commands. The exception being "print" which should exist everywhere as
  // `window.print`, and that we don't want to trigger from the console.
  const availableHelpers = [
    ...WebConsoleCommands._originalCommands.keys(),
  ].filter(h => h !== "print");

  let helpersToDisable = [];
  const helperCache = {};

  // do not override command functions if we are using the command key `:`
  // before the command string
  if (!isCmd) {
    if (frame) {
      const env = frame.environment;
      if (env) {
        helpersToDisable = availableHelpers.filter(name => !!env.find(name));
      }
    } else {
      helpersToDisable = availableHelpers.filter(
        name => !!dbgWindow.getOwnPropertyDescriptor(name)
      );
    }
    // if we do not have the command key as a prefix, screenshot is disabled by default
    helpersToDisable.push("screenshot");
  }

  for (const helper of helpersToDisable) {
    helperCache[helper] = bindings[helper];
    delete bindings[helper];
  }
  return { bindings, helperCache };
}
