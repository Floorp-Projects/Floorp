// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

try {
    clone(null);  // don't crash
} catch (exc if exc instanceof TypeError) {
}

reportCompare(0, 0, 'ok');
