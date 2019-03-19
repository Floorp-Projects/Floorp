// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

let source = `class C {
    x
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    -2;
    -2 = 2;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    x += 2;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    #2;
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    #["h" + "i"];
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    #"hi";
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `function f() {
class C {
    #"should still throw error during lazy parse";
}
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `#outside;`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `class C {
    x = super();
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

source = `class C {
    x = sper();
}`;
assertThrowsInstanceOf(() => Function(source), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
