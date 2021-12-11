/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// See https://bugzilla.mozilla.org/show_bug.cgi?id=1273251

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

function promiseEvent(target, event) {
  return new Promise(resolve => {
    target.addEventListener(event, resolve, {capture: true, once: true});
  });
}

add_task(async function() {
  let principal = Services.scriptSecurityManager
    .createContentPrincipalFromOrigin("http://example.com/");

  let webnav = Services.appShell.createWindowlessBrowser(false);

  let docShell = webnav.docShell;

  docShell.createAboutBlankContentViewer(principal, principal);

  let window = webnav.document.defaultView;
  let sandbox = Cu.Sandbox(window, {sandboxPrototype: window});

  function sandboxContent() {
    window.onload = function SandboxOnLoad() {};

    window.addEventListener("FromTest", () => {
      window.dispatchEvent(new CustomEvent("FromSandbox"));
    }, true);
  }

  Cu.evalInSandbox(`(${sandboxContent})()`, sandbox);


  let fromTestPromise = promiseEvent(window, "FromTest");
  let fromSandboxPromise = promiseEvent(window, "FromSandbox");

  equal(typeof window.onload, "function",
        "window.onload should contain sandbox event listener");
  equal(window.onload.name, "SandboxOnLoad",
        "window.onload have the correct function name");

  info("Dispatch FromTest event");
  window.dispatchEvent(new window.CustomEvent("FromTest"));

  await fromTestPromise;
  info("Got event from test");

  await fromSandboxPromise;
  info("Got response from sandbox");


  window.addEventListener("FromSandbox", () => {
    ok(false, "Got unexpected reply from sandbox");
  }, true);

  info("Nuke sandbox");
  Cu.nukeSandbox(sandbox);


  info("Dispatch FromTest event");
  fromTestPromise = promiseEvent(window, "FromTest");
  window.dispatchEvent(new window.CustomEvent("FromTest"));
  await fromTestPromise;
  info("Got event from test");


  // Force cycle collection, which should cause our callback reference
  // to be dropped, and dredge up potential issues there.
  Cu.forceGC();
  Cu.forceCC();

  ok(Cu.isDeadWrapper(window.onload),
     "window.onload should contain a dead wrapper after sandbox is nuked");

  info("Dispatch FromTest event");
  fromTestPromise = promiseEvent(window, "FromTest");
  window.dispatchEvent(new window.CustomEvent("FromTest"));
  await fromTestPromise;
  info("Got event from test");

  let listeners = Services.els.getListenerInfoFor(window);
  ok(!listeners.some(info => info.type == "FromTest"),
     "No 'FromTest' listeners returned for nuked sandbox");

  webnav.close();
});
