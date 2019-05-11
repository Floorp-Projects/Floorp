/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function setup() {
  await setupPolicyEngineWithJson({
                                    "policies": {
                                      "3rdparty": {
                                        "Extensions": {
                                          "3rdparty-policy@mozilla.com": {
                                            "string": "value",
                                          },
                                        },
                                      },
                                    },
                                  });

  let extensionPolicy = Services.policies.getExtensionPolicy("3rdparty-policy@mozilla.com");
  deepEqual(extensionPolicy, {"string": "value"});
});
