class Base {
    constructor(b) {
        this.b = b;
    }
}
class Derived extends Base {
    constructor(a, b) {
        super(b);
        this.a = a;
    }
}

function testSimple() {
    var boundCtor = Derived.bind(null, 1);
    for (var i = 0; i < 100; i++) {
        var o = new boundCtor(2);
        assertEq(o.a, 1);
        assertEq(o.b, 2);

    }
}
testSimple();

function testMegamorphic() {
    var ctors = [
        function(a, b) { this.a = a; this.b = b; this.c = 1; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 2; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 3; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 4; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 5; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 6; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 7; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 8; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 9; }.bind(null, 1),
        function(a, b) { this.a = a; this.b = b; this.c = 10; }.bind(null, 1),
        Derived.bind(null, 1),
        Derived.bind(null, 1),
    ];
    for (var i = 0; i < 100; i++) {
        var ctor = ctors[i % ctors.length];
        var o = new ctor(2);
        assertEq(o.a, 1);
        assertEq(o.b, 2);
    }
}
testMegamorphic();
