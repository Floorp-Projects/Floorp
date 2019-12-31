/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

Cu.import("resource://gre/modules/Services.jsm");

function loadSubscript(path) {
  let file = do_get_file(path, false);
  let uri = Services.io.newFileURI(file);
  Services.scriptloader.loadSubScript(uri.spec);
}

loadSubscript("../head-shared.js");
loadSubscript("head-shared.js");
