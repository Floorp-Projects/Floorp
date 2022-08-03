/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cu } = require("chrome");

const CONSOLE_WORKER_IDS = (exports.CONSOLE_WORKER_IDS = new Set([
  "SharedWorker",
  "ServiceWorker",
  "Worker",
]));

var WebConsoleUtils = {
  /**
   * Given a message, return one of CONSOLE_WORKER_IDS if it matches
   * one of those.
   *
   * @return string
   */
  getWorkerType(message) {
    const innerID = message?.innerID;
    return CONSOLE_WORKER_IDS.has(innerID) ? innerID : null;
  },

  /**
   * Gets the ID of the inner window of this DOM window.
   *
   * @param nsIDOMWindow window
   * @return integer|null
   *         Inner ID for the given window, null if we can't access it.
   */
  getInnerWindowId(window) {
    // Might throw with SecurityError: Permission denied to access property
    // "windowGlobalChild" on cross-origin object.
    try {
      return window.windowGlobalChild.innerWindowId;
    } catch (e) {
      return null;
    }
  },

  /**
   * Recursively gather a list of inner window ids given a
   * top level window.
   *
   * @param nsIDOMWindow window
   * @return Array
   *         list of inner window ids.
   */
  getInnerWindowIDsForFrames(window) {
    const innerWindowID = this.getInnerWindowId(window);
    if (innerWindowID === null) {
      return [];
    }

    let ids = [innerWindowID];

    if (window.frames) {
      for (let i = 0; i < window.frames.length; i++) {
        const frame = window.frames[i];
        ids = ids.concat(this.getInnerWindowIDsForFrames(frame));
      }
    }

    return ids;
  },

  /**
   * Create a grip for the given value. If the value is an object,
   * an object wrapper will be created.
   *
   * @param mixed value
   *        The value you want to create a grip for, before sending it to the
   *        client.
   * @param function objectWrapper
   *        If the value is an object then the objectWrapper function is
   *        invoked to give us an object grip. See this.getObjectGrip().
   * @return mixed
   *         The value grip.
   */
  createValueGrip(value, objectWrapper) {
    switch (typeof value) {
      case "boolean":
        return value;
      case "string":
        return objectWrapper(value);
      case "number":
        if (value === Infinity) {
          return { type: "Infinity" };
        } else if (value === -Infinity) {
          return { type: "-Infinity" };
        } else if (Number.isNaN(value)) {
          return { type: "NaN" };
        } else if (!value && 1 / value === -Infinity) {
          return { type: "-0" };
        }
        return value;
      case "undefined":
        return { type: "undefined" };
      case "object":
        if (value === null) {
          return { type: "null" };
        }
      // Fall through.
      case "function":
        return objectWrapper(value);
      default:
        console.error(
          "Failed to provide a grip for value of " + typeof value + ": " + value
        );
        return null;
    }
  },

  /**
   * Remove any frames in a stack that are above a debugger-triggered evaluation
   * and will correspond with devtools server code, which we never want to show
   * to the user.
   *
   * @param array stack
   *        An array of frames, with the topmost first, and each of which has a
   *        'filename' property.
   * @return array
   *         An array of stack frames with any devtools server frames removed.
   *         The original array is not modified.
   */
  removeFramesAboveDebuggerEval(stack) {
    const debuggerEvalFilename = "debugger eval code";

    // Remove any frames for server code above the last debugger eval frame.
    const evalIndex = stack.findIndex(({ filename }, idx, arr) => {
      const nextFrame = arr[idx + 1];
      return (
        filename == debuggerEvalFilename &&
        (!nextFrame || nextFrame.filename !== debuggerEvalFilename)
      );
    });
    if (evalIndex != -1) {
      return stack.slice(0, evalIndex + 1);
    }

    // In some cases (e.g. evaluated expression with SyntaxError), we might not have a
    // "debugger eval code" frame but still have internal ones. If that's the case, we
    // return null as the end user shouldn't see those frames.
    if (
      stack.some(
        ({ filename }) =>
          filename && filename.startsWith("resource://devtools/")
      )
    ) {
      return null;
    }

    return stack;
  },
};

exports.WebConsoleUtils = WebConsoleUtils;

/**
 * WebConsole commands manager.
 *
 * Defines a set of functions /variables ("commands") that are available from
 * the Web Console but not from the web page.
 *
 */
var WebConsoleCommands = {
  _registeredCommands: new Map(),
  _originalCommands: new Map(),

  /**
   * @private
   * Reserved for built-in commands. To register a command from the code of an
   * add-on, see WebConsoleCommands.register instead.
   *
   * @see WebConsoleCommands.register
   */
  _registerOriginal(name, command) {
    this.register(name, command);
    this._originalCommands.set(name, this.getCommand(name));
  },

  /**
   * Register a new command.
   * @param {string} name The command name (exemple: "$")
   * @param {(function|object)} command The command to register.
   *  It can be a function so the command is a function (like "$()"),
   *  or it can also be a property descriptor to describe a getter / value (like
   *  "$0").
   *
   *  The command function or the command getter are passed a owner object as
   *  their first parameter (see the example below).
   *
   *  Note that setters don't work currently and "enumerable" and "configurable"
   *  are forced to true.
   *
   * @example
   *
   *   WebConsoleCommands.register("$", function JSTH_$(owner, selector)
   *   {
   *     return owner.window.document.querySelector(selector);
   *   });
   *
   *   WebConsoleCommands.register("$0", {
   *     get: function(owner) {
   *       return owner.makeDebuggeeValue(owner.selectedNode);
   *     }
   *   });
   */
  register(name, command) {
    this._registeredCommands.set(name, command);
  },

  /**
   * Unregister a command.
   *
   * If the command being unregister overrode a built-in command,
   * the latter is restored.
   *
   * @param {string} name The name of the command
   */
  unregister(name) {
    this._registeredCommands.delete(name);
    if (this._originalCommands.has(name)) {
      this.register(name, this._originalCommands.get(name));
    }
  },

  /**
   * Returns a command by its name.
   *
   * @param {string} name The name of the command.
   *
   * @return {(function|object)} The command.
   */
  getCommand(name) {
    return this._registeredCommands.get(name);
  },

  /**
   * Returns true if a command is registered with the given name.
   *
   * @param {string} name The name of the command.
   *
   * @return {boolean} True if the command is registered.
   */
  hasCommand(name) {
    return this._registeredCommands.has(name);
  },
};

exports.WebConsoleCommands = WebConsoleCommands;

/*
 * Built-in commands.
 *
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 */

/**
 * Find a node by ID.
 *
 * @param string id
 *        The ID of the element you want.
 * @return Node or null
 *         The result of calling document.querySelector(selector).
 */
WebConsoleCommands._registerOriginal("$", function(owner, selector) {
  try {
    return owner.window.document.querySelector(selector);
  } catch (err) {
    // Throw an error like `err` but that belongs to `owner.window`.
    throw new owner.window.DOMException(err.message, err.name);
  }
});

/**
 * Find the nodes matching a CSS selector.
 *
 * @param string selector
 *        A string that is passed to window.document.querySelectorAll.
 * @return NodeList
 *         Returns the result of document.querySelectorAll(selector).
 */
WebConsoleCommands._registerOriginal("$$", function(owner, selector) {
  let nodes;
  try {
    nodes = owner.window.document.querySelectorAll(selector);
  } catch (err) {
    // Throw an error like `err` but that belongs to `owner.window`.
    throw new owner.window.DOMException(err.message, err.name);
  }

  // Calling owner.window.Array.from() doesn't work without accessing the
  // wrappedJSObject, so just loop through the results instead.
  const result = new owner.window.Array();
  for (let i = 0; i < nodes.length; i++) {
    result.push(nodes[i]);
  }
  return result;
});

/**
 * Returns the result of the last console input evaluation
 *
 * @return object|undefined
 * Returns last console evaluation or undefined
 */
WebConsoleCommands._registerOriginal("$_", {
  get(owner) {
    return owner.consoleActor.getLastConsoleInputEvaluation();
  },
});

/**
 * Runs an xPath query and returns all matched nodes.
 *
 * @param string xPath
 *        xPath search query to execute.
 * @param [optional] Node context
 *        Context to run the xPath query on. Uses window.document if not set.
 * @param [optional] string|number resultType
          Specify the result type. Default value XPathResult.ANY_TYPE
 * @return array of Node
 */
WebConsoleCommands._registerOriginal("$x", function(
  owner,
  xPath,
  context,
  resultType = owner.window.XPathResult.ANY_TYPE
) {
  const nodes = new owner.window.Array();
  // Not waiving Xrays, since we want the original Document.evaluate function,
  // instead of anything that's been redefined.
  const doc = owner.window.document;
  context = context || doc;
  switch (resultType) {
    case "number":
      resultType = owner.window.XPathResult.NUMBER_TYPE;
      break;

    case "string":
      resultType = owner.window.XPathResult.STRING_TYPE;
      break;

    case "bool":
      resultType = owner.window.XPathResult.BOOLEAN_TYPE;
      break;

    case "node":
      resultType = owner.window.XPathResult.FIRST_ORDERED_NODE_TYPE;
      break;

    case "nodes":
      resultType = owner.window.XPathResult.UNORDERED_NODE_ITERATOR_TYPE;
      break;
  }
  const results = doc.evaluate(xPath, context, null, resultType, null);
  if (results.resultType === owner.window.XPathResult.NUMBER_TYPE) {
    return results.numberValue;
  }
  if (results.resultType === owner.window.XPathResult.STRING_TYPE) {
    return results.stringValue;
  }
  if (results.resultType === owner.window.XPathResult.BOOLEAN_TYPE) {
    return results.booleanValue;
  }
  if (
    results.resultType === owner.window.XPathResult.ANY_UNORDERED_NODE_TYPE ||
    results.resultType === owner.window.XPathResult.FIRST_ORDERED_NODE_TYPE
  ) {
    return results.singleNodeValue;
  }
  if (
    results.resultType ===
      owner.window.XPathResult.UNORDERED_NODE_SNAPSHOT_TYPE ||
    results.resultType === owner.window.XPathResult.ORDERED_NODE_SNAPSHOT_TYPE
  ) {
    for (let i = 0; i < results.snapshotLength; i++) {
      nodes.push(results.snapshotItem(i));
    }
    return nodes;
  }

  let node;
  while ((node = results.iterateNext())) {
    nodes.push(node);
  }

  return nodes;
});

/**
 * Returns the currently selected object in the highlighter.
 *
 * @return Object representing the current selection in the
 *         Inspector, or null if no selection exists.
 */
WebConsoleCommands._registerOriginal("$0", {
  get(owner) {
    return owner.makeDebuggeeValue(owner.selectedNode);
  },
});

/**
 * Clears the output of the WebConsole.
 */
WebConsoleCommands._registerOriginal("clear", function(owner) {
  owner.helperResult = {
    type: "clearOutput",
  };
});

/**
 * Clears the input history of the WebConsole.
 */
WebConsoleCommands._registerOriginal("clearHistory", function(owner) {
  owner.helperResult = {
    type: "clearHistory",
  };
});

/**
 * Returns the result of Object.keys(object).
 *
 * @param object object
 *        Object to return the property names from.
 * @return array of strings
 */
WebConsoleCommands._registerOriginal("keys", function(owner, object) {
  // Need to waive Xrays so we can iterate functions and accessor properties
  return Cu.cloneInto(Object.keys(Cu.waiveXrays(object)), owner.window);
});

/**
 * Returns the values of all properties on object.
 *
 * @param object object
 *        Object to display the values from.
 * @return array of string
 */
WebConsoleCommands._registerOriginal("values", function(owner, object) {
  const values = [];
  // Need to waive Xrays so we can iterate functions and accessor properties
  const waived = Cu.waiveXrays(object);
  const names = Object.getOwnPropertyNames(waived);

  for (const name of names) {
    values.push(waived[name]);
  }

  return Cu.cloneInto(values, owner.window);
});

/**
 * Opens a help window in MDN.
 */
WebConsoleCommands._registerOriginal("help", function(owner) {
  owner.helperResult = { type: "help" };
});

/**
 * Inspects the passed object. This is done by opening the PropertyPanel.
 *
 * @param object object
 *        Object to inspect.
 */
WebConsoleCommands._registerOriginal("inspect", function(
  owner,
  object,
  forceExpandInConsole = false
) {
  const dbgObj = owner.preprocessDebuggerObject(
    owner.makeDebuggeeValue(object)
  );

  const grip = owner.createValueGrip(dbgObj);
  owner.helperResult = {
    type: "inspectObject",
    input: owner.evalInput,
    object: grip,
    forceExpandInConsole,
  };
});

/**
 * Copy the String representation of a value to the clipboard.
 *
 * @param any value
 *        A value you want to copy as a string.
 * @return void
 */
WebConsoleCommands._registerOriginal("copy", function(owner, value) {
  let payload;
  try {
    if (Element.isInstance(value)) {
      payload = value.outerHTML;
    } else if (typeof value == "string") {
      payload = value;
    } else {
      payload = JSON.stringify(value, null, "  ");
    }
  } catch (ex) {
    owner.helperResult = {
      type: "error",
      message: "webconsole.error.commands.copyError",
      messageArgs: [ex.toString()],
    };
    return;
  }
  owner.helperResult = {
    type: "copyValueToClipboard",
    value: payload,
  };
});

/**
 * Take a screenshot of a page.
 *
 * @param object args
 *               The arguments to be passed to the screenshot
 * @return void
 */
WebConsoleCommands._registerOriginal("screenshot", function(owner, args = {}) {
  owner.helperResult = (async () => {
    // everything is handled on the client side, so we return a very simple object with
    // the args
    return {
      type: "screenshotOutput",
      args,
    };
  })();
});

/**
 * Shows a history of commands and expressions previously executed within the command line.
 *
 * @param object args
 *               The arguments to be passed to the history
 * @return void
 */
WebConsoleCommands._registerOriginal("history", function(owner, args = {}) {
  owner.helperResult = (async () => {
    // everything is handled on the client side, so we return a very simple object with
    // the args
    return {
      type: "historyOutput",
      args,
    };
  })();
});

/**
 * Block specific resource from loading
 *
 * @param object args
 *               an object with key "url", i.e. a filter
 *
 * @return void
 */
WebConsoleCommands._registerOriginal("block", function(owner, args = {}) {
  if (!args.url) {
    owner.helperResult = {
      type: "error",
      message: "webconsole.messages.commands.blockArgMissing",
    };
    return;
  }

  owner.helperResult = (async () => {
    await owner.consoleActor.blockRequest(args);

    return {
      type: "blockURL",
      args,
    };
  })();
});

/*
 * Unblock a blocked a resource
 *
 * @param object filter
 *               an object with key "url", i.e. a filter
 *
 * @return void
 */
WebConsoleCommands._registerOriginal("unblock", function(owner, args = {}) {
  if (!args.url) {
    owner.helperResult = {
      type: "error",
      message: "webconsole.messages.commands.blockArgMissing",
    };
    return;
  }

  owner.helperResult = (async () => {
    await owner.consoleActor.unblockRequest(args);

    return {
      type: "unblockURL",
      args,
    };
  })();
});

/**
 * (Internal only) Add the bindings to |owner.sandbox|.
 * This is intended to be used by the WebConsole actor only.
 *
 * @param object owner
 *        The owning object.
 */
function addWebConsoleCommands(owner) {
  // Not supporting extra commands in workers yet.  This should be possible to
  // add one by one as long as they don't require jsm, Cu, etc.
  const commands = isWorker ? [] : WebConsoleCommands._registeredCommands;
  if (!owner) {
    throw new Error("The owner is required");
  }
  for (const [name, command] of commands) {
    if (typeof command === "function") {
      owner.sandbox[name] = command.bind(undefined, owner);
    } else if (typeof command === "object") {
      const clone = Object.assign({}, command, {
        // We force the enumerability and the configurability (so the
        // WebConsoleActor can reconfigure the property).
        enumerable: true,
        configurable: true,
      });

      if (typeof command.get === "function") {
        clone.get = command.get.bind(undefined, owner);
      }
      if (typeof command.set === "function") {
        clone.set = command.set.bind(undefined, owner);
      }

      Object.defineProperty(owner.sandbox, name, clone);
    }
  }
}

exports.addWebConsoleCommands = addWebConsoleCommands;
