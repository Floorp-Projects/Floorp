/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft= javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu, components} = require("chrome");
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
 * @param   string aStr
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
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == "'") {
          state = STATE_QUOTE;
        }
        else if (c == ";") {
          start = i + 1;
        }
        else if (c == " ") {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == "}") {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quote state > ' <
      case STATE_QUOTE:
        if (c == "\\") {
          i++;
        }
        else if (c == "\n") {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == "'") {
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
 * @param object aDbgObject
 *        When the debugger is not paused this Debugger.Object wraps the scope for autocompletion.
 *        It is null if the debugger is paused.
 * @param object anEnvironment
 *        When the debugger is paused this Debugger.Environment is the scope for autocompletion.
 *        It is null if the debugger is not paused.
 * @param string aInputValue
 *        Value that should be completed.
 * @param number [aCursor=aInputValue.length]
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
function JSPropertyProvider(aDbgObject, anEnvironment, aInputValue, aCursor)
{
  if (aCursor === undefined) {
    aCursor = aInputValue.length;
  }

  let inputValue = aInputValue.substring(0, aCursor);

  // Analyse the inputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(inputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = inputValue.substring(beginning.startPos);
  let lastDot = completionPart.lastIndexOf(".");

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  // Catch literals like [1,2,3] or "foo" and return the matches from
  // their prototypes.
  // Don't run this is a worker, migrating to acorn should allow this
  // to run in a worker - Bug 1217198.
  if (!isWorker && lastDot > 0) {
    let parser = new Parser();
    parser.logExceptions = false;
    let syntaxTree = parser.get(completionPart.slice(0, lastDot));
    let lastTree = syntaxTree.getLastSyntaxTree();
    let lastBody = lastTree && lastTree.AST.body[lastTree.AST.body.length - 1];

    // Finding the last expression since we've sliced up until the dot.
    // If there were parse errors this won't exist.
    if (lastBody) {
      let expression = lastBody.expression;
      let matchProp = completionPart.slice(lastDot + 1);
      if (expression.type === "ArrayExpression") {
        return getMatchedProps(Array.prototype, matchProp);
      } else if (expression.type === "Literal" &&
                 (typeof expression.value === "string")) {
        return getMatchedProps(String.prototype, matchProp);
      }
    }
  }

  // We are completing a variable / a property lookup.
  let properties = completionPart.split(".");
  let matchProp = properties.pop().trimLeft();
  let obj = aDbgObject;

  // The first property must be found in the environment of the paused debugger
  // or of the global lexical scope.
  let env = anEnvironment || obj.asEnvironment();

  if (properties.length === 0) {
    return getMatchedPropsInEnvironment(env, matchProp);
  }

  let firstProp = properties.shift().trim();
  if (firstProp === "this") {
    // Special case for 'this' - try to get the Object from the Environment.
    // No problem if it throws, we will just not autocomplete.
    try {
      obj = env.object;
    } catch(e) { }
  }
  else if (hasArrayIndex(firstProp)) {
    obj = getArrayMemberProperty(null, env, firstProp);
  } else {
    obj = getVariableInEnvironment(env, firstProp);
  }

  if (!isObjectUsable(obj)) {
    return null;
  }

  // We get the rest of the properties recursively starting from the Debugger.Object
  // that wraps the first property
  for (let i = 0; i < properties.length; i++) {
    let prop = properties[i].trim();
    if (!prop) {
      return null;
    }

    if (hasArrayIndex(prop)) {
      // The property to autocomplete is a member of array. For example
      // list[i][j]..[n]. Traverse the array to get the actual element.
      obj = getArrayMemberProperty(obj, null, prop);
    }
    else {
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
 * Get the array member of aObj for the given aProp. For example, given
 * aProp='list[0][1]' the element at [0][1] of aObj.list is returned.
 *
 * @param object aObj
 *        The object to operate on. Should be null if aEnv is passed.
 * @param object aEnv
 *        The Environment to operate in. Should be null if aObj is passed.
 * @param string aProp
 *        The property to return.
 * @return null or Object
 *         Returns null if the property couldn't be located. Otherwise the array
 *         member identified by aProp.
 */
function getArrayMemberProperty(aObj, aEnv, aProp)
{
  // First get the array.
  let obj = aObj;
  let propWithoutIndices = aProp.substr(0, aProp.indexOf("["));

  if (aEnv) {
    obj = getVariableInEnvironment(aEnv, propWithoutIndices);
  } else {
    obj = DevToolsUtils.getProperty(obj, propWithoutIndices);
  }

  if (!isObjectUsable(obj)) {
    return null;
  }

  // Then traverse the list of indices to get the actual element.
  let result;
  let arrayIndicesRegex = /\[[^\]]*\]/g;
  while ((result = arrayIndicesRegex.exec(aProp)) !== null) {
    let indexWithBrackets = result[0];
    let indexAsText = indexWithBrackets.substr(1, indexWithBrackets.length - 2);
    let index = parseInt(indexAsText);

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
 * @param Debugger.Object aObject
 *        The Debugger.Object to check.
 * @return boolean
 *         True if further inspection into the object is possible, or false
 *         otherwise.
 */
function isObjectUsable(aObject)
{
  if (aObject == null) {
    return false;
  }

  if (typeof aObject == "object" && aObject.class == "DeadObject") {
    return false;
  }

  return true;
}

/**
 * @see getExactMatch_impl()
 */
function getVariableInEnvironment(anEnvironment, aName)
{
  return getExactMatch_impl(anEnvironment, aName, DebuggerEnvironmentSupport);
}

/**
 * @see getMatchedProps_impl()
 */
function getMatchedPropsInEnvironment(anEnvironment, aMatch)
{
  return getMatchedProps_impl(anEnvironment, aMatch, DebuggerEnvironmentSupport);
}

/**
 * @see getMatchedProps_impl()
 */
function getMatchedPropsInDbgObject(aDbgObject, aMatch)
{
  return getMatchedProps_impl(aDbgObject, aMatch, DebuggerObjectSupport);
}

/**
 * @see getMatchedProps_impl()
 */
function getMatchedProps(aObj, aMatch)
{
  if (typeof aObj != "object") {
    aObj = aObj.constructor.prototype;
  }
  return getMatchedProps_impl(aObj, aMatch, JSObjectSupport);
}

/**
 * Get all properties in the given object (and its parent prototype chain) that
 * match a given prefix.
 *
 * @param mixed aObj
 *        Object whose properties we want to filter.
 * @param string aMatch
 *        Filter for properties that match this string.
 * @return object
 *         Object that contains the matchProp and the list of names.
 */
function getMatchedProps_impl(aObj, aMatch, {chainIterator, getProperties})
{
  let matches = new Set();
  let numProps = 0;

  // We need to go up the prototype chain.
  let iter = chainIterator(aObj);
  for (let obj of iter) {
    let props = getProperties(obj);
    numProps += props.length;

    // If there are too many properties to event attempt autocompletion,
    // or if we have already added the max number, then stop looping
    // and return the partial set that has already been discovered.
    if (numProps >= MAX_AUTOCOMPLETE_ATTEMPTS ||
        matches.size >= MAX_AUTOCOMPLETIONS) {
      break;
    }

    for (let i = 0; i < props.length; i++) {
      let prop = props[i];
      if (prop.indexOf(aMatch) != 0) {
        continue;
      }
      if (prop.indexOf('-') > -1) {
        continue;
      }
      // If it is an array index, we can't take it.
      // This uses a trick: converting a string to a number yields NaN if
      // the operation failed, and NaN is not equal to itself.
      if (+prop != +prop) {
        matches.add(prop);
      }

      if (matches.size >= MAX_AUTOCOMPLETIONS) {
        break;
      }
    }
  }

  return {
    matchProp: aMatch,
    matches: [...matches],
  };
}

/**
 * Returns a property value based on its name from the given object, by
 * recursively checking the object's prototype.
 *
 * @param object aObj
 *        An object to look the property into.
 * @param string aName
 *        The property that is looked up.
 * @returns object|undefined
 *        A Debugger.Object if the property exists in the object's prototype
 *        chain, undefined otherwise.
 */
function getExactMatch_impl(aObj, aName, {chainIterator, getProperty})
{
  // We need to go up the prototype chain.
  let iter = chainIterator(aObj);
  for (let obj of iter) {
    let prop = getProperty(obj, aName, aObj);
    if (prop) {
      return prop.value;
    }
  }
  return undefined;
}


var JSObjectSupport = {
  chainIterator: function*(aObj)
  {
    while (aObj) {
      yield aObj;
      aObj = Object.getPrototypeOf(aObj);
    }
  },

  getProperties: function(aObj)
  {
    return Object.getOwnPropertyNames(aObj);
  },

  getProperty: function()
  {
    // getProperty is unsafe with raw JS objects.
    throw "Unimplemented!";
  },
};

var DebuggerObjectSupport = {
  chainIterator: function*(aObj)
  {
    while (aObj) {
      yield aObj;
      aObj = aObj.proto;
    }
  },

  getProperties: function(aObj)
  {
    return aObj.getOwnPropertyNames();
  },

  getProperty: function(aObj, aName, aRootObj)
  {
    // This is left unimplemented in favor to DevToolsUtils.getProperty().
    throw "Unimplemented!";
  },
};

var DebuggerEnvironmentSupport = {
  chainIterator: function*(aObj)
  {
    while (aObj) {
      yield aObj;
      aObj = aObj.parent;
    }
  },

  getProperties: function(aObj)
  {
    let names = aObj.names();

    // Include 'this' in results (in sorted order)
    for (let i = 0; i < names.length; i++) {
      if (i === names.length - 1 || names[i+1] > "this") {
        names.splice(i+1, 0, "this");
        break;
      }
    }

    return names;
  },

  getProperty: function(aObj, aName)
  {
    let result;
    // Try/catch since aName can be anything, and getVariable throws if
    // it's not a valid ECMAScript identifier name
    try {
      // TODO: we should use getVariableDescriptor() here - bug 725815.
      result = aObj.getVariable(aName);
    } catch(e) { }

    // FIXME: Need actual UI, bug 941287.
    if (result === undefined || result.optimizedOut || result.missingArguments) {
      return null;
    }
    return { value: result };
  },
};


exports.JSPropertyProvider = DevToolsUtils.makeInfallible(JSPropertyProvider);

// Export a version that will throw (for tests)
exports.FallibleJSPropertyProvider = JSPropertyProvider;
