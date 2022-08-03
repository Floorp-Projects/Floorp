/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DevToolsUtils = require("devtools/shared/DevToolsUtils");

const {
  evalWithDebugger,
} = require("devtools/server/actors/webconsole/eval-with-debugger");

if (!isWorker) {
  loader.lazyRequireGetter(
    this,
    "getSyntaxTrees",
    "devtools/shared/webconsole/parser-helper",
    true
  );
}
loader.lazyRequireGetter(
  this,
  "Reflect",
  "resource://gre/modules/reflect.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  [
    "analyzeInputString",
    "shouldInputBeAutocompleted",
    "shouldInputBeEagerlyEvaluated",
  ],
  "devtools/shared/webconsole/analyze-input-string",
  true
);

// Provide an easy way to bail out of even attempting an autocompletion
// if an object has way too many properties. Protects against large objects
// with numeric values that wouldn't be tallied towards MAX_AUTOCOMPLETIONS.
const MAX_AUTOCOMPLETE_ATTEMPTS = (exports.MAX_AUTOCOMPLETE_ATTEMPTS = 100000);
// Prevent iterating over too many properties during autocomplete suggestions.
const MAX_AUTOCOMPLETIONS = (exports.MAX_AUTOCOMPLETIONS = 1500);

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
 * - {Array<string>}: expressionVars
 *        Optional array containing variable defined in the expression. Those variables
 *        are extracted from CodeMirror state.
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
// eslint-disable-next-line complexity
function JSPropertyProvider({
  dbgObject,
  environment,
  inputValue,
  cursor,
  authorizedEvaluations = [],
  webconsoleActor,
  selectedNodeActor,
  expressionVars = [],
}) {
  if (cursor === undefined) {
    cursor = inputValue.length;
  }

  inputValue = inputValue.substring(0, cursor);

  // Analyse the inputValue and find the beginning of the last part that
  // should be completed.
  const inputAnalysis = analyzeInputString(inputValue);

  if (!shouldInputBeAutocompleted(inputAnalysis)) {
    return null;
  }

  let {
    lastStatement,
    isElementAccess,
    mainExpression,
    matchProp,
    isPropertyAccess,
  } = inputAnalysis;

  // Eagerly evaluate the main expression and return the results properties.
  // e.g. `obj.func().a` will evaluate `obj.func()` and return properties matching `a`.
  // NOTE: this is only useful when the input has a property access.
  if (webconsoleActor && shouldInputBeEagerlyEvaluated(inputAnalysis)) {
    const eagerResponse = evalWithDebugger(
      mainExpression,
      { eager: true, selectedNodeActor },
      webconsoleActor
    );

    const ret = eagerResponse?.result?.return;

    // Only send matches if eager evaluation returned something meaningful
    if (ret && ret !== undefined) {
      const matches =
        typeof ret != "object"
          ? getMatchedProps(ret, matchProp)
          : getMatchedPropsInDbgObject(ret, matchProp);

      return prepareReturnedObject({
        matches,
        search: matchProp,
        isElementAccess,
      });
    }
  }

  // AST representation of the expression before the last access char (`.` or `[`).
  let astExpression;
  const startQuoteRegex = /^('|"|`)/;
  const env = environment || dbgObject.asEnvironment();

  // Catch literals like [1,2,3] or "foo" and return the matches from
  // their prototypes.
  // Don't run this is a worker, migrating to acorn should allow this
  // to run in a worker - Bug 1217198.
  if (!isWorker && isPropertyAccess) {
    const syntaxTrees = getSyntaxTrees(mainExpression);
    const lastTree = syntaxTrees[syntaxTrees.length - 1];
    const lastBody = lastTree?.body[lastTree.body.length - 1];

    // Finding the last expression since we've sliced up until the dot.
    // If there were parse errors this won't exist.
    if (lastBody) {
      if (!lastBody.expression) {
        return null;
      }

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
          /\d[^\.]{0}\.$/.test(lastStatement) === false
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
    if (isPropertyAccess) {
      properties = getPropertiesFromAstExpression(astExpression);

      if (properties === null) {
        return null;
      }
    }
  } else {
    properties = lastStatement.split(".");
    if (isElementAccess) {
      const lastPart = properties[properties.length - 1];
      const openBracketIndex = lastPart.lastIndexOf("[");
      matchProp = lastPart.substr(openBracketIndex + 1);
      properties[properties.length - 1] = lastPart.substring(
        0,
        openBracketIndex
      );
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
    const environmentProperties = getMatchedPropsInEnvironment(env, search);
    const expressionVariables = new Set(
      expressionVars.filter(variableName => variableName.startsWith(matchProp))
    );

    for (const prop of environmentProperties) {
      expressionVariables.add(prop);
    }

    return {
      isElementAccess,
      matchProp,
      matches: expressionVariables,
    };
  }

  let firstProp = properties.shift();
  if (typeof firstProp == "string") {
    firstProp = firstProp.trim();
  }

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
      x => JSON.stringify(x) === JSON.stringify(propPath)
    );

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

  const matches =
    typeof obj != "object"
      ? getMatchedProps(obj, search)
      : getMatchedPropsInDbgObject(obj, search);
  return prepareReturnedObject({
    matches,
    search,
    isElementAccess,
    elementAccessQuote,
  });
}

function hasArrayIndex(str) {
  return /\[\d+\]$/.test(str);
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
  while (outermostEnv?.parent) {
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
  const { type, property, object, name, expression } = ast;
  if (type === "ThisExpression") {
    result.unshift("this");
  } else if (type === "Identifier" && name) {
    result.unshift(name);
  } else if (type === "OptionalExpression" && expression) {
    result = (getPropertiesFromAstExpression(expression) || []).concat(result);
  } else if (
    type === "MemberExpression" ||
    type === "OptionalMemberExpression"
  ) {
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
  return new Set(
    [...matches].map(p => {
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
    })
  );
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
    const indexAsText = indexWithBrackets.substr(
      1,
      indexWithBrackets.length - 2
    );
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

function prepareReturnedObject({
  matches,
  search,
  isElementAccess,
  elementAccessQuote,
}) {
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

  return { isElementAccess, matchProp: search, matches };
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
function getMatchedPropsImpl(obj, match, { chainIterator, getProperties }) {
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
    if (
      numProps >= MAX_AUTOCOMPLETE_ATTEMPTS ||
      matches.size >= MAX_AUTOCOMPLETIONS
    ) {
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
function getExactMatchImpl(obj, name, { chainIterator, getProperty }) {
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
  *chainIterator(obj) {
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

  getProperties(obj) {
    try {
      return Object.getOwnPropertyNames(obj);
    } catch (error) {
      // The above can throw e.g. for some proxy objects.
      return null;
    }
  },

  getProperty() {
    // getProperty is unsafe with raw JS objects.
    throw new Error("Unimplemented!");
  },
};

var DebuggerObjectSupport = {
  *chainIterator(obj) {
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

  getProperties(obj) {
    try {
      return obj.getOwnPropertyNames();
    } catch (error) {
      // The above can throw e.g. for some proxy objects.
      return null;
    }
  },

  getProperty(obj, name, rootObj) {
    // This is left unimplemented in favor to DevToolsUtils.getProperty().
    throw new Error("Unimplemented!");
  },
};

var DebuggerEnvironmentSupport = {
  *chainIterator(obj) {
    while (obj) {
      yield obj;
      obj = obj.parent;
    }
  },

  getProperties(obj) {
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

  getProperty(obj, name) {
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
    if (
      result == null ||
      (typeof result == "object" &&
        (result.optimizedOut || result.missingArguments))
    ) {
      return null;
    }
    return { value: result };
  },
};

exports.JSPropertyProvider = DevToolsUtils.makeInfallible(JSPropertyProvider);

// Export a version that will throw (for tests)
exports.FallibleJSPropertyProvider = JSPropertyProvider;
