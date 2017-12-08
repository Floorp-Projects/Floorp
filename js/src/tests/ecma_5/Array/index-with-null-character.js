/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var testArray = [1, 2, 3]
assertEq(testArray['0' + '\0'], undefined);
assertEq(testArray['1' + '\0' + 'aaaa'], undefined)
assertEq(testArray['\0' + '2'], undefined);
assertEq(testArray['\0' + ' 2'], undefined);

testArray['\0'] = 'hello';
testArray[' \0'] = 'world';
assertEq(testArray['\0'], 'hello');
assertEq(testArray[' \0'], 'world');

if (typeof reportCompare == 'function')
    reportCompare(true, true);
