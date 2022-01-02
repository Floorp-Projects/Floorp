///////////////////////////////////////////////////////////////////////////////
// FIRST TEST /////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

(function() {

function debug() {
    throw new Error('gotcha');
}

var imports = {
    numCalls:0,
    main: {
        f() {
            debug();
        }
    }
};

var instance = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (import "main" "f" (func))
    (func (export "add") (param i32) (param i32) (result i32)
     local.get 0
     local.get 1
     call 2
    )
    (func (param i32) (param i32) (result i32)
     local.get 0
     i32.const 5000
     i32.eq
     if
         call 0
     end

     local.get 0
     local.get 1
     i32.add
    )
)`)), imports).exports;

function loopBody(i) {
    var caught = null;
    try {
        assertEq(instance.add(i, i), 2 * i);
    } catch(e) {
        // TODO check stack trace
        print(e.stack);
        caught = e;
    }
    assertEq(!!caught, i === 5000);
}

function main() {
    for (var i = 0; i < 100000; i++) {
        loopBody(i);
    }
    assertEq(i, 100000);
}

main();

})();

///////////////////////////////////////////////////////////////////////////////
// SECOND TEST ////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

(function() {

function debug() {
    gc();
}

var imports = {
    numCalls:0,
    main: {
        f() {
            debug();
        }
    }
};

var instance = new WebAssembly.Instance(new WebAssembly.Module(wasmTextToBinary(`(module
    (import "main" "f" (func))
    (func (export "add") (param i32) (param i32) (result i32)
     local.get 0
     local.get 1
     call 2
    )
    (func (param i32) (param i32) (result i32)
     local.get 0
     i32.const 5000
     i32.eq
     if
         call 0
         unreachable
     end

     local.get 0
     local.get 1
     i32.add
    )
)`)), imports).exports;

function loopBody(i) {
    var caught = null;
    try {
        assertEq(instance.add(i, i), 2 * i);
    } catch(e) {
        // TODO check stack trace
        print(e.stack);
        caught = e;
    }
    assertEq(!!caught, i === 5000);
}

function main() {
    for (var i = 0; i < 100000; i++) {
        loopBody(i);
    }
    assertEq(i, 100000);
}

main();

})();
