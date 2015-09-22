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

var Conversion = require('./types').Conversion;
var Status = require('./types').Status;
var BlankArgument = require('./types').BlankArgument;

/**
 * The types we expose for registration
 */
exports.items = [
  // A type for "we don't know right now, but hope to soon"
  {
    item: 'type',
    name: 'delegate',

    getSpec: function(commandName, paramName) {
      return {
        name: 'delegate',
        param: paramName
      };
    },

    // Child types should implement this method to return an instance of the type
    // that should be used. If no type is available, or some sort of temporary
    // placeholder is required, BlankType can be used.
    delegateType: undefined,

    stringify: function(value, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.stringify(value, context);
      }.bind(this));
    },

    parse: function(arg, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.parse(arg, context);
      }.bind(this));
    },

    nudge: function(value, by, context) {
      return this.getType(context).then(function(delegated) {
        return delegated.nudge ?
               delegated.nudge(value, by, context) :
               undefined;
      }.bind(this));
    },

    getType: function(context) {
      if (this.delegateType === undefined) {
        return Promise.resolve(this.types.createType('blank'));
      }

      var type = this.delegateType(context);
      if (typeof type.parse !== 'function') {
        type = this.types.createType(type);
      }
      return Promise.resolve(type);
    },

    // DelegateType is designed to be inherited from, so DelegateField needs a
    // way to check if something works like a delegate without using 'name'
    isDelegate: true,

    // Technically we perhaps should proxy this, except that properties are
    // inherently synchronous, so we can't. It doesn't seem important enough to
    // change the function definition to accommodate this right now
    isImportant: false
  },
  {
    item: 'type',
    name: 'remote',
    paramName: undefined,
    blankIsValid: false,

    getSpec: function(commandName, paramName) {
      return {
        name: 'remote',
        commandName: commandName,
        paramName: paramName,
        blankIsValid: this.blankIsValid
      };
    },

    getBlank: function(context) {
      if (this.blankIsValid) {
        return new Conversion({ stringified: '' },
                              new BlankArgument(), Status.VALID);
      }
      else {
        return new Conversion(undefined, new BlankArgument(),
                              Status.INCOMPLETE, '');
      }
    },

    stringify: function(value, context) {
      if (value == null) {
        return '';
      }
      // remote types are client only, and we don't attempt to transfer value
      // objects to the client (we can't be sure the are jsonable) so it is a
      // bit strange to be asked to stringify a value object, however since
      // parse creates a Conversion with a (fake) value object we might be
      // asked to stringify that. We can stringify fake value objects.
      if (typeof value.stringified === 'string') {
        return value.stringified;
      }
      throw new Error('Can\'t stringify that value');
    },

    parse: function(arg, context) {
      return this.front.parseType(context.typed, this.paramName).then(function(json) {
        var status = Status.fromString(json.status);
        return new Conversion(undefined, arg, status, json.message, json.predictions);
      }.bind(this));
    },

    nudge: function(value, by, context) {
      return this.front.nudgeType(context.typed, by, this.paramName).then(function(json) {
        return { stringified: json.arg };
      }.bind(this));
    }
  },
  // 'blank' is a type for use with DelegateType when we don't know yet.
  // It should not be used anywhere else.
  {
    item: 'type',
    name: 'blank',

    getSpec: function(commandName, paramName) {
      return 'blank';
    },

    stringify: function(value, context) {
      return '';
    },

    parse: function(arg, context) {
      return Promise.resolve(new Conversion(undefined, arg));
    }
  }
];
