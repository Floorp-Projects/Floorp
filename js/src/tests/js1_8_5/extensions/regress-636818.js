// |reftest| skip-if(!xulRuntime.shell)
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

a = evalcx('');
a.__proto__ = ''.__proto__;
a.length = 3;  // don't assert

reportCompare(0, 0, 'ok');
