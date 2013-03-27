// 'arguments' works in nested genexprs.

function f() {
    return ((((((arguments for (u of [0]))
                for (v of [1]))
               for (w of [2]))
              for (x of [3]))
             for (y of [4]))
            for (z of [5]));
}
var args = f("ponies").next().next().next().next().next().next();
assertEq(args[0], "ponies");
