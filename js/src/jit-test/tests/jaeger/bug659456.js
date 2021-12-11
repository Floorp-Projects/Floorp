// |jit-test| error: InternalError
function foo() { return "tracejit,methodjit"; };
function baz(on) {
    foo('bar');
}
eval("\
test();\
function test() {\
  baz(true);\
  test();\
}\
");
