// |reftest| pref(javascript.options.xml.content,true) skip-if(!xulRuntime.shell)
// -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function check(v) {
    try {
        serialize(v);
    } catch (exc) {
        return;
    }
    throw new Error("serializing " + uneval(v) + " should have failed with an exception");
}

// Unsupported object types.
check(new Error("oops"));
check(this);
check(Math);
check(function () {});
check(Proxy.create({enumerate: function () { return []; }}));
check(<element/>);
check(new Namespace("x"));
check(new QName("x", "y"));

// A failing getter.
check({get x() { throw new Error("fail"); }});

reportCompare(0, 0, "ok");
