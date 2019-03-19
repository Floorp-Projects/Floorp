// * * * THIS TEST IS DISABLED - Fields are not fully implemented yet

class C {
    x;
    y = 2;
}

class D {
    #x;
    #y = 2;
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
