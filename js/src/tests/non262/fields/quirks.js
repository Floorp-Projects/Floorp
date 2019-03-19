// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

class C {
    x;;;;
    y
    ;
}

class D {
    x = 5;
    y = (x += 1);
    // TODO: Assert values of x and y
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
