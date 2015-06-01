// new.target is valid inside Function() invocations
var func = new Function("new.target");

// new.target is invalid inside eval, even (for now!) eval inside a function.
assertThrowsInstanceOf(() => eval('new.target'), SyntaxError);

function evalInFunction() { eval('new.target'); }
assertThrowsInstanceOf(() => evalInFunction(), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
