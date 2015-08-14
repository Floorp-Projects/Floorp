var a = {};
a[2 ** 53 - 2] = 1;
a.length = 2 ** 53 - 1;

var exception;
try {
    [].splice.call(a, 2 ** 53 - 2, 0, 2, 3, 4, 5);
} catch (e) {
    exception = e;
}
reportCompare(a[2 ** 53 - 2], 1);
reportCompare(a.length, 2 ** 53 - 1);
reportCompare(exception instanceof TypeError, true, "Array#splice throws TypeError for length overflows");
reportCompare(exception.message.indexOf('array length') > -1, true, "Array#splice throws correct error for length overflows");
