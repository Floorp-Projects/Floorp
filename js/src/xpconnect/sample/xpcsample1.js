
var clazz = Components.classes["xpc.sample_hookerupper.1"];
var iface = Components.interfaces.nsIXPCSample_HookerUpper;
var hookerupper = clazz.createInstance(iface);
hookerupper.createSampleObjectAtGlobalScope("A",5);

dump("A = "+A+"\n");
dump("A.someValue = "+A.someValue+"\n");
A.someValue++ ;
dump("A.someValue = "+A.someValue+"\n");

dump("A.B = "+A.B+"\n");
dump("A.B.someValue = "+A.B.someValue+"\n");
A.B.someValue++ ;
dump("A.B.someValue = "+A.B.someValue+"\n");

dump("A.B.C = "+A.B.C+"\n");
dump("A.B.C.someValue = "+A.B.C.someValue+"\n");
A.B.C.someValue++ ;
dump("A.B.C.someValue = "+A.B.C.someValue+"\n");

