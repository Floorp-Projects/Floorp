// |jit-test| --fast-warmup

function Mixin(Target) {
    var c = class extends Target {};
    Target.prototype.x = 1; // Add shadowing property to disable teleporting.
    return c;
}
function MixinFoo(Target) {
    var c = class extends Target {
        get foo() { return 2; }
        set foo(value) {}
    };
    Target.prototype.x = 1; // Add shadowing property to disable teleporting.
    return c;
}

class Base {}
class MyClass extends Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(Mixin(MixinFoo(Base)))))))))))) {}

function test() {
    var instance = new MyClass();
    assertEq(instance.x, 1);
    for (var i = 0; i < 500; i++) {
        assertEq(instance.foo, 2);
    }
}
test();
