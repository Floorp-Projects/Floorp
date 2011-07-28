// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

var obj = {};
obj.watch(-1, function(){});
obj.unwatch("-1");  // don't assert

reportCompare(0, 0, 'ok');