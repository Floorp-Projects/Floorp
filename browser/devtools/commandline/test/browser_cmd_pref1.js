/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the pref commands work

var prefBranch = Cc["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService).getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);

const TEST_URI = "data:text/html;charset=utf-8,gcli-pref1";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);
  yield helpers.openToolbar(options);

  let tiltEnabledOrig = prefBranch.getBoolPref("devtools.tilt.enabled");
  info("originally: devtools.tilt.enabled = " + tiltEnabledOrig);

  yield helpers.audit(options, [
    {
      setup: 'pref',
      check: {
        input:  'pref',
        hints:      ' reset',
        markup: 'IIII',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref s',
      check: {
        input:  'pref s',
        hints:        'et',
        markup: 'IIIIVI',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref sh',
      check: {
        input:  'pref sh',
        hints:         'ow',
        markup: 'IIIIVII',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref show ',
      check: {
        input:  'pref show ',
        markup: 'VVVVVVVVVV',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref show usetexttospeech',
      check: {
        input:  'pref show usetexttospeech',
        hints:                           ' -> accessibility.usetexttospeech',
        markup: 'VVVVVVVVVVIIIIIIIIIIIIIII',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref show devtools.til',
      check: {
        input:  'pref show devtools.til',
        hints:                        't.enabled',
        markup: 'VVVVVVVVVVIIIIIIIIIIII',
        status: 'ERROR',
        tooltipState: 'true:importantFieldFlag',
        args: {
          setting: { value: undefined, status: 'INCOMPLETE' },
        }
      },
    },
    {
      setup: 'pref reset devtools.tilt.enabled',
      check: {
        input:  'pref reset devtools.tilt.enabled',
        hints:                                  '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID'
      },
    },
    {
      setup: 'pref show devtools.tilt.enabled 4',
      check: {
        input:  'pref show devtools.tilt.enabled 4',
        hints:                                   '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref set devtools.tilt.enabled 4',
      check: {
        input:  'pref set devtools.tilt.enabled 4',
        hints:                                  '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVE',
        status: 'ERROR',
        args: {
          setting: { arg: ' devtools.tilt.enabled' },
          value: { status: 'ERROR', message: 'Can\'t use \'4\'.' },
        }
      },
    },
    {
      setup: 'pref set devtools.editor.tabsize 4',
      check: {
        input:  'pref set devtools.editor.tabsize 4',
        hints:                                    '',
        markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
        status: 'VALID',
        args: {
          setting: { arg: ' devtools.editor.tabsize' },
          value: { value: 4 },
        }
      },
    },
    {
      setup: 'pref list',
      check: {
        input:  'pref list',
        hints:           ' -> pref set',
        markup: 'IIIIVIIII',
        status: 'ERROR'
      },
    },
    {
      setup: 'pref show devtools.tilt.enabled',
      check: {
        args: {
          setting: {
            value: options.requisition.system.settings.get("devtools.tilt.enabled")
          }
        },
      },
      exec: {
        output: "devtools.tilt.enabled: " + tiltEnabledOrig,
      },
      post: function() {
        prefBranch.setBoolPref("devtools.tilt.enabled", tiltEnabledOrig);
      }
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
