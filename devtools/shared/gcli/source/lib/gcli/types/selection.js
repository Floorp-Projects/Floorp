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
var spell = require('../util/spell');
var Type = require('./types').Type;
var Status = require('./types').Status;
var Conversion = require('./types').Conversion;
var BlankArgument = require('./types').BlankArgument;

/**
 * A selection allows the user to pick a value from known set of options.
 * An option is made up of a name (which is what the user types) and a value
 * (which is passed to exec)
 * @param typeSpec Object containing properties that describe how this
 * selection functions. Properties include:
 * - lookup: An array of objects, one for each option, which contain name and
 *   value properties. lookup can be a function which returns this array
 * - data: An array of strings - alternative to 'lookup' where the valid values
 *   are strings. i.e. there is no mapping between what is typed and the value
 *   that is used by the program
 * - stringifyProperty: Conversion from value to string is generally a process
 *   of looking through all the valid options for a matching value, and using
 *   the associated name. However the name maybe available directly from the
 *   value using a property lookup. Setting 'stringifyProperty' allows
 *   SelectionType to take this shortcut.
 * - cacheable: If lookup is a function, then we normally assume that
 *   the values fetched can change. Setting 'cacheable:true' enables internal
 *   caching.
 */
function SelectionType(typeSpec) {
  if (typeSpec) {
    Object.keys(typeSpec).forEach(function(key) {
      this[key] = typeSpec[key];
    }, this);
  }

  if (this.name !== 'selection' &&
      this.lookup == null && this.data == null) {
    throw new Error(this.name + ' has no lookup or data');
  }

  this._dataToLookup = this._dataToLookup.bind(this);
}

SelectionType.prototype = Object.create(Type.prototype);

SelectionType.prototype.getSpec = function(commandName, paramName) {
  var spec = { name: 'selection' };
  if (this.lookup != null && typeof this.lookup !== 'function') {
    spec.lookup = this.lookup;
  }
  if (this.data != null && typeof this.data !== 'function') {
    spec.data = this.data;
  }
  if (this.stringifyProperty != null) {
    spec.stringifyProperty = this.stringifyProperty;
  }
  if (this.cacheable) {
    spec.cacheable = true;
  }
  if (typeof this.lookup === 'function' || typeof this.data === 'function') {
    spec.commandName = commandName;
    spec.paramName = paramName;
    spec.remoteLookup = true;
  }
  return spec;
};

SelectionType.prototype.stringify = function(value, context) {
  if (value == null) {
    return '';
  }
  if (this.stringifyProperty != null) {
    return value[this.stringifyProperty];
  }

  return this.getLookup(context).then(lookup => {
    var name = null;
    lookup.some(function(item) {
      if (item.value === value) {
        name = item.name;
        return true;
      }
      return false;
    }, this);
    return name;
  });
};

/**
 * If typeSpec contained cacheable:true then calls to parse() work on cached
 * data. clearCache() enables the cache to be cleared.
 */
SelectionType.prototype.clearCache = function() {
  this._cachedLookup = undefined;
};

/**
 * There are several ways to get selection data. This unifies them into one
 * single function.
 * @return An array of objects with name and value properties.
 */
SelectionType.prototype.getLookup = function(context) {
  if (this._cachedLookup != null) {
    return this._cachedLookup;
  }

  var reply;

  if (this.remoteLookup) {
    reply = this.front.getSelectionLookup(this.commandName, this.paramName);
    reply = resolve(reply, context);
  }
  else if (typeof this.lookup === 'function') {
    reply = resolve(this.lookup.bind(this), context);
  }
  else if (this.lookup != null) {
    reply = resolve(this.lookup, context);
  }
  else if (this.data != null) {
    reply = resolve(this.data, context).then(this._dataToLookup);
  }
  else {
    throw new Error(this.name + ' has no lookup or data');
  }

  if (this.cacheable) {
    this._cachedLookup = reply;
  }

  if (reply == null) {
    console.error(arguments);
  }
  return reply;
};

/**
 * Both 'lookup' and 'data' properties (see docs on SelectionType constructor)
 * in addition to being real data can be a function or a promise, or even a
 * function which returns a promise of real data, etc. This takes a thing and
 * returns a promise of actual values.
 */
function resolve(thing, context) {
  return Promise.resolve(thing).then(function(resolved) {
    if (typeof resolved === 'function') {
      return resolve(resolved(context), context);
    }
    return resolved;
  });
}

/**
 * Selection can be provided with either a lookup object (in the 'lookup'
 * property) or an array of strings (in the 'data' property). Internally we
 * always use lookup, so we need a way to convert a 'data' array to a lookup.
 */
SelectionType.prototype._dataToLookup = function(data) {
  if (!Array.isArray(data)) {
    throw new Error('data for ' + this.name + ' resolved to non-array');
  }

  return data.map(function(option) {
    return { name: option, value: option };
  });
};

/**
 * Return a list of possible completions for the given arg.
 * @param arg The initial input to match
 * @return A trimmed array of string:value pairs
 */
exports.findPredictions = function(arg, lookup) {
  var predictions = [];
  var i, option;
  var maxPredictions = Conversion.maxPredictions;
  var match = arg.text.toLowerCase();

  // If the arg has a suffix then we're kind of 'done'. Only an exact match
  // will do.
  if (arg.suffix.length > 0) {
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option.name === arg.text) {
        predictions.push(option);
      }
    }

    return predictions;
  }

  // Cache lower case versions of all the option names
  for (i = 0; i < lookup.length; i++) {
    option = lookup[i];
    if (option._gcliLowerName == null) {
      option._gcliLowerName = option.name.toLowerCase();
    }
  }

  // Exact hidden matches. If 'hidden: true' then we only allow exact matches
  // All the tests after here check that !isHidden(option)
  for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
    option = lookup[i];
    if (option.name === arg.text) {
      predictions.push(option);
    }
  }

  // Start with prefix matching
  for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
    option = lookup[i];
    if (option._gcliLowerName.indexOf(match) === 0 && !isHidden(option)) {
      if (predictions.indexOf(option) === -1) {
        predictions.push(option);
      }
    }
  }

  // Try infix matching if we get less half max matched
  if (predictions.length < (maxPredictions / 2)) {
    for (i = 0; i < lookup.length && predictions.length < maxPredictions; i++) {
      option = lookup[i];
      if (option._gcliLowerName.indexOf(match) !== -1 && !isHidden(option)) {
        if (predictions.indexOf(option) === -1) {
          predictions.push(option);
        }
      }
    }
  }

  // Try fuzzy matching if we don't get a prefix match
  if (predictions.length === 0) {
    var names = [];
    lookup.forEach(function(opt) {
      if (!isHidden(opt)) {
        names.push(opt.name);
      }
    });
    var corrected = spell.correct(match, names);
    if (corrected) {
      lookup.forEach(function(opt) {
        if (opt.name === corrected) {
          predictions.push(opt);
        }
      }, this);
    }
  }

  return predictions;
};

SelectionType.prototype.parse = function(arg, context) {
  return Promise.resolve(this.getLookup(context)).then(lookup => {
    var predictions = exports.findPredictions(arg, lookup);
    return exports.convertPredictions(arg, predictions);
  });
};

/**
 * Decide what sort of conversion to return based on the available predictions
 * and how they match the passed arg
 */
exports.convertPredictions = function(arg, predictions) {
  if (predictions.length === 0) {
    var msg = l10n.lookupFormat('typesSelectionNomatch', [ arg.text ]);
    return new Conversion(undefined, arg, Status.ERROR, msg,
                          Promise.resolve(predictions));
  }

  if (predictions[0].name === arg.text) {
    var value = predictions[0].value;
    return new Conversion(value, arg, Status.VALID, '',
                          Promise.resolve(predictions));
  }

  return new Conversion(undefined, arg, Status.INCOMPLETE, '',
                        Promise.resolve(predictions));
};

/**
 * Checking that an option is hidden involves messing in properties on the
 * value right now (which isn't a good idea really) we really should be marking
 * that on the option, so this encapsulates the problem
 */
function isHidden(option) {
  return option.hidden === true ||
         (option.value != null && option.value.hidden);
}

SelectionType.prototype.getBlank = function(context) {
  var predictFunc = context2 => {
    return Promise.resolve(this.getLookup(context2)).then(function(lookup) {
      return lookup.filter(function(option) {
        return !isHidden(option);
      }).slice(0, Conversion.maxPredictions - 1);
    });
  };

  return new Conversion(undefined, new BlankArgument(), Status.INCOMPLETE, '',
                        predictFunc);
};

/**
 * Increment and decrement are confusing for selections. +1 is -1 and -1 is +1.
 * Given an array e.g. [ 'a', 'b', 'c' ] with the current selection on 'b',
 * displayed to the user in the natural way, i.e.:
 *
 *   'a'
 *   'b' <- highlighted as current value
 *   'c'
 *
 * Pressing the UP arrow should take us to 'a', which decrements this index
 * (compare pressing UP on a number which would increment the number)
 *
 * So for selections, we treat +1 as -1 and -1 as +1.
 */
SelectionType.prototype.nudge = function(value, by, context) {
  return this.getLookup(context).then(lookup => {
    var index = this._findValue(lookup, value);
    if (index === -1) {
      if (by < 0) {
        // We're supposed to be doing a decrement (which means +1), but the
        // value isn't found, so we reset the index to the top of the list
        // which is index 0
        index = 0;
      }
      else {
        // For an increment operation when there is nothing to start from, we
        // want to start from the top, i.e. index 0, so the value before we
        // 'increment' (see note above) must be 1.
        index = 1;
      }
    }

    // This is where we invert the sense of up/down (see doc comment)
    index -= by;

    if (index >= lookup.length) {
      index = 0;
    }
    return lookup[index].value;
  });
};

/**
 * Walk through an array of { name:.., value:... } objects looking for a
 * matching value (using strict equality), returning the matched index (or -1
 * if not found).
 * @param lookup Array of objects with name/value properties to search through
 * @param value The value to search for
 * @return The index at which the match was found, or -1 if no match was found
 */
SelectionType.prototype._findValue = function(lookup, value) {
  var index = -1;
  for (var i = 0; i < lookup.length; i++) {
    var pair = lookup[i];
    if (pair.value === value) {
      index = i;
      break;
    }
  }
  return index;
};

/**
 * This is how we indicate to SelectionField that we have predictions that
 * might work in a menu.
 */
SelectionType.prototype.hasPredictions = true;

SelectionType.prototype.name = 'selection';

exports.SelectionType = SelectionType;
exports.items = [ SelectionType ];
