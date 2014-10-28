var JSMSG_ILLEGAL_CHARACTER = "illegal character";

function test_reflect(code) {
  var caught = false;
  try {
    Reflect.parse(code);
  } catch (e) {
    caught = true;
    assertEq(e instanceof SyntaxError, true, code);
    assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, code);
  }
  assertEq(caught, true);
}

function test_eval(code) {
  var caught = false;
  try {
    eval(code);
  } catch (e) {
    caught = true;
    assertEq(e instanceof SyntaxError, true, code);
    assertEq(e.message, JSMSG_ILLEGAL_CHARACTER, code);
  }
  assertEq(caught, true);
}


function test(code) {
  test_reflect(code);
  test_eval(code);
}

test("(function() { 'use asm'; @");
test("(function() { 'use asm'; var @");
test("(function() { 'use asm'; var a @");
test("(function() { 'use asm'; var a = @");
test("(function() { 'use asm'; var a = 1 @");
test("(function() { 'use asm'; var a = 1; @");
test("(function() { 'use asm'; var a = 1; function @");
test("(function() { 'use asm'; var a = 1; function f @");
test("(function() { 'use asm'; var a = 1; function f( @");
test("(function() { 'use asm'; var a = 1; function f() @");
test("(function() { 'use asm'; var a = 1; function f() { @");
test("(function() { 'use asm'; var a = 1; function f() { } @");
test("(function() { 'use asm'; var a = 1; function f() { } var @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [ @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f] @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; } @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; }) @");
test("(function() { 'use asm'; var a = 1; function f() { } var tbl = [f]; return f; }); @");
