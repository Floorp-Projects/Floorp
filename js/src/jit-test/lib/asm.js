/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const ASM_OK_STRING = "successfully compiled asm.js code";
const ASM_TYPE_FAIL_STRING = "asm.js type error:";
const ASM_DIRECTIVE_FAIL_STRING = "\"use asm\" is only meaningful in the Directive Prologue of a function body";

const USE_ASM = "'use asm';";
const HEAP_IMPORTS = "var i8=new glob.Int8Array(b);var u8=new glob.Uint8Array(b);"+
                     "var i16=new glob.Int16Array(b);var u16=new glob.Uint16Array(b);"+
                     "var i32=new glob.Int32Array(b);var u32=new glob.Uint32Array(b);"+
                     "var f32=new glob.Float32Array(b);var f64=new glob.Float64Array(b);";
const BUF_64KB = new ArrayBuffer(64 * 1024);

function asmCompile()
{
    var f = Function.apply(null, arguments);
    assertEq(!isAsmJSCompilationAvailable() || isAsmJSModule(f), true);
    return f;
}

function assertAsmDirectiveFail(str)
{
    if (!isAsmJSCompilationAvailable())
        return;

    // Turn on warnings-as-errors
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);

    // Verify an error is thrown
    var caught = false;
    try {
        eval(str);
    } catch (e) {
        if ((''+e).indexOf(ASM_DIRECTIVE_FAIL_STRING) == -1)
            throw new Error("Didn't catch the expected directive failure error; instead caught: " + e);
        caught = true;
    }
    if (!caught)
        throw new Error("Didn't catch the directive failure error");

    // Turn warnings-as-errors back off
    options("werror");
}

function assertAsmTypeFail()
{
    if (!isAsmJSCompilationAvailable())
        return;

    // Verify no error is thrown with warnings off
    Function.apply(null, arguments);

    // Turn on warnings-as-errors
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);

    // Verify an error is thrown
    var caught = false;
    try {
        Function.apply(null, arguments);
    } catch (e) {
        if ((''+e).indexOf(ASM_TYPE_FAIL_STRING) == -1)
            throw new Error("Didn't catch the expected type failure error; instead caught: " + e);
        caught = true;
    }
    if (!caught)
        throw new Error("Didn't catch the type failure error");

    // Turn warnings-as-errors back off
    options("werror");
}

function assertAsmLinkFail(f)
{
    if (!isAsmJSCompilationAvailable())
        return;

    // Verify no error is thrown with warnings off
    f.apply(null, Array.slice(arguments, 1));

    // Turn on warnings-as-errors
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);

    // Verify an error is thrown
    var caught = false;
    try {
        f.apply(null, Array.slice(arguments, 1));
    } catch (e) {
        // Arbitrary code an run in the GetProperty, so don't assert any
        // particular string
        caught = true;
    }
    if (!caught)
        throw new Error("Didn't catch the link failure error");

    // Turn warnings-as-errors back off
    options("werror");
}

// Linking should throw an exception even without warnings-as-errors
function assertAsmLinkAlwaysFail(f)
{
    var caught = false;
    try {
        f.apply(null, Array.slice(arguments, 1));
    } catch (e) {
        caught = true;
    }
    if (!caught)
        throw new Error("Didn't catch the link failure error");

    // Turn on warnings-as-errors
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);

    // Verify an error is thrown
    var caught = false;
    try {
        f.apply(null, Array.slice(arguments, 1));
    } catch (e) {
        caught = true;
    }
    if (!caught)
        throw new Error("Didn't catch the link failure error");

    // Turn warnings-as-errors back off
    options("werror");
}

// Linking should throw a warning-as-error but otherwise run fine
function asmLink(f)
{
    if (!isAsmJSCompilationAvailable())
        return f.apply(null, Array.slice(arguments, 1));

    // Turn on warnings-as-errors
    var oldOpts = options("werror");
    assertEq(oldOpts.indexOf("werror"), -1);

    var ret = f.apply(null, Array.slice(arguments, 1));

    // Turn warnings-as-errors back off
    options("werror");

    return ret;
}
