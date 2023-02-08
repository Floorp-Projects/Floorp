// Scaffolding for testing x86 Ion code generation patterns .  See
// codegen-x64-test.js in this directory for more information.

load(libdir + "codegen-test-common.js");

// Note that Zydis disassembles x86 absolute addresses as relative, so
// the binary encoding and the text encoding may not correspond precisely.

// Absolute address (disp32) following the instruction mnemonic.
var ABS = `0x${HEXES}`;

// Absolute address (disp32) in the binary encoding.
var ABSADDR = `${HEX}{2} ${HEX}{2} ${HEX}{2} ${HEX}{2}`;

// End of prologue.  The mov to eax is debug code, inserted by the register
// allocator to clobber eax before a move group.  But it is only present if
// there is a move group there.
//
// -0x21524111 is 0xDEADBEEF.
var x86_prefix = `
8b ec            mov %esp, %ebp(
b8 ef be ad de   mov \\$-0x21524111, %eax)?
`

// `.bp` because zydis chooses 'rbp' even on 32-bit systems
var x86_loadarg0 = `
f3 0f 6f 45 ${HEX}{2}            movdqux 0x${HEXES}\\(%.bp\\), %xmm0
`;

// Start of epilogue.  `.bp` for the same reason as above.
var x86_suffix = `5d      pop %.bp`;

// v128 OP literal -> v128
// inputs: [[complete-opname, rhs-literal, expected-pattern], ...]
function codegenTestX86_v128xLITERAL_v128(inputs, options = {}) {
    for ( let [op, literal, expected] of inputs ) {
        codegenTestX86_adhoc(wrap(options, `
    (func (export "f") (param v128) (result v128)
      (${op} (local.get 0) ${literal}))`),
                             'f',
                             x86_loadarg0 + expected,
                             options)
    }
}

// For when nothing else applies: `module_text` is the complete source text of
// the module, `export_name` is the name of the function to be tested,
// `expected` is the non-preprocessed pattern, and options is an options bag,
// described above.
function codegenTestX86_adhoc(module_text, export_name, expected, options = {}) {
    assertEq(hasDisassembler(), true);

    let ins = wasmEvalText(module_text);
    let output = wasmDis(ins.exports[export_name], {tier:"ion", asString:true});

    const expected_initial = expected;
    if (!options.no_prefix)
        expected = x86_prefix + '\n' + expected;
    if (!options.no_suffix)
        expected = expected + '\n' + x86_suffix;
    expected = fixlines(expected);

    const output_matches_expected = output.match(new RegExp(expected)) != null;
    if (!output_matches_expected) {
        print("---- codegen-x86-test.js: TEST FAILED ----");
    }
    if (options.log && output_matches_expected) {
        print("---- codegen-x86-test.js: TEST PASSED ----");
    }
    if (options.log || !output_matches_expected) {
        print("---- module text");
        print(module_text);
        print("---- actual");
        print(output);
        print("---- expected (initial)");
        print(expected_initial);
        print("---- expected (as used)");
        print(expected);
        print("----");
    }

    assertEq(output_matches_expected, true);
}

