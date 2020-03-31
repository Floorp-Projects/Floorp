/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Give a builtin constructor that we can use as the default. When we give
// it to our newly made class, we will be sure to set it up with the correct name
// and .prototype, so that everything works properly.

var DefaultDerivedClassConstructor =
    class extends null { constructor(...args) { super(...allowContentIter(args)); } };

var DefaultBaseClassConstructor =
    class { constructor() { } };
