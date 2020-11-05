// Scaffolding for testing x64 Ion code generation patterns (currently mostly
// for wasm SIMD).  See ../tests/wasm/simd/README-codegen.md for general
// information, and the *codegen.js tests in that directory for specifics.
//
// The structure of the inputs can vary for the different testing predicates but
// each case generally has an operator name and an expected-pattern; the latter is a
// string that represents a regular expression, possibly with newlines,
// representing the instruction or instructions we are looking for for the operator.
// Spaces in the expected-pattern are arbitrary, we preprocess the pattern to
// replace any space string with \s+.  Lines are separated by newlines and leading
// and trailing spaces are currently stripped.
//
// Lines in expected-pattern starting with '*' are not prefixed by an address during
// preprocessing.
//
// Lines in expected-pattern that end with a space (lines that are split do) must use \s
// to represent that space.
//
// The testers additionally take an optional options bag with the following optional
// entries:
//  no_prefix: if true, do not add a prefix string (normally the end of the prologue)
//  no_suffix: if true, do not add a suffix string (normally the start of the epilogue)
//  memory: if present, add a memory of length given by this property
//  log: for debugging -- print the disassembly, then the preprocessed pattern

// Set to true to emit ' +' instead of the unreadable '\s+'.
var SPACEDEBUG = false;

// Any hex string
var HEXES = `[0-9a-fA-F]+`;

// RIP-relative address
var RIPR = `0x${HEXES}`;

// End of prologue
var x64_prefix = `48 8b ec                  mov %rsp, %rbp`

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

    let ins = wasmEvalText(module_text);
    let output = wasmDis(ins.exports[export_name], "ion", true);
    if (!options.no_prefix)
        expected = x64_prefix + '\n' + expected;
    if (!options.no_suffix)
        expected = expected + '\n' + x64_suffix;
    expected = fixlines(expected);
    if (options.log) {
        print(output);
        print(expected);
    }
    assertEq(output.match(new RegExp(expected)) != null, true);
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

function wrap(options, funcs) {
    if ('memory' in options)
        return `(module (memory ${options.memory}) ${funcs})`;
    return `(module ${funcs})`;
}

function fixlines(s) {
    return s.split(/\n+/)
        .map(strip)
        .filter(x => x.length > 0)
        .map(x => x.charAt(0) == '*' ? x.substring(1) : (HEXES + ' ' + x))
        .map(spaces)
        .join('\n');
}

function strip(s) {
    while (s.length > 0 && isspace(s.charAt(0)))
        s = s.substring(1);
    while (s.length > 0 && isspace(s.charAt(s.length-1)))
        s = s.substring(0, s.length-1);
    return s;
}

function spaces(s) {
    let t = '';
    let i = 0;
    while (i < s.length) {
        if (isspace(s.charAt(i))) {
            t += SPACEDEBUG ? ' +' : '\\s+';
            i++;
            while (i < s.length && isspace(s.charAt(i)))
                i++;
        } else {
            t += s.charAt(i++);
        }
    }
    return t;
}

function isspace(c) {
    return c == ' ' || c == '\t';
}
