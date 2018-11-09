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

const STATE_NORMAL = Symbol("STATE_NORMAL");
const STATE_QUOTE = Symbol("STATE_QUOTE");
const STATE_DQUOTE = Symbol("STATE_DQUOTE");
const STATE_TEMPLATE_LITERAL = Symbol("STATE_TEMPLATE_LITERAL");

const OPEN_BODY = "{[(".split("");
const CLOSE_BODY = "}])".split("");
const OPEN_CLOSE_BODY = {
  "{": "}",
  "[": "]",
  "(": ")",
};

const NO_AUTOCOMPLETE_PREFIXES = ["var", "const", "let", "function", "class"];

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
 *              lastStatement: the last statement in the string,
 *              isElementAccess: boolean that indicates if the lastStatement has an open
 *                               element access (e.g. `x["match`).
 *            }
 */
function analyzeInputString(str) {
  const bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;

  // Use an array in order to handle character with a length > 2 (e.g. 😎).
  const characters = Array.from(str);

  const buildReturnObject = () => {
    let isElementAccess = false;
    if (bodyStack.length === 1 && bodyStack[0].token === "[") {
      start = bodyStack[0].start;
      isElementAccess = true;
      if ([STATE_DQUOTE, STATE_QUOTE, STATE_TEMPLATE_LITERAL].includes(state)) {
        state = STATE_NORMAL;
      }
    }

    return {
      state,
      lastStatement: characters.slice(start).join(""),
      isElementAccess,
    };
  };

  for (let i = 0; i < characters.length; i++) {
    c = characters[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        } else if (c == "'") {
          state = STATE_QUOTE;
        } else if (c == "`") {
          state = STATE_TEMPLATE_LITERAL;
        } else if (";,:=<>+-*/%|&^~?!".split("").includes(c)) {
          // If the character is an operator, we need to update the start position.
          start = i + 1;
        } else if (c == " ") {
          const currentLastStatement = characters.slice(start, i).join("");
          const before = characters.slice(0, i);
          const after = characters.slice(i + 1);
          const trimmedBefore = Array.from(before.join("").trimRight());
          const trimmedAfter = Array.from(after.join("").trimLeft());

          const nextNonSpaceChar = trimmedAfter[0];
          const nextNonSpaceCharIndex = after.indexOf(nextNonSpaceChar);
          const previousNonSpaceChar = trimmedBefore[trimmedBefore.length - 1];

          // There's only spaces after that, so we can return.
          if (!nextNonSpaceChar) {
            return buildReturnObject();
          }

          // If the previous char in't a dot, and the next one isn't a dot either,
          // and the current computed statement is not a variable/function/class
          // declaration, update the start position.
          if (
            previousNonSpaceChar !== "." && nextNonSpaceChar !== "."
            && !NO_AUTOCOMPLETE_PREFIXES.includes(currentLastStatement)
          ) {
            start = i + nextNonSpaceCharIndex;
          }

          // Let's jump to handle the next non-space char.
          i = i + nextNonSpaceCharIndex;
        } else if (OPEN_BODY.includes(c)) {
          bodyStack.push({
            token: c,
            start,
          });
          start = i + 1;
        } else if (CLOSE_BODY.includes(c)) {
          const last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error",
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
            err: "unterminated string literal",
          };
        } else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Template literal state > ` <
      case STATE_TEMPLATE_LITERAL:
        if (c == "\\") {
          i++;
        } else if (c == "`") {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          i++;
        } else if (c == "\n") {
          return {
            err: "unterminated string literal",
          };
        } else if (c == "'") {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return buildReturnObject();
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * Debugger.Environment/Debugger.Object and inputValue.
 *
 * @param {Object} dbgObject
 *        When the debugger is not paused this Debugger.Object wraps
 *        the scope for autocompletion.
 *        It is null if the debugger is paused.
 * @param {Object} anEnvironment
 *        When the debugger is paused this Debugger.Environment is the
 *        scope for autocompletion.
 *        It is null if the debugger is not paused.
 * @param {String} inputValue
 *        Value that should be completed.
 * @param {Number} cursor (defaults to inputValue.length).
 *        Optional offset in the input where the cursor is located. If this is
 *        omitted then the cursor is assumed to be at the end of the input
 *        value.
 * @param {Boolean} invokeUnsafeGetter (defaults to false).
 *        Optional boolean to indicate if the function should execute unsafe getter
 *        in order to retrieve its result's properties.
 *        ⚠️ This should be set to true *ONLY* on user action as it may cause side-effects
 *        in the content page ⚠️
 * @returns null or object
 *          If the inputValue is an unsafe getter and invokeUnsafeGetter is false, the
 *          following form is returned:
 *
 *          {
 *            isUnsafeGetter: true,
 *            getterName: {String} The name of the unsafe getter
 *          }
 *
 *          If no completion valued could be computed, and the input is not an unsafe
 *          getter, null is returned.
 *
 *          Otherwise an object with the following form is returned:
 *            {
 *              matches: Set<string>
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *              isElementAccess: Boolean set to true if the evaluation is an element
 *                               access (e.g. `window["addEvent`).
 *            }
 */
function JSPropertyProvider(
  dbgObject,
  anEnvironment,
  inputValue,
  cursor,
  invokeUnsafeGetter = false
) {
  if (cursor === undefined) {
    cursor = inputValue.length;
  }

  inputValue = inputValue.substring(0, cursor);

  // Analyse the inputValue and find the beginning of the last part that
  // should be completed.
  const {
    err,
    state,
    lastStatement,
    isElementAccess,
  } = analyzeInputString(inputValue);

  // There was an error analysing the string.
  if (err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (state != STATE_NORMAL) {
    return null;
  }

  // Don't complete on just an empty string.
  if (lastStatement.trim() == "") {
    return null;
  }

  if (NO_AUTOCOMPLETE_PREFIXES.some(prefix => lastStatement.startsWith(prefix + " "))) {
    return null;
  }

  const completionPart = lastStatement;
  const lastDotIndex = completionPart.lastIndexOf(".");
  const lastOpeningBracketIndex = isElementAccess ? completionPart.lastIndexOf("[") : -1;
  const lastCompletionCharIndex = Math.max(lastDotIndex, lastOpeningBracketIndex);
  const startQuoteRegex = /^('|"|`)/;

  // Catch literals like [1,2,3] or "foo" and return the matches from
  // their prototypes.
  // Don't run this is a worker, migrating to acorn should allow this
  // to run in a worker - Bug 1217198.
  if (!isWorker && lastCompletionCharIndex > 0) {
    const parser = new Parser();
    parser.logExceptions = false;
    const syntaxTree = parser.get(completionPart.slice(0, lastCompletionCharIndex));
    const lastTree = syntaxTree.getLastSyntaxTree();
    const lastBody = lastTree && lastTree.AST.body[lastTree.AST.body.length - 1];

    // Finding the last expression since we've sliced up until the dot.
    // If there were parse errors this won't exist.
    if (lastBody) {
      const expression = lastBody.expression;
      const matchProp = completionPart.slice(lastCompletionCharIndex + 1).trimLeft();
      let search = matchProp;

      let elementAccessQuote;
      if (isElementAccess && startQuoteRegex.test(matchProp)) {
        elementAccessQuote = matchProp[0];
        search = matchProp.replace(startQuoteRegex, "");
      }

      if (expression.type === "ArrayExpression") {
        let arrayProtoProps = getMatchedProps(Array.prototype, search);
        if (isElementAccess) {
          arrayProtoProps = wrapMatchesInQuotes(arrayProtoProps, elementAccessQuote);
        }

        return {
          isElementAccess,
          matchProp,
          matches: arrayProtoProps,
        };
      }

      if (expression.type === "Literal" && typeof expression.value === "string") {
        let stringProtoProps = getMatchedProps(String.prototype, search);
        if (isElementAccess) {
          stringProtoProps = wrapMatchesInQuotes(stringProtoProps, elementAccessQuote);
        }

        return {
          isElementAccess,
          matchProp,
          matches: stringProtoProps,
        };
      }
    }
  }

  // We are completing a variable / a property lookup.
  const properties = completionPart.split(".");
  let matchProp;
  if (isElementAccess) {
    const lastPart = properties[properties.length - 1];
    const openBracketIndex = lastPart.lastIndexOf("[");
    matchProp = lastPart.substr(openBracketIndex + 1);
    properties[properties.length - 1] = lastPart.substring(0, openBracketIndex);
  } else {
    matchProp = properties.pop().trimLeft();
  }

  let search = matchProp;
  let elementAccessQuote;
  if (isElementAccess && startQuoteRegex.test(search)) {
    elementAccessQuote = search[0];
    search = search.replace(startQuoteRegex, "");
  }

  let obj = dbgObject;

  // The first property must be found in the environment of the paused debugger
  // or of the global lexical scope.
  const env = anEnvironment || obj.asEnvironment();

  if (properties.length === 0) {
    return {
      isElementAccess,
      matchProp,
      matches: getMatchedPropsInEnvironment(env, search),
    };
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
  for (let [index, prop] of properties.entries()) {
    prop = prop.trim();
    if (!prop) {
      return null;
    }

    if (!invokeUnsafeGetter && DevToolsUtils.isUnsafeGetter(obj, prop)) {
      // If the unsafe getter is not the last property access of the input, bail out as
      // things might get complex.
      if (index !== properties.length - 1) {
        return null;
      }

      // If we try to access an unsafe getter, return its name so we can consume that
      // on the frontend.
      return {
        isUnsafeGetter: true,
        getterName: prop,
      };
    }

    if (hasArrayIndex(prop)) {
      // The property to autocomplete is a member of array. For example
      // list[i][j]..[n]. Traverse the array to get the actual element.
      obj = getArrayMemberProperty(obj, null, prop);
    } else {
      obj = DevToolsUtils.getProperty(obj, prop, invokeUnsafeGetter);
    }

    if (!isObjectUsable(obj)) {
      return null;
    }
  }

  // If the final property is a primitive
  if (typeof obj != "object") {
    return {
      isElementAccess,
      matchProp,
      matches: getMatchedProps(obj, search),
    };
  }

  let matches = getMatchedPropsInDbgObject(obj, search);
  if (isElementAccess) {
    // If it's an element access, we need to wrap properties in quotes (either the one
    // the user already typed, or `"`).
    matches = wrapMatchesInQuotes(matches, elementAccessQuote);
  }
  return {isElementAccess, matchProp, matches};
}

function wrapMatchesInQuotes(matches, quote = `"`) {
  return new Set([...matches].map(p =>
    `${quote}${p.replace(new RegExp(`${quote}`, "g"), `\\${quote}`)}${quote}`));
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
 * @param {Mixed} obj
 *        Object whose properties we want to filter.
 * @param {string} match
 *        Filter for properties that match this string.
 * @returns {Set} List of matched properties.
 */
function getMatchedPropsImpl(obj, match, {chainIterator, getProperties}) {
  const matches = new Set();
  let numProps = 0;

  const insensitiveMatching = match && match[0].toUpperCase() !== match[0];
  const propertyMatches = prop => {
    return insensitiveMatching
      ? prop.toLocaleLowerCase().startsWith(match.toLocaleLowerCase())
      : prop.startsWith(match);
  };

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
      if (!propertyMatches(prop)) {
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

  return matches;
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
