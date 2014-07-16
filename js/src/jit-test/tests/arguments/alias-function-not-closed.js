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
  function b() {
    return 53;
  }
  function c() {
    return 54;
  }

  assertEq(a(), 52); // after function declaration
  assertEq(b(), 53); // after function declaration
  assertEq(c(), 55); // before last function declaration

  c = ()=>72;
  assertEq(c(), 72); // after assignment in body

  // function re-declaration after assignment
  function c() {
    return 55;
  }
}
f1(()=>42, 42, undefined, 62, undefined, 63);
f1(()=>42, 42, undefined, 62, ()=>44, 44);
f1(()=>42, 42, ()=>43, 43, undefined, 63);
f1(()=>42, 42, ()=>43, 43, ()=>44, 44);

function f2(a,
            // assignment before body
            b=a=()=>62,
            c=(assertEq(a(), 62)),
            // function declaration before body
            d=eval("function a() { return 72; }"),
            e=(assertEq(a(), 72))) {
  function a() {
    return 52;
  }

  assertEq(a(), 52);
}
f2(()=>42);

function f3(a, b, c, d) {
  // before var/function declarations
  assertEq(a(), 52);
  assertEq(b(), 53);
  assertEq(c(), 54);
  assertEq(d(), 55);

  var a, b = ()=>63;
  let c, d = ()=>65;

  // after var declarations, before function declarations
  assertEq(a(), 52);
  assertEq(b(), 63);
  assertEq(c(), 54);
  assertEq(d(), 65);

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

  // after var/function declarations
  assertEq(a(), 52);
  assertEq(b(), 63);
  assertEq(c(), 54);
  assertEq(d(), 65);
}
f3(()=>42, ()=>43, ()=>44, ()=>45);
