/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function AsyncIteratorIdentity() {
    return this;
}

function AsyncGeneratorNext(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "next");
}

function AsyncGeneratorThrow(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "throw");
}

function AsyncGeneratorReturn(val) {
    assert(IsAsyncGeneratorObject(this),
           "ThisArgument must be a generator object for async generators");
    return resumeGenerator(this, val, "return");
}
