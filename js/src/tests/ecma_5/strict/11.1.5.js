/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/* Simple identifier labels. */
assertEq(testLenientAndStrict('({x:1, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({x:1, y:1})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

assertEq(testLenientAndStrict('({x:1, y:1, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Property names can be written as strings, too. */
assertEq(testLenientAndStrict('({x:1,   "x":1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({"x":1, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({"x":1, "x":1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Numeric property names. */
assertEq(testLenientAndStrict('({1.5:1, 1.5:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({1.5:1, 15e-1:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({6.02214179e23:1, 6.02214179e23:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({6.02214179e23:1, 3.1415926535:1})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

assertEq(testLenientAndStrict('({ 1: 1, "1": 2 })',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({ "1": 1, 1: 2 })',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({ 2.5: 1, "2.5": 2 })',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({ "2.5": 1, 2.5: 2 })',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/* Many properties, to exercise JSAtomList's hash-table variant. */
assertEq(testLenientAndStrict('({a:1, b:1, c:1, d:1, e:1, f:1, g:1, h:1, i:1, j:1, k:1, l:1, m:1, n:1, o:1, p:1, q:1, r:1, s:1, t:1, u:1, v:1, w:1, x:1, y:1, z:1})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

assertEq(testLenientAndStrict('({a:1, b:1, c:1, d:1, e:1, f:1, g:1, h:1, i:1, j:1, k:1, l:1, m:1, n:1, o:1, p:1, q:1, r:1, s:1, t:1, u:1, v:1, w:1, x:1, y:1, a:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

/*
 * Getters, setters, and value properties should conflict only when
 * appropriate.
 */
assertEq(testLenientAndStrict('({get x() {}, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({x:1, get x() {}})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({set x() {}, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({x:1, set x() {}})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({get x() {}, set x() {}})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

assertEq(testLenientAndStrict('({set x() {}, get x() {}})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

assertEq(testLenientAndStrict('({get x() {}, set x() {}, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({set x() {}, get x() {}, x:1})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({get x() {}, get x() {}})',
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict('({get x() {}, set x() {}, y:1})',
                              parsesSuccessfully,
                              parsesSuccessfully),
         true);

reportCompare(true, true);
