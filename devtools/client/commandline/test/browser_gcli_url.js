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

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// PLEASE TALK TO SOMEONE IN DEVELOPER TOOLS BEFORE EDITING IT

const exports = {};

function test() {
  helpers.runTestModule(exports, "browser_gcli_url.js");
}

// var assert = require('../testharness/assert');
// var helpers = require('./helpers');

exports.testDefault = function(options) {
  return helpers.audit(options, [
    {
      skipRemainingIf: options.isPhantomjs, // PhantomJS URL type is broken
      setup:    'urlc',
      check: {
        input:  'urlc',
        markup: 'VVVV',
        hints:        ' <url>',
        status: 'ERROR',
        args: {
          url: {
            value: undefined,
            arg: '',
            status: 'INCOMPLETE'
          }
        }
      }
    },
    {
      setup:    'urlc example',
      check: {
        input:  'urlc example',
        markup: 'VVVVVIIIIIII',
        hints:              ' -> http://example/',
        predictions: [
          'http://example/',
          'https://example/',
          'http://localhost:9999/example'
        ],
        status: 'ERROR',
        args: {
          url: {
            value: undefined,
            arg: ' example',
            status: 'INCOMPLETE'
          }
        }
      },
    },
    {
      setup:    'urlc example.com/',
      check: {
        input:  'urlc example.com/',
        markup: 'VVVVVIIIIIIIIIIII',
        hints:                   ' -> http://example.com/',
        status: 'ERROR',
        args: {
          url: {
            value: undefined,
            arg: ' example.com/',
            status: 'INCOMPLETE'
          }
        }
      },
    },
    {
      setup:    'urlc http://example.com/index?q=a#hash',
      check: {
        input:  'urlc http://example.com/index?q=a#hash',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        hints:                                        '',
        status: 'VALID',
        args: {
          url: {
            value: function(data) {
              assert.is(data.hash, '#hash', 'url hash');
            },
            arg: ' http://example.com/index?q=a#hash',
            status: 'VALID'
          }
        }
      },
      exec: { output: /"url": ?/ }
    }
  ]);
};
