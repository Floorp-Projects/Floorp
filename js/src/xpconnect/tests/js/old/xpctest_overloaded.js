var clazz = Components.classes["@mozilla.org/js/xpc/test/Overloaded;1"];
var iface = Components.interfaces.nsIXPCTestOverloaded;

foo = clazz.createInstance(iface);

try {
    print("foo.Foo1(1)...  ");  foo.Foo1(1)
    print("foo.Foo2(1,2)...");  foo.Foo2(1,2)
    print("foo.Foo(3)...   ");  foo.Foo(3)
    print("foo.Foo(3,4)... ");  foo.Foo(3,4)
    print("foo.Foo()...    ");  foo.Foo();
} catch(e) {
    print("caught exception: "+e);
}
