// |jit-test| test-also-no-wasm-baseline
// Checking existence of all frame.offset references during onEnterFrame,
// onLeaveFrame and onStep events in the source code, and that we can
// potentially resolve offset back to the line/column.

load(libdir + "wasm.js");

if (!wasmDebuggingIsSupported())
    quit();

var offsets;
wasmRunWithDebugger(
    '(module (func (nop) (nop)) (export "test" 0))',
    undefined,
    function ({dbg}) {
        offsets = [];
        dbg.onEnterFrame = function (frame) {
            if (frame.type != 'wasmcall') {
                return;
            }
            offsets.push(frame.offset);
            frame.onStep = function () {
                offsets.push(frame.offset);
            };
            frame.onPop = function () {
                offsets.push(frame.offset);
            };
        };
    },
    function ({wasmScript, error}) {
        assertEq(error, undefined);
        assertEq(offsets.length, 5);
        offsets.forEach(offset => {
            var loc = wasmScript.getOffsetLocation(offset);
            assertEq(loc.isEntryPoint, true);
            assertEq(loc.lineNumber > 0, true);
            assertEq(loc.columnNumber > 0, true);
            assertEq(wasmScript.getLineOffsets(loc.lineNumber).length, 1);
        });
    }
);
