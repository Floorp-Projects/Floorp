/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Ensure function length is set correctly when a destructuring rest parameter
// is present.

assertEq(function(...[]) {}.length, 0);
assertEq(function(...[a]) {}.length, 0);
assertEq(function(...[a = 0]) {}.length, 0);
assertEq(function(...{}) {}.length, 0);
assertEq(function(...{p: a}) {}.length, 0);
assertEq(function(...{p: a = 0}) {}.length, 0);
assertEq(function(...{a = 0}) {}.length, 0);

assertEq(function(x, ...[]) {}.length, 1);
assertEq(function(x, ...[a]) {}.length, 1);
assertEq(function(x, ...[a = 0]) {}.length, 1);
assertEq(function(x, ...{}) {}.length, 1);
assertEq(function(x, ...{p: a}) {}.length, 1);
assertEq(function(x, ...{p: a = 0}) {}.length, 1);
assertEq(function(x, ...{a = 0}) {}.length, 1);

assertEq(function(x, y, ...[]) {}.length, 2);
assertEq(function(x, y, ...[a]) {}.length, 2);
assertEq(function(x, y, ...[a = 0]) {}.length, 2);
assertEq(function(x, y, ...{}) {}.length, 2);
assertEq(function(x, y, ...{p: a}) {}.length, 2);
assertEq(function(x, y, ...{p: a = 0}) {}.length, 2);
assertEq(function(x, y, ...{a = 0}) {}.length, 2);

assertEq(function(x, y = 0, ...[]) {}.length, 1);
assertEq(function(x, y = 0, ...[a]) {}.length, 1);
assertEq(function(x, y = 0, ...[a = 0]) {}.length, 1);
assertEq(function(x, y = 0, ...{}) {}.length, 1);
assertEq(function(x, y = 0, ...{p: a}) {}.length, 1);
assertEq(function(x, y = 0, ...{p: a = 0}) {}.length, 1);
assertEq(function(x, y = 0, ...{a = 0}) {}.length, 1);

if (typeof reportCompare === "function")
    reportCompare(0, 0);
