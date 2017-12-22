// |reftest| skip-if(!xulRuntime.shell)
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
check(new Proxy({}, {}));

// A failing getter.
check({get x() { throw new Error("fail"); }});

// Mismatched scopes.
for (let [write_scope, read_scope] of [['SameProcessSameThread', 'SameProcessDifferentThread'],
                                       ['SameProcessSameThread', 'DifferentProcess'],
                                       ['SameProcessDifferentThread', 'DifferentProcess']])
{
  var ab = new ArrayBuffer(12);
  var buffer = serialize(ab, [ab], { scope: write_scope });
  var caught = false;
  try {
    deserialize(buffer, { scope: read_scope });
  } catch (exc) {
    caught = true;
  }
  assertEq(caught, true, `${write_scope} clone buffer should not be deserializable as ${read_scope}`);
}

reportCompare(0, 0, "ok");
