// test case...

var NS_ISUPPORTS_IID    = new nsID("{00000000-0000-0000-c000-000000000046}");
var NS_ITESTXPC_FOO_IID = new nsID("{159E36D0-991E-11d2-AC3F-00C09300144B}");

var baz = foo.QueryInterface(NS_ITESTXPC_FOO_IID);
print("baz = "+baz);
print("distinct wrapper test "+ (foo != baz ? "passed" : "failed"));
var baz2 = foo.QueryInterface(NS_ITESTXPC_FOO_IID);
print("shared wrapper test "+ (baz == baz2 ? "passed" : "failed"));
print("root wrapper identity test "+ 
    (foo.QueryInterface(NS_ISUPPORTS_IID) == 
     baz.QueryInterface(NS_ISUPPORTS_IID) ? 
        "passed" : "failed"));

print("foo = "+foo);
foo.toString = new Function("return 'foo toString called';")
foo.toStr = new Function("return 'foo toStr called';")
print("foo = "+foo);
print("foo.toString() = "+foo.toString());
print("foo.toStr() = "+foo.toStr());
print("foo.five = "+ foo.five);
print("foo.six = "+ foo.six);
print("foo.bogus = "+ foo.bogus);
print("setting bogus explicitly to '5'...");
foo.bogus = 5;
print("foo.bogus = "+ foo.bogus);
print("foo.Test(10,20) returned: "+foo.Test(10,20));

function _Test(p1, p2)
{
    print("test called in JS with p1 = "+p1+" and p2 = "+p2);
    return p1+p2;
}

function _QI(iid)
{
    print("QueryInterface called in JS with iid = "+iid); 
    return  this;
}

print("creating bar");
bar = new Object();
bar.Test = _Test;
bar.QueryInterface = _QI;
// this 'bar' object is accessed from native code after this script is run

print("foo properties:");
for(i in foo)
    print("  foo."+i+" = "+foo[i]);

