// |jit-test| --enable-experimental-fields

load(libdir + "asserts.js");

source = `var y = {
    x;
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

// This is legal, and is equivalent to `var y = { x: x };`
// source = `var y = {
//     x
// }`;
// assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    #x;
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

// Temporarily disabled due to the same reason above.
// source = `var y = {
//     #x
// }`;
// assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    x = 2;
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

source = `var y = {
    x = 2
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

source = `var y = {
    #x = 2;
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

source = `var y = {
    #x = 2
}`;
assertErrorMessage(() => eval(source), SyntaxError, /./);

if (typeof reportCompare === "function")
  reportCompare(true, true);
