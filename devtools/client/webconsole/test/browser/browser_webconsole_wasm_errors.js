/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that WASM errors are reported to the console.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Wasm errors`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const onCompileError = waitForMessageByType(
    hud,
    `Uncaught (in promise) CompileError: wasm validation error: at offset 0: failed to match magic number`,
    ".error"
  );
  execute(hud, `WebAssembly.instantiate(new Uint8Array())`);
  await onCompileError;
  ok(true, "The expected error message is displayed for CompileError");

  const onLinkError = waitForMessageByType(
    hud,
    `Uncaught (in promise) LinkError: import object field 'f' is not a Function`,
    ".error"
  );
  execute(
    hud,
    `WebAssembly.instantiate(
      new Uint8Array([0,97,115,109,1,0,0,0,1,4,1,96,0,0,2,7,1,1,109,1,102,0,0]),
      { m: { f: 3 } }
    )`
  );
  await onLinkError;
  ok(true, "The expected error message is displayed for LinkError");

  const onRuntimeError = waitForMessageByType(
    hud,
    "Uncaught RuntimeError: unreachable executed",
    ".error"
  );
  execute(
    hud,
    `
    const uintArray = new Uint8Array([0,97,115,109,1,0,0,0,1,4,1,96,0,0,3,2,1,0,7,7,1,3,114,117,110,0,0,10,5,1,3,0,0,11]);
    const module = new WebAssembly.Module(uintArray);
    new WebAssembly.Instance(module).exports.run()`
  );
  await onRuntimeError;
  ok(true, "The expected error message is displayed for RuntimeError");
});
