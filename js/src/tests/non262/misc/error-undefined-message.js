// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertEq(new Error().hasOwnProperty('message'), false);
assertEq(new Error(undefined).hasOwnProperty('message'), false);

reportCompare(0, 0, 'ok');
