/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
// This caused security exceptions in the past, make sure it doesn't!
var myConstructor2 = {}.constructor;

// Try to call a function defined in the imported script.
function importedScriptFunction2() {
}
