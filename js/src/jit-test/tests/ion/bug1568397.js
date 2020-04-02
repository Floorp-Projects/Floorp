// |jit-test| error:TypeError: can't access property
let obj = {x: 1};
obj.x = 1.1;

function Foo(val, phase){
    if (phase == 3) {
        // Phase 3: Modify the prototype of this constructor.
        Foo.prototype.__proto__ = proto;
    }

    // Phase 4: Trigger the getter on the new proto.
    this.d;

    this.c = val;

    if (phase == 2) {
        // Phase 2: Stash |this| in a global variable.
        g_partial = this;

        // Trigger Phase 3.
        new Foo(1.1, 3);
    }
    this.b = 2.2;
}

let proto = {get d() {
    function accessC(arg){
        var tmp = arg.c;
        return tmp.x;
    }

    // Phase 5: Ion-compile |accessC|, using the stashed |this| from phase 2.
    // This is a partially initialized object with a C property but not a B
    // property.
    for (var i = 0; i < 100000; i++) {
        accessC(g_partial);
    }

    // Phase 6: call |accessC| with |this|, which is a partially initialized
    // object *without* a C (and B) property.
    x = accessC(this);
}};

// Phase 1: Warm up the |Foo| constructor with normal data.
for(let i = 0;i < 100;i++){
    new Foo(obj, 1);
}

// Trigger Phase 2.
new Foo(obj, 2);
