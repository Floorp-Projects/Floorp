/*
 * Annotated Definitions
 */

const showerrors = false;


function title() {
    return "Annotated Definitions";
}

class U { }
class T { static var d:U; }

T::d = new U;

function run() {
    var x:T;
    global internal var y;
}

extend(U) static var PI = 3.1415;
showerrors extend(U) var RHO = 10;
extend(U) const LAMBDA = 12;
static var NOT = 0;

function makeLiter(x,y=0) {
    return Unit.dm.pow(3)(x,y);
}

unit const liter = makeLiter;
showerrors unit const liter = Unit.dm.pow(3);

/*
 * Copyright (c) 1999-2001, Mountain View Compiler Company. All rights reserved.
 */
