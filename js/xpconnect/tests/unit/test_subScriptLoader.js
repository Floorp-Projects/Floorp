"use strict";

add_task(async function test_executeScriptAfterNuked() {
  let scriptUrl = Services.io.newFileURI(do_get_file("file_simple_script.js")).spec;

  // Load the script for the first time into a sandbox, and then nuke
  // that sandbox.
  let sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  Services.scriptloader.loadSubScript(scriptUrl, sandbox);
  Cu.nukeSandbox(sandbox);

  // Load the script again into a new sandbox, and make sure it
  // succeeds.
  sandbox = Cu.Sandbox(Services.scriptSecurityManager.getSystemPrincipal());
  Services.scriptloader.loadSubScript(scriptUrl, sandbox);
});
