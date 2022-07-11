/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// The path to the top level directory.
const depth = "../../../../../";

loadScript("dom/quota/test/xpcshell/common/head.js");

function loadScript(path) {
  let uri = Services.io.newFileURI(do_get_file(depth + path));
  Services.scriptloader.loadSubScript(uri.spec);
}
