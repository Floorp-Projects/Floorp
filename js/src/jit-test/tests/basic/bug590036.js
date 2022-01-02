/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

load(libdir + 'eqArrayHelper.js');

assertEqArray(eval('[]'), []);
assertEqArray(eval('[,]'), [,]);
assertEqArray(eval('[,,]'), [,,]);
assertEqArray(eval('[1, 1, ]'), [1,1, ]);
assertEqArray(eval('[1, 1, true]'), [1, 1, true]);
assertEqArray(eval('[1, false, true]'), [1, false, true]);
