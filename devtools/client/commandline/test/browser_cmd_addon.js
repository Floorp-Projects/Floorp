/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab("about:blank");
  yield helpers.openToolbar(options);

  yield helpers.audit(options, [
    {
      setup: "addon list dictionary",
      check: {
        input:  "addon list dictionary",
        hints:                       "",
        markup: "VVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "There are no add-ons of that type installed."
      }
    },
    {
      setup: "addon list extension",
      check: {
        input:  "addon list extension",
        hints:                      "",
        markup: "VVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: [/The following/, /Mochitest/, /Special Powers/]
      }
    },
    {
      setup: "addon list locale",
      check: {
        input:  "addon list locale",
        hints:                   "",
        markup: "VVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "There are no add-ons of that type installed."
      }
    },
    {
      setup: "addon list plugin",
      check: {
        input:  "addon list plugin",
        hints:                   "",
        markup: "VVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: [/Test Plug-in/, /Second Test Plug-in/]
      }
    },
    {
      setup: "addon list theme",
      check: {
        input:  "addon list theme",
        hints:                  "",
        markup: "VVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: [/following themes/, /Default/]
      }
    },
    {
      setup: "addon list all",
      check: {
        input:  "addon list all",
        hints:                "",
        markup: "VVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: [/The following/, /Default/, /Mochitest/, /Test Plug-in/,
                 /Second Test Plug-in/, /Special Powers/]
      }
    },
    {
      setup: "addon disable Test_Plug-in_1.0.0.0",
      check: {
        input:  "addon disable Test_Plug-in_1.0.0.0",
        hints:                                    "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID"
      },
      exec: {
        output: "Test Plug-in 1.0.0.0 disabled."
      }
    },
    {
      setup: "addon disable WRONG",
      check: {
        input:  "addon disable WRONG",
        hints:                     "",
        markup: "VVVVVVVVVVVVVVEEEEE",
        status: "ERROR"
      }
    },
    {
      setup: "addon enable Test_Plug-in_1.0.0.0",
      check: {
        input:  "addon enable Test_Plug-in_1.0.0.0",
        hints:                                   "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          command: { name: "addon enable" },
          addon: {
            value: function (addon) {
              is(addon.name, "Test Plug-in", "test plugin name");
            },
            status: "VALID"
          }
        }
      },
      exec: {
        output: "Test Plug-in 1.0.0.0 enabled."
      }
    },
    {
      setup: "addon ctp Test_Plug-in_1.0.0.0",
      check: {
        input:  "addon ctp Test_Plug-in_1.0.0.0",
        hints:                                "",
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          command: { name: "addon ctp" },
          addon: {
            value: function (addon) {
              is(addon.name, "Test Plug-in", "test plugin name");
            },
            status: "VALID"
          }
        }
      },
      exec: {
        output: "Test Plug-in 1.0.0.0 set to click-to-play."
      }
    },
    {
      setup:  "addon ctp OpenH264_Video_Codec_provided_by_Cisco_Systems,_Inc._null",
      check: {
        input: "addon ctp OpenH264_Video_Codec_provided_by_Cisco_Systems,_Inc._null",
        hints:                                                                    "",
        status: "VALID",
        args: {
          command: { name: "addon ctp" },
          addon: {
            value: function (addon) {
              is(addon.name, "OpenH264 Video Codec provided by Cisco Systems, Inc.", "openh264");
            },
            status: "VALID"
          }
	                                                                                }
      },
      exec: {
        output: "OpenH264 Video Codec provided by Cisco Systems, Inc. null cannot be set to click-to-play."
      }
    },
    {
      setup:  "addon ctp Mochitest_1.0",
      check: {
        input: "addon ctp Mochitest_1.0",
        hints:                        "",
        status: "VALID",
        args: {
          command: { name: "addon ctp" },
          addon: {
            value: function (addon) {
              is(addon.name, "Mochitest", "mochitest");
            },
            status: "VALID"
          }
	                                                                                }
      },
      exec: {
        output: "Mochitest 1.0 cannot be set to click-to-play because it is not a plugin."
      }
    }
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
