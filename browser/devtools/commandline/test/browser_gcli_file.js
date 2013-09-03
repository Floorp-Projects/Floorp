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

var TEST_URI = "data:text/html;charset=utf-8,<p id='gcli-input'>gcli-testFile.js</p>";

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

var local = false;

exports.testBasic = function(options) {
  return helpers.audit(options, [
    {
      // These tests require us to be using node directly or to be in
      // PhantomJS connected to an execute enabled node server or to be in
      // firefox.
      skipRemainingIf: options.isPhantomjs || options.isFirefox,
      setup:    'tsfile open /',
      check: {
        input:  'tsfile open /',
        hints:               '',
        markup: 'VVVVVVVVVVVVI',
        cursor: 13,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' is not a file',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' is not a file'
          }
        }
      }
    },
    {
      setup:    'tsfile open /zxcv',
      check: {
        input:  'tsfile open /zxcv',
        // hints:                   ' -> /etc/',
        markup: 'VVVVVVVVVVVVIIIII',
        cursor: 17,
        current: 'p1',
        status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile open /mach_kernel',
      check: {
        input:  'tsfile open /mach_kernel',
        hints:                          '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 24,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
           }
        }
      }
    },
    {
      setup:    'tsfile saveas /',
      check: {
        input:  'tsfile saveas /',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVVI',
        cursor: 15,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' already exists',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' already exists'
          }
        }
      }
    },
    {
      setup:    'tsfile saveas /zxcv',
      check: {
        input:  'tsfile saveas /zxcv',
        // hints:                     ' -> /etc/',
        markup: 'VVVVVVVVVVVVVVVVVVV',
        cursor: 19,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile saveas /mach_kernel',
      check: {
        input:  'tsfile saveas /mach_kernel',
        hints:                            '',
        markup: 'VVVVVVVVVVVVVVIIIIIIIIIIII',
        cursor: 26,
        current: 'p1',
        status: 'ERROR',
        message: '\'/mach_kernel\' already exists',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: undefined,
            arg: ' /mach_kernel',
            status: 'INCOMPLETE',
            message: '\'/mach_kernel\' already exists'
          }
        }
      }
    },
    {
      setup:    'tsfile save /',
      check: {
        input:  'tsfile save /',
        hints:               '',
        markup: 'VVVVVVVVVVVVI',
        cursor: 13,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' is not a file',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' is not a file'
          }
        }
      }
    },
    {
      setup:    'tsfile save /zxcv',
      check: {
        input:  'tsfile save /zxcv',
        // hints:                   ' -> /etc/',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile save /mach_kernel',
      check: {
        input:  'tsfile save /mach_kernel',
        hints:                          '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 24,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsfile cd /',
      check: {
        input:  'tsfile cd /',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: '/',
            arg: ' /',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsfile cd /zxcv',
      check: {
        input:  'tsfile cd /zxcv',
        // hints:                 ' -> /dev/',
        markup: 'VVVVVVVVVVIIIII',
        cursor: 15,
        current: 'p1',
        status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: true || !local,
      setup:    'tsfile cd /etc/passwd',
      check: {
        input:  'tsfile cd /etc/passwd',
        hints:                       ' -> /etc/pam.d/',
        markup: 'VVVVVVVVVVIIIIIIIIIII',
        cursor: 21,
        current: 'p1',
        status: 'ERROR',
        message: '\'/etc/passwd\' is not a directory',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: undefined,
            arg: ' /etc/passwd',
            status: 'INCOMPLETE',
            message: '\'/etc/passwd\' is not a directory'
          }
        }
      }
    },
    {
      setup:    'tsfile mkdir /',
      check: {
        input:  'tsfile mkdir /',
        hints:                '',
        markup: 'VVVVVVVVVVVVVI',
        cursor: 14,
        current: 'p1',
        status: 'ERROR',
        message: ''/' already exists',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' already exists'
          }
        }
      }
    },
    {
      setup:    'tsfile mkdir /zxcv',
      check: {
        input:  'tsfile mkdir /zxcv',
        // hints:                    ' -> /dev/',
        markup: 'VVVVVVVVVVVVVVVVVV',
        cursor: 18,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile mkdir /mach_kernel',
      check: {
        input:  'tsfile mkdir /mach_kernel',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVIIIIIIIIIIII',
        cursor: 25,
        current: 'p1',
        status: 'ERROR',
        message: '\'/mach_kernel\' already exists',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: undefined,
            arg: ' /mach_kernel',
            status: 'INCOMPLETE',
            message: '\'/mach_kernel\' already exists'
          }
        }
      }
    },
    {
      setup:    'tsfile rm /',
      check: {
        input:  'tsfile rm /',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: '/',
            arg: ' /',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsfile rm /zxcv',
      check: {
        input:  'tsfile rm /zxcv',
        // hints:                 ' -> /etc/',
        markup: 'VVVVVVVVVVIIIII',
        cursor: 15,
        current: 'p1',
        status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile rm /mach_kernel',
      check: {
        input:  'tsfile rm /mach_kernel',
        hints:                        '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVV',
        cursor: 22,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};

exports.testFirefoxBasic = function(options) {
  return helpers.audit(options, [
    {
      // These tests are just like the ones above tailored for running in
      // Firefox
      skipRemainingIf: true,
      // skipRemainingIf: !options.isFirefox,
      skipIf: true,
      setup:    'tsfile open /',
      check: {
        input:  'tsfile open /',
        hints:               '',
        markup: 'VVVVVVVVVVVVI',
        cursor: 13,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' is not a file',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' is not a file'
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile open /zxcv',
      check: {
        input:  'tsfile open /zxcv',
        // hints:                   ' -> /etc/',
        markup: 'VVVVVVVVVVVVIIIII',
        cursor: 17,
        current: 'p1',
        status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile open /mach_kernel',
      check: {
        input:  'tsfile open /mach_kernel',
        hints:                          '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 24,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile open' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
           }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile saveas /',
      check: {
        input:  'tsfile saveas /',
        hints:                 '',
        markup: 'VVVVVVVVVVVVVVI',
        cursor: 15,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' already exists',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' already exists'
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile saveas /zxcv',
      check: {
        input:  'tsfile saveas /zxcv',
        // hints:                     ' -> /etc/',
        markup: 'VVVVVVVVVVVVVVVVVVV',
        cursor: 19,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile saveas /mach_kernel',
      check: {
        input:  'tsfile saveas /mach_kernel',
        hints:                            '',
        markup: 'VVVVVVVVVVVVVVIIIIIIIIIIII',
        cursor: 26,
        current: 'p1',
        status: 'ERROR',
        message: '\'/mach_kernel\' already exists',
        args: {
          command: { name: 'tsfile saveas' },
          p1: {
            value: undefined,
            arg: ' /mach_kernel',
            status: 'INCOMPLETE',
            message: '\'/mach_kernel\' already exists'
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile save /',
      check: {
        input:  'tsfile save /',
        hints:               '',
        markup: 'VVVVVVVVVVVVI',
        cursor: 13,
        current: 'p1',
        status: 'ERROR',
        message: '\'/\' is not a file',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' is not a file'
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile save /zxcv',
      check: {
        input:  'tsfile save /zxcv',
        // hints:                   ' -> /etc/',
        markup: 'VVVVVVVVVVVVVVVVV',
        cursor: 17,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile save /mach_kernel',
      check: {
        input:  'tsfile save /mach_kernel',
        hints:                          '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        cursor: 24,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile save' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsfile cd /',
      check: {
        input:  'tsfile cd /',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: '/',
            arg: ' /',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      setup:    'tsfile cd /zxcv',
      check: {
        input:  'tsfile cd /zxcv',
        // hints:                 ' -> /dev/',
        // markup: 'VVVVVVVVVVIIIII',
        cursor: 15,
        current: 'p1',
        // status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            // status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: true || !local,
      setup:    'tsfile cd /etc/passwd',
      check: {
        input:  'tsfile cd /etc/passwd',
        hints:                       ' -> /etc/pam.d/',
        markup: 'VVVVVVVVVVIIIIIIIIIII',
        cursor: 21,
        current: 'p1',
        status: 'ERROR',
        message: '\'/etc/passwd\' is not a directory',
        args: {
          command: { name: 'tsfile cd' },
          p1: {
            value: undefined,
            arg: ' /etc/passwd',
            status: 'INCOMPLETE',
            message: '\'/etc/passwd\' is not a directory'
          }
        }
      }
    },
    {
      setup:    'tsfile mkdir /',
      check: {
        input:  'tsfile mkdir /',
        hints:                '',
        markup: 'VVVVVVVVVVVVVI',
        cursor: 14,
        current: 'p1',
        status: 'ERROR',
        message: ''/' already exists',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: undefined,
            arg: ' /',
            status: 'INCOMPLETE',
            message: '\'/\' already exists'
          }
        }
      }
    },
    {
      setup:    'tsfile mkdir /zxcv',
      check: {
        input:  'tsfile mkdir /zxcv',
        // hints:                    ' -> /dev/',
        markup: 'VVVVVVVVVVVVVVVVVV',
        cursor: 18,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: '/zxcv',
            arg: ' /zxcv',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile mkdir /mach_kernel',
      check: {
        input:  'tsfile mkdir /mach_kernel',
        hints:                           '',
        markup: 'VVVVVVVVVVVVVIIIIIIIIIIII',
        cursor: 25,
        current: 'p1',
        status: 'ERROR',
        message: '\'/mach_kernel\' already exists',
        args: {
          command: { name: 'tsfile mkdir' },
          p1: {
            value: undefined,
            arg: ' /mach_kernel',
            status: 'INCOMPLETE',
            message: '\'/mach_kernel\' already exists'
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile rm /',
      check: {
        input:  'tsfile rm /',
        hints:             '',
        markup: 'VVVVVVVVVVV',
        cursor: 11,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: '/',
            arg: ' /',
            status: 'VALID',
            message: ''
          }
        }
      }
    },
    {
      skipIf: true,
      setup:    'tsfile rm /zxcv',
      check: {
        input:  'tsfile rm /zxcv',
        // hints:                 ' -> /etc/',
        markup: 'VVVVVVVVVVIIIII',
        cursor: 15,
        current: 'p1',
        status: 'ERROR',
        message: '\'/zxcv\' doesn\'t exist',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: undefined,
            arg: ' /zxcv',
            status: 'INCOMPLETE',
            message: '\'/zxcv\' doesn\'t exist'
          }
        }
      }
    },
    {
      skipIf: !local,
      setup:    'tsfile rm /mach_kernel',
      check: {
        input:  'tsfile rm /mach_kernel',
        hints:                        '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVV',
        cursor: 22,
        current: 'p1',
        status: 'VALID',
        message: '',
        args: {
          command: { name: 'tsfile rm' },
          p1: {
            value: '/mach_kernel',
            arg: ' /mach_kernel',
            status: 'VALID',
            message: ''
          }
        }
      }
    }
  ]);
};
