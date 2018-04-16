// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
"use strict"

// This function runs a number of tests where:
//
//  1. An iframe is created
//  2. The target callback is executed with the iframe's contentWindow as
//     an argument.
//  3. The iframe is destroyed and GC is forced.
//  4. Verifies that the iframe's contentWindow has been GC'd.
//
// Different ways of destroying the iframe are checked.  Simple
// remove(), destruction via bfcache, or replacement by document.open().
//
// Please pass a target callback that exercises the API under
// test using the given window.  The callback should try to leave the
// API active to increase the liklihood of provoking an API.  Any activity
// should be canceled by the destruction of the window.
async function checkForEventListenerLeaks(name, target) {
  // Test if we leak in the case where we do nothing special to
  // the frame before removing it from the DOM.
  await _eventListenerLeakStep(target, `${name} default`);

  // Test the case where we navigate the frame before removing it
  // from the DOM so that the window using the target API ends up
  // in bfcache.
  await _eventListenerLeakStep(target, `${name} bfcache`, frame => {
    frame.src = "about:blank";
    return new Promise(resolve => frame.onload = resolve);
  });

  // Test the case where we document.open() the frame before removing
  // it from the DOM so that the window using the target API ends
  // up getting replaced.
  await _eventListenerLeakStep(target, `${name} document.open()`, frame => {
    frame.contentDocument.open();
    frame.contentDocument.close();
  });
}

// ----------------
// Internal helpers
// ----------------

// Utility function to create a loaded iframe.
async function _withFrame(doc, url) {
  let frame = doc.createElement('iframe');
  frame.src = url;
  doc.body.appendChild(frame);
  await new Promise(resolve => frame.onload = resolve);
  return frame;
}

// This function defines the basic form of the test cases.  We create an
// iframe, execute the target callback to manipulate the DOM, remove the frame
// from the DOM, and then check to see if the frame was GC'd.  The caller
// may optionally pass in a callback that will be executed with the
// frame as an argument before removing it from the DOM.
async function _eventListenerLeakStep(target, name, extra) {
  let frame = await _withFrame(document, "empty.html");

  await target(frame.contentWindow);

  let weakRef = SpecialPowers.Cu.getWeakReference(frame.contentWindow);
  ok(weakRef.get(), `should be able to create a weak reference - ${name}`);

  if (extra) {
    await extra(frame);
  }

  frame.remove();
  frame = null;

  // Perform many GC's to avoid intermittent delayed collection.
  await new Promise(resolve => SpecialPowers.exactGC(resolve));
  await new Promise(resolve => SpecialPowers.exactGC(resolve));
  await new Promise(resolve => SpecialPowers.exactGC(resolve));

  ok(!weakRef.get(), `iframe content window should be garbage collected - ${name}`);
}

