/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The list of checker classes that are compatible with clang-tidy and are considered
// to be in alpha stage development.

// CHECK(AlphaChecker, "alpha-checker")
CHECK(NonStdMoveChecker, "non-std-move")
CHECK(TempRefPtrChecker, "performance-temp-refptr")
