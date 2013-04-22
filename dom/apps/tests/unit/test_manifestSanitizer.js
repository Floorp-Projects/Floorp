/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function testEntryPoint(aRoot) {
  do_check_true(aRoot.name == "hello world");
  do_check_true(aRoot.description == "A bold name");
  do_check_true(aRoot.developer.name == "Blink Inc.");

  let permissions = aRoot.permissions;
  do_check_true(permissions.contacts.description == "Required for autocompletion in the share screen");
  do_check_true(permissions.alarms.description == "Required to schedule notifications");
}

function run_test() {
  Components.utils.import("resource:///modules/AppsUtils.jsm");

  do_check_true(!!AppsUtils);

  // Test manifest, with one entry point.
  let manifest = {
    name: "hello <b>world</b>",
    description: "A bold name",
    developer: {
      name: "<blink>Blink</blink> Inc.",
      url: "http://blink.org"
    },
    permissions : {
      "contacts": {
        "description": "Required for autocompletion in the <a href='http://shareme.com'>share</a> screen",
        "access": "readcreate"
        },
      "alarms": {
        "description": "Required to schedule notifications"
      }
    },

    entry_points: {
      "subapp": {
        name: "hello <b>world</b>",
        description: "A bold name",
        developer: {
          name: "<blink>Blink</blink> Inc.",
          url: "http://blink.org"
        },
        permissions : {
          "contacts": {
            "description": "Required for autocompletion in the <a href='http://shareme.com'>share</a> screen",
            "access": "readcreate"
            },
          "alarms": {
            "description": "Required to schedule notifications"
          }
        }
      }
    }
  }

  AppsUtils.sanitizeManifest(manifest);

  // Check the main section and the subapp entry point.
  testEntryPoint(manifest);
  testEntryPoint(manifest.entry_points.subapp);
}
