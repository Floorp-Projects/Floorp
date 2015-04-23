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
  helpers.runTestModule(exports, "browser_gcli_keyboard4.js");
}

// var helpers = require('./helpers');

exports.testIncrFloat = function(options) {
  return helpers.audit(options, [
    /*
    // See notes at top of testIncr
    {
      setup: 'tsf -70<UP>',
      check: { input: 'tsf -6.5' }
    },
    */
    {
      setup: 'tsf -6.5<UP>',
      check: { input: 'tsf -6' }
    },
    {
      setup: 'tsf -6<UP>',
      check: { input: 'tsf -4.5' }
    },
    {
      setup: 'tsf -4.5<UP>',
      check: { input: 'tsf -3' }
    },
    {
      setup: 'tsf -4<UP>',
      check: { input: 'tsf -3' }
    },
    {
      setup: 'tsf -3<UP>',
      check: { input: 'tsf -1.5' }
    },
    {
      setup: 'tsf -1.5<UP>',
      check: { input: 'tsf 0' }
    },
    {
      setup: 'tsf 0<UP>',
      check: { input: 'tsf 1.5' }
    },
    {
      setup: 'tsf 1.5<UP>',
      check: { input: 'tsf 3' }
    },
    {
      setup: 'tsf 2<UP>',
      check: { input: 'tsf 3' }
    },
    {
      setup: 'tsf 3<UP>',
      check: { input: 'tsf 4.5' }
    },
    {
      setup: 'tsf 5<UP>',
      check: { input: 'tsf 6' }
    }
    /*
    // See notes at top of testIncr
    {
      setup: 'tsf 100<UP>',
      check: { input: 'tsf -6.5' }
    }
    */
  ]);
};

exports.testDecrFloat = function(options) {
  return helpers.audit(options, [
    /*
    // See notes at top of testIncr
    {
      setup: 'tsf -70<DOWN>',
      check: { input: 'tsf 11.5' }
    },
    */
    {
      setup: 'tsf -6.5<DOWN>',
      check: { input: 'tsf -6.5' }
    },
    {
      setup: 'tsf -6<DOWN>',
      check: { input: 'tsf -6.5' }
    },
    {
      setup: 'tsf -4.5<DOWN>',
      check: { input: 'tsf -6' }
    },
    {
      setup: 'tsf -4<DOWN>',
      check: { input: 'tsf -4.5' }
    },
    {
      setup: 'tsf -3<DOWN>',
      check: { input: 'tsf -4.5' }
    },
    {
      setup: 'tsf -1.5<DOWN>',
      check: { input: 'tsf -3' }
    },
    {
      setup: 'tsf 0<DOWN>',
      check: { input: 'tsf -1.5' }
    },
    {
      setup: 'tsf 1.5<DOWN>',
      check: { input: 'tsf 0' }
    },
    {
      setup: 'tsf 2<DOWN>',
      check: { input: 'tsf 1.5' }
    },
    {
      setup: 'tsf 3<DOWN>',
      check: { input: 'tsf 1.5' }
    },
    {
      setup: 'tsf 5<DOWN>',
      check: { input: 'tsf 4.5' }
    }
    /*
    // See notes at top of testIncr
    {
      setup: 'tsf 100<DOWN>',
      check: { input: 'tsf 11.5' }
    }
    */
  ]);
};

exports.testIncrSelection = function(options) {
  /*
  // Bug 829516:  GCLI up/down navigation over selection is sometimes bizarre
  return helpers.audit(options, [
    {
      setup: 'tselarr <DOWN>',
      check: { hints: '2' },
      exec: {}
    },
    {
      setup: 'tselarr <DOWN><DOWN>',
      check: { hints: '3' },
      exec: {}
    },
    {
      setup: 'tselarr <DOWN><DOWN><DOWN>',
      check: { hints: '1' },
      exec: {}
    }
  ]);
  */
};

exports.testDecrSelection = function(options) {
  /*
  // Bug 829516:  GCLI up/down navigation over selection is sometimes bizarre
  return helpers.audit(options, [
    {
      setup: 'tselarr <UP>',
      check: { hints: '3' }
    }
  ]);
  */
};
