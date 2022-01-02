/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 650621;
var summary = 'String object length test';

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

assertEq(raisesException(InternalError)('for (args = "" ;;) args+=new String(args)+1'), true);

reportCompare(true, true);

