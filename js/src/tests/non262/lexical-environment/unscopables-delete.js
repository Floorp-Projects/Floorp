// If obj[@@unscopables][id], then `delete id` works across `with (obj)` scope.

this.niche = 7;
let obj = { niche: 8, [Symbol.unscopables]: { niche: true } };
with (obj) {
    delete niche;
}

assertEq(obj.niche, 8);
assertEq("niche" in this, false);

// Same thing, but delete a variable introduced by sloppy direct eval.
this.niche = 9;
function f() {
    eval("var niche = 10;");
    with (obj) {
        assertEq(niche, 10);
        delete niche;
    }
    assertEq(niche, 9);
}

// Of course none of this affects a qualified delete.
assertEq(delete this.niche, true);
assertEq("niche" in this, false);

reportCompare(0, 0);
