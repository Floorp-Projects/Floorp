// |jit-test| test-also-wasm-baseline; error: TestComplete
// Tests that wasm module scripts raises onEnterFrame and onLeaveFrame events.

load(libdir + "asserts.js");

if (!wasmIsSupported())
     throw "TestComplete";

function runWasmWithDebugger(wast, lib, init, done) {
    let g = newGlobal('');
    let dbg = new Debugger(g);

    g.eval(`
var wasm = wasmTextToBinary('${wast}');
var lib = ${lib || 'undefined'};
var m = new WebAssembly.Instance(new WebAssembly.Module(wasm), lib);`);

    init(dbg, g);
    let result = undefined, error = undefined;
    try {
        result = g.eval("m.exports.test()");
    } catch (ex) {
        error = ex;
    }
    done(dbg, result, error, g);
}

// Checking if onEnterFrame is fired for wasm frames and verifying the content
// of the frame and environment properties.
var onEnterFrameCalled, onLeaveFrameCalled, onExceptionUnwindCalled, testComplete;
runWasmWithDebugger(
    '(module (func (result i32) (i32.const 42)) (export "test" 0))', undefined,
    function (dbg) {
        var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];
        assertEq(!!wasmScript, true);
        onEnterFrameCalled = 0;
        onLeaveFrameCalled = 0;
        testComplete = false;
        var evalFrame;
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== 'wasmcall') {
                if (frame.type === 'eval')
                    evalFrame = frame;
                return;
            }

            onEnterFrameCalled++;

            assertEq(frame.script, wasmScript);
            assertEq(frame.older, evalFrame);
            assertEq(frame.type, 'wasmcall');

            let env = frame.environment;
            assertEq(env instanceof Object, true);
            assertEq(env.inspectable, true);
            assertEq(env.parent !== null, true);
            assertEq(env.type, 'declarative');
            assertEq(env.callee, null);
            assertEq(Array.isArray(env.names()), true);
            assertEq(env.names().length, 0);

            frame.onPop = function() {
                onLeaveFrameCalled++;
                testComplete = true;
            };
       };
    },
    function (dbg, result, error) {
        assertEq(testComplete, true);
        assertEq(onEnterFrameCalled, 1);
        assertEq(onLeaveFrameCalled, 1);
        assertEq(result, 42);
        assertEq(error, undefined);
    }
);

// Checking the dbg.getNewestFrame() and frame.older.
runWasmWithDebugger(
    '(module (import $fn1 "env" "ex") (func $fn2 (call $fn1)) (export "test" $fn2))',
    '{env: { ex: () => { }}}',
    function (dbg) {
        onEnterFrameCalled = 0;
        onLeaveFrameCalled = 0;
        testComplete = false;
        var evalFrame, wasmFrame;
        dbg.onEnterFrame = function (frame) {
            onEnterFrameCalled++;

            assertEq(dbg.getNewestFrame(), frame);

            switch (frame.type) {
              case 'eval':
                evalFrame = frame;
                break;
              case 'wasmcall':
                wasmFrame = frame;
                break;
              case 'call':
                assertEq(frame.older, wasmFrame);
                assertEq(frame.older.older, evalFrame);
                assertEq(frame.older.older.older, null);
                testComplete = true;
                break;
            }

            frame.onPop = function() {
                onLeaveFrameCalled++;
            };
        };
    },
    function (dbg, result, error) {
        assertEq(testComplete, true);
        assertEq(onEnterFrameCalled, 3);
        assertEq(onLeaveFrameCalled, 3);
        assertEq(error, undefined);
    }
);

// Checking if we can enumerate frames and find 'wasmcall' one.
runWasmWithDebugger(
    '(module (import $fn1 "env" "ex") (func $fn2 (call $fn1)) (export "test" $fn2))',
    '{env: { ex: () => { debugger; }}}',
    function (dbg) {
        testComplete = false;
        dbg.onDebuggerStatement = function (frame) {
            assertEq(frame.type, 'call');
            assertEq(frame.older.type, 'wasmcall');
            assertEq(frame.older.older.type, 'eval');
            assertEq(frame.older.older.older, null);
            testComplete = true;
        }
    },
    function (dbg, result, error) {
        assertEq(testComplete, true);
        assertEq(error, undefined);
    }
);

// Checking if onPop works without onEnterFrame handler.
runWasmWithDebugger(
    '(module (import $fn1 "env" "ex") (func $fn2 (call $fn1)) (export "test" $fn2))',
    '{env: { ex: () => { debugger; }}}',
    function (dbg) {
        onLeaveFrameCalled = 0;
        dbg.onDebuggerStatement = function (frame) {
            if (!frame.older || frame.older.type != 'wasmcall')
                return;
            frame.older.onPop = function () {
                onLeaveFrameCalled++;
            };
        }
    },
    function (dbg, result, error) {
        assertEq(onLeaveFrameCalled, 1);
        assertEq(error, undefined);
    }
);

// Checking if function return values are not changed.
runWasmWithDebugger(
    '(module (func (result f64) (f64.const 0.42)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.onEnterFrame = function (frame) {
            dbg.onPop = function () {};
        };
    },
    function (dbg, result, error) {
        assertEq(result, 0.42);
        assertEq(error, undefined);
    }
);
runWasmWithDebugger(
    '(module (func (result f32) (f32.const 4.25)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.onEnterFrame = function (frame) {
            dbg.onPop = function () {};
        };
    },
    function (dbg, result, error) {
        assertEq(result, 4.25);
        assertEq(error, undefined);
    }
);

// Checking if onEnterFrame/onExceptionUnwind work during exceptions --
// `unreachable` causes wasm to throw WebAssembly.RuntimeError exception.
runWasmWithDebugger(
    '(module (func (unreachable)) (export "test" 0))', undefined,
    function (dbg) {
       onEnterFrameCalled = 0;
       onLeaveFrameCalled = 0;
       onExceptionUnwindCalled = 0;
       dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            onEnterFrameCalled++;
            frame.onPop = function() {
                onLeaveFrameCalled++;
            };
       };
       dbg.onExceptionUnwind = function (frame) {
         if (frame.type !== "wasmcall") return;
         onExceptionUnwindCalled++;
       };
    },
    function (dbg, result, error, g) {
        assertEq(onEnterFrameCalled, 1);
        assertEq(onLeaveFrameCalled, 1);
        assertEq(onExceptionUnwindCalled, 1);
        assertEq(error instanceof g.WebAssembly.RuntimeError, true);
    }
);

// Checking if onEnterFrame/onExceptionUnwind work during exceptions
// originated in the JavaScript import call.
runWasmWithDebugger(
    '(module (import $fn1 "env" "ex") (func $fn2 (call $fn1)) (export "test" $fn2))',
    '{env: { ex: () => { throw new Error(); }}}',
    function (dbg) {
       onEnterFrameCalled = 0;
       onLeaveFrameCalled = 0;
       onExceptionUnwindCalled = 0;
       dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            onEnterFrameCalled++;
            frame.onPop = function() {
                onLeaveFrameCalled++;
            };
       };
       dbg.onExceptionUnwind = function (frame) {
         if (frame.type !== "wasmcall") return;
         onExceptionUnwindCalled++;
       };
    },
    function (dbg, result, error, g) {
        assertEq(onEnterFrameCalled, 1);
        assertEq(onLeaveFrameCalled, 1);
        assertEq(onExceptionUnwindCalled, 1);
        assertEq(error instanceof g.Error, true);
    }
);

// Checking throwing in the handler.
runWasmWithDebugger(
    '(module (func (unreachable)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.uncaughtExceptionHook = function (value) {
            assertEq(value instanceof Error, true);
            return {throw: 'test'};
        };
        dbg.onEnterFrame = function (frame) {
           if (frame.type !== "wasmcall") return;
           throw new Error();
        };
    },
    function (dbg, result, error) {
        assertEq(error, 'test');
    }
);
runWasmWithDebugger(
    '(module (func (unreachable)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.uncaughtExceptionHook = function (value) {
            assertEq(value instanceof Error, true);
            return {throw: 'test'};
        };
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            frame.onPop = function () {
                throw new Error();
            }
        };
    },
    function (dbg, result, error) {
        assertEq(error, 'test');
    }
);

// Checking resumption values for JS_THROW.
runWasmWithDebugger(
    '(module (func (nop)) (export "test" 0))', undefined,
    function (dbg, g) {
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            return {throw: 'test'};
        };
    },
    function (dbg, result, error, g) {
        assertEq(error, 'test');
    }
);
runWasmWithDebugger(
    '(module (func (nop)) (export "test" 0))', undefined,
    function (dbg, g) {
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            frame.onPop = function () {
                return {throw: 'test'};
            }
        };
    },
    function (dbg, result, error, g) {
        assertEq(error, 'test');
    }
);

// Checking resumption values for JS_RETURN (not implemented by wasm baseline).
runWasmWithDebugger(
    '(module (func (unreachable)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            return {return: 2};
        };
    },
    function (dbg, result, error) {
        assertEq(result, undefined, 'NYI: result == 2, if JS_RETURN is implemented');
        assertEq(error != undefined, true, 'NYI: error == undefined, if JS_RETURN is implemented');
    }
);
runWasmWithDebugger(
    '(module (func (unreachable)) (export "test" 0))', undefined,
    function (dbg) {
        dbg.onEnterFrame = function (frame) {
            if (frame.type !== "wasmcall") return;
            frame.onPop = function () {
                return {return: 2};
            }
        };
    },
    function (dbg, result, error) {
        assertEq(result, undefined, 'NYI: result == 2, if JS_RETURN is implemented');
        assertEq(error != undefined, true, 'NYI: error == undefined, if JS_RETURN is implemented');
    }
);
throw "TestComplete";
