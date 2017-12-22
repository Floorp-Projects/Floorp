// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Bob Clary <bclary@bclary.com>

try {
    new {prototype: TypeError.prototype};
} catch (e) {}

reportCompare(0, 0, 'ok');
