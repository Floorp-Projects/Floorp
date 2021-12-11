// super() invalid outside derived class constructors, including in dynamic
// functions and eval
assertThrowsInstanceOf(() => new Function("super();"), SyntaxError);
assertThrowsInstanceOf(() => eval("super()"), SyntaxError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
