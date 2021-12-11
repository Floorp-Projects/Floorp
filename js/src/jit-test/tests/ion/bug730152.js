if (typeof verifybarriers !== "undefined") {
    for (var i = 0; i < 30; i++) {}
    for (i in Function("gc(verifybarriers()); yield")()) {}
}
