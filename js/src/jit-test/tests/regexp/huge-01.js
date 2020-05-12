
function g(N, p) {
    var prefix = p.repeat(N);
    var str = prefix + "[AB]";

    try {
        var re = new RegExp(str);
        re.exec(prefix + "A");
    } catch(e) {
        // 1. Regexp too big is raised by the lack of the 64k virtual registers
        // reserved for the regexp evaluation.
        // 2. The stack overflow can occur during the analysis of the regexp
        assertEq(e.message.includes("regexp too big") || e.message.includes("Stack overflow"), true);
    }
}

var prefix = "/(?=k)ok/";
for (var i = 0; i < 18; i++)
    g(Math.pow(2, i), prefix)
