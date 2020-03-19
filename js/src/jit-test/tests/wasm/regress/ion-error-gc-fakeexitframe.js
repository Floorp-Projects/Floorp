var lfLogBuffer = `
//corefuzz-dcd-endofdata
for (var i = 0; gczeal(4,10); g(buffer))
  assertEq(assignParameterGetElement(42), 17);
//corefuzz-dcd-endofdata
//corefuzz-dcd-endofdata
//corefuzz-dcd-endofdata
g = newGlobal({newCompartment: true});
g.parent = this
g.eval("Debugger(parent).onExceptionUnwind=(function(){})")
`;
lfLogBuffer = lfLogBuffer.split('\n');

gcPreserveCode();

var letext =`(module
  (type $type0 (func (param i32 i64)))
  (type $type1 (func (param i32) (result i64)))
  (type $type2 (func (result i32)))
  (memory 1)
  (export "store" $func0)
  (export "load" $func1)
  (export "assert_0" $func2)
  (func $func0 (param $var0 i32) (param $var1 i64)
    local.get $var0
    local.get $var1
    i64.store16 offset=16
  )
  (func $func1 (param $var0 i32) (result i64)
    local.get $var0
    i64.load16_s offset=16
  )
  (func $func2 (result i32)
    i32.const 65519
    i64.const -32768
    call $func0
    i32.const 1
  )
  (data (i32.const 0)
    "\\00\\01\\02\\03\\04\\05\\06\\07\\08\t\n\\0b\\0c\\0d\\0e\\0f"
  )
  (data (i32.const 16)
    "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff"
  )
)`;

var binary = wasmTextToBinary(letext);
var module = new WebAssembly.Module(binary);

var lfCodeBuffer = "";
while (true) {
    var line = lfLogBuffer.shift();
    if (line == null) {
        break;
    } else if (line == "//corefuzz-dcd-endofdata") {
        processCode(lfCodeBuffer);
    } else {
        lfCodeBuffer += line + "\n";
    }
}

if (lfCodeBuffer) processCode(lfCodeBuffer);

function processCode(code) {
    evaluate(code);
    while (true) {
        imports = {}
        try {
            instance = new WebAssembly.Instance(module, imports);
            break;
        } catch (exc) {}
    }
    for (let descriptor of WebAssembly.Module.exports(module)) {
        switch (descriptor.kind) {
            case "function":
                try {
                    print(instance.exports[descriptor.name]())
                } catch (exc1) {}
        }
    }
}
