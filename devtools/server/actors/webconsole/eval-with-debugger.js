/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Debugger = require("Debugger");
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
loader.lazyRequireGetter(
  this,
  "eagerEcmaWhitelist",
  "devtools/server/actors/webconsole/eager-ecma-whitelist"
);
loader.lazyRequireGetter(
  this,
  "eagerFunctionWhitelist",
  "devtools/server/actors/webconsole/eager-function-whitelist"
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
 *        - eager: Set to true if you want the evaluation to bail if it may have side effects.
 *        - url: the url to evaluate the script as. Defaults to "debugger eval code",
 *        or "debugger eager eval code" if eager is true.
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

  const { dbgWindow, bindSelf } = getDbgWindow(options, dbg, webConsole);
  const helpers = getHelpers(dbgWindow, options, webConsole);
  let { bindings, helperCache } = bindCommands(
    isCommand(string),
    dbgWindow,
    bindSelf,
    frame,
    helpers
  );

  if (options.bindings) {
    bindings = { ...(bindings || {}), ...options.bindings };
  }

  // Ready to evaluate the string.
  helpers.evalInput = string;
  const evalOptions = {};

  const urlOption =
    options.url || (options.eager ? "debugger eager eval code" : null);
  if (typeof urlOption === "string") {
    evalOptions.url = urlOption;
  }

  if (typeof options.lineNumber === "number") {
    evalOptions.lineNumber = options.lineNumber;
  }

  updateConsoleInputEvaluation(dbg, dbgWindow, webConsole);

  let noSideEffectDebugger = null;
  if (options.eager) {
    noSideEffectDebugger = makeSideeffectFreeDebugger();
  }

  let result;
  try {
    result = getEvalResult(
      dbg,
      evalString,
      evalOptions,
      bindings,
      frame,
      dbgWindow,
      noSideEffectDebugger
    );
  } finally {
    // We need to be absolutely sure that the sideeffect-free debugger's
    // debuggees are removed because otherwise we risk them terminating
    // execution of later code in the case of unexpected exceptions.
    if (noSideEffectDebugger) {
      noSideEffectDebugger.removeAllDebuggees();
    }
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

function getEvalResult(
  dbg,
  string,
  evalOptions,
  bindings,
  frame,
  dbgWindow,
  noSideEffectDebugger
) {
  if (noSideEffectDebugger) {
    // When a sideeffect-free debugger has been created, we need to eval
    // in the context of that debugger in order for the side-effect tracking
    // to apply.
    frame = frame ? noSideEffectDebugger.adoptFrame(frame) : null;
    dbgWindow = noSideEffectDebugger.adoptDebuggeeValue(dbgWindow);
    if (bindings) {
      bindings = Object.keys(bindings).reduce((acc, key) => {
        acc[key] = noSideEffectDebugger.adoptDebuggeeValue(bindings[key]);
        return acc;
      }, {});
    }
  }

  let result;
  if (frame) {
    result = frame.evalWithBindings(string, bindings, evalOptions);
  } else {
    result = dbgWindow.executeInGlobalWithBindings(
      string,
      bindings,
      evalOptions
    );
  }
  if (noSideEffectDebugger && result) {
    if ("return" in result) {
      result.return = dbg.adoptDebuggeeValue(result.return);
    }
    if ("throw" in result) {
      result.throw = dbg.adoptDebuggeeValue(result.throw);
    }
  }
  return result;
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

function makeSideeffectFreeDebugger() {
  // We ensure that the metadata for native functions is loaded before we
  // initialize sideeffect-prevention because the data is lazy-loaded, and this
  // logic can run inside of debuggee compartments because the
  // "addAllGlobalsAsDebuggees" considers the vast majority of realms
  // valid debuggees. Without this, eager-eval runs the risk of failing
  // because building the list of valid native functions is itself a
  // side-effectful operation because it needs to populate a
  // module cache, among any number of other things.
  ensureSideEffectFreeNatives();

  // Note: It is critical for debuggee performance that we implement all of
  // this debuggee tracking logic with a separate Debugger instance.
  // Bug 1617666 arises otherwise if we set an onEnterFrame hook on the
  // existing debugger object and then later clear it.
  const dbg = new Debugger();
  dbg.addAllGlobalsAsDebuggees();

  const timeoutDuration = 100;
  const endTime = Date.now() + timeoutDuration;
  let count = 0;
  function shouldCancel() {
    // To keep the evaled code as quick as possible, we avoid querying the
    // current time on ever single step and instead check every 100 steps
    // as an arbitrary count that seemed to be "often enough".
    return ++count % 100 === 0 && Date.now() > endTime;
  }

  const executedScripts = new Set();
  const handler = {
    hit: () => null,
  };
  dbg.onEnterFrame = frame => {
    if (shouldCancel()) {
      return null;
    }
    frame.onStep = () => {
      if (shouldCancel()) {
        return null;
      }
      return undefined;
    };

    const script = frame.script;

    if (executedScripts.has(script)) {
      return undefined;
    }
    executedScripts.add(script);

    const offsets = script.getEffectfulOffsets();
    for (const offset of offsets) {
      script.setBreakpoint(offset, handler);
    }

    return undefined;
  };

  // The debugger only calls onNativeCall handlers on the debugger that is
  // explicitly calling eval, so we need to add this hook on "dbg" even though
  // the rest of our hooks work via "newDbg".
  dbg.onNativeCall = (callee, reason) => {
    try {
      // Getters are never considered effectful, and setters are always effectful.
      // Natives called normally are handled with a whitelist.
      if (
        reason == "get" ||
        (reason == "call" && nativeHasNoSideEffects(callee))
      ) {
        // Returning undefined causes execution to continue normally.
        return undefined;
      }
    } catch (err) {
      DevToolsUtils.reportException(
        "evalWithDebugger onNativeCall",
        new Error("Unable to validate native function against whitelist")
      );
    }
    // Returning null terminates the current evaluation.
    return null;
  };

  return dbg;
}

// Native functions which are considered to be side effect free.
let gSideEffectFreeNatives; // string => Array(Function)

function ensureSideEffectFreeNatives() {
  if (gSideEffectFreeNatives) {
    return;
  }

  const natives = [
    ...eagerEcmaWhitelist,

    // Pull in all of the non-ECMAScript native functions that we want to
    // whitelist as well.
    ...eagerFunctionWhitelist,
  ];

  const map = new Map();
  for (const n of natives) {
    if (!map.has(n.name)) {
      map.set(n.name, []);
    }
    map.get(n.name).push(n);
  }

  gSideEffectFreeNatives = map;
}

function nativeHasNoSideEffects(fn) {
  if (fn.isBoundFunction) {
    fn = fn.boundTargetFunction;
  }

  // Natives with certain names are always considered side effect free.
  switch (fn.name) {
    case "toString":
    case "toLocaleString":
    case "valueOf":
      return true;
  }

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

function getDbgWindow(options, dbg, webConsole) {
  const dbgWindow = dbg.makeGlobalObjectReference(webConsole.evalWindow);

  // If we have an object to bind to |_self|, create a Debugger.Object
  // referring to that object, belonging to dbg.
  if (!options.selectedObjectActor) {
    return { bindSelf: null, dbgWindow };
  }

  const actor = webConsole.actor(options.selectedObjectActor);

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
  // commands.
  const availableHelpers = [...WebConsoleCommands._originalCommands.keys()];

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
