// Copyright 2013 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/**
 * @description Tests that constructing a Collator doesn't create or modify
 *     unwanted properties on the RegExp constructor.
 * @author Norbert Lindenberg
 */

$INCLUDE("testIntl.js");

testForUnwantedRegExpChanges(function () {
    new Intl.Collator("de-DE-u-co-phonebk");
});
