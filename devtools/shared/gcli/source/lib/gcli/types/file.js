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

/*
 * The file type is a bit of a spiders-web, but there isn't a nice solution
 * yet. The core of the problem is that the modules used by Firefox and NodeJS
 * intersect with the modules used by the web, but not each other. Except here.
 * So we have to do something fancy to get the sharing but not mess up the web.
 *
 * This file requires 'gcli/types/fileparser', and there are 4 implementations
 * of this:
 * - '/lib/gcli/types/fileparser.js', the default web version that uses XHR to
 *   talk to the node server
 * - '/lib/server/gcli/types/fileparser.js', an NodeJS stub, and ...
 * - '/mozilla/gcli/types/fileparser.js', the Firefox implementation both of
 *   these are shims which import
 * - 'gcli/util/fileparser', does the real work, except the actual file access
 *
 * The file access comes from the 'gcli/util/filesystem' module, and there are
 * 2 implementations of this:
 * - '/lib/server/gcli/util/filesystem.js', which uses NodeJS APIs
 * - '/mozilla/gcli/util/filesystem.js', which uses OS.File APIs
 */

var fileparser = require('./fileparser');
var Conversion = require('./types').Conversion;

exports.items = [
  {
    item: 'type',
    name: 'file',

    filetype: 'any',    // One of 'file', 'directory', 'any'
    existing: 'maybe',  // Should be one of 'yes', 'no', 'maybe'
    matches: undefined, // RegExp to match the file part of the path

    hasPredictions: true,

    constructor: function() {
      if (this.filetype !== 'any' && this.filetype !== 'file' &&
          this.filetype !== 'directory') {
        throw new Error('filetype must be one of [any|file|directory]');
      }

      if (this.existing !== 'yes' && this.existing !== 'no' &&
          this.existing !== 'maybe') {
        throw new Error('existing must be one of [yes|no|maybe]');
      }
    },

    getSpec: function(commandName, paramName) {
      return {
        name: 'remote',
        commandName: commandName,
        paramName: paramName
      };
    },

    stringify: function(file) {
      if (file == null) {
        return '';
      }

      return file.toString();
    },

    parse: function(arg, context) {
      var options = {
        filetype: this.filetype,
        existing: this.existing,
        matches: this.matches
      };
      var promise = fileparser.parse(context, arg.text, options);

      return promise.then(function(reply) {
        return new Conversion(reply.value, arg, reply.status,
                              reply.message, reply.predictor);
      });
    }
  }
];
