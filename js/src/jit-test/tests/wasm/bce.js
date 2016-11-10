load(libdir + "wasm.js");

mem='\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f'+
    '\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff'+
    '\x00'.repeat(65488) +
    '\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff'

print (mem.lengt)

let accessWidth = {
  '8_s':  1,
  '8_u':    1,
  '16_s': 2,
  '16_u':   2,
  '': 4,
  'f32':  4,
  'f64':  8,
}

let baseOp = {
  '8_s':  'i32',
  '8_u':    'i32',
  '16_s': 'i32',
  '16_u':   'i32',
  '': 'i32',
  'f32':  'f32',
  'f64':  'f64',
}

function toSigned(width, num) {
  let unsignedMax = Math.pow(2, accessWidth[width] * 8) - 1;
  let signedMax = Math.pow(2, accessWidth[width] * 8 - 1) - 1;

  return (num <= signedMax ? num : -(unsignedMax + 1 - num));
}

function fromLittleEndianNum(width, bytes) {
  let base = 1;
  var res = 0;
  for (var i = 0; i < accessWidth[width]; i++) {
    res += base * bytes[i];
    base *= 256;
  }
  return res;
}

function getInt(width, offset, mem) {
  var bytes = [ ];
  for (var i = offset; i < offset + accessWidth[width]; i++) {
    if (i < mem.length)
      bytes.push(mem.charCodeAt(i));
    else
      bytes.push(0);
  }

  var res = fromLittleEndianNum(width, bytes);
  if (width == '8_s' || width == '16_s' || width == '')
    res = toSigned(width, res);
  return res;
}

function loadTwiceModule(type, ext, offset, align) {
    // TODO: Generate memory from byte string
    return wasmEvalText(
    `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (data (i32.const 65520) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func (param i32) (param i32) (result ${type})
         (drop (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
         ))
         (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 1)
         )
       ) (export "" 0))`
    ).exports[""];
}

function loadTwiceSameBasePlusConstModule(type, ext, offset, align, addConst) {
    return wasmEvalText(
    `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (data (i32.const 65520) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func (param i32) (result ${type})
         (drop (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
         ))
         (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (i32.add (get_local 0) (i32.const ${addConst}))
         )
       ) (export "" 0))`
    ).exports[""];
}

function loadTwiceSameBasePlusNonConstModule(type, ext, offset, align) {
    return wasmEvalText(
    `(module
       (memory 1)
       (data (i32.const 0) "\\00\\01\\02\\03\\04\\05\\06\\07\\08\\09\\0a\\0b\\0c\\0d\\0e\\0f")
       (data (i32.const 16) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (data (i32.const 65520) "\\f0\\f1\\f2\\f3\\f4\\f5\\f6\\f7\\f8\\f9\\fa\\fb\\fc\\fd\\fe\\ff")
       (func (param i32) (param i32) (result ${type})
         (drop (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (get_local 0)
         ))
         (${type}.load${ext}
          offset=${offset}
          ${align != 0 ? 'align=' + align : ''}
          (i32.add (get_local 0) (get_local 1))
         )
       ) (export "" 0))`
    ).exports[""];
}

/*
 * On x64 falsely removed bounds checks will be masked by the signal handlers.
 * Thus it is important that these tests be run on x86.
 */

function testOOB(mod, args) {
    assertErrorMessage(() => mod(...args), WebAssembly.RuntimeError, /index out of bounds/);
}

function testOk(mod, args, expected, expectedType) {
    if (expectedType === 'i64')
        assertEqI64(mod(...args), createI64(expected));
    else
        assertEq(mod(...args), expected);
}

// TODO: It would be nice to verify how many BCs are eliminated on positive tests.

const align = 0;
for (let offset of [0, 1, 2, 3, 4, 8, 16, 41, 0xfff8]) {

  var widths = ['8_s', '8_u', '16_s', '16_u', '']

  for(let width of widths) {
    // Accesses of 1 byte.
    let lastValidIndex = 0x10000 - offset - accessWidth[width];
    let op = baseOp[width];

    print("Width: " + width + " offset: " + offset + " op: " + op)
    var mod = loadTwiceModule(op, width, offset, align);

    // Two consecutive loads from two different bases
    testOk(mod, [lastValidIndex, lastValidIndex], getInt(width, lastValidIndex + offset, mem), op);
    testOOB(mod, [lastValidIndex + 42, lastValidIndex + 42]);
    testOOB(mod, [lastValidIndex, lastValidIndex + 42]);

    mod = loadTwiceSameBasePlusConstModule(op, width, offset, align, 1);

    testOk(mod, [lastValidIndex-1], getInt(width, lastValidIndex + offset, mem), op);
    testOOB(mod, [lastValidIndex]);

    // Two consecutive loads from same base with different offsets
    mod = loadTwiceSameBasePlusConstModule(op, width, offset, align, 2);

    testOk(mod, [lastValidIndex-2], getInt(width, lastValidIndex + offset, mem), op);
    testOOB(mod, [lastValidIndex-1, 2]);

    mod = loadTwiceSameBasePlusConstModule(op, width, offset, align, lastValidIndex);

    testOk(mod, [0], getInt(width, lastValidIndex + offset, mem), op);
    testOOB(mod, [1]);

    mod = loadTwiceSameBasePlusNonConstModule(op, width, offset, align);
    testOk(mod, [0, 1], getInt(width, 1 + offset, mem), op);
    testOk(mod, [0, lastValidIndex], getInt(width, lastValidIndex + offset, mem), op);
    testOOB(mod, [1, lastValidIndex])

    // TODO: All of the above with mixed loads and stores

    // TODO: Branching - what do we want?

    // TODO: Just loops
    //         - loop invariant checks
    //         - loop dependant checks remaining inbounds
    //         - loop dependant checks going out-of bounds.
    //
    // TODO: Loops + branching
    //         - loop invariant checks guarded by a loop invariant branch?
  }
}
