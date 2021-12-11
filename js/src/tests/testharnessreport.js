/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_completion_callback(function(tests, harness_status) {
    // This output is parsed in TestResult.from_wpt_output.
    print("WPT OUTPUT: " + JSON.stringify([
        harness_status.status,
        harness_status.message,
        harness_status.stack,
        tests.map(function(t) {
            return [t.name, t.status, t.message, t.stack];
        }),
    ]));
});
