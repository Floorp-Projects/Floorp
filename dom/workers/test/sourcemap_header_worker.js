/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Send a message pack so the test knows that the source has loaded before
// it tries to search for the Debugger.Source.
postMessage("loaded");
