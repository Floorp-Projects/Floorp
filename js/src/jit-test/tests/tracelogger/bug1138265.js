try {
    (function(b, foreign, p) {
         "use asm"
         var ff = foreign.ff
         function f() {
            ff() | 0
         }
         return f
    })(this, {
        ff: startTraceLogger
    }, ArrayBuffer)()
} catch(e) {
    // Will throw exception if odinmonkey is on top of the stack upon calling startTraceLogger.
    // If there is another frame added in between (ion/baseline/interpreter).
    // This will just run to completion.
}

assertEq(true, true);
