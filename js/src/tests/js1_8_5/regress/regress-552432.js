/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

(function (y) {
    arguments.y = 2;
    var x = Object.create(arguments);
    x[0] = 3;
    assertEq(x[0], 3);
    assertEq(x.y, 2);
    assertEq(y, 1);
})(1);

reportCompare(0, 0, 'ok');
