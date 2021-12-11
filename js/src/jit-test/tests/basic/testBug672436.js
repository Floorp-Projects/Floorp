try { new Error() } catch (e) {}

const N = 18;

var arr = [];
for (var i = 0; i < N; ++i)
    arr[i] = 'a';
arr[N] = '%';

function inner(i) {
    decodeURI(arr[i]);
}
function outer() {
    for (var i = 0; i <= N; ++i)
        inner(i);
}

var caught = false;
try {
    outer();
} catch (e) {
    caught = true;
}
assertEq(caught, true);

