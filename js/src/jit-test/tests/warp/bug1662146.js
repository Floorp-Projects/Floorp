var sum = 0;
function f1(a) {
    try {
        not_a_function();
    } catch (e) {
        sum++;
    }
}
for (var i = 0; i < 50; ++i) {
    f1.call();
}
assertEq(sum, 50);
