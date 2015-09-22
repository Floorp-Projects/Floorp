/*
 * Copyright 2012, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

'use strict';

var l10n = require('../util/l10n');

var Conversion = require('./types').Conversion;
var Type = require('./types').Type;
var Status = require('./types').Status;

/**
 * 'javascript' handles scripted input
 */
function JavascriptType(typeSpec) {
}

JavascriptType.prototype = Object.create(Type.prototype);

JavascriptType.prototype.getSpec = function(commandName, paramName) {
  return {
    name: 'remote',
    paramName: paramName
  };
};

JavascriptType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  return value;
};

/**
 * When sorting out completions, there is no point in displaying millions of
 * matches - this the number of matches that we aim for
 */
JavascriptType.MAX_COMPLETION_MATCHES = 10;

JavascriptType.prototype.parse = function(arg, context) {
  var typed = arg.text;
  var scope = (context.environment.window == null) ?
              null : context.environment.window;

  // No input is undefined
  if (typed === '') {
    return Promise.resolve(new Conversion(undefined, arg, Status.INCOMPLETE));
  }
  // Just accept numbers
  if (!isNaN(parseFloat(typed)) && isFinite(typed)) {
    return Promise.resolve(new Conversion(typed, arg));
  }
  // Just accept constants like true/false/null/etc
  if (typed.trim().match(/(null|undefined|NaN|Infinity|true|false)/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // Analyze the input text and find the beginning of the last part that
  // should be completed.
  var beginning = this._findCompletionBeginning(typed);

  // There was an error analyzing the string.
  if (beginning.err) {
    return Promise.resolve(new Conversion(typed, arg, Status.ERROR, beginning.err));
  }

  // If the current state is ParseState.COMPLEX, then we can't do completion.
  // so bail out now
  if (beginning.state === ParseState.COMPLEX) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // If the current state is not ParseState.NORMAL, then we are inside of a
  // string which means that no completion is possible.
  if (beginning.state !== ParseState.NORMAL) {
    return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
  }

  var completionPart = typed.substring(beginning.startPos);
  var properties = completionPart.split('.');
  var matchProp;
  var prop;

  if (properties.length > 1) {
    matchProp = properties.pop().trimLeft();
    for (var i = 0; i < properties.length; i++) {
      prop = properties[i].trim();

      // We can't complete on null.foo, so bail out
      if (scope == null) {
        return Promise.resolve(new Conversion(typed, arg, Status.ERROR,
                                        l10n.lookup('jstypeParseScope')));
      }

      if (prop === '') {
        return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
      }

      // Check if prop is a getter function on 'scope'. Functions can change
      // other stuff so we can't execute them to get the next object. Stop here.
      if (this._isSafeProperty(scope, prop)) {
        return Promise.resolve(new Conversion(typed, arg));
      }

      try {
        scope = scope[prop];
      }
      catch (ex) {
        // It would be nice to be able to report this error in some way but
        // as it can happen just when someone types '{sessionStorage.', it
        // almost doesn't really count as an error, so we ignore it
        return Promise.resolve(new Conversion(typed, arg, Status.VALID, ''));
      }
    }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If the reason we just stopped adjusting the scope was a non-simple string,
  // then we're not sure if the input is valid or invalid, so accept it
  if (prop && !prop.match(/^[0-9A-Za-z]*$/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // However if the prop was a simple string, it is an error
  if (scope == null) {
    var msg = l10n.lookupFormat('jstypeParseMissing', [ prop ]);
    return Promise.resolve(new Conversion(typed, arg, Status.ERROR, msg));
  }

  // If the thing we're looking for isn't a simple string, then we're not going
  // to find it, but we're not sure if it's valid or invalid, so accept it
  if (!matchProp.match(/^[0-9A-Za-z]*$/)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  // Skip Iterators and Generators.
  if (this._isIteratorOrGenerator(scope)) {
    return Promise.resolve(new Conversion(typed, arg));
  }

  var matchLen = matchProp.length;
  var prefix = matchLen === 0 ? typed : typed.slice(0, -matchLen);
  var status = Status.INCOMPLETE;
  var message = '';

  // We really want an array of matches (for sorting) but it's easier to
  // detect existing members if we're using a map initially
  var matches = {};

  // We only display a maximum of MAX_COMPLETION_MATCHES, so there is no point
  // in digging up the prototype chain for matches that we're never going to
  // use. Initially look for matches directly on the object itself and then
  // look up the chain to find more
  var distUpPrototypeChain = 0;
  var root = scope;
  try {
    while (root != null &&
        Object.keys(matches).length < JavascriptType.MAX_COMPLETION_MATCHES) {

      /* jshint loopfunc:true */
      Object.keys(root).forEach(function(property) {
        // Only add matching properties. Also, as we're walking up the
        // prototype chain, properties on 'higher' prototypes don't override
        // similarly named properties lower down
        if (property.indexOf(matchProp) === 0 && !(property in matches)) {
          matches[property] = {
            prop: property,
            distUpPrototypeChain: distUpPrototypeChain
          };
        }
      });

      distUpPrototypeChain++;
      root = Object.getPrototypeOf(root);
    }
  }
  catch (ex) {
    return Promise.resolve(new Conversion(typed, arg, Status.INCOMPLETE, ''));
  }

  // Convert to an array for sorting, and while we're at it, note if we got
  // an exact match so we know that this input is valid
  matches = Object.keys(matches).map(function(property) {
    if (property === matchProp) {
      status = Status.VALID;
    }
    return matches[property];
  });

  // The sort keys are:
  // - Being on the object itself, not in the prototype chain
  // - The lack of existence of a vendor prefix
  // - The name
  matches.sort(function(m1, m2) {
    if (m1.distUpPrototypeChain !== m2.distUpPrototypeChain) {
      return m1.distUpPrototypeChain - m2.distUpPrototypeChain;
    }
    // Push all vendor prefixes to the bottom of the list
    return isVendorPrefixed(m1.prop) ?
      (isVendorPrefixed(m2.prop) ? m1.prop.localeCompare(m2.prop) : 1) :
      (isVendorPrefixed(m2.prop) ? -1 : m1.prop.localeCompare(m2.prop));
  });

  // Trim to size. There is a bug for doing a better job of finding matches
  // (bug 682694), but in the mean time there is a performance problem
  // associated with creating a large number of DOM nodes that few people will
  // ever read, so trim ...
  if (matches.length > JavascriptType.MAX_COMPLETION_MATCHES) {
    matches = matches.slice(0, JavascriptType.MAX_COMPLETION_MATCHES - 1);
  }

  // Decorate the matches with:
  // - a description
  // - a value (for the menu) and,
  // - an incomplete flag which reports if we should assume that the user isn't
  //   going to carry on the JS expression with this input so far
  var predictions = matches.map(function(match) {
    var description;
    var incomplete = true;

    if (this._isSafeProperty(scope, match.prop)) {
      description = '(property getter)';
    }
    else {
      try {
        var value = scope[match.prop];

        if (typeof value === 'function') {
          description = '(function)';
        }
        else if (typeof value === 'boolean' || typeof value === 'number') {
          description = '= ' + value;
          incomplete = false;
        }
        else if (typeof value === 'string') {
          if (value.length > 40) {
            value = value.substring(0, 37) + '…';
          }
          description = '= \'' + value + '\'';
          incomplete = false;
        }
        else {
          description = '(' + typeof value + ')';
        }
      }
      catch (ex) {
        description = '(' + l10n.lookup('jstypeParseError') + ')';
      }
    }

    return {
      name: prefix + match.prop,
      value: {
        name: prefix + match.prop,
        description: description
      },
      description: description,
      incomplete: incomplete
    };
  }, this);

  if (predictions.length === 0) {
    status = Status.ERROR;
    message = l10n.lookupFormat('jstypeParseMissing', [ matchProp ]);
  }

  // If the match is the only one possible, and its VALID, predict nothing
  if (predictions.length === 1 && status === Status.VALID) {
    predictions = [];
  }

  return Promise.resolve(new Conversion(typed, arg, status, message,
                                  Promise.resolve(predictions)));
};

/**
 * Does the given property have a prefix that indicates that it is vendor
 * specific?
 */
function isVendorPrefixed(name) {
  return name.indexOf('moz') === 0 ||
         name.indexOf('webkit') === 0 ||
         name.indexOf('ms') === 0;
}

/**
 * Constants used in return value of _findCompletionBeginning()
 */
var ParseState = {
  /**
   * We have simple input like window.foo, without any punctuation that makes
   * completion prediction be confusing or wrong
   */
  NORMAL: 0,

  /**
   * The cursor is in some Javascript that makes completion hard to predict,
   * like console.log(
   */
  COMPLEX: 1,

  /**
   * The cursor is inside single quotes (')
   */
  QUOTE: 2,

  /**
   * The cursor is inside single quotes (")
   */
  DQUOTE: 3
};

var OPEN_BODY = '{[('.split('');
var CLOSE_BODY = '}])'.split('');
var OPEN_CLOSE_BODY = {
  '{': '}',
  '[': ']',
  '(': ')'
};

/**
 * How we distinguish between simple and complex JS input. We attempt
 * completion against simple JS.
 */
var simpleChars = /[a-zA-Z0-9.]/;

/**
 * Analyzes a given string to find the last statement that is interesting for
 * later completion.
 * @param text A string to analyze
 * @return If there was an error in the string detected, then a object like
 *   { err: 'ErrorMesssage' }
 * is returned, otherwise a object like
 *   {
 *     state: ParseState.NORMAL|ParseState.QUOTE|ParseState.DQUOTE,
 *     startPos: index of where the last statement begins
 *   }
 */
JavascriptType.prototype._findCompletionBeginning = function(text) {
  var bodyStack = [];

  var state = ParseState.NORMAL;
  var start = 0;
  var c;
  var complex = false;

  for (var i = 0; i < text.length; i++) {
    c = text[i];
    if (!simpleChars.test(c)) {
      complex = true;
    }

    switch (state) {
      // Normal JS state.
      case ParseState.NORMAL:
        if (c === '"') {
          state = ParseState.DQUOTE;
        }
        else if (c === '\'') {
          state = ParseState.QUOTE;
        }
        else if (c === ';') {
          start = i + 1;
        }
        else if (c === ' ') {
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
            return { err: l10n.lookup('jstypeBeginSyntax') };
          }
          if (c === '}') {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case ParseState.DQUOTE:
        if (c === '\\') {
          i ++;
        }
        else if (c === '\n') {
          return { err: l10n.lookup('jstypeBeginUnterm') };
        }
        else if (c === '"') {
          state = ParseState.NORMAL;
        }
        break;

      // Single quote state > ' <
      case ParseState.QUOTE:
        if (c === '\\') {
          i ++;
        }
        else if (c === '\n') {
          return { err: l10n.lookup('jstypeBeginUnterm') };
        }
        else if (c === '\'') {
          state = ParseState.NORMAL;
        }
        break;
    }
  }

  if (state === ParseState.NORMAL && complex) {
    state = ParseState.COMPLEX;
  }

  return {
    state: state,
    startPos: start
  };
};

/**
 * Return true if the passed object is either an iterator or a generator, and
 * false otherwise
 * @param obj The object to check
 */
JavascriptType.prototype._isIteratorOrGenerator = function(obj) {
  if (obj === null) {
    return false;
  }

  if (typeof aObject === 'object') {
    if (typeof obj.__iterator__ === 'function' ||
        obj.constructor && obj.constructor.name === 'Iterator') {
      return true;
    }

    try {
      var str = obj.toString();
      if (typeof obj.next === 'function' &&
          str.indexOf('[object Generator') === 0) {
        return true;
      }
    }
    catch (ex) {
      // window.history.next throws in the typeof check above.
      return false;
    }
  }

  return false;
};

/**
 * Would calling 'scope[prop]' cause the invocation of a non-native (i.e. user
 * defined) function property?
 * Since calling functions can have side effects, it's only safe to do that if
 * explicitly requested, rather than because we're trying things out for the
 * purposes of completion.
 */
JavascriptType.prototype._isSafeProperty = function(scope, prop) {
  if (typeof scope !== 'object') {
    return false;
  }

  // Walk up the prototype chain of 'scope' looking for a property descriptor
  // for 'prop'
  var propDesc;
  while (scope) {
    try {
      propDesc = Object.getOwnPropertyDescriptor(scope, prop);
      if (propDesc) {
        break;
      }
    }
    catch (ex) {
      // Native getters throw here. See bug 520882.
      if (ex.name === 'NS_ERROR_XPC_BAD_CONVERT_JS' ||
          ex.name === 'NS_ERROR_XPC_BAD_OP_ON_WN_PROTO') {
        return false;
      }
      return true;
    }
    scope = Object.getPrototypeOf(scope);
  }

  if (!propDesc) {
    return false;
  }

  if (!propDesc.get) {
    return false;
  }

  // The property is safe if 'get' isn't a function or if the function has a
  // prototype (in which case it's native)
  return typeof propDesc.get !== 'function' || 'prototype' in propDesc.get;
};

JavascriptType.prototype.name = 'javascript';

exports.items = [ JavascriptType ];
