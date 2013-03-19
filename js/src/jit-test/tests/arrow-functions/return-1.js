// return from a block function works when there is no other enclosing function

var f = a => {
    if (a)
        return a + 1;
    throw "FAIL";
};

assertEq(f(1), 2);
