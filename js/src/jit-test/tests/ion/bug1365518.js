function init() {
    foo = () => 1;
    bar = () => 2;
    foo.__proto__ = function() {};
}
function test() {
    var arr = [foo, bar];
    for (var i = 0; i < 1300; i++) {
        assertEq(arr[i % 2](), i % 2 + 1);
    }
}
init();
test();
