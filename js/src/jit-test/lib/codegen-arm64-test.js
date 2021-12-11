// Scaffolding for testing arm64 Ion code generation patterns .  See
// codegen-x64-test.js in this directory for more information.

load(libdir + "codegen-test-common.js");

// End of prologue
var arm64_prefix = `
910003fd  mov     x29, sp
910003fc  mov     x28, sp
`;

// Start of epilogue
var arm64_suffix = `
f94003fd  ldr     x29, \\[sp\\]
`;

// For when nothing else applies: `module_text` is the complete source text of
// the module, `export_name` is the name of the function to be tested,
// `expected` is the non-preprocessed pattern, and options is an options bag,
// described above.
function codegenTestARM64_adhoc(module_text, export_name, expected, options = {}) {
    assertEq(hasDisassembler(), true);

    let ins = wasmEvalText(module_text, {}, options.features);
    if (options.instanceBox)
        options.instanceBox.value = ins;
    let output = wasmDis(ins.exports[export_name], {tier:"ion", asString:true});
    if (!options.no_prefix)
        expected = arm64_prefix + '\n' + expected;
    if (!options.no_suffix)
        expected = expected + '\n' + arm64_suffix;
    expected = fixlines(expected);
    if (options.log) {
        print(module_text);
        print(output);
        print(expected);
    }
    assertEq(output.match(new RegExp(expected)) != null, true);
}

