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
// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testCompletion1.js</p>";

function test() {
  return Task.spawn(function() {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);
    gcli.addItems(mockCommands.items);

    yield helpers.runTests(options, exports);

    gcli.removeItems(mockCommands.items);
    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}

// <INJECTED SOURCE:END>

// var helpers = require('./helpers');

exports.testActivate = function(options) {
  return helpers.audit(options, [
    {
      setup: '',
      check: {
        hints: ''
      }
    },
    {
      setup: ' ',
      check: {
        hints: ''
      }
    },
    {
      setup: 'tsr',
      check: {
        hints: ' <text>'
      }
    },
    {
      setup: 'tsr ',
      check: {
        hints: '<text>'
      }
    },
    {
      setup: 'tsr b',
      check: {
        hints: ''
      }
    },
    {
      setup: 'tsb',
      check: {
        hints: ' [toggle]'
      }
    },
    {
      setup: 'tsm',
      check: {
        hints: ' <abc> <txt> <num>'
      }
    },
    {
      setup: 'tsm ',
      check: {
        hints: 'a <txt> <num>'
      }
    },
    {
      setup: 'tsm a',
      check: {
        hints: ' <txt> <num>'
      }
    },
    {
      setup: 'tsm a ',
      check: {
        hints: '<txt> <num>'
      }
    },
    {
      setup: 'tsm a  ',
      check: {
        hints: '<txt> <num>'
      }
    },
    {
      setup: 'tsm a  d',
      check: {
        hints: ' <num>'
      }
    },
    {
      setup: 'tsm a "d d"',
      check: {
        hints: ' <num>'
      }
    },
    {
      setup: 'tsm a "d ',
      check: {
        hints: ' <num>'
      }
    },
    {
      setup: 'tsm a "d d" ',
      check: {
        hints: '<num>'
      }
    },
    {
      setup: 'tsm a "d d ',
      check: {
        hints: ' <num>'
      }
    },
    {
      setup: 'tsm d r',
      check: {
        hints: ' <num>'
      }
    },
    {
      setup: 'tsm a d ',
      check: {
        hints: '<num>'
      }
    },
    {
      setup: 'tsm a d 4',
      check: {
        hints: ''
      }
    },
    {
      setup: 'tsg',
      check: {
        hints: ' <solo> [options]'
      }
    },
    {
      setup: 'tsg ',
      check: {
        hints: 'aaa [options]'
      }
    },
    {
      setup: 'tsg a',
      check: {
        hints: 'aa [options]'
      }
    },
    {
      setup: 'tsg b',
      check: {
        hints: 'bb [options]'
      }
    },
    {
      skipIf: options.isPhantomjs,
      setup: 'tsg d',
      check: {
        hints: ' [options] -> ccc'
      }
    },
    {
      setup: 'tsg aa',
      check: {
        hints: 'a [options]'
      }
    },
    {
      setup: 'tsg aaa',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsg aaa ',
      check: {
        hints: '[options]'
      }
    },
    {
      setup: 'tsg aaa d',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsg aaa dddddd',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsg aaa dddddd ',
      check: {
        hints: '[options]'
      }
    },
    {
      setup: 'tsg aaa "d',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsg aaa "d d',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsg aaa "d d"',
      check: {
        hints: ' [options]'
      }
    },
    {
      setup: 'tsn ex ',
      check: {
        hints: ''
      }
    },
    {
      setup: 'selarr',
      check: {
        hints: ' -> tselarr'
      }
    },
    {
      setup: 'tselar 1',
      check: {
        hints: ''
      }
    },
    {
      name: 'tselar |1',
      setup: function() {
        return helpers.setInput(options, 'tselar 1', 7);
      },
      check: {
        hints: ''
      }
    },
    {
      name: 'tselar| 1',
      setup: function() {
        return helpers.setInput(options, 'tselar 1', 6);
      },
      check: {
        hints: ' -> tselarr'
      }
    },
    {
      name: 'tsela|r 1',
      setup: function() {
        return helpers.setInput(options, 'tselar 1', 5);
      },
      check: {
        hints: ' -> tselarr'
      }
    },
  ]);
};
