// Copyright 2013 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/**
 * @description Tests that Intl.NumberFormat.supportedLocalesOf
 *     doesn't access arguments that it's not given.
 * @author Norbert Lindenberg
 */

$INCLUDE("testIntl.js");

taintDataProperty(Object.prototype, "1");
new Intl.NumberFormat("und");
