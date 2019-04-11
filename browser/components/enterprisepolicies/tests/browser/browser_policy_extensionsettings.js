/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_extensionsettings() {
  await setupPolicyEngineWithJson({
    "policies": {
      "ExtensionSettings": {
        "extension1@mozilla.com": {
          "blocked_install_message": "Extension1 error message.",
        },
        "*": {
          "blocked_install_message": "Generic error message.",
        },
      },
    },
  });

  let extensionSettings =  Services.policies.getExtensionSettings("extension1@mozilla.com");
  is(extensionSettings.blocked_install_message, "Extension1 error message.", "Should have extension specific message.");
  extensionSettings =  Services.policies.getExtensionSettings("extension2@mozilla.com");
  is(extensionSettings.blocked_install_message, "Generic error message.", "Should have generic message.");
});
