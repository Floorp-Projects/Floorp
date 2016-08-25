function f1(a, aIs,        // without default parameter
            b=()=>62, bIs, // with default parameter
            // before function body
            c=(assertEq(a(), aIs), assertEq(b(), bIs),
               ()=>63),
            cIs) {
  // before function declarations
  assertEq(a(), 52);
  assertEq(b(), 53);
  assertEq(c(), 55);

  function a() {
    return 52;
  }
  function c() {
    return 54;
  }
  function g() {
    // close here
    assertEq(a(), 52); // after function declaration
    assertEq(b(), 53); // before function declaration
    assertEq(c(), 55); // before last function declaration
  }
  // function declaration after closed
  function b() {
    return 53;
  }

  assertEq(a(), 52); // after function declaration
  assertEq(b(), 53); // after function declaration
  assertEq(c(), 55); // before last function declaration

  g();
  c = ()=>72;
  assertEq(c(), 72); // after assignment in body
  h();
  assertEq(c(), 82); // after assignment in closed function

  function h() {
    assertEq(c(), 72); // after assignment in body
    c = ()=>82;
    assertEq(c(), 82); // after assignment in closed function
  }
  // function re-declaration after closed and assignment
  function c() {
    return 55;
  }
}
f1(()=>42, 42, undefined, 62, undefined, 63);
f1(()=>42, 42, undefined, 62, ()=>44, 44);
f1(()=>42, 42, ()=>43, 43, undefined, 63);
f1(()=>42, 42, ()=>43, 43, ()=>44, 44);

function f2(a, aIs,
            // call before body
            b=(function() { assertEq(a(), aIs); })(),
            // a inside body not accessible from defaults
            c=function() { assertEq(a(), 42); }) {
  function a() {
    return 52;
  }
  function g() {
    // close here
    assertEq(a(), 52);
  }

  assertEq(a(), 52);
  g();
  c();
}
f2(()=>42, 42);

function f3(a, aIs,
            // call before body
            // close here
            b=(function() { assertEq(a(), aIs); })(),
            // a inside body not accessible from defaults
            c=function() { assertEq(a(), 42); }) {
  function a() {
    return 52;
  }

  assertEq(a(), 52);
  c();
}
f3(()=>42, 42);

function f4(a,
            // assignment before body
            b=a=()=>62,
            c=(assertEq(a(), 62)),
            // eval in defaults exprs get own var envs
            d=eval("function a() { return 72; }"),
            e=(assertEq(a(), 62))) {
  function a() {
    return 52;
  }
  function g() {
    // close here
    assertEq(a(), 52);
  }

  assertEq(a(), 52);
  g();
}
f4(()=>42);

function f5(a, b, c, d) {
  // before var/function declarations
  assertEq(a(), 52);
  assertEq(b(), 53);
  assertEq(c(), 54);
  assertEq(d(), 55);

  function g() {
    // before var/function declarations, called after var declarations
    // close here
    assertEq(a(), 52);
    assertEq(b(), 63);
    assertEq(c(), 54);
    assertEq(d(), 65);
  }

  var a, b = ()=>63;
  var c, d = ()=>65;

  // after var declarations, before function declarations
  assertEq(a(), 52);
  assertEq(b(), 63);
  assertEq(c(), 54);
  assertEq(d(), 65);

  function h() {
    // after var declarations, before function declarations
    assertEq(a(), 52);
    assertEq(b(), 63);
    assertEq(c(), 54);
    assertEq(d(), 65);
  }
  function a() {
    return 52;
  }
  function b() {
    return 53;
  }
  function c() {
    return 54;
  }
  function d() {
    return 55;
  }
  function i() {
    // after var/function declarations
    assertEq(a(), 52);
    assertEq(b(), 63);
    assertEq(c(), 54);
    assertEq(d(), 65);
  }

  // after var/function declarations
  assertEq(a(), 52);
  assertEq(b(), 63);
  assertEq(c(), 54);
  assertEq(d(), 65);
  g();
  h();
  i();
}
f5(()=>42, ()=>43, ()=>44, ()=>45);
