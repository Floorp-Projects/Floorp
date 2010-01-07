/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributor: Jeff Walden
 */

var x = undefined;
var passed = false;
switch (x)
{
  case undefined:
    passed = true;
  default:
    break;
}
assertEq(passed, true);

reportCompare(true, true);
