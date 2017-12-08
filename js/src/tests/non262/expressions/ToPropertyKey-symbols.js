var symbols = [
    Symbol(), Symbol("iterator"), Symbol.for("iterator"), Symbol.iterator
];

for (var sym of symbols) {
    var key = {
        toString() { return sym; }
    };

    // Test that ToPropertyKey can return a symbol in each of the following
    // contexts.

    // Computed property names.
    var obj = {[key]: 13};
    var found = Reflect.ownKeys(obj);
    assertEq(found.length, 1);
    assertEq(found[0], sym);

    // Computed accessor property names.
    var obj2 = {
        get [key]() { return "got"; },
        set [key](v) { this.v = v; }
    };
    assertEq(obj2[sym], "got");
    obj2[sym] = 33;
    assertEq(obj2.v, 33);

    // Getting and setting properties.
    assertEq(obj[key], 13);
    obj[key] = 19;
    assertEq(obj[sym], 19);
    (function () { "use strict"; obj[key] = 20; })();
    assertEq(obj[sym], 20);
    obj[key]++;
    assertEq(obj[sym], 21);

    // Getting properties of primitive values.
    Number.prototype[sym] = "success";
    assertEq(Math.PI[key], "success");
    delete Number.prototype[sym];

    // Getting a super property.
    class X {
        [sym]() { return "X"; }
    }
    class Y extends X {
        [sym]() { return super[key]() + "Y"; }
    }
    var y = new Y();
    assertEq(y[sym](), "XY");

    // Setting a super property.
    class Z {
        set [sym](v) {
            this.self = this;
            this.value = v;
        }
    }
    class W extends Z {
        set [sym](v) {
            this.isW = true;
            super[key] = v;
        }
    }
    var w = new W();
    w[key] = "ok";
    assertEq(w.self, w);
    assertEq(w.value, "ok");
    assertEq(w.isW, true);

    // Deleting properties.
    obj = {[sym]: 1};
    assertEq(delete obj[key], true);
    assertEq(sym in obj, false);

    // LHS of `in` expressions.
    assertEq(key in {iterator: 0}, false);
    assertEq(key in {[sym]: 0}, true);

    // Methods of Object and Object.prototype
    obj = {};
    Object.defineProperty(obj, key, {value: "ok", enumerable: true});
    assertEq(obj[sym], "ok");
    assertEq(obj.hasOwnProperty(key), true);
    assertEq(obj.propertyIsEnumerable(key), true);
    var desc = Object.getOwnPropertyDescriptor(obj, key);
    assertEq(desc.value, "ok");
}

reportCompare(0, 0);
