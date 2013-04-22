// Copyright 2013 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/**
 * @description Tests that Date.prototype.toLocaleString & Co. use the standard
 *     built-in Intl.DateTimeFormat constructor.
 * @author Norbert Lindenberg
 */

$INCLUDE("testIntl.js");

taintDataProperty(Intl, "DateTimeFormat");
new Date().toLocaleString();
new Date().toLocaleDateString();
new Date().toLocaleTimeString();
