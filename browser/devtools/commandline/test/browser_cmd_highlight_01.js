/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests the various highlight command parameters and options

// Creating a test page with many elements to test the --showall option
let TEST_PAGE = "data:text/html;charset=utf-8,<body><ul>";
for (let i = 0; i < 200; i ++) {
  TEST_PAGE += "<li class='item'>" + i + "</li>";
}
TEST_PAGE += "</ul></body>";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_PAGE);
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: 'highlight',
      check: {
        input:  'highlight',
        hints:           ' [selector] [options]',
        markup: 'VVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '0 nodes highlighted'
      }
    },
    {
      setup: 'highlight bo',
      check: {
        input:  'highlight bo',
        hints:              ' [options]',
        markup: 'VVVVVVVVVVII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: No matches'
      }
    },
    {
      setup: 'highlight body',
      check: {
        input:  'highlight body',
        hints:                ' [options]',
        markup: 'VVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight body --hide',
      check: {
        input:  'highlight body --hide',
        hints:                       'guides [options]',
        markup: 'VVVVVVVVVVVVVVVIIIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --hideguides',
      check: {
        input:  'highlight body --hideguides',
        hints:                             ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight body --show',
      check: {
        input:  'highlight body --show',
        hints:                       'infobar [options]',
        markup: 'VVVVVVVVVVVVVVVIIIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --showinfobar',
      check: {
        input:  'highlight body --showinfobar',
        hints:                              ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight body --showa',
      check: {
        input:  'highlight body --showa',
        hints:                        'll [options]',
        markup: 'VVVVVVVVVVVVVVVIIIIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --showall',
      check: {
        input:  'highlight body --showall',
        hints:                          ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight body --r',
      check: {
        input:  'highlight body --r',
        hints:                    'egion [options]',
        markup: 'VVVVVVVVVVVVVVVIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --region',
      check: {
        input:  'highlight body --region',
        hints:                         ' <selection> [options]',
        markup: 'VVVVVVVVVVVVVVVIIIIIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Value required for \'region\'.'
      }
    },
    {
      setup: 'highlight body --fi',
      check: {
        input:  'highlight body --fi',
        hints:                     'll [options]',
        markup: 'VVVVVVVVVVVVVVVIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --fill',
      check: {
        input:  'highlight body --fill',
        hints:                       ' <string> [options]',
        markup: 'VVVVVVVVVVVVVVVIIIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Value required for \'fill\'.'
      }
    },
    {
      setup: 'highlight body --ke',
      check: {
        input:  'highlight body --ke',
        hints:                     'ep [options]',
        markup: 'VVVVVVVVVVVVVVVIIII',
        status: 'ERROR'
      },
      exec: {
        output: 'Error: Too many arguments'
      }
    },
    {
      setup: 'highlight body --keep',
      check: {
        input:  'highlight body --keep',
        hints:                       ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight body --hideguides --showinfobar --showall --region ' +
        'content --fill red --keep',
      check: {
        input:  'highlight body --hideguides --showinfobar --showall --region ' +
          'content --fill red --keep',
        hints: '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV' +
          'VVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '1 node highlighted'
      }
    },
    {
      setup: 'highlight .item',
      check: {
        input:  'highlight .item',
        hints:                 ' [options]',
        markup: 'VVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '200 nodes matched, but only 100 nodes highlighted. Use ' +
          '\'--showall\' to show all'
      }
    },
    {
      setup: 'highlight .item --showall',
      check: {
        input:  'highlight .item --showall',
        hints:                           ' [options]',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
      exec: {
        output: '200 nodes highlighted'
      }
    },
    {
      setup: 'unhighlight',
      check: {
        input:  'unhighlight',
        hints:  '',
        markup: 'VVVVVVVVVVV',
        status: 'VALID'
      }
    }
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
