load(libdir + "asserts.js");

assertThrowsInstanceOf(
    () => Function(`
for (let {
    [
        function(x) {;
        }
    ]: {}
} in 0
`),
    SyntaxError)
