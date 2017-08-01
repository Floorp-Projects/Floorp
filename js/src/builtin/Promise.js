/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6, 25.4.5.1.
function Promise_catch(onRejected) {
    // Steps 1-2.
    return callContentFunction(this.then, this, undefined, onRejected);
}
