// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

try {
    version(4096);  // don't assert
} catch (exc) {
}

try {
    version(-1);  // don't assert
} catch (exc) {
}

reportCompare(0, 0, 'ok');
