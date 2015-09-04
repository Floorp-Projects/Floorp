// Test errors due to duplicate lexically declared names.

load(libdir + "asserts.js");

function testNoError(source) {
    parseModule(source);
}

function testParseError(source, expectedError) {
    print(source);
    assertThrowsInstanceOf(function () {
        parseModule(source);
    }, expectedError);
}

function testSyntaxError(source) {
    testParseError(source, SyntaxError);
}

function testTypeError(source) {
    testParseError(source, TypeError);
}

testNoError("import { a } from 'm';");
testNoError("import { a as b } from 'm';");
testNoError("import * as a from 'm';");
testNoError("import a from 'm';");

// TODO: The spec says redeclaration is a syntax error but we report it as a
// type error.
testTypeError("import { a } from 'm'; let a = 1;");
testTypeError("let a = 1; import { a } from 'm';");
testTypeError("import { a } from 'm'; var a = 1;");
testTypeError("var a = 1; import { a } from 'm';");
testTypeError("import { a, b } from 'm'; const b = 1;");
testTypeError("import { a } from 'm'; import { a } from 'm2';");
testTypeError("import { a } from 'm'; import { b as a } from 'm2';");
testTypeError("import { a } from 'm'; import * as a from 'm2';");
testTypeError("import { a } from 'm'; import a from 'm2';");

