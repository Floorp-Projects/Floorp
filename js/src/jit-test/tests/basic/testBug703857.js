Function.prototype.X = 42;
function ownProperties() {
    var props = {};
    var r = function () {};
    for (var a in r) {
        let (a = function() { for (var r=0;r<6;++r) ++a; }) {
            a();
        }
        props[a] = true;
    }
}
ownProperties();
