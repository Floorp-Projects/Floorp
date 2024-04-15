// |jit-test| --more-compartments

var threw = false;
try {
    const v2 = evalcx("lazy");
    const o4 = {
        "global": v2,
    };
    o4.envChainObject = v2;
    evaluate("{ let eval = parseInt; eval()}", o4);
} catch (e) {
    threw = true;
    assertEq(e.toString().includes("envChainObject"), true);
}
assertEq(threw, true);
