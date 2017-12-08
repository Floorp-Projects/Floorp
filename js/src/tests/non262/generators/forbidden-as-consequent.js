/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

assertThrowsInstanceOf(() => eval("if (1) function* foo() {}"),
                       SyntaxError);
assertThrowsInstanceOf(() => eval("'use strict'; if (1) function* foo() {}"),
                       SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
