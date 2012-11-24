var callees = [function a() {}, function b() {}, function c() {}, function d() {}, Array.prototype.forEach];

function f() {
    for (var i = 0; i < callees.length; ++i) {
    	callees[i](function(){});
    }
}

f();