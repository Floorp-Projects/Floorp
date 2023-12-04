/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Debugger = require("Debugger");
const DevToolsUtils = require("resource://devtools/shared/DevToolsUtils.js");

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  Reflect: "resource://gre/modules/reflect.sys.mjs",
});
loader.lazyRequireGetter(
  this,
  ["isCommand"],
  "resource://devtools/server/actors/webconsole/commands/parser.js",
  true
);
loader.lazyRequireGetter(
  this,
  "WebConsoleCommandsManager",
  "resource://devtools/server/actors/webconsole/commands/manager.js",
  true
);

loader.lazyRequireGetter(
  this,
  "LongStringActor",
  "resource://devtools/server/actors/string.js",
  true
);
loader.lazyRequireGetter(
  this,
  "eagerEcmaAllowlist",
  "resource://devtools/server/actors/webconsole/eager-ecma-allowlist.js"
);
loader.lazyRequireGetter(
  this,
  "eagerFunctionAllowlist",
  "resource://devtools/server/actors/webconsole/eager-function-allowlist.js"
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
 *        - innerWindowID: An optional window id to use instead of webConsole.evalWindow.
 *        This is used by function that need to evaluate in a different window for which
 *        we don't have a dedicated target (for example a non-remote iframe).
 *        - eager: Set to true if you want the evaluation to bail if it may have side effects.
 *        - url: the url to evaluate the script as. Defaults to "debugger eval code",
 *        or "debugger eager eval code" if eager is true.
 * @param object webConsole
 *
 * @return object
 *         An object that holds the following properties:
 *         - dbg: the debugger where the string was evaluated.
 *         - frame: (optional) the frame where the string was evaluated.
 *         - global: the Debugger.Object for the global where the string was evaluated in.
 *         - result: the result of the evaluation.
 */
function evalWithDebugger(string, options = {}, webConsole) {
  const trimmedString = string.trim();
  // The help function needs to be easy to guess, so accept "?" as a shortcut
  if (trimmedString === "?") {
    return evalWithDebugger(":help", options, webConsole);
  }

  const isCmd = isCommand(trimmedString);

  if (isCmd && options.eager) {
    return {
      result: null,
    };
  }

  const { frame, dbg } = getFrameDbg(options, webConsole);

  const { dbgGlobal, bindSelf } = getDbgGlobal(options, dbg, webConsole);

  // If the strings starts with a `:`, do not try to evaluate the strings
  // and instead only call the related command function directly from
  // the privileged codebase.
  if (isCmd) {
    try {
      return WebConsoleCommandsManager.executeCommand(
        webConsole,
        dbgGlobal,
        options.selectedNodeActor,
        string
      );
    } catch (e) {
      // Catch any exception and return a result similar to the output
      // of executeCommand to notify the client about this unexpected error.
      return {
        helperResult: {
          type: "exception",
          message: e.message,
        },
      };
    }
  }

  const helpers = WebConsoleCommandsManager.getWebConsoleCommands(
    webConsole,
    dbgGlobal,
    frame,
    string,
    options.selectedNodeActor,
    !!options.disableBreaks
  );
  let { bindings } = helpers;

  // Ease calling the help command by not requiring the "()".
  // But wait for the bindings computation in order to know if "help" variable
  // was overloaded by the page. If it is missing from bindings, it is overloaded and we should
  // display its value by doing a regular evaluation.
  if (trimmedString === "help" && bindings.help) {
    return evalWithDebugger(":help", options, webConsole);
  }

  // '_self' refers to the JS object references via options.selectedObjectActor.
  // This isn't exposed on typical console evaluation, but only when "Store As Global"
  // runs an invisible script storing `_self` into `temp${i}`.
  if (bindSelf) {
    bindings._self = bindSelf;
  }

  // Log points calls this method from the server side and pass additional variables
  // to be exposed to the evaluated JS string
  if (options.bindings) {
    bindings = { ...bindings, ...options.bindings };
  }

  const evalOptions = {};

  const urlOption =
    options.url || (options.eager ? "debugger eager eval code" : null);
  if (typeof urlOption === "string") {
    evalOptions.url = urlOption;
  }

  if (typeof options.lineNumber === "number") {
    evalOptions.lineNumber = options.lineNumber;
  }

  if (options.disableBreaks || options.eager) {
    // When we are disabling breakpoints for a given evaluation, or when we are doing an eager evaluation,
    // also prevent spawning related Debugger.Source object to avoid showing it
    // in the debugger UI
    evalOptions.hideFromDebugger = true;
  }

  if (options.disableBreaks) {
    // disableBreaks is used for all non-user-provided code, and in this case
    // extra bindings shouldn't be shadowed.
    evalOptions.useInnerBindings = true;
  }

  updateConsoleInputEvaluation(dbg, webConsole);

  const evalString = getEvalInput(string, bindings);
  const result = getEvalResult(
    dbg,
    evalString,
    evalOptions,
    bindings,
    frame,
    dbgGlobal,
    options.eager
  );

  // Attempt to initialize any declarations found in the evaluated string
  // since they may now be stuck in an "initializing" state due to the
  // error. Already-initialized bindings will be ignored.
  if (!frame && result && "throw" in result) {
    forceLexicalInitForVariableDeclarationsInThrowingExpression(
      dbgGlobal,
      string
    );
  }

  return {
    result,
    // Retrieve the result of commands, if any ran
    helperResult: helpers.getHelperResult(),
    dbg,
    frame,
    dbgGlobal,
  };
}
exports.evalWithDebugger = evalWithDebugger;

/**
 * Sub-function to reduce the complexity of evalWithDebugger.
 * This focuses on calling Debugger.Frame or Debugger.Object eval methods.
 *
 * @param {Debugger} dbg
 * @param {String} string
 *        The string to evaluate.
 * @param {Object} evalOptions
 *        Spidermonkey options to pass to eval methods.
 * @param {Object} bindings
 *        Dictionary object with symbols to override in the evaluation.
 * @param {Debugger.Frame} frame
 *        If paused, the paused frame.
 * @param {Debugger.Object} dbgGlobal
 *        The target's global.
 * @param {Boolean} eager
 *        Is this an eager evaluation?
 * @return {Object}
 *        The evaluation result object.
 *        See `Debugger.Ojbect.executeInGlobalWithBindings` definition.
 */
function getEvalResult(
  dbg,
  string,
  evalOptions,
  bindings,
  frame,
  dbgGlobal,
  eager
) {
  // When we are doing an eager evaluation, we aren't using the target's Debugger object
  // but a special one, dedicated to each evaluation.
  let noSideEffectDebugger = null;
  if (eager) {
    noSideEffectDebugger = makeSideeffectFreeDebugger(dbg);

    // When a sideeffect-free debugger has been created, we need to eval
    // in the context of that debugger in order for the side-effect tracking
    // to apply.
    if (frame) {
      frame = noSideEffectDebugger.adoptFrame(frame);
    } else {
      dbgGlobal = noSideEffectDebugger.adoptDebuggeeValue(dbgGlobal);
    }
    if (bindings) {
      bindings = Object.keys(bindings).reduce((acc, key) => {
        acc[key] = noSideEffectDebugger.adoptDebuggeeValue(bindings[key]);
        return acc;
      }, {});
    }
  }

  try {
    let result;
    if (frame) {
      result = frame.evalWithBindings(string, bindings, evalOptions);
    } else {
      result = dbgGlobal.executeInGlobalWithBindings(
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
  } finally {
    // We need to be absolutely sure that the sideeffect-free debugger's
    // debuggees are removed because otherwise we risk them terminating
    // execution of later code in the case of unexpected exceptions.
    if (noSideEffectDebugger) {
      noSideEffectDebugger.removeAllDebuggees();
      noSideEffectDebugger.onNativeCall = undefined;
    }
  }
}

/**
 * Force lexical initialization for let/const variables declared in a throwing expression.
 * By spec, a lexical declaration is added to the *page-visible* global lexical environment
 * for those variables, meaning they can't be redeclared (See Bug 1246215).
 *
 * This function gets the AST of the throwing expression to collect all the let/const
 * declarations and call `forceLexicalInitializationByName`, which will initialize them
 * to undefined, making it possible for them to be redeclared.
 *
 * @param {DebuggerObject} dbgGlobal
 * @param {String} string: The expression that was evaluated and threw
 * @returns
 */
function forceLexicalInitForVariableDeclarationsInThrowingExpression(
  dbgGlobal,
  string
) {
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
    ast = lazy.Reflect.parse(string);
  } catch (e) {
    return;
  }

  try {
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
              if (prop.key?.type == "Identifier") {
                identifiers.push(prop.key.name);
              }
              // value
              if (prop.value?.type == "Identifier") {
                identifiers.push(prop.value.name);
              } else if (prop.value?.type == "AssignmentExpression") {
                identifiers.push(prop.value.left.name);
              } else if (prop.type === "SpreadExpression") {
                identifiers.push(prop.expression.name);
              }
            }
            break;
        }
      }

      for (const name of identifiers) {
        dbgGlobal.forceLexicalInitializationByName(name);
      }
    }
  } catch (ex) {
    console.error(
      "Error in forceLexicalInitForVariableDeclarationsInThrowingExpression:",
      ex
    );
  }
}

/**
 * Creates a side-effect-free Debugger instance.
 *
 * @param {Debugger} targetActorDbg
 *        The target actor's dbg object, crafted by make-debugger.js module.
 * @return {Debugger}
 *         Side-effect-free Debugger instance.
 */
function makeSideeffectFreeDebugger(targetActorDbg) {
  // Populate the cached Map once before the evaluation
  ensureSideEffectFreeNatives();

  // Note: It is critical for debuggee performance that we implement all of
  // this debuggee tracking logic with a separate Debugger instance.
  // Bug 1617666 arises otherwise if we set an onEnterFrame hook on the
  // existing debugger object and then later clear it.
  //
  // Also note that we aren't registering any global to this debugger.
  // We will only adopt values into it: the paused frame (if any) or the
  // target's global (when not paused).
  const dbg = new Debugger();

  // Special flag in order to ensure that any evaluation or call being
  // made via this debugger will be ignored by all debuggers except this one.
  dbg.exclusiveDebuggerOnEval = true;

  // We need to register all target actor's globals.
  // In most cases, this will be only one global, except for the browser toolbox,
  // where process target actors may interact with many.
  // On the browser toolbox, we may have many debuggees and this is important to register
  // them in order to detect native call made from/to these others globals.
  for (const global of targetActorDbg.findDebuggees()) {
    try {
      dbg.addDebuggee(global);
    } catch (e) {
      // Ignore the following exception which can happen for some globals in the browser toolbox
      if (
        !e.message.includes(
          "debugger and debuggee must be in different compartments"
        )
      ) {
        throw e;
      }
    }
  }

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
  // explicitly calling either eval, DebuggerObject.apply or DebuggerObject.call,
  // so we need to add this hook on "dbg" even though the rest of our hooks work via "newDbg".
  const { SIDE_EFFECT_FREE } = WebConsoleCommandsManager;
  dbg.onNativeCall = (callee, reason) => {
    try {
      // Setters are always effectful. Natives called normally or called via
      // getters are handled with an allowlist.
      if (
        (reason == "get" || reason == "call") &&
        nativeIsEagerlyEvaluateable(callee)
      ) {
        // Returning undefined causes execution to continue normally.
        return undefined;
      }
    } catch (err) {
      DevToolsUtils.reportException(
        "evalWithDebugger onNativeCall",
        new Error("Unable to validate native function against allowlist")
      );
    }

    // The WebConsole Commands manager will use Cu.exportFunction which will force
    // to call a native method which is hard to identify.
    // getEvalResult will flag those getter methods with a magic attribute.
    if (
      reason == "call" &&
      callee.unsafeDereference().isSideEffectFree === SIDE_EFFECT_FREE
    ) {
      // Returning undefined causes execution to continue normally.
      return undefined;
    }

    // Returning null terminates the current evaluation.
    return null;
  };

  return dbg;
}

// Native functions which are considered to be side effect free.
let gSideEffectFreeNatives; // string => Array(Function)

/**
 * Generate gSideEffectFreeNatives map.
 */
function ensureSideEffectFreeNatives() {
  if (gSideEffectFreeNatives) {
    return;
  }

  const { natives: domNatives } = eagerFunctionAllowlist;

  const natives = [
    ...eagerEcmaAllowlist.functions,
    ...eagerEcmaAllowlist.getters,

    // Pull in all of the non-ECMAScript native functions that we want to
    // allow as well.
    ...domNatives,
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

function nativeIsEagerlyEvaluateable(fn) {
  if (fn.isBoundFunction) {
    fn = fn.boundTargetFunction;
  }

  // We assume all DOM getters have no major side effect, and they are
  // eagerly-evaluateable.
  //
  // JitInfo is used only by methods/accessors in WebIDL, and being
  // "a getter with JitInfo" can be used as a condition to check if given
  // function is DOM getter.
  //
  // This includes privileged interfaces in addition to standard web APIs.
  if (fn.isNativeGetterWithJitInfo()) {
    return true;
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

function updateConsoleInputEvaluation(dbg, webConsole) {
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

function getEvalInput(string, bindings) {
  const trimmedString = string.trim();
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

/**
 * Get debugger object for given debugger and Web Console.
 *
 * @param object options
 *        See the `options` parameter of evalWithDebugger
 * @param {Debugger} dbg
 *        Debugger object
 * @param {WebConsoleActor} webConsole
 *        A reference to a webconsole actor which is used to get the target
 *        eval global and optionally the target actor
 * @return object
 *         An object that holds the following properties:
 *         - bindSelf: (optional) the self object for the evaluation
 *         - dbgGlobal: the global object reference in the debugger
 */
function getDbgGlobal(options, dbg, webConsole) {
  let evalGlobal = webConsole.evalGlobal;

  if (options.innerWindowID) {
    const window = Services.wm.getCurrentInnerWindowWithId(
      options.innerWindowID
    );

    if (window) {
      evalGlobal = window;
    }
  }

  const dbgGlobal = dbg.makeGlobalObjectReference(evalGlobal);

  // If we have an object to bind to |_self|, create a Debugger.Object
  // referring to that object, belonging to dbg.
  if (!options.selectedObjectActor) {
    return { bindSelf: null, dbgGlobal };
  }

  // For objects related to console messages, they will be registered under the Target Actor
  // instead of the WebConsoleActor. That's because console messages are resources and all resources
  // are emitted by the Target Actor.
  const actor =
    webConsole.getActorByID(options.selectedObjectActor) ||
    webConsole.parentActor.getActorByID(options.selectedObjectActor);

  if (!actor) {
    return { bindSelf: null, dbgGlobal };
  }

  const jsVal = actor instanceof LongStringActor ? actor.str : actor.rawValue();
  if (!isObject(jsVal)) {
    return { bindSelf: jsVal, dbgGlobal };
  }

  // If we use the makeDebuggeeValue method of jsVal's own global, then
  // we'll get a D.O that sees jsVal as viewed from its own compartment -
  // that is, without wrappers. The evalWithBindings call will then wrap
  // jsVal appropriately for the evaluation compartment.
  const bindSelf = dbgGlobal.makeDebuggeeValue(jsVal);
  return { bindSelf, dbgGlobal };
}
