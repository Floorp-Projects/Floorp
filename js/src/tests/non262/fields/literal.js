// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

source = `var y = {
    x;
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

// This is legal, and is equivalent to `var y = { x: x };`
// source = `var y = {
//     x
// }`;
// assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    #x;
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

// Temporarily disabled due to the same reason above.
// source = `var y = {
//     #x
// }`;
// assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    x = 2;
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    x = 2
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    #x = 2;
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

source = `var y = {
    #x = 2
}`;
assertThrowsInstanceOf(() => eval(source), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
