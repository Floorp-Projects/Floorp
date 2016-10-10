/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

assertEq(function([a = 0]){}.length, 1);
assertEq(function({p: a = 0}){}.length, 1);
assertEq(function({a = 0}){}.length, 1);
assertEq(function({[0]: a}){}.length, 1);

assertEq(function([a = 0], [b = 0]){}.length, 2);
assertEq(function({p: a = 0}, [b = 0]){}.length, 2);
assertEq(function({a = 0}, [b = 0]){}.length, 2);
assertEq(function({[0]: a}, [b = 0]){}.length, 2);

assertEq(function(x, [a = 0]){}.length, 2);
assertEq(function(x, {p: a = 0}){}.length, 2);
assertEq(function(x, {a = 0}){}.length, 2);
assertEq(function(x, {[0]: a}){}.length, 2);

assertEq(function(x = 0, [a = 0]){}.length, 0);
assertEq(function(x = 0, {p: a = 0}){}.length, 0);
assertEq(function(x = 0, {a = 0}){}.length, 0);
assertEq(function(x = 0, {[0]: a}){}.length, 0);

assertEq(function([a = 0], ...r){}.length, 1);
assertEq(function({p: a = 0}, ...r){}.length, 1);
assertEq(function({a = 0}, ...r){}.length, 1);
assertEq(function({[0]: a}, ...r){}.length, 1);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
