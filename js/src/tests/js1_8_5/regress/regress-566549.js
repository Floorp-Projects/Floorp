// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributors: Jesse Ruderman <jruderman@gmail.com>,
//               Gary Kwong <gary@rumblingedge.com>,
//               Jason Orendorff <jorendorff@mozilla.com>

try {
    evalcx('var p;', []);
} catch (exc) {}

try {
    evalcx('');
    Function("evalcx(\"var p\",[])")();
} catch (exc) {}

try {
    evalcx('var p;', <x/>);
} catch (exc) {}

try {
    evalcx('var p;', <x><p/></x>);
} catch (exc) {}

reportCompare(0, 0, "ok");
