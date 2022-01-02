// |jit-test| error: TypeError
this.__defineSetter__("x", [].map);
evaluate('[x]="";');
