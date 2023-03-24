// Scaffolding for testing x64 Ion code generation patterns).  See
// ../tests/wasm/README-codegen.md for general information, and the *codegen.js
// tests in that directory and its subdirectories for specifics.
//
// The structure of the inputs can vary for the different testing predicates but
// each case generally has an operator name and an expected-pattern; the latter
// is a string that represents a regular expression, possibly with newlines,
// representing the instruction or instructions we are looking for for the
// operator.  Spaces in the expected-pattern are arbitrary, we preprocess the
// pattern to replace any space string with \s+.  Lines are separated by
// newlines and leading and trailing spaces are currently stripped.
//
// The testers additionally take an optional options bag with the following
// optional entries:
//  features: if present, an object to pass as the last argument to functions
//            that compile wasm bytecode
//  instanceBox: if present, an object with a `value` property that will
//               receive the constructed instance
//  no_prefix: by default, the required pattern must be immediately preceded
//             by `x64_prefix`, and this is checked.  Setting this to true skips
//             the check.
//  no_suffix: by default, the required pattern must be immediately followed
//             by `x64_suffix`, and this is checked.  Setting this to true skips
//             the check.
//  memory: if present, add a memory of length given by this property
//  log: for debugging -- print the disassembly, then the preprocessed pattern

load(libdir + "codegen-test-common.js");

// RIP-relative address following the instruction mnemonic
var RIPR = `0x${HEXES}`;

// RIP-relative address in the binary encoding
var RIPRADDR = `${HEX}{2} ${HEX}{2} ${HEX}{2} ${HEX}{2}`;

// End of prologue
var x64_prefix = `48 89 e5                  mov %rsp, %rbp`

// Start of epilogue
var x64_suffix = `5d                        pop %rbp`;

// v128 OP v128 -> v128
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_v128xv128_v128(inputs, options = {}) {
    for ( let [op, expected] of inputs ) {
        codegenTestX64BinopInternal(op, expected, options, 'v128', 'v128', 'v128', '0', '1');
    }
}

// v128 OP param1-type -> v128
// inputs: [[complete-opname, param1-type, expected-pattern], ...]
function codegenTestX64_v128xPTYPE_v128(inputs, options = {}) {
    for ( let [op, p1type, expected] of inputs ) {
        codegenTestX64BinopInternal(op, expected, options, 'v128', p1type, 'v128', '0', '1');
    }
}

// v128 OP literal -> v128
// inputs: [[complete-opname, rhs-literal, expected-pattern], ...]
function codegenTestX64_v128xLITERAL_v128(inputs, options = {}) {
    for ( let [op, literal, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0) ${literal}))`),
                              'f',
                              expected,
                              options)
    }
}

// literal OP v128 -> v128
// inputs: [[complete-opname, lhs-literal, expected-pattern], ...]
function codegenTestX64_LITERALxv128_v128(inputs, options = {}) {
    for ( let [op, literal, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param v128) (result v128)
      (${op} ${literal} (local.get 0)))`),
                              'f',
                              expected,
                              options)
    }
}

// v128 OP v128 -> v128, but operands are swapped
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_v128xv128_v128_reversed(inputs, options = {}) {
    for ( let [op, expected] of inputs ) {
        codegenTestX64BinopInternal(op, expected, options, 'v128', 'v128', 'v128', '1', '0');
    }
}

// OP v128 -> v128
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_v128_v128(inputs, options = {}) {
    for ( let [op, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0)))`),
                             'f',
                             expected,
                             options);
    }
}

// OP param-type -> v128
// inputs [[complete-opname, param-type, expected-pattern], ...]
function codegenTestX64_PTYPE_v128(inputs, options = {}) {
    for ( let [op, ptype, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param ${ptype}) (result v128)
      (${op} (local.get 0)))`),
                             'f',
                             expected,
                             options);
    }
}

// OP v128 -> v128, but the function takes two arguments and the first is ignored
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_IGNOREDxv128_v128(inputs, options = {}) {
    for ( let [op, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param v128) (param v128) (result v128)
      (${op} (local.get 1)))`),
                             'f',
                             expected,
                             options);
    }
}

// () -> v128
// inputs: [[complete-opname, expected-pattern], ...]
function codegenTestX64_unit_v128(inputs, options = {}) {
    for ( let [op, expected] of inputs ) {
        codegenTestX64_adhoc(wrap(options, `
   (func (export "f") (result v128)
     (${op}))`),
                             'f',
                             expected,
                             options);
    }
}

// For when nothing else applies: `module_text` is the complete source text of
// the module, `export_name` is the name of the function to be tested,
// `expected` is the non-preprocessed pattern, and options is an options bag,
// described above.
function codegenTestX64_adhoc(module_text, export_name, expected, options = {}) {
    assertEq(hasDisassembler(), true);

    let ins = wasmEvalText(module_text, {}, options.features);
    if (options.instanceBox)
        options.instanceBox.value = ins;
    let output = wasmDis(ins.exports[export_name], {tier:"ion", asString:true});
    if (!options.no_prefix)
        expected = x64_prefix + '\n' + expected;
    if (!options.no_suffix)
        expected = expected + '\n' + x64_suffix;
    const expected_pretty = striplines(expected);
    expected = fixlines(expected);

    const success = output.match(new RegExp(expected)) != null;
    if (options.log || !success) {
        print("Module text:")
        print(module_text);
        print("Actual output:")
        print(output);
        print("Expected output (easy-to-read and fully-regex'd):")
        print(expected_pretty);
        print(expected);
    }
    assertEq(success, true);
}

// Internal code below this line

function codegenTestX64BinopInternal(op, expected, options, p0type, p1type, restype, arg0, arg1) {
    codegenTestX64_adhoc(wrap(options, `
    (func (export "f") (param ${p0type}) (param ${p1type}) (result ${restype})
      (${op} (local.get ${arg0}) (local.get ${arg1})))`),
                         'f',
                         expected,
                         options);
}
