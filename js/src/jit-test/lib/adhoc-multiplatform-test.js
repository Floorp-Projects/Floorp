
// This file provides a version of the functions
//
//   codegenTestX64_adhoc   (src/jit-test/lib/codegen-x64-test.js)
//   codegenTestX86_adhoc   (src/jit-test/lib/codegen-x86-test.js)
//   codegenTestARM64_adhoc (src/jit-test/lib/codegen-arm64-test.js)
//   (and the equivalent arm(32) function)
//
// generalised so the output can be specified for all 4 targets in one place.
//
// Usage:
//   codegenTestMultiplatform_adhoc(module_text, export_name,
//                                  expectedAllTargets, options = {})
//
// where
//   `expectedAllTargets` states the expected-output regexps for each target,
//   thusly:
//
//   {x64: 'text', x86: 'text', arm64: 'text', arm: 'text'}
//
//   The arm(32) expected output is optional.  The other 3 must be present.
//
//   Each 'text' is a string that represents a regular expression, possibly
//   with newlines, representing the instruction or instructions we are looking
//   for for the operator.  Spaces in the expected-pattern are arbitrary, we
//   preprocess the pattern to replace any space string with \s+.  Lines are
//   separated by newlines and leading and trailing spaces are stripped.
//   Pattern strings may be empty, denoting "no instruction(s)".
//
//  options specifies options thusly:
//
//    instanceBox: if present, an object with a `value` property that will
//                 receive the constructed instance
//
//    log: for debugging -- print the disassembly and other info helpful to
//         resolving test failures.  This is also printed on a test failure
//         regardless of the log setting.
//
//    features: this is passed on verbatim to wasmEvalText,
//              as its third argument.
//
//    no_prefix: by default, the required pattern must be immediately preceded
//               by `<target>_prefix`, and this is checked.  Setting this to
//               true skips the check.  Try not to use this.
//
//    no_suffix: by default, the required pattern must be immediately followed
//               by `<target>_suffix`, and this is checked.  Setting this to
//               true skips the check.  Try not to use this.
//
//    no_prefix/no_suffix apply to all 4 targets.  Per-target overrides are
//    supported, by putting them in a suitably tagged sub-object, eg:
//    options = {x86: {no_prefix: true}}

load(libdir + "codegen-test-common.js");

// Architectures supported by this script.
const knownArchs = ["x64", "x86", "arm64", "arm"];

// Architectures for which `expectedAllTargets` must supply an expected result.
const requiredArchs = ["x64", "x86", "arm64"];

// These define the end-of-prologue ("prefix") and start-of-epilogue
// ("suffix") to be matched.
const prefixAndSuffix =
      {x64: {
           prefix: `48 89 e5        mov %rsp, %rbp`,
           suffix: `5d              pop %rbp`
       },
       x86: {
           // The mov to e[ac]x is debug code, inserted by the register
           // allocator to clobber e[ac]x before a move group.  But it is only
           // present if there is a move group there.
           prefix: `8b ec           mov %esp, %ebp(
                    b. ef be ad de  mov \\$0xDEADBEEF, %e.x)?`,
           // `.bp` because zydis chooses `rbp` even on 32-bit systems.
           suffix: `5d              pop %.bp`
       },
       arm64: {
           prefix: `910003fd        mov x29, sp
                    910003fc        mov x28, sp`,
           suffix: `f94003fd        ldr x29, \\[sp\\]`
       },
       arm: {
           prefix: `e52db004        str fp, \\[sp, #-4\\]!
                    e1a0b00d        mov fp, sp`,
           suffix: `e49db004        ldr fp, \\[sp\\], #\\+4`
       }
      };

// The options object may a mix of generic (all-targets) options and contain
// sub-objects containing arch-specific options, for example:
//
//   {a_generic_option: 1337, x86: {no_prefix:true}, arm64: {foo:4771}}
//
// promoteArchSpecificOptions lifts options for `archName` to the top level
// and deletes *all* arch-specific subobjects, hence producing the final
// to-be-used option set.  For the above example, if `archName` is "x86" we
// get:
//
//   {a_generic_option: 1337, no_prefix: true}
//
function promoteArchSpecificOptions(options, archName) {
    assertEq(true, knownArchs.some(a => archName == a));
    if (options.hasOwnProperty(archName)) {
        let archOptions = options[archName];
        for (optName in archOptions) {
            options[optName] = archOptions[optName];
            if (options.log) {
                print("---- adding " + archName + "-specific option {"
                      + optName + ":" + archOptions[optName] + "}");
            }
        }
    }
    for (a of knownArchs) {
        delete options[a];
    }
    if (options.log) {
        print("---- final options");
        for (optName in options) {
            print("{" + optName + ":" + options[optName] + "}");
        }
    }
    return options;
}

// Main test function.  See comments at top of this file.
function codegenTestMultiplatform_adhoc(module_text, export_name,
                                        expectedAllTargets, options = {}) {
    assertEq(hasDisassembler(), true);

    // Check that we've been provided with an expected result for at least
    // x64, x86 and arm64.
    assertEq(true,
             requiredArchs.every(a => expectedAllTargets.hasOwnProperty(a)));

    // Poke the build-configuration object to find out what target we're
    // generating code for.
    let genX64   = getBuildConfiguration("x64");
    let genX86   = getBuildConfiguration("x86");
    let genArm64 = getBuildConfiguration("arm64");
    let genArm   = getBuildConfiguration("arm");
    // So far so good, except .. X64 or X86 might be emulating something else.
    if (genX64 && genArm64 && getBuildConfiguration("arm64-simulator")) {
        genX64 = false;
    }
    if (genX86 && genArm && getBuildConfiguration("arm-simulator")) {
        genX86 = false;
    }

    // Check we've definitively identified exactly one architecture to test.
    assertEq(1, [genX64, genX86, genArm64, genArm].map(x => x ? 1 : 0)
                                                  .reduce((a,b) => a+b, 0));

    // Decide on the arch name for which we're testing.  Everything is keyed
    // off this.
    let archName = "";
    if (genX64) {
        archName = "x64";
    } else if (genX86) {
        archName = "x86";
    } else if (genArm64) {
        archName = "arm64";
    } else if (genArm) {
        archName = "arm";
    }
    if (options.log) {
        print("---- testing for architecture \"" + archName + "\"");
    }
    // If this fails, it means we're running on an "unknown" architecture.
    assertEq(true, archName.length > 0);

    // Finalise options, by promoting arch-specific ones to the top level of
    // the options object.
    options = promoteArchSpecificOptions(options, archName);

    // Get the prefix and suffix strings for the target.
    assertEq(true, prefixAndSuffix.hasOwnProperty(archName));
    let prefix = prefixAndSuffix[archName].prefix;
    let suffix = prefixAndSuffix[archName].suffix;
    assertEq(true, prefix.length >= 10);
    assertEq(true, suffix.length >= 10);

    // Get the expected output string, or skip the test if no expected output
    // has been provided.  Note, because of the assertion near the top of this
    // file, this will currently only allow arm(32) tests to be skipped.
    let expected = "";
    if (expectedAllTargets.hasOwnProperty(archName)) {
        expected = expectedAllTargets[archName];
    } else {
        // Paranoia.  Don't want to silently skip tests due to logic bugs above.
        assertEq(archName, "arm");
        if (options.log) {
            print("---- !! no expected output for target, skipping !!");
        }
        return;
    }

    // Finalise the expected-result string, and stash the original for
    // debug-printing.
    expectedInitial = expected;
    if (!options.no_prefix) {
        expected = prefix + '\n' + expected;
    }
    if (!options.no_suffix) {
        expected = expected + '\n' + suffix;
    }
    if (genArm) {
        // For obscure reasons, the arm(32) disassembler prints the
        // instruction word twice.  Rather than forcing all expected lines to
        // do the same, we detect any line starting with 8 hex digits followed
        // by a space, and duplicate them so as to match the
        // disassembler's output.
        let newExpected = "";
        let pattern = /^[0-9a-fA-F]{8} /;
        for (line of expected.split(/\n+/)) {
            // Remove whitespace at the start of the line.  This could happen
            // for continuation lines in backtick-style expected strings.
            while (line.match(/^\s/)) {
                line = line.slice(1);
            }
            if (line.match(pattern)) {
                line = line.slice(0,9) + line;
            }
            newExpected = newExpected + line + "\n";
        }
        expected = newExpected;
    }
    expected = fixlines(expected);

    // Compile the test case and collect disassembly output.
    let ins = wasmEvalText(module_text, {}, options.features);
    if (options.instanceBox)
        options.instanceBox.value = ins;
    let output = wasmDis(ins.exports[export_name], {tier:"ion", asString:true});

    // Check for success, print diagnostics
    let output_matches_expected = output.match(new RegExp(expected)) != null;
    if (!output_matches_expected) {
        print("---- adhoc-tier1-test.js: TEST FAILED ----");
    }
    if (options.log && output_matches_expected) {
        print("---- adhoc-tier1-test.js: TEST PASSED ----");
    }
    if (options.log || !output_matches_expected) {
        print("---- module text");
        print(module_text);
        print("---- actual");
        print(output);
        print("---- expected (initial)");
        print(expectedInitial);
        print("---- expected (as used)");
        print(expected);
        print("----");
    }

    // Finally, the whole point of this:
    assertEq(output_matches_expected, true);
}
