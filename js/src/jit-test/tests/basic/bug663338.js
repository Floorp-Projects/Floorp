/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */


assertEq(parseInt(1.0e-7, 10), 1);
assertEq(parseInt(-1.0e-7, 10), -1);

assertEq(parseInt(9e-8, 10), 9);
assertEq(parseInt(-9e-8, 10), -9);

assertEq(parseInt(1.5e-8, 10), 1);
assertEq(parseInt(-1.5e-8, 10), -1);

assertEq(parseInt(1.0e-6, 10), 0);

assertEq(parseInt(0, 10), 0);
assertEq(parseInt(-0, 10), 0);

assertEq(parseInt('0', 10), 0);
assertEq(parseInt('-0', 10), -0);

/* this is not very hacky, but we try to get a double value of 0, instead of int */
assertEq(parseInt(Math.asin(0), 10), 0); 
assertEq(parseInt(Math.asin(-0), 10), 0); 
