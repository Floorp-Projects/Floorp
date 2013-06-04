// Copyright 2013 Mozilla Corporation. All rights reserved.
// This code is governed by the license found in the LICENSE file.

/**
 * @description Tests that String.prototype.localeCompare uses the standard
 *     built-in Intl.Collator constructor.
 * @author Norbert Lindenberg
 */

$INCLUDE("testIntl.js");

taintDataProperty(Intl, "Collator");
"a".localeCompare("b");
