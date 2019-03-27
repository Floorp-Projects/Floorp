// |reftest| skip-if(!xulRuntime.shell) -- preventExtensions on global
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * Contributors: Jan de Mooij
 */

Object.preventExtensions(this);
delete Function;

try {
    /* Don't assert. */
    Object.getOwnPropertyNames(this);
} catch(e) {
    reportCompare(true, false, "this shouldn't have thrown");
}
reportCompare(0, 0, "ok");

