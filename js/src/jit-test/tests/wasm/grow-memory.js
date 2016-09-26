// |jit-test| test-also-wasm-baseline
load(libdir + "wasm.js");

function linearModule(min, max, ops) {
  var opsText = ops.map(function (op) {
    if (op[0] == "CM") {
      res = `(if i32 (i32.ne (current_memory) (i32.const ${op[1]}))
                  (i32.load offset=10 (i32.const 4294967295))
                  (i32.const 0))`
    } else if (op[0] == "GM") {
      res = `(if i32 (i32.ne (grow_memory (i32.const ${op[1]})) (i32.const ${op[2]}))
                 (i32.load offset=10 (i32.const 4294967295))
                 (i32.const 0))`
    } else if (op[0] == "L") {
      var type = op[1];
      var ext = op[2];
      var off = op[3];
      var loc = op[4]
      var align = 0;
      res = `(${type}.load${ext} offset=${off} (i32.const ${op[4]}))`;
    } else if (op[0] == "S") {
      var type = op[1];
      var ext = op[2];
      var off = op[3];
      var loc = op[4]
      var align = 0;
      res = `(${type}.store${ext} offset=${off} (i32.const ${op[4]}) (i32.const 42))`;
    }
    return res;
  }).join("\n")

  text =
    `(module
       (memory ${min} ${max})
         ` + (min != 0 ? `(data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
                          (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")`
                      : "") +
       `
       (func (result i32)
         (drop ` + opsText + `)
         (current_memory)
       ) (export "" 0))`;

  return wasmEvalText(text);
}

function assertOOB(lambda) {
  assertErrorMessage(lambda, Error, /invalid or out-of-range index/);
}

// Just grow some memory
assertEq(linearModule(3,5, [["CM", 3]]).exports[""](), 3);
