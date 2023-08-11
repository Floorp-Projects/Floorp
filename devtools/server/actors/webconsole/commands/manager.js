/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * WebConsole commands manager.
 *
 * Defines a set of functions / variables ("commands") that are available from
 * the Web Console but not from the web page.
 *
 */
const WebConsoleCommandsManager = {
  _registeredCommands: new Map(),

  /**
   * Register a new command.
   * @param {string} name The command name (exemple: "$")
   * @param {(function|object)} command The command to register.
   *  It can be a function so the command is a function (like "$()"),
   *  or it can also be a property descriptor to describe a getter(like
   *  "$0").
   *
   *  The command function or the command getter are passed a owner object as
   *  their first parameter (see the example below).
   *
   * @example
   *
   *   WebConsoleCommandsManager.register("$", function JSTH_$(owner, selector)
   *   {
   *     return owner.window.document.querySelector(selector);
   *   });
   *
   *   WebConsoleCommandsManager.register("$0", {
   *     get: function(owner) {
   *       return owner.makeDebuggeeValue(owner.selectedNode);
   *     }
   *   });
   */
  register(name, command) {
    if (
      typeof command != "function" &&
      !(typeof command == "object" && typeof command.get == "function")
    ) {
      throw new Error(
        "Invalid web console command. It can only be a function, or an object with a function as 'get' attribute"
      );
    }
    this._registeredCommands.set(name, command);
  },

  /**
   * Return the name of all registered commands.
   *
   * @return {array} List of all command names.
   */
  getAllCommandNames() {
    return [...this._registeredCommands.keys()];
  },

  /**
   * There is two types of "commands" here.
   *
   * - Functions or variables exposed in the scope of the evaluated string from the WebConsole input.
   *   Example: $(), $0, copy(), clear(),...
   * - "True commands", which can also be ran from the WebConsole input with ":" prefix.
   *   Example: this list of commands.
   *   Note that some "true commands" are not exposed as function (see getColonOnlyCommandNames).
   *
   * The following list distinguish these "true commands" from the first category.
   * It especially avoid any JavaScript evaluation when the frontend tries to execute
   * a string starting with ':' character.
   */
  getAllColonCommandNames() {
    return ["block", "help", "history", "screenshot", "unblock"];
  },

  /**
   * Some commands are not exposed in the scope of the evaluated string,
   * and can only be used via `:command-name`.
   */
  getColonOnlyCommandNames() {
    return ["screenshot"];
  },

  /**
   * Map of all command objects keyed by command name.
   * Commands object are the objects passed to register() method.
   *
   * @return {Map<string -> command>}
   */
  getAllCommands() {
    return this._registeredCommands;
  },

  /**
   * Is the command name possibly overriding a symbol which
   * already exists in the paused frame or the global into which
   * we are about to execute into?
   */
  _isCommandNameAlreadyInScope(name, frame, dbgGlobal) {
    if (frame && frame.environment) {
      return !!frame.environment.find(name);
    }

    // Fallback on global scope when Debugger.Frame doesn't come along an
    // Environment, or is not a frame.

    try {
      // This can throw in Browser Toolbox tests
      const globalEnv = dbgGlobal.asEnvironment();
      if (globalEnv) {
        return !!dbgGlobal.asEnvironment().find(name);
      }
    } catch {}

    return !!dbgGlobal.getOwnPropertyDescriptor(name);
  },

  _createOwnerObject(
    consoleActor,
    debuggerGlobal,
    evalInput,
    selectedNodeActorID
  ) {
    const owner = {
      window: consoleActor.evalGlobal,
      makeDebuggeeValue: debuggerGlobal.makeDebuggeeValue.bind(debuggerGlobal),
      createValueGrip: consoleActor.createValueGrip.bind(consoleActor),
      preprocessDebuggerObject:
        consoleActor.preprocessDebuggerObject.bind(consoleActor),
      helperResult: null,
      consoleActor,
      evalInput,
    };
    if (selectedNodeActorID) {
      const actor = consoleActor.conn.getActor(selectedNodeActorID);
      if (actor) {
        owner.selectedNode = actor.rawNode;
      }
    }
    return owner;
  },

  _getCommandsForCurrentEnvironment() {
    // Not supporting extra commands in workers yet.  This should be possible to
    // add one by one as long as they don't require jsm/mjs, Cu, etc.
    return isWorker ? new Map() : this.getAllCommands();
  },

  /**
   * Create an object with the API we expose to the Web Console during
   * JavaScript evaluation.
   * This object inherits properties and methods from the Web Console actor.
   *
   * @param object consoleActor
   *        The related web console actor evaluating some code.
   * @param object debuggerGlobal
   *        A Debugger.Object that wraps a content global. This is used for the
   *        Web Console Commands.
   * @param object frame (optional)
   *        The frame where the string was evaluated.
   * @param string evalInput
   *        String to evaluate.
   * @param string selectedNodeActorID
   *        The Node actor ID of the currently selected DOM Element, if any is selected.
   * @param bool ignoreExistingBindings
   *        If true, define all bindings even if there's conflicting existing
   *        symbols.  This is for the case evaluating non-user code in frame
   *        environment.
   *
   * @return object
   *         Object with two properties:
   *         - 'bindings', the object with all commands set as attribute on this object.
   *         - 'getHelperResult', a live getter returning the additional data the last command
   *           which executed want to convey to the frontend.
   *           (The return value of commands isn't returned to the client but it only
   *            returned to the code ran from console evaluation)
   */
  getWebConsoleCommands(
    consoleActor,
    debuggerGlobal,
    frame,
    evalInput,
    selectedNodeActorID,
    ignoreExistingBindings
  ) {
    const bindings = Object.create(null);

    const owner = this._createOwnerObject(
      consoleActor,
      debuggerGlobal,
      evalInput,
      selectedNodeActorID
    );

    const evalGlobal = consoleActor.evalGlobal;
    function maybeExport(obj, name) {
      if (typeof obj[name] != "function") {
        return;
      }

      // By default, chrome-implemented functions that are exposed to content
      // refuse to accept arguments that are cross-origin for the caller. This
      // is generally the safe thing, but causes problems for certain console
      // helpers like cd(), where we users sometimes want to pass a cross-origin
      // window. To circumvent this restriction, we use exportFunction along
      // with a special option designed for this purpose. See bug 1051224.
      obj[name] = Cu.exportFunction(obj[name], evalGlobal, {
        allowCrossOriginArguments: true,
      });
    }

    const commands = this._getCommandsForCurrentEnvironment();

    const colonOnlyCommandNames = this.getColonOnlyCommandNames();
    for (const [name, command] of commands) {
      // When we run user code in frame, we want to avoid overriding existing
      // symbols with commands.
      //
      // When we run user code in global scope, all bindings are automatically
      // shadowed, except for "help" function which is checked by getEvalInput.
      //
      // When we run internal code, always override existing symbols.
      if (
        !ignoreExistingBindings &&
        (frame || name === "help") &&
        this._isCommandNameAlreadyInScope(name, frame, debuggerGlobal)
      ) {
        continue;
      }
      // Also ignore commands which can only be run with the `:` prefix.
      if (colonOnlyCommandNames.includes(name)) {
        continue;
      }

      const descriptor = {
        // We force the enumerability and the configurability (so the
        // WebConsoleActor can reconfigure the property).
        enumerable: true,
        configurable: true,
      };

      if (typeof command === "function") {
        // Function commands
        descriptor.value = command.bind(undefined, owner);
        maybeExport(descriptor, "value");
        // Make sure the helpers can be used during eval.
        descriptor.value = debuggerGlobal.makeDebuggeeValue(descriptor.value);
      } else if (typeof command?.get === "function") {
        // Getter commands
        descriptor.get = command.get.bind(undefined, owner);
        maybeExport(descriptor, "get");
      }
      Object.defineProperty(bindings, name, descriptor);
    }

    return {
      // Use a method as commands will update owner.helperResult later
      getHelperResult() {
        return owner.helperResult;
      },
      bindings,
    };
  },

  /**
   * Create a function for given ':command'-style command.
   *
   * @param object consoleActor
   *        The related web console actor evaluating some code.
   * @param object debuggerGlobal
   *        A Debugger.Object that wraps a content global. This is used for the
   *        Web Console Commands.
   * @param string evalInput
   *        String to evaluate.
   * @param string selectedNodeActorID
   *        The Node actor ID of the currently selected DOM Element, if any is selected.
   * @param string commandName
   *        the name of the command used in 'evalInput'
   *
   * @return object
   *         Object with two properties:
   *         - 'commandFunc', a function corresponds to the 'commandName'
   *         - 'getHelperResult', a live getter returning the data the command
   *           which executed want to convey to the frontend.
   */
  getColonCommandFunction(
    consoleActor,
    debuggerGlobal,
    evalInput,
    selectedNodeActorID,
    commandName
  ) {
    const owner = this._createOwnerObject(
      consoleActor,
      debuggerGlobal,
      evalInput,
      selectedNodeActorID
    );
    const commands = this._getCommandsForCurrentEnvironment();
    if (!commands.has(commandName)) {
      return null;
    }

    const commandFunc = commands.get(commandName).bind(undefined, owner);

    return {
      commandFunc,
      // Use a method as commands will update owner.helperResult later
      getHelperResult() {
        return owner.helperResult;
      },
    };
  },
};

exports.WebConsoleCommandsManager = WebConsoleCommandsManager;

/*
 * Built-in commands.
 *
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 */

/**
 * Find the first node matching a CSS selector.
 *
 * @param string selector
 *        A string that is passed to window.document.querySelector
 * @param [optional] Node element
 *        An optional Node to replace window.document
 * @return Node or null
 *         The result of calling document.querySelector(selector).
 */
WebConsoleCommandsManager.register("$", function (owner, selector, element) {
  try {
    if (
      element &&
      element.querySelector &&
      (element.nodeType == Node.ELEMENT_NODE ||
        element.nodeType == Node.DOCUMENT_NODE ||
        element.nodeType == Node.DOCUMENT_FRAGMENT_NODE)
    ) {
      return element.querySelector(selector);
    }
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
 * @param [optional] Node element
 *        An optional Node to replace window.document
 * @return array of Node
 *         The result of calling document.querySelector(selector) in an array.
 */
WebConsoleCommandsManager.register("$$", function (owner, selector, element) {
  let scope = owner.window.document;
  try {
    if (
      element &&
      element.querySelectorAll &&
      (element.nodeType == Node.ELEMENT_NODE ||
        element.nodeType == Node.DOCUMENT_NODE ||
        element.nodeType == Node.DOCUMENT_FRAGMENT_NODE)
    ) {
      scope = element;
    }
    const nodes = scope.querySelectorAll(selector);
    const result = new owner.window.Array();
    // Calling owner.window.Array.from() doesn't work without accessing the
    // wrappedJSObject, so just loop through the results instead.
    for (let i = 0; i < nodes.length; i++) {
      result.push(nodes[i]);
    }
    return result;
  } catch (err) {
    // Throw an error like `err` but that belongs to `owner.window`.
    throw new owner.window.DOMException(err.message, err.name);
  }
});

/**
 * Returns the result of the last console input evaluation
 *
 * @return object|undefined
 * Returns last console evaluation or undefined
 */
WebConsoleCommandsManager.register("$_", {
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
WebConsoleCommandsManager.register(
  "$x",
  function (
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
  }
);

/**
 * Returns the currently selected object in the highlighter.
 *
 * @return Object representing the current selection in the
 *         Inspector, or null if no selection exists.
 */
WebConsoleCommandsManager.register("$0", {
  get(owner) {
    return owner.makeDebuggeeValue(owner.selectedNode);
  },
});

/**
 * Clears the output of the WebConsole.
 */
WebConsoleCommandsManager.register("clear", function (owner) {
  owner.helperResult = {
    type: "clearOutput",
  };
});

/**
 * Clears the input history of the WebConsole.
 */
WebConsoleCommandsManager.register("clearHistory", function (owner) {
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
WebConsoleCommandsManager.register("keys", function (owner, object) {
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
WebConsoleCommandsManager.register("values", function (owner, object) {
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
WebConsoleCommandsManager.register("help", function (owner) {
  owner.helperResult = { type: "help" };
});

/**
 * Inspects the passed object. This is done by opening the PropertyPanel.
 *
 * @param object object
 *        Object to inspect.
 */
WebConsoleCommandsManager.register(
  "inspect",
  function (owner, object, forceExpandInConsole = false) {
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
  }
);

/**
 * Copy the String representation of a value to the clipboard.
 *
 * @param any value
 *        A value you want to copy as a string.
 * @return void
 */
WebConsoleCommandsManager.register("copy", function (owner, value) {
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
WebConsoleCommandsManager.register("screenshot", function (owner, args = {}) {
  owner.helperResult = {
    type: "screenshotOutput",
    args,
  };
});

/**
 * Shows a history of commands and expressions previously executed within the command line.
 *
 * @param object args
 *               The arguments to be passed to the history
 * @return void
 */
WebConsoleCommandsManager.register("history", function (owner, args = {}) {
  owner.helperResult = {
    type: "historyOutput",
    args,
  };
});

/**
 * Block specific resource from loading
 *
 * @param object args
 *               an object with key "url", i.e. a filter
 *
 * @return void
 */
WebConsoleCommandsManager.register("block", function (owner, args = {}) {
  if (!args.url) {
    owner.helperResult = {
      type: "error",
      message: "webconsole.messages.commands.blockArgMissing",
    };
    return;
  }

  owner.helperResult = {
    type: "blockURL",
    args,
  };
});

/*
 * Unblock a blocked a resource
 *
 * @param object filter
 *               an object with key "url", i.e. a filter
 *
 * @return void
 */
WebConsoleCommandsManager.register("unblock", function (owner, args = {}) {
  if (!args.url) {
    owner.helperResult = {
      type: "error",
      message: "webconsole.messages.commands.blockArgMissing",
    };
    return;
  }

  owner.helperResult = {
    type: "unblockURL",
    args,
  };
});
