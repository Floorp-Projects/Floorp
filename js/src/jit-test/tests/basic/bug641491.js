function f1() {
}
function f2() {
}
function f3(o) {
    f2 = XML.prototype;
}
var key = Object.getOwnPropertyNames(f1)[30];
if(key) {
    f3 = f1[key];
}
gc(); 
gc();
try {
for(var i=0; i<10; i++) {
    delete f2[1];
    f3(function() {});
}
} catch (e) {}
