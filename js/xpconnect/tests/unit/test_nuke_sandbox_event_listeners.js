/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1273251

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

function promiseEvent(target, event) {
  return new Promise(resolve => {
    target.addEventListener(event, resolve, {capture: true, once: true});
  });
}

add_task(async function() {
  let principal = Services.scriptSecurityManager
    .createCodebasePrincipalFromOrigin("http://example.com/");

  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDocShell);

  docShell.createAboutBlankContentViewer(principal);

  let window = webnav.document.defaultView;
  let sandbox = Cu.Sandbox(window, {sandboxPrototype: window});

  function sandboxContent() {
    window.addEventListener("FromTest", () => {
      window.dispatchEvent(new CustomEvent("FromSandbox"));
    }, true);
  }

  Cu.evalInSandbox(`(${sandboxContent})()`, sandbox);


  let fromTestPromise = promiseEvent(window, "FromTest");
  let fromSandboxPromise = promiseEvent(window, "FromSandbox");

  do_print("Dispatch FromTest event");
  window.dispatchEvent(new window.CustomEvent("FromTest"));

  await fromTestPromise;
  do_print("Got event from test");

  await fromSandboxPromise;
  do_print("Got response from sandbox");


  window.addEventListener("FromSandbox", () => {
    ok(false, "Got unexpected reply from sandbox");
  }, true);

  do_print("Nuke sandbox");
  Cu.nukeSandbox(sandbox);


  do_print("Dispatch FromTest event");
  fromTestPromise = promiseEvent(window, "FromTest");
  window.dispatchEvent(new window.CustomEvent("FromTest"));
  await fromTestPromise;
  do_print("Got event from test");


  // Force cycle collection, which should cause our callback reference
  // to be dropped, and dredge up potential issues there.
  Cu.forceGC();
  Cu.forceCC();


  do_print("Dispatch FromTest event");
  fromTestPromise = promiseEvent(window, "FromTest");
  window.dispatchEvent(new window.CustomEvent("FromTest"));
  await fromTestPromise;
  do_print("Got event from test");

  let listeners = Services.els.getListenerInfoFor(window);
  ok(!listeners.some(info => info.type == "FromTest"),
     "No 'FromTest' listeners returned for nuked sandbox");

  webnav.close();
});
