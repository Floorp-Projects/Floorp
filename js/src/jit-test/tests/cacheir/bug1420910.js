// Testing InstanceOf IC.

Array.prototype.sum = function() {
    return this.reduce(( acc, cur ) => acc + cur, 0);
}


Iters = 20;

function resultArray(fn, obj) {
    res = new Array();
    for (var x = 0; x < Iters; x++) {
        res.push(fn(obj) ? 1 : 0);
    }
    return res;
}

// Ensure alteration of .prototype invalidates IC
function basic() {};

protoA = { prop1: "1"};
basic.prototype = protoA;

io1 = x => { return x instanceof basic; }

var x = new basic();
beforePrototypeModification = resultArray(io1,x).sum(); //Attach and test IC
assertEq(beforePrototypeModification,Iters);

basic.prototype = {}; // Invalidate IC
afterPrototypeModification  = resultArray(io1,x).sum(); //Test
assertEq(afterPrototypeModification,0);

//Primitive LHS returns false.
assertEq(resultArray(io1,0).sum(),0);