// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var obj = {};
try {
    obj.watch(QName(), function () {});
} catch (exc) {
}
gc();

reportCompare(0, 0, 'ok');
