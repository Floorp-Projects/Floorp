// |jit-test| error: TypeError
gczeal(4);
var g_rx = /(?:)/;
(3).replace(g_rx.compile("test", "g"), {});

