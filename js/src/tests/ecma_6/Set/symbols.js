/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

if (typeof Symbol === "function") {
    var s = new Set;

    // Symbols can be stored in Sets.
    var sym = Symbol();
    s.add(sym);
    assertEq(s.has(sym), true);
    assertEq(s.has(Symbol()), false);
    assertEq([...s][0], sym);
    s.add(sym);
    assertEq(s.has(sym), true);
    assertEq(s.size, 1);
    s.delete(sym);
    assertEq(s.has(sym), false);
    assertEq(s.size, 0);

    // Symbols returned by Symbol.for() can be in Sets.
    var str = "how much wood would a woodchuck chuck if a woodchuck could chuck wood";
    var s2  = "how much wood would a woodchuck chuck if could";
    var arr = [for (word of str.split(" ")) Symbol.for(word)];
    s = new Set(arr);
    assertDeepEq([...s], [for (word of s2.split(" ")) Symbol.for(word)]);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
