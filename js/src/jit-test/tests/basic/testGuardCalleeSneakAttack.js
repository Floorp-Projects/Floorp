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
   this.m = f;
   return f;
}

var obj = {
   set m(f) {
       if (f())  // Call once to resolve x on the Call object,
                 // for shape consistency. Otherwise loop gets
                 // recorded twice.
           loop(f, true);
   }
};

loop(C.call(obj, false), false);
C.call(obj, true);

checkStats({
    recorderStarted: 1,
    recorderAborted: 0,
    traceCompleted: 2,
    traceTriggered: 4
});
