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

// define(function(require, exports, module) {

// <INJECTED SOURCE:START>

// THIS FILE IS GENERATED FROM SOURCE IN THE GCLI PROJECT
// DO NOT EDIT IT DIRECTLY

var exports = {};

const TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testKeyboard.js</p>";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.runTests(options, exports);
  }).then(finish);
}

// <INJECTED SOURCE:END>

'use strict';

var javascript = require('gcli/types/javascript');
// var helpers = require('gclitest/helpers');
// var mockCommands = require('gclitest/mockCommands');
var canon = require('gcli/canon');

var tempWindow = undefined;

exports.setup = function(options) {
  mockCommands.setup();

  tempWindow = javascript.getGlobalObject();
  javascript.setGlobalObject(options.window);
};

exports.shutdown = function(options) {
  javascript.setGlobalObject(tempWindow);
  tempWindow = undefined;

  mockCommands.shutdown();
};

// Bug 664377: Add tests for internal completion. i.e. "tsela<TAB> 1"

exports.testSimple = function(options) {
  return helpers.audit(options, [
    {
      setup: 'tsela<TAB>',
      check: { input: 'tselarr ', cursor: 8 }
    },
    {
      setup: 'tsn di<TAB>',
      check: { input: 'tsn dif ', cursor: 8 }
    },
    {
      setup: 'tsg a<TAB>',
      check: { input: 'tsg aaa ', cursor: 8 }
    }
  ]);
};

exports.testComplete = function(options) {
  return helpers.audit(options, [
    {
      setup: 'tsn e<DOWN><DOWN><DOWN><DOWN><DOWN><TAB>',
      check: { input: 'tsn exte ' }
    },
    {
      setup: 'tsn e<DOWN><DOWN><DOWN><DOWN><TAB>',
      check: { input: 'tsn ext ' }
    },
    {
      setup: 'tsn e<DOWN><DOWN><DOWN><TAB>',
      check: { input: 'tsn extend ' }
    },
    {
      setup: 'tsn e<DOWN><DOWN><TAB>',
      check: { input: 'tsn exten ' }
    },
    {
      setup: 'tsn e<DOWN><TAB>',
      check: { input: 'tsn exte ' }
    },
    {
      setup: 'tsn e<TAB>',
      check: { input: 'tsn ext ' }
    },
    {
      setup: 'tsn e<UP><TAB>',
      check: { input: 'tsn extend ' }
    },
    {
      setup: 'tsn e<UP><UP><TAB>',
      check: { input: 'tsn exten ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><TAB>',
      check: { input: 'tsn exte ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><UP><TAB>',
      check: { input: 'tsn ext ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><UP><UP><TAB>',
      check: { input: 'tsn extend ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><UP><UP><UP><TAB>',
      check: { input: 'tsn exten ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><UP><UP><UP><UP><TAB>',
      check: { input: 'tsn exte ' }
    },
    {
      setup: 'tsn e<UP><UP><UP><UP><UP><UP><UP><UP><TAB>',
      check: { input: 'tsn ext ' }
    }
  ]);
};

exports.testScript = function(options) {
  return helpers.audit(options, [
    {
      skipIf: function commandJsMissing() {
        return canon.getCommand('{') == null;
      },
      setup: '{ wind<TAB>',
      check: { input: '{ window' }
    },
    {
      skipIf: function commandJsMissing() {
        return canon.getCommand('{') == null;
      },
      setup: '{ window.docum<TAB>',
      check: { input: '{ window.document' }
    }
  ]);
};

exports.testJsdom = function(options) {
  return helpers.audit(options, [
    {
      skipIf: function jsDomOrCommandJsMissing() {
        return options.isJsdom || canon.getCommand('{') == null;
      },
      setup: '{ window.document.titl<TAB>',
      check: { input: '{ window.document.title ' }
    }
  ]);
};

exports.testIncr = function(options) {
  return helpers.audit(options, [
    /*
    // We currently refuse to increment/decrement things with a non-valid
    // status which makes sense for many cases, and is a decent default.
    // However in theory we could do better, these tests are there for then
    {
      setup: 'tsu -70<UP>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -7<UP>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -6<UP>',
      check: { input: 'tsu -5' }
    },
    */
    {
      setup: 'tsu -5<UP>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -4<UP>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -3<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu -2<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu -1<UP>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 0<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 1<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 2<UP>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 3<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 4<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 5<UP>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 6<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 7<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 8<UP>',
      check: { input: 'tsu 9' }
    },
    {
      setup: 'tsu 9<UP>',
      check: { input: 'tsu 10' }
    },
    {
      setup: 'tsu 10<UP>',
      check: { input: 'tsu 10' }
    }
    /*
    // See notes above
    {
      setup: 'tsu 100<UP>',
      check: { input: 'tsu 10' }
    }
    */
  ]);
};

exports.testDecr = function(options) {
  return helpers.audit(options, [
    /*
    // See notes at top of testIncr
    {
      setup: 'tsu -70<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -7<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -6<DOWN>',
      check: { input: 'tsu -5' }
    },
    */
    {
      setup: 'tsu -5<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -4<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -3<DOWN>',
      check: { input: 'tsu -5' }
    },
    {
      setup: 'tsu -2<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu -1<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu 0<DOWN>',
      check: { input: 'tsu -3' }
    },
    {
      setup: 'tsu 1<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 2<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 3<DOWN>',
      check: { input: 'tsu 0' }
    },
    {
      setup: 'tsu 4<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 5<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 6<DOWN>',
      check: { input: 'tsu 3' }
    },
    {
      setup: 'tsu 7<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 8<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 9<DOWN>',
      check: { input: 'tsu 6' }
    },
    {
      setup: 'tsu 10<DOWN>',
      check: { input: 'tsu 9' }
    }
    /*
    // See notes at top of testIncr
    {
      setup: 'tsu 100<DOWN>',
      check: { input: 'tsu 9' }
    }
    */
  ]);
};

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

// });
