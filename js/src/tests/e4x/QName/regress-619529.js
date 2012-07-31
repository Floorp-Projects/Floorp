// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var b = Proxy.create({ enumerateOwn: function () { @f; }});
Object.freeze(this);

try {
    @r;
    throw 1; // should not be reached
} catch (e) {
    assertEq(e instanceof ReferenceError, true);
}

reportCompare(0, 0, "ok");
