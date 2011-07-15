var y = 7;

switch (function () { with ({}) return y; }()) {
case 7:
    let y;
    break;
default:
    throw 'FAIL';
}
