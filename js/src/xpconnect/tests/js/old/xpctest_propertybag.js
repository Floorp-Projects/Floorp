
const nsIVariant = Components.interfaces.nsIVariant;
const nsIProperty = Components.interfaces.nsIProperty;

const TestVariant = Components.Constructor("@mozilla.org/js/xpc/test/TestVariant;1", 
                                           "nsITestVariant");

var tv = new TestVariant;

var obj = {foo : "fooString", 
           five : "5", 
           bar : {}, 
           6 : "six",
           fun : function(){},
           bignum : 1.2345678901234567890,
           now : new Date().toString() };

print();
print(tv.getNamedProperty(obj, "foo"));
print(tv.getNamedProperty(obj, "five"));
print(tv.getNamedProperty(obj, "bar"));
print(tv.getNamedProperty(obj, 6));
print(tv.getNamedProperty(obj, "fun"));
print(tv.getNamedProperty(obj, "fun"));
print(tv.getNamedProperty(obj, "bignum"));
print(tv.getNamedProperty(obj, "now"));
print();

var e = tv.getEnumerator(obj);

while(e.hasMoreElements()) {
    var prop = e.getNext().QueryInterface(nsIProperty)
    print(prop.name+" = "+prop.value);
}

print();
