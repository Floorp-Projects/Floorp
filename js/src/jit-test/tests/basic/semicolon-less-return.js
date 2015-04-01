// Warning should be shown for expression-like statement after semicolon-less
// return (bug 1005110).

if (options().indexOf("werror") == -1)
  options("werror");

function testWarn(code, lineNumber, columnNumber) {
  var caught = false;
  try {
    eval(code);
  } catch (e) {
    caught = true;
    assertEq(e.message, "unreachable expression after semicolon-less return statement", code);
    assertEq(e.lineNumber, lineNumber);
    assertEq(e.columnNumber, columnNumber);
  }
  assertEq(caught, true, "warning should be caught for " + code);

  caught = false;
  try {
    Reflect.parse(code);
  } catch (e) {
    caught = true;
    assertEq(e.message, "unreachable expression after semicolon-less return statement", code);
  }
  assertEq(caught, true, "warning should be caught for " + code);
}

function testPass(code) {
  var caught = false;
  try {
    eval(code);
  } catch (e) {
    caught = true;
  }
  assertEq(caught, false, "warning should not be caught for " + code);

  caught = false;
  try {
    Reflect.parse(code);
  } catch (e) {
    caught = true;
  }
  assertEq(caught, false, "warning should not be caught for " + code);
}

// not EOL

testPass(`
function f() {
  return (
    1 + 2
  );
}
`);
testPass(`
function f() {
  return;
  1 + 2;
}
`);

// starts expression

// TOK_INC
testWarn(`
function f() {
  var i = 0;
  return
    ++i;
}
`, 5, 4);

// TOK_DEC
testWarn(`
function f() {
  var i = 0;
  return
    --i;
}
`, 5, 4);

// TOK_LB
testWarn(`
function f() {
  return
    [1, 2, 3];
}
`, 4, 4);

// TOK_LC
testWarn(`
function f() {
  return
    {x: 10};
}
`, 4, 4);
testWarn(`
function f() {
  return
  {
    method()
    {
      f();
    }
  };
}
`, 4, 2);

// TOK_LP
testWarn(`
function f() {
  return
    (1 + 2);
}
`, 4, 4);

// TOK_NAME
testWarn(`
function f() {
  return
    f;
}
`, 4, 4);

// TOK_NUMBER
testWarn(`
function f() {
  return
    1 + 2;
}
`, 4, 4);
testWarn(`
function f() {
  return
    .1 + .2;
}
`, 4, 4);

// TOK_STRING
testWarn(`
function f() {
  return
    "foo";
}
`, 4, 4);
testWarn(`
function f() {
  return
    "use struct";
}
`, 4, 4);
testWarn(`
function f() {
  return
    'foo';
}
`, 4, 4);

// TOK_TEMPLATE_HEAD
testWarn(`
function f() {
  return
    \`foo\${1 + 2}\`;
}
`, 4, 4);

// TOK_NO_SUBS_TEMPLATE
testWarn(`
function f() {
  return
    \`foo\`;
}
`, 4, 4);

// TOK_REGEXP
testWarn(`
function f() {
  return
    /foo/;
}
`, 4, 4);

// TOK_TRUE
testWarn(`
function f() {
  return
    true;
}
`, 4, 4);

// TOK_FALSE
testWarn(`
function f() {
  return
    false;
}
`, 4, 4);

// TOK_NULL
testWarn(`
function f() {
  return
    null;
}
`, 4, 4);

// TOK_THIS
testWarn(`
function f() {
  return
    this;
}
`, 4, 4);

// TOK_NEW
testWarn(`
function f() {
  return
    new Array();
}
`, 4, 4);

// TOK_DELETE
testWarn(`
function f() {
  var a = {x: 10};
  return
    delete a.x;
}
`, 5, 4);

// TOK_YIELD
testWarn(`
function* f() {
  return
    yield 1;
}
`, 4, 4);

// TOK_CLASS
testWarn(`
function f() {
  return
    class A { constructor() {} };
}
`, 4, 4);

// TOK_ADD
testWarn(`
function f() {
  return
    +1;
}
`, 4, 4);

// TOK_SUB
testWarn(`
function f() {
  return
    -1;
}
`, 4, 4);

// TOK_NOT
testWarn(`
function f() {
  return
    !1;
}
`, 4, 4);

// TOK_BITNOT
testWarn(`
function f() {
  return
    ~1;
}
`, 4, 4);

// don't start expression

// TOK_EOF
testPass(`
var f = new Function("return\\n");
`);

// TOK_SEMI
testPass(`
function f() {
  return
  ;
}
`);

// TOK_RC
testPass(`
function f() {
  {
    return
  }
}
`);

// TOK_FUNCTION
testPass(`
function f() {
  g();
  return
  function g() {}
}
`);

// TOK_IF
testPass(`
function f() {
  return
  if (true)
    1 + 2;
}
`);

// TOK_ELSE
testPass(`
function f() {
  if (true)
    return
  else
    1 + 2;
}
`);

// TOK_SWITCH
testPass(`
function f() {
  return
  switch (1) {
    case 1:
      break;
  }
}
`);

// TOK_CASE
testPass(`
function f() {
  switch (1) {
    case 0:
      return
    case 1:
      break;
  }
}
`);

// TOK_DEFAULT
testPass(`
function f() {
  switch (1) {
    case 0:
      return
    default:
      break;
  }
}
`);

// TOK_WHILE
testPass(`
function f() {
  return
  while (false)
    1 + 2;
}
`);
testPass(`
function f() {
  do
    return
  while (false);
}
`);

// TOK_DO
testPass(`
function f() {
  return
  do {
    1 + 2;
  } while (false);
}
`);

// TOK_FOR
testPass(`
function f() {
  return
  for (;;) {
    break;
  }
}
`);

// TOK_BREAK
testPass(`
function f() {
  for (;;) {
    return
    break;
  }
}
`);

// TOK_CONTINUE
testPass(`
function f() {
  for (;;) {
    return
    continue;
  }
}
`);

// TOK_VAR
testPass(`
function f() {
  return
  var a = 1;
}
`);

// TOK_CONST
testPass(`
function f() {
  return
  const a = 1;
}
`);

// TOK_WITH
testPass(`
function f() {
  return
  with ({}) {
    1;
  }
}
`);

// TOK_RETURN
testPass(`
function f() {
  return
  return;
}
`);

// TOK_TRY
testPass(`
function f() {
  return
  try {
  } catch (e) {
  }
}
`);

// TOK_THROW
testPass(`
function f() {
  return
  throw 1;
}
`);

// TOK_DEBUGGER
testPass(`
function f() {
  return
  debugger;
}
`);

// TOK_LET
testPass(`
function f() {
  return
  let a = 1;
}
`);

// exceptional case

// It's not possible to distinguish between a label statement and an expression
// starts with identifier, by checking a token next to return.
testWarn(`
function f() {
  return
  a: 1;
}
`, 4, 2);
