// |jit-test| skip-if: !wasmStreamingIsSupported()

load(libdir + "wasm-binary.js");

function testInstantiate(source, importObj, exportName, expectedValue) {
    var result;
    WebAssembly.instantiateStreaming(code, importObj).then(r => { result = r });
    drainJobQueue();
    assertEq(result !== undefined, true);
    assertEq(result.module instanceof WebAssembly.Module, true);
    assertEq(result.instance instanceof WebAssembly.Instance, true);
    assertEq(result.instance.exports[exportName](), expectedValue);
}
function testBoth(source, exportName, expectedValue) {
    var module;
    WebAssembly.compileStreaming(code).then(m => { module = m });
    drainJobQueue();
    assertEq(module !== undefined, true);
    assertEq(module instanceof WebAssembly.Module, true);
    assertEq(new WebAssembly.Instance(module).exports[exportName](), expectedValue);

    testInstantiate(source, undefined, exportName, expectedValue);
}

function testFailInstantiate(source, importObj, error) {
    var caught = false;
    WebAssembly.instantiateStreaming(source).catch(err => {
        assertEq(err instanceof error, true);
        caught = true;
    });
    drainJobQueue();
    assertEq(caught, true);
}
function testFailBoth(source, error) {
    var caught = false;
    WebAssembly.compileStreaming(source).catch(err => {
        assertEq(err instanceof error, true);
        caught = true;
    });
    drainJobQueue();
    assertEq(caught, true);

    testFailInstantiate(source, undefined, error);
}

var code = wasmTextToBinary('(module (func (export "run") (result i32) i32.const 42))');
testBoth(code, 'run', 42);
testFailBoth(42, TypeError);
testBoth(Promise.resolve(code), 'run', 42);
testFailBoth(Promise.resolve(42), TypeError);
testFailBoth(Promise.reject(new String("fail")), String);
testBoth({then(resolve) { resolve(code) }}, 'run', 42);
testFailBoth({then(resolve) { resolve(42) }}, TypeError);
testFailBoth(new Promise((resolve, reject) => { reject(new Error("hi")) }), Error);
testFailBoth(new Promise((resolve, reject) => { reject(new String("hi")) }), String);

var code = wasmTextToBinary('(module (func $im (import "js" "foo") (result i32)) (func (export "run") (result i32) (i32.add (i32.const 1) (call $im))))');
testInstantiate(code, {js:{foo() { return 42 }}}, 'run', 43);
testFailInstantiate(code, null, TypeError);
testFailInstantiate(code, {js:42}, TypeError);
testFailInstantiate(code, {js:{foo:42}}, TypeError);

var text = `(module\n`;
text += ` (func (result i32) i32.const 0)\n`;
for (var i = 1; i <= 100; i++)
    text += ` (func (result i32) (i32.add (i32.const ${i}) (call ${i-1})))\n`;
text += ` (func (export "run") (result i32) call 100)\n`;
text += `)`;
var code = wasmTextToBinary(text);

assertEq(code.length > 1000, true);
for ([delayMillis, chunkSize] of [[0, 10], [1, 10], [0, 100], [1, 100], [0, 1000], [1, 1000], [10, 1000]]) {
    setBufferStreamParams(delayMillis, chunkSize);
    testBoth(code, 'run', 5050);
}

setBufferStreamParams(1, 100);
var arr = [];
for (var i = 0; i < 10; i++)
    arr.push(WebAssembly.instantiateStreaming(code));
var results;
Promise.all(arr).then(r => results = r);
drainJobQueue();
assertEq(results.length === 10, true);
for (var i = 0; i < 10; i++)
    assertEq(results[i].instance.exports.run(), 5050);

// No code section, but data section:
var code = wasmTextToBinary('(module (memory (import "js" "mem") 1) (data (i32.const 0) "a"))');
var mem = new WebAssembly.Memory({initial:1});
WebAssembly.instantiateStreaming(code, {js:{mem}});
drainJobQueue();
assertEq(new Uint8Array(mem.buffer)[0], 97);

// Junk section before code section.
testFailBoth(moduleWithSections([{name: 100, body: [1, 2, 3]}, bodySection([])]), WebAssembly.CompileError);
