var BUGNUMBER = 1352429;
var summary = 'Error message should provide enough infomation for use of in operator';

print(BUGNUMBER + ": " + summary);

function checkErr(substr, str, messageSubstr, messageStr) {
    var caught = false;
    try {
        substr in str;
    } catch (e) {
        caught = true;
        assertEq(e.message.includes(messageSubstr), true);
        assertEq(e.message.includes(messageStr), true);
        assertEq(e.message.length < 100, true);
    }
    assertEq(caught, true);
}

// These test cases check if long string is omitted properly.
checkErr('subString', 'base', 'subString', 'base');
checkErr('this is subString', 'base', 'this is subStrin...', 'base');
checkErr('subString', 'this is baseString', 'subString', 'this is baseStri...');
checkErr('this is subString', 'this is base', 'this is subStrin...', 'this is base');
checkErr('HEAD' + 'subString'.repeat(30000), 'HEAD' + 'base'.repeat(30000), 'HEADsubStringsub...', 'HEADbasebasebase...');

// These test cases check if it does not crash and throws appropriate error.
assertThrowsInstanceOf(() => { 1 in 'hello' }, TypeError);
assertThrowsInstanceOf(() => { 'hello' in 1 }, TypeError);
assertThrowsInstanceOf(() => { 'hello' in null }, TypeError);
assertThrowsInstanceOf(() => { null in 'hello' }, TypeError);
assertThrowsInstanceOf(() => { null in null }, TypeError);
assertThrowsInstanceOf(() => { 'hello' in true }, TypeError);
assertThrowsInstanceOf(() => { false in 1.1 }, TypeError);
assertThrowsInstanceOf(() => { Symbol.iterator in undefined }, TypeError);
assertThrowsInstanceOf(() => { [] in undefined }, TypeError);
assertThrowsInstanceOf(() => { /a/ in 'hello' }, TypeError);
var str = 'hello';
assertThrowsInstanceOf(() => { str in 'hello' }, TypeError);
class A {};
assertThrowsInstanceOf(() => { new A() in undefined }, TypeError);
var a = new A();
a.b = 1.1;
assertThrowsInstanceOf(() => { a.b in 1.1 }, TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0, 0);
