// Note: These tests should pass eventually (and this file deleted): this test
// is just asserting that fields don't crash the engine, even if disabled.

let source = `class C {
    x
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    x = 0;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    0 = 0;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    [0] = 0;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    "hi" = 0;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    "hi" = 0;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    d = function(){};
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    d = class D { x = 0; };
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
