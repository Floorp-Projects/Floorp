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
loader.lazyRequireGetter(this, "Reflect", "resource://gre/modules/reflect.jsm", true);

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

          // If the previous char isn't a dot or opening bracket, and the next one isn't
          // one either, and the current computed statement is not a
          // variable/function/class declaration, update the start position.
          if (
            previousNonSpaceChar !== "." && nextNonSpaceChar !== "." &&
            previousNonSpaceChar !== "[" && nextNonSpaceChar !== "[" &&
            !NO_AUTOCOMPLETE_PREFIXES.includes(currentLastStatement)
          ) {
            start = i + (
              nextNonSpaceCharIndex >= 0
                ? nextNonSpaceCharIndex
                : (after.length + 1)
            );
          }

          // There's only spaces after that, so we can return.
          if (!nextNonSpaceChar) {
            return buildReturnObject();
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
 * @param {Object} An object of the following shape:
 * - {Object} dbgObject
 *        When the debugger is not paused this Debugger.Object wraps
 *        the scope for autocompletion.
 *        It is null if the debugger is paused.
 * - {Object} environment
 *        When the debugger is paused this Debugger.Environment is the
 *        scope for autocompletion.
 *        It is null if the debugger is not paused.
 * - {String} inputValue
 *        Value that should be completed.
 * - {Number} cursor (defaults to inputValue.length).
 *        Optional offset in the input where the cursor is located. If this is
 *        omitted then the cursor is assumed to be at the end of the input
 *        value.
 * - {Array} authorizedEvaluations (defaults to []).
 *        Optional array containing all the different properties access that the engine
 *        can execute in order to retrieve its result's properties.
 *        ⚠️ This should be set to true *ONLY* on user action as it may cause side-effects
 *        in the content page ⚠️
 * - {WebconsoleActor} webconsoleActor
 *        A reference to a webconsole actor which we can use to retrieve the last
 *        evaluation result or create a debuggee value.
 * - {String}: selectedNodeActor
 *        The actor id of the selected node in the inspector.
 * @returns null or object
 *          If the inputValue is an unsafe getter and invokeUnsafeGetter is false, the
 *          following form is returned:
 *
 *          {
 *            isUnsafeGetter: true,
 *            getterPath: {Array<String>} An array of the property chain leading to the
 *                        getter. Example: ["x", "myGetter"]
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
function JSPropertyProvider({
  dbgObject,
  environment,
  inputValue,
  cursor,
  authorizedEvaluations = [],
  webconsoleActor,
  selectedNodeActor,
}) {
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

  const env = environment || dbgObject.asEnvironment();
  const completionPart = lastStatement;
  const lastDotIndex = completionPart.lastIndexOf(".");
  const lastOpeningBracketIndex = isElementAccess ? completionPart.lastIndexOf("[") : -1;
  const lastCompletionCharIndex = Math.max(lastDotIndex, lastOpeningBracketIndex);
  const startQuoteRegex = /^('|"|`)/;

  // AST representation of the expression before the last access char (`.` or `[`).
  let astExpression;
  let matchProp = completionPart.slice(lastCompletionCharIndex + 1).trimLeft();

  // Catch literals like [1,2,3] or "foo" and return the matches from
  // their prototypes.
  // Don't run this is a worker, migrating to acorn should allow this
  // to run in a worker - Bug 1217198.
  if (!isWorker && lastCompletionCharIndex > 0) {
    const parser = new Parser();
    parser.logExceptions = false;
    const parsedExpression = completionPart.slice(0, lastCompletionCharIndex);
    const syntaxTree = parser.get(parsedExpression);
    const lastTree = syntaxTree.getLastSyntaxTree();
    const lastBody = lastTree && lastTree.AST.body[lastTree.AST.body.length - 1];

    // Finding the last expression since we've sliced up until the dot.
    // If there were parse errors this won't exist.
    if (lastBody) {
      astExpression = lastBody.expression;
      let matchingObject;

      if (astExpression.type === "ArrayExpression") {
        matchingObject = getContentPrototypeObject(env, "Array");
      } else if (
        astExpression.type === "Literal" &&
        typeof astExpression.value === "string"
      ) {
        matchingObject = getContentPrototypeObject(env, "String");
      } else if (
        astExpression.type === "Literal" &&
        Number.isFinite(astExpression.value)
      ) {
        // The parser rightfuly indicates that we have a number in some cases (e.g. `1.`),
        // but we don't want to return Number proto properties in that case since
        // the result would be invalid (i.e. `1.toFixed()` throws).
        // So if the expression value is an integer, it should not end with `{Number}.`
        // (but the following are fine: `1..`, `(1.).`).
        if (
          !Number.isInteger(astExpression.value) ||
          /\d[^\.]{0}\.$/.test(completionPart) === false
        ) {
          matchingObject = getContentPrototypeObject(env, "Number");
        } else {
          return null;
        }
      }

      if (matchingObject) {
        let search = matchProp;

        let elementAccessQuote;
        if (isElementAccess && startQuoteRegex.test(matchProp)) {
          elementAccessQuote = matchProp[0];
          search = matchProp.replace(startQuoteRegex, "");
        }

        let props = getMatchedPropsInDbgObject(matchingObject, search);
        if (isElementAccess) {
          props = wrapMatchesInQuotes(props, elementAccessQuote);
        }

        return {
          isElementAccess,
          matchProp,
          matches: props,
        };
      }
    }
  }

  // We are completing a variable / a property lookup.
  let properties = [];

  if (astExpression) {
    if (lastCompletionCharIndex > -1) {
      properties = getPropertiesFromAstExpression(astExpression);

      if (properties === null) {
        return null;
      }
    }
  } else {
    properties = completionPart.split(".");
    if (isElementAccess) {
      const lastPart = properties[properties.length - 1];
      const openBracketIndex = lastPart.lastIndexOf("[");
      matchProp = lastPart.substr(openBracketIndex + 1);
      properties[properties.length - 1] = lastPart.substring(0, openBracketIndex);
    } else {
      matchProp = properties.pop().trimLeft();
    }
  }

  let search = matchProp;
  let elementAccessQuote;
  if (isElementAccess && startQuoteRegex.test(search)) {
    elementAccessQuote = search[0];
    search = search.replace(startQuoteRegex, "");
  }

  let obj = dbgObject;
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
  } else if (firstProp === "$_" && webconsoleActor) {
    obj = webconsoleActor.getLastConsoleInputEvaluation();
  } else if (firstProp === "$0" && selectedNodeActor && webconsoleActor) {
    const actor = webconsoleActor.conn.getActor(selectedNodeActor);
    if (actor) {
      try {
        obj = webconsoleActor.makeDebuggeeValue(actor.rawNode);
      } catch (e) {
        // Ignore.
      }
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
    if (typeof prop === "string") {
      prop = prop.trim();
    }

    if (prop === undefined || prop === null || prop === "") {
      return null;
    }

    const propPath = [firstProp].concat(properties.slice(0, index + 1));
    const authorized = authorizedEvaluations.some(
      x => JSON.stringify(x) === JSON.stringify(propPath));

    if (!authorized && DevToolsUtils.isUnsafeGetter(obj, prop)) {
      // If we try to access an unsafe getter, return its name so we can consume that
      // on the frontend.
      return {
        isUnsafeGetter: true,
        getterPath: propPath,
      };
    }

    if (hasArrayIndex(prop)) {
      // The property to autocomplete is a member of array. For example
      // list[i][j]..[n]. Traverse the array to get the actual element.
      obj = getArrayMemberProperty(obj, null, prop);
    } else {
      obj = DevToolsUtils.getProperty(obj, prop, authorized);
    }

    if (!isObjectUsable(obj)) {
      return null;
    }
  }

  const prepareReturnedObject = matches => {
    if (isElementAccess) {
      // If it's an element access, we need to wrap properties in quotes (either the one
      // the user already typed, or `"`).
      matches = wrapMatchesInQuotes(matches, elementAccessQuote);
    } else if (!isWorker) {
      // If we're not performing an element access, we need to check that the property
      // are suited for a dot access. (Reflect.jsm is not available in worker context yet,
      // see Bug 1507181).
      for (const match of matches) {
        try {
          // In order to know if the property is suited for dot notation, we use Reflect
          // to parse an expression where we try to access the property with a dot. If it
          // throws, this means that we need to do an element access instead.
          Reflect.parse(`({${match}: true})`);
        } catch (e) {
          matches.delete(match);
        }
      }
    }

    return {isElementAccess, matchProp, matches};
  };

  // If the final property is a primitive
  if (typeof obj != "object") {
    return prepareReturnedObject(getMatchedProps(obj, search));
  }

  return prepareReturnedObject(getMatchedPropsInDbgObject(obj, search));
}

/**
 * For a given environment and constructor name, returns its Debugger.Object wrapped
 * prototype.
 *
 * @param {Environment} env
 * @param {String} name: Name of the constructor object we want the prototype of.
 * @returns {Debugger.Object|null} the prototype, or null if it not found.
 */
function getContentPrototypeObject(env, name) {
  // Retrieve the outermost environment to get the global object.
  let outermostEnv = env;
  while (outermostEnv && outermostEnv.parent) {
    outermostEnv = outermostEnv.parent;
  }

  const constructorObj = DevToolsUtils.getProperty(outermostEnv.object, name);
  if (!constructorObj) {
    return null;
  }

  return DevToolsUtils.getProperty(constructorObj, "prototype");
}

/**
 * @param {Object} ast: An AST representing a property access (e.g. `foo.bar["baz"].x`)
 * @returns {Array|null} An array representing the property access
 *                       (e.g. ["foo", "bar", "baz", "x"]).
 */
function getPropertiesFromAstExpression(ast) {
  let result = [];
  if (!ast) {
    return result;
  }
  const {type, property, object, name} = ast;
  if (type === "ThisExpression") {
    result.unshift("this");
  } else if (type === "Identifier" && name) {
    result.unshift(name);
  } else if (type === "MemberExpression") {
    if (property) {
      if (property.type === "Identifier" && property.name) {
        result.unshift(property.name);
      } else if (property.type === "Literal") {
        result.unshift(property.value);
      }
    }
    if (object) {
      result = (getPropertiesFromAstExpression(object) || []).concat(result);
    }
  } else {
    return null;
  }
  return result;
}

function wrapMatchesInQuotes(matches, quote = `"`) {
  return new Set([...matches].map(p => {
    // Escape as a double-quoted string literal
    p = JSON.stringify(p);

    // We don't have to do anything more when using double quotes
    if (quote == `"`) {
      return p;
    }

    // Remove surrounding double quotes
    p = p.slice(1, -1);

    // Unescape inner double quotes (all must be escaped, so no need to count backslashes)
    p = p.replace(/\\(?=")/g, "");

    // Escape the specified quote (assuming ' or `, which are treated literally in regex)
    p = p.replace(new RegExp(quote, "g"), "\\$&");

    // Template literals treat ${ specially, escape it
    if (quote == "`") {
      p = p.replace(/\${/g, "\\$&");
    }

    // Surround the result with quotes
    return `${quote}${p}${quote}`;
  }));
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
function getVariableInEnvironment(environment, name) {
  return getExactMatchImpl(environment, name, DebuggerEnvironmentSupport);
}

/**
 * @see getMatchedPropsImpl()
 */
function getMatchedPropsInEnvironment(environment, match) {
  return getMatchedPropsImpl(environment, match, DebuggerEnvironmentSupport);
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
