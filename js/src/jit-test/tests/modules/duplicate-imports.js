// Test errors due to duplicate lexically declared names.

load(libdir + "asserts.js");

function testNoError(source) {
    parseModule(source);
}

function testSyntaxError(source) {
    assertThrowsInstanceOf(() => parseModule(source), SyntaxError);
}

testNoError("import { a } from 'm';");
testNoError("import { a as b } from 'm';");
testNoError("import * as a from 'm';");
testNoError("import a from 'm';");

testSyntaxError("import { a } from 'm'; let a = 1;");
testSyntaxError("let a = 1; import { a } from 'm';");
testSyntaxError("import { a } from 'm'; var a = 1;");
testSyntaxError("var a = 1; import { a } from 'm';");
testSyntaxError("import { a, b } from 'm'; const b = 1;");
testSyntaxError("import { a } from 'm'; import { a } from 'm2';");
testSyntaxError("import { a } from 'm'; import { b as a } from 'm2';");
testSyntaxError("import { a } from 'm'; import * as a from 'm2';");
testSyntaxError("import { a } from 'm'; import a from 'm2';");

