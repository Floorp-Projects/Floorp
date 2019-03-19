// |jit-test| --enable-experimental-fields

load(libdir + "asserts.js");

let source = `class C {
    x
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    -2;
    -2 = 2;
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    x += 2;
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    #2;
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    #["h" + "i"];
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    #"hi";
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `function f() {
class C {
    #"should still throw error during lazy parse";
}
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

// TODO
//source = `#outside;`;
//assertErrorMessage(() => eval(source), SyntaxError);

source = `class C {
    x = super();
}`;
assertErrorMessage(() => Function(source), SyntaxError, /./);

source = `class C {
    x = sper();
}`;
eval(source);

if (typeof reportCompare === "function")
  reportCompare(true, true);
