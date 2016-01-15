/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Testing that the gcli 'inspect' command works as it should.

const TEST_URI = URL_ROOT + "doc_inspector_gcli-inspect-command.html";

add_task(function* () {
  return helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: "inspect",
        check: {
          input:  'inspect',
          hints:         ' <selector>',
          markup: 'VVVVVVV',
          status: 'ERROR',
          args: {
            selector: {
              message: 'Value required for \'selector\'.'
            },
          }
        },
      },
      {
        setup: "inspect h1",
        check: {
          input:  'inspect h1',
          hints:            '',
          markup: 'VVVVVVVVII',
          status: 'ERROR',
          args: {
            selector: { message: 'No matches' },
          }
        },
      },
      {
        setup: "inspect span",
        check: {
          input:  'inspect span',
          hints:              '',
          markup: 'VVVVVVVVEEEE',
          status: 'ERROR',
          args: {
            selector: { message: 'Too many matches (2)' },
          }
        },
      },
      {
        setup: "inspect div",
        check: {
          input:  'inspect div',
          hints:             '',
          markup: 'VVVVVVVVVVV',
          status: 'VALID',
          args: {
            selector: { message: '' },
          }
        },
      },
      {
        setup: "inspect .someclas",
        check: {
          input:  'inspect .someclas',
          hints:                   '',
          markup: 'VVVVVVVVIIIIIIIII',
          status: 'ERROR',
          args: {
            selector: { message: 'No matches' },
          }
        },
      },
      {
        setup: "inspect .someclass",
        check: {
          input:  'inspect .someclass',
          hints:                    '',
          markup: 'VVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            selector: { message: '' },
          }
        },
      },
      {
        setup: "inspect #someid",
        check: {
          input:  'inspect #someid',
          hints:                 '',
          markup: 'VVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            selector: { message: '' },
          }
        },
      },
      {
        setup: "inspect button[disabled]",
        check: {
          input:  'inspect button[disabled]',
          hints:                          '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            selector: { message: '' },
          }
        },
      },
      {
        setup: "inspect p>strong",
        check: {
          input:  'inspect p>strong',
          hints:                  '',
          markup: 'VVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            selector: { message: '' },
          }
        },
      },
      {
        setup: "inspect :root",
        check: {
          input:  'inspect :root',
          hints:               '',
          markup: 'VVVVVVVVVVVVV',
          status: 'VALID'
        },
      },
    ]); // helpers.audit
  }); // helpers.addTabWithToolbar
}); // add_task
