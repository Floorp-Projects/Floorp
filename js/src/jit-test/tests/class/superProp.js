var g_get_this = "get_this";
var g_prop_this = "prop_this";

class Base
{
    get get_prop() { return 7; }
    get get_this() { return this; }
    prop_call() { return 11; }
    prop_this() { return this.x; }
}
Base.prototype.prop_proto = 5;
Base.prototype.x = (-1);
Base.prototype[0] = 100;
Base.prototype[1] = 101;
Base.prototype[2] = 102;

class Derived extends Base
{
    get get_prop() { throw "Bad"; }
    get get_this() { throw "Bad"; }
    prop_call() { throw "Bad"; }
    prop_this() { throw "Bad"; }

    do_test_getprop()
    {
        this.x = 13;

        assertEq(super.prop_proto, 5);

        assertEq(super.get_prop, 7);
        assertEq(super.get_this, this);

        assertEq(super.prop_call(), 11);
        assertEq(super.prop_this(), 13);
    }

    do_test_getelem()
    {
        this.x = 13;

        assertEq(super[g_get_this], this);

        assertEq(super[g_prop_this](), 13);
        assertEq(super[0], 100);
        assertEq(super[1], 101);
        assertEq(super[2], 102);
    }
}
Derived.prototype.prop_proto = (-1);
Derived.prototype.x = (-2);
Derived.prototype[0] = (-3);
Derived.prototype[1] = (-4);
Derived.prototype[2] = (-5);

for (var i = 0; i < 20; ++i) {
    let t = new Derived();

    for (var j = 0; j < 20; ++j) {
        t.do_test_getprop();
        t.do_test_getelem();
    }
}
