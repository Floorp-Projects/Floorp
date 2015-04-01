/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Library file for tests to load.

function Range(start, stop) {
    this.i = start;
    this.stop = stop;
}
Range.prototype = {
    __iterator__: function() { return this; },
    next: function() {
        if (this.i >= this.stop)
            throw StopIteration;
        return this.i++;
    }
};

function range(start, stop) {
    return new Range(start, stop);
}
