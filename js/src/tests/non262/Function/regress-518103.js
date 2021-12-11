/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 518103;
var summary = 'lambda constructor "method" vs. instanceof';
var actual;
var expect;

printBugNumber(BUGNUMBER);
printStatus(summary);

var Y = {widget: {}};

Y.widget.DataSource = function () {};
Y.widget.DS_JSArray = function (A) { this.data = A; };
Y.widget.DS_JSArray.prototype = new Y.widget.DataSource();

var J = new Y.widget.DS_JSArray( [ ] );

actual = J instanceof Y.widget.DataSource;
expect = true;

reportCompare(expect, actual, summary);

printStatus("All tests passed!");
