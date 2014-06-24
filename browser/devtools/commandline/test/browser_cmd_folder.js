/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the folder commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,cmd-folder";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: 'folder',
        check: {
          input:  'folder',
          hints:  ' open',
          markup: 'IIIIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'folder open',
        check: {
          input:  'folder open',
          hints:  ' [path]',
          markup: 'VVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'folder open ~',
        check: {
          input:  'folder open ~',
          hints:  '',
          markup: 'VVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'folder openprofile',
        check: {
          input:  'folder openprofile',
          hints:  '',
          markup: 'VVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'folder openprofile WRONG',
        check: {
          input:  'folder openprofile WRONG',
          hints:  '',
          markup: 'VVVVVVVVVVVVVVVVVVVEEEEE',
          status: 'ERROR'
        }
      }
    ]);
  }).then(finish, helpers.handleError);
}
