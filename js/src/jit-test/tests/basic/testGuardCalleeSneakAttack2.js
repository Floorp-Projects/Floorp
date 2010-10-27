function loop(f, expected) {
   // This is the loop that breaks us.
   // At record time, f's parent is a Call object with no fp.
   // At second execute time, it is a Call object with fp,
   // and all the Call object's dslots are still JSVAL_VOID.
   for (var i = 0; i < 9; i++)
       assertEq(f(), expected);
}

function C(bad) {
   var x = bad;
   function f() {
       return x;  // We trick TR::callProp() into emitting code that gets
                  // JSVAL_VOID (from the Call object's dslots)
                  // rather than the actual value (true or false).
   }
   if (bad)
       void (f + "a!");
   return f;
}

var obj = {
};

// Warm up and trace with C's Call object entrained but its stack frame gone.
loop(C.call(obj, false), false);

// Sneaky access to f via a prototype method called implicitly by operator +.
Function.prototype.toString = function () { loop(this, true); return "hah"; };

// Fail hard if we don't handle the implicit call out of C to F.p.toString.
C.call(obj, true);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 4
});
