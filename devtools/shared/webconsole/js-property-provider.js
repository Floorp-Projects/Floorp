/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

if (!isWorker) {
  loader.lazyImporter(this, "Parser", "resource://devtools/shared/Parser.jsm");
}

// Provide an easy way to bail out of even attempting an autocompletion
// if an object has way too many properties. Protects against large objects
// with numeric values that wouldn't be tallied towards MAX_AUTOCOMPLETIONS.
const MAX_AUTOCOMPLETE_ATTEMPTS = exports.MAX_AUTOCOMPLETE_ATTEMPTS = 100000;
// Prevent iterating over too many properties during autocomplete suggestions.
const MAX_AUTOCOMPLETIONS = exports.MAX_AUTOCOMPLETIONS = 1500;

const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = "{[(".split("");
const CLOSE_BODY = "}])".split("");
const OPEN_CLOSE_BODY = {
  "{": "}",
  "[": "]",
  "(": ")",
};

function hasArrayIndex(str) {
  return /\[\d+\]$/.test(str);
}

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string str
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(str) {
  const bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < str.length; i++) {
    c = str[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        } else if (c == "'") {
          state = STATE_QUOTE;
        } else if (c == ";") {
          start = i + 1;
        } else if (c == " ") {
          start = i + 1;
        } else if (OPEN_BODY.includes(c)) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        } else if (CLOSE_BODY.includes(c)) {
          const last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == "}") {
            start = i + 1;
          } else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == "\\") {
          i++;
        } else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        } else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          i++;
        } else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        } else if (c == "'") {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * Debugger.Environment/Debugger.Object and inputValue.
 *
 * @param object dbgObject
 *        When the debugger is not paused this Debugger.Object wraps
 *        the scope for autocompletion.
 *        It is null if the debugger is paused.
 * @param object anEnvironment
 *        When the debugger is paused this Debugger.Environment is the
 *        scope for autocompletion.
 *        It is null if the debugger is not paused.
 * @param string inputValue
 *        Value that should be completed.
 * @param number [cursor=inputValue.length]
 *        Optional offset in the input where the cursor is located. If this is
 *        omitted then the cursor is assumed to be at the end of the input
 *        value.
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(dbgObject, anEnvironment, inputValue, cursor) {
  if (cursor === undefined) {
    cursor = inputValue.length;
  }

  inputValue = inputValue.substring(0, cursor);

  // Analyse the inputValue and find the beginning of the last part that
  // should be completed.
  const beginning = findCompletionBeginning(inputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  const completionPart = inputValue.substring(beginning.startPos);
  const lastDot = completionPart.lastIndexOf(".");

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  // Catch literals like [1,2,3] or "foo" and return the matches from
  // their prototypes.
  // Don't run this is a worker, migrating to acorn should allow this
  // to run in a worker - Bug 1217198.
  if (!isWorker && lastDot > 0) {
    const parser = new Parser();
    parser.logExceptions = false;
    const syntaxTree = parser.get(completionPart.slice(0, lastDot));
    const lastTree = syntaxTree.getLastSyntaxTree();
    const lastBody = lastTree && lastTree.AST.body[lastTree.AST.body.length - 1];

    // Finding the last expression since we've sliced up until the dot.
    // If there were parse errors this won't exist.
    if (lastBody) {
      const expression = lastBody.expression;
      const matchProp = completionPart.slice(lastDot + 1);
      if (expression.type === "ArrayExpression") {
        return getMatchedProps(Array.prototype, matchProp);
      } else if (expression.type === "Literal" &&
                 (typeof expression.value === "string")) {
        return getMatchedProps(String.prototype, matchProp);
      }
    }
  }

  // We are completing a variable / a property lookup.
  const properties = completionPart.split(".");
  const matchProp = properties.pop().trimLeft();
  let obj = dbgObject;

  // The first property must be found in the environment of the paused debugger
  // or of the global lexical scope.
  const env = anEnvironment || obj.asEnvironment();

  if (properties.length === 0) {
    return getMatchedPropsInEnvironment(env, matchProp);
  }

  const firstProp = properties.shift().trim();
  if (firstProp === "this") {
    // Special case for 'this' - try to get the Object from the Environment.
    // No problem if it throws, we will just not autocomplete.
    try {
      obj = env.object;
    } catch (e) {
      // Ignore.
    }
  } else if (hasArrayIndex(firstProp)) {
    obj = getArrayMemberProperty(null, env, firstProp);
  } else {
    obj = getVariableInEnvironment(env, firstProp);
  }

  if (!isObjectUsable(obj)) {
    return null;
  }

  // We get the rest of the properties recursively starting from the
  // Debugger.Object that wraps the first property
  for (let i = 0; i < properties.length; i++) {
    const prop = properties[i].trim();
    if (!prop) {
      return null;
    }

    if (hasArrayIndex(prop)) {
      // The property to autocomplete is a member of array. For example
      // list[i][j]..[n]. Traverse the array to get the actual element.
      obj = getArrayMemberProperty(obj, null, prop);
    } else {
      obj = DevToolsUtils.getProperty(obj, prop);
    }

    if (!isObjectUsable(obj)) {
      return null;
    }
  }

  // If the final property is a primitive
  if (typeof obj != "object") {
    return getMatchedProps(obj, matchProp);
  }

  return getMatchedPropsInDbgObject(obj, matchProp);
}

/**
 * Get the array member of obj for the given prop. For example, given
 * prop='list[0][1]' the element at [0][1] of obj.list is returned.
 *
 * @param object obj
 *        The object to operate on. Should be null if env is passed.
 * @param object env
 *        The Environment to operate in. Should be null if obj is passed.
 * @param string prop
 *        The property to return.
 * @return null or Object
 *         Returns null if the property couldn't be located. Otherwise the array
 *         member identified by prop.
 */
function getArrayMemberProperty(obj, env, prop) {
  // First get the array.
  const propWithoutIndices = prop.substr(0, prop.indexOf("["));

  if (env) {
    obj = getVariableInEnvironment(env, propWithoutIndices);
  } else {
    obj = DevToolsUtils.getProperty(obj, propWithoutIndices);
  }

  if (!isObjectUsable(obj)) {
    return null;
  }

  // Then traverse the list of indices to get the actual element.
  let result;
  const arrayIndicesRegex = /\[[^\]]*\]/g;
  while ((result = arrayIndicesRegex.exec(prop)) !== null) {
    const indexWithBrackets = result[0];
    const indexAsText = indexWithBrackets.substr(1, indexWithBrackets.length - 2);
    const index = parseInt(indexAsText, 10);

    if (isNaN(index)) {
      return null;
    }

    obj = DevToolsUtils.getProperty(obj, index);

    if (!isObjectUsable(obj)) {
      return null;
    }
  }

  return obj;
}

/**
 * Check if the given Debugger.Object can be used for autocomplete.
 *
 * @param Debugger.Object object
 *        The Debugger.Object to check.
 * @return boolean
 *         True if further inspection into the object is possible, or false
 *         otherwise.
 */
function isObjectUsable(object) {
  if (object == null) {
    return false;
  }

  if (typeof object == "object" && object.class == "DeadObject") {
    return false;
  }

  return true;
}

/**
 * @see getExactMatchImpl()
 */
function getVariableInEnvironment(anEnvironment, name) {
  return getExactMatchImpl(anEnvironment, name, DebuggerEnvironmentSupport);
}

/**
 * @see getMatchedPropsImpl()
 */
function getMatchedPropsInEnvironment(anEnvironment, match) {
  return getMatchedPropsImpl(anEnvironment, match, DebuggerEnvironmentSupport);
}

/**
 * @see getMatchedPropsImpl()
 */
function getMatchedPropsInDbgObject(dbgObject, match) {
  return getMatchedPropsImpl(dbgObject, match, DebuggerObjectSupport);
}

/**
 * @see getMatchedPropsImpl()
 */
function getMatchedProps(obj, match) {
  if (typeof obj != "object") {
    obj = obj.constructor.prototype;
  }
  return getMatchedPropsImpl(obj, match, JSObjectSupport);
}

/**
 * Get all properties in the given object (and its parent prototype chain) that
 * match a given prefix.
 *
 * @param mixed obj
 *        Object whose properties we want to filter.
 * @param string match
 *        Filter for properties that match this string.
 * @return object
 *         Object that contains the matchProp and the list of names.
 */
function getMatchedPropsImpl(obj, match, {chainIterator, getProperties}) {
  const matches = new Set();
  let numProps = 0;

  // We need to go up the prototype chain.
  const iter = chainIterator(obj);
  for (obj of iter) {
    const props = getProperties(obj);
    if (!props) {
      continue;
    }
    numProps += props.length;

    // If there are too many properties to event attempt autocompletion,
    // or if we have already added the max number, then stop looping
    // and return the partial set that has already been discovered.
    if (numProps >= MAX_AUTOCOMPLETE_ATTEMPTS ||
        matches.size >= MAX_AUTOCOMPLETIONS) {
      break;
    }

    for (let i = 0; i < props.length; i++) {
      const prop = props[i];
      if (prop.indexOf(match) != 0) {
        continue;
      }
      if (prop.indexOf("-") > -1) {
        continue;
      }
      // If it is an array index, we can't take it.
      // This uses a trick: converting a string to a number yields NaN if
      // the operation failed, and NaN is not equal to itself.
      // eslint-disable-next-line no-self-compare
      if (+prop != +prop) {
        matches.add(prop);
      }

      if (matches.size >= MAX_AUTOCOMPLETIONS) {
        break;
      }
    }
  }

  return {
    matchProp: match,
    matches: [...matches],
  };
}

/**
 * Returns a property value based on its name from the given object, by
 * recursively checking the object's prototype.
 *
 * @param object obj
 *        An object to look the property into.
 * @param string name
 *        The property that is looked up.
 * @returns object|undefined
 *        A Debugger.Object if the property exists in the object's prototype
 *        chain, undefined otherwise.
 */
function getExactMatchImpl(obj, name, {chainIterator, getProperty}) {
  // We need to go up the prototype chain.
  const iter = chainIterator(obj);
  for (obj of iter) {
    const prop = getProperty(obj, name, obj);
    if (prop) {
      return prop.value;
    }
  }
  return undefined;
}

var JSObjectSupport = {
  chainIterator: function* (obj) {
    while (obj) {
      yield obj;
      try {
        obj = Object.getPrototypeOf(obj);
      } catch (error) {
        // The above can throw e.g. for some proxy objects.
        return;
      }
    }
  },

  getProperties: function(obj) {
    try {
      return Object.getOwnPropertyNames(obj);
    } catch (error) {
      // The above can throw e.g. for some proxy objects.
      return null;
    }
  },

  getProperty: function() {
    // getProperty is unsafe with raw JS objects.
    throw new Error("Unimplemented!");
  },
};

var DebuggerObjectSupport = {
  chainIterator: function* (obj) {
    while (obj) {
      yield obj;
      try {
        obj = obj.proto;
      } catch (error) {
        // The above can throw e.g. for some proxy objects.
        return;
      }
    }
  },

  getProperties: function(obj) {
    try {
      return obj.getOwnPropertyNames();
    } catch (error) {
      // The above can throw e.g. for some proxy objects.
      return null;
    }
  },

  getProperty: function(obj, name, rootObj) {
    // This is left unimplemented in favor to DevToolsUtils.getProperty().
    throw new Error("Unimplemented!");
  },
};

var DebuggerEnvironmentSupport = {
  chainIterator: function* (obj) {
    while (obj) {
      yield obj;
      obj = obj.parent;
    }
  },

  getProperties: function(obj) {
    const names = obj.names();

    // Include 'this' in results (in sorted order)
    for (let i = 0; i < names.length; i++) {
      if (i === names.length - 1 || names[i + 1] > "this") {
        names.splice(i + 1, 0, "this");
        break;
      }
    }

    return names;
  },

  getProperty: function(obj, name) {
    let result;
    // Try/catch since name can be anything, and getVariable throws if
    // it's not a valid ECMAScript identifier name
    try {
      // TODO: we should use getVariableDescriptor() here - bug 725815.
      result = obj.getVariable(name);
    } catch (e) {
      // Ignore.
    }

    // FIXME: Need actual UI, bug 941287.
    if (result === undefined || result.optimizedOut ||
        result.missingArguments) {
      return null;
    }
    return { value: result };
  },
};

exports.JSPropertyProvider = DevToolsUtils.makeInfallible(JSPropertyProvider);

// Export a version that will throw (for tests)
exports.FallibleJSPropertyProvider = JSPropertyProvider;
