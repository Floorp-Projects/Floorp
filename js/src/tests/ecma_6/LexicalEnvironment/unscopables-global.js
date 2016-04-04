// @@unscopables does not affect the global environment.

this.x = "global property x";
let y = "global lexical y";
this[Symbol.unscopables] = {x: true, y: true};
assertEq(x, "global property x");
assertEq(y, "global lexical y");
assertEq(eval("x"), "global property x");
assertEq(eval("y"), "global lexical y");

// But it does affect `with` statements targeting the global object.
{
    let x = "local x";
    with (this)
        assertEq(x, "local x");
}

reportCompare(0, 0);
