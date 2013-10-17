/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var std_iterator = (function() {
    try {
        for (var _ of new Proxy({}, { get: function(_, name) { throw name; } }))
            break;
    } catch (name) {
        return name;
    }
    throw 'wat';
})();

function assertFalse(a) { assertEq(a, false) }
function assertTrue(a) { assertEq(a, true) }
function assertNotEq(found, not_expected) { assertFalse(found === expected) }
function assertIteratorResult(result, value, done) {
    assertDeepEq(result.value, value);
    assertEq(result.done, done);
}
