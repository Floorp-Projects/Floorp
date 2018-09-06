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

var ArrayConversion = require('./types').ArrayConversion;
var ArrayArgument = require('./types').ArrayArgument;

exports.items = [
  {
    // A set of objects of the same type
    item: 'type',
    name: 'array',
    subtype: undefined,

    constructor: function() {
      if (!this.subtype) {
        console.error('Array.typeSpec is missing subtype. Assuming string.' +
            this.name);
        this.subtype = 'string';
      }
      this.subtype = this.types.createType(this.subtype);
    },

    getSpec: function(commandName, paramName) {
      return {
        name: 'array',
        subtype: this.subtype.getSpec(commandName, paramName),
      };
    },

    stringify: function(values, context) {
      if (values == null) {
        return '';
      }
      // BUG 664204: Check for strings with spaces and add quotes
      return values.join(' ');
    },

    parse: function(arg, context) {
      if (arg.type !== 'ArrayArgument') {
        console.error('non ArrayArgument to ArrayType.parse', arg);
        throw new Error('non ArrayArgument to ArrayType.parse');
      }

      // Parse an argument to a conversion
      // Hack alert. ArrayConversion needs to be able to answer questions about
      // the status of individual conversions in addition to the overall state.
      // |subArg.conversion| allows us to do that easily.
      var subArgParse = subArg => {
        return this.subtype.parse(subArg, context).then(conversion => {
          subArg.conversion = conversion;
          return conversion;
        });
      };

      var conversionPromises = arg.getArguments().map(subArgParse);
      return Promise.all(conversionPromises).then(function(conversions) {
        return new ArrayConversion(conversions, arg);
      });
    },

    getBlank: function(context) {
      return new ArrayConversion([], new ArrayArgument());
    }
  },
];
