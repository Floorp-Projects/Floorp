/* Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var isOSX = Services.appinfo.OS === "Darwin";

add_task(async function() {
  const shortcuts = new KeyShortcuts({
    window
  });

  await testSimple(shortcuts);
  await testNonLetterCharacter(shortcuts);
  await testPlusCharacter(shortcuts);
  await testFunctionKey(shortcuts);
  await testMixup(shortcuts);
  await testLooseDigits(shortcuts);
  await testExactModifiers(shortcuts);
  await testLooseShiftModifier(shortcuts);
  await testStrictLetterShiftModifier(shortcuts);
  await testAltModifier(shortcuts);
  await testCommandOrControlModifier(shortcuts);
  await testCtrlModifier(shortcuts);
  await testInvalidShortcutString(shortcuts);
  await testCmdShiftShortcut(shortcuts);
  shortcuts.destroy();

  await testTarget();
});

// Test helper to listen to the next key press for a given key,
// returning a promise to help using Tasks.
function once(shortcuts, key, listener) {
  let called = false;
  return new Promise(done => {
    const onShortcut = event => {
      shortcuts.off(key, onShortcut);
      ok(!called, "once listener called only once (i.e. off() works)");
      called = true;
      listener(event);
      done();
    };
    shortcuts.on(key, onShortcut);
  });
}

async function testSimple(shortcuts) {
  info("Test simple key shortcuts");

  const onKey = once(shortcuts, "0", event => {
    is(event.key, "0");

    // Display another key press to ensure that once() correctly stop listening
    EventUtils.synthesizeKey("0", {}, window);
  });

  EventUtils.synthesizeKey("0", {}, window);
  await onKey;
}

async function testNonLetterCharacter(shortcuts) {
  info("Test non-naive character key shortcuts");

  const onKey = once(shortcuts, "[", event => {
    is(event.key, "[");
  });

  EventUtils.synthesizeKey("[", {}, window);
  await onKey;
}

async function testFunctionKey(shortcuts) {
  info("Test function key shortcuts");

  const onKey = once(shortcuts, "F12", event => {
    is(event.key, "F12");
  });

  EventUtils.synthesizeKey("F12", { keyCode: 123 }, window);
  await onKey;
}

// Plus is special. It's keycode is the one for "=". That's because it requires
// shift to be pressed and is behind "=" key. So it should be considered as a
// character key
async function testPlusCharacter(shortcuts) {
  info("Test 'Plus' key shortcuts");

  const onKey = once(shortcuts, "Plus", event => {
    is(event.key, "+");
  });

  EventUtils.synthesizeKey("+", { keyCode: 61, shiftKey: true }, window);
  await onKey;
}

// Test they listeners are not mixed up between shortcuts
async function testMixup(shortcuts) {
  info("Test possible listener mixup");

  let hitFirst = false, hitSecond = false;
  const onFirstKey = once(shortcuts, "0", event => {
    is(event.key, "0");
    hitFirst = true;
  });
  const onSecondKey = once(shortcuts, "Alt+A", event => {
    is(event.key, "a");
    ok(event.altKey);
    hitSecond = true;
  });

  // Dispatch the first shortcut and expect only this one to be notified
  ok(!hitFirst, "First shortcut isn't notified before firing the key event");
  EventUtils.synthesizeKey("0", {}, window);
  await onFirstKey;
  ok(hitFirst, "Got the first shortcut notified");
  ok(!hitSecond, "No mixup, second shortcut is still not notified (1/2)");

  // Wait an extra time, just to be sure this isn't racy
  await new Promise(done => {
    window.setTimeout(done, 0);
  });
  ok(!hitSecond, "No mixup, second shortcut is still not notified (2/2)");

  // Finally dispatch the second shortcut
  EventUtils.synthesizeKey("a", { altKey: true }, window);
  await onSecondKey;
  ok(hitSecond, "Got the second shortcut notified once it is actually fired");
}

// On azerty keyboard, digits are only available by pressing Shift/Capslock,
// but we accept them even if we omit doing that.
async function testLooseDigits(shortcuts) {
  info("Test Loose digits");
  let onKey = once(shortcuts, "0", event => {
    is(event.key, "à");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
  });
  // Simulate a press on the "0" key, without shift pressed on a french
  // keyboard
  EventUtils.synthesizeKey(
    "à",
    { keyCode: 48 },
    window);
  await onKey;

  onKey = once(shortcuts, "0", event => {
    is(event.key, "0");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(event.shiftKey);
  });
  // Simulate the same press with shift pressed
  EventUtils.synthesizeKey(
    "0",
    { keyCode: 48, shiftKey: true },
    window);
  await onKey;
}

// Test that shortcuts is notified only when the modifiers match exactly
async function testExactModifiers(shortcuts) {
  info("Test exact modifiers match");

  let hit = false;
  const onKey = once(shortcuts, "Alt+A", event => {
    is(event.key, "a");
    ok(event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
    hit = true;
  });

  // Dispatch with unexpected set of modifiers
  ok(!hit, "Shortcut isn't notified before firing the key event");
  EventUtils.synthesizeKey("a",
    { accelKey: true, altKey: true, shiftKey: true },
    window);
  EventUtils.synthesizeKey(
    "a",
    { accelKey: true, altKey: false, shiftKey: false },
    window);
  EventUtils.synthesizeKey(
    "a",
    { accelKey: false, altKey: false, shiftKey: true },
    window);
  EventUtils.synthesizeKey(
    "a",
    { accelKey: false, altKey: false, shiftKey: false },
    window);

  // Wait an extra time to let a chance to call the listener
  await new Promise(done => {
    window.setTimeout(done, 0);
  });
  ok(!hit, "Listener isn't called when modifiers aren't exactly matching");

  // Dispatch the expected modifiers
  EventUtils.synthesizeKey("a", { accelKey: false, altKey: true, shiftKey: false},
                           window);
  await onKey;
  ok(hit, "Got shortcut notified once it is actually fired");
}

// Some keys are only accessible via shift and listener should also be called
// even if the key didn't explicitely requested Shift modifier.
// For example, `%` on french keyboards is only accessible via Shift.
// Same thing for `@` on US keybords.
async function testLooseShiftModifier(shortcuts) {
  info("Test Loose shift modifier");
  let onKey = once(shortcuts, "%", event => {
    is(event.key, "%");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(event.shiftKey);
  });
  EventUtils.synthesizeKey(
    "%",
    { accelKey: false, altKey: false, ctrlKey: false, shiftKey: true},
    window);
  await onKey;

  onKey = once(shortcuts, "@", event => {
    is(event.key, "@");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(event.shiftKey);
  });
  EventUtils.synthesizeKey(
    "@",
    { accelKey: false, altKey: false, ctrlKey: false, shiftKey: true},
    window);
  await onKey;
}

// But Shift modifier is strict on all letter characters (a to Z)
async function testStrictLetterShiftModifier(shortcuts) {
  info("Test strict shift modifier on letters");
  let hitFirst = false;
  const onKey = once(shortcuts, "a", event => {
    is(event.key, "a");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
    hitFirst = true;
  });
  const onShiftKey = once(shortcuts, "Shift+a", event => {
    is(event.key, "a");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(event.shiftKey);
  });
  EventUtils.synthesizeKey(
    "a",
    { shiftKey: true},
    window);
  await onShiftKey;
  ok(!hitFirst, "Didn't fire the explicit shift+a");

  EventUtils.synthesizeKey(
    "a",
    { shiftKey: false},
    window);
  await onKey;
}

async function testAltModifier(shortcuts) {
  info("Test Alt modifier");
  const onKey = once(shortcuts, "Alt+F1", event => {
    is(event.keyCode, window.KeyboardEvent.DOM_VK_F1);
    ok(event.altKey);
    ok(!event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
  });
  EventUtils.synthesizeKey(
    "VK_F1",
    { altKey: true },
    window);
  await onKey;
}

async function testCommandOrControlModifier(shortcuts) {
  info("Test CommandOrControl modifier");
  const onKey = once(shortcuts, "CommandOrControl+F1", event => {
    is(event.keyCode, window.KeyboardEvent.DOM_VK_F1);
    ok(!event.altKey);
    if (isOSX) {
      ok(!event.ctrlKey);
      ok(event.metaKey);
    } else {
      ok(event.ctrlKey);
      ok(!event.metaKey);
    }
    ok(!event.shiftKey);
  });
  const onKeyAlias = once(shortcuts, "CmdOrCtrl+F1", event => {
    is(event.keyCode, window.KeyboardEvent.DOM_VK_F1);
    ok(!event.altKey);
    if (isOSX) {
      ok(!event.ctrlKey);
      ok(event.metaKey);
    } else {
      ok(event.ctrlKey);
      ok(!event.metaKey);
    }
    ok(!event.shiftKey);
  });
  if (isOSX) {
    EventUtils.synthesizeKey(
      "VK_F1",
      { metaKey: true },
      window);
  } else {
    EventUtils.synthesizeKey(
      "VK_F1",
      { ctrlKey: true },
      window);
  }
  await onKey;
  await onKeyAlias;
}

async function testCtrlModifier(shortcuts) {
  info("Test Ctrl modifier");
  const onKey = once(shortcuts, "Ctrl+F1", event => {
    is(event.keyCode, window.KeyboardEvent.DOM_VK_F1);
    ok(!event.altKey);
    ok(event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
  });
  const onKeyAlias = once(shortcuts, "Control+F1", event => {
    is(event.keyCode, window.KeyboardEvent.DOM_VK_F1);
    ok(!event.altKey);
    ok(event.ctrlKey);
    ok(!event.metaKey);
    ok(!event.shiftKey);
  });
  EventUtils.synthesizeKey(
    "VK_F1",
    { ctrlKey: true },
    window);
  await onKey;
  await onKeyAlias;
}

async function testCmdShiftShortcut(shortcuts) {
  if (!isOSX) {
    // This test is OSX only (Bug 1300458).
    return;
  }

  const onCmdKey = once(shortcuts, "CmdOrCtrl+[", event => {
    is(event.key, "[");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(event.metaKey);
    ok(!event.shiftKey);
  });
  const onCmdShiftKey = once(shortcuts, "CmdOrCtrl+Shift+[", event => {
    is(event.key, "[");
    ok(!event.altKey);
    ok(!event.ctrlKey);
    ok(event.metaKey);
    ok(event.shiftKey);
  });

  EventUtils.synthesizeKey(
    "[",
    { metaKey: true, shiftKey: true },
    window);
  EventUtils.synthesizeKey(
    "[",
    { metaKey: true },
    window);

  await onCmdKey;
  await onCmdShiftKey;
}

async function testTarget() {
  info("Test KeyShortcuts with target argument");

  const target = document.createElementNS("http://www.w3.org/1999/xhtml",
    "input");
  document.documentElement.appendChild(target);
  target.focus();

  const shortcuts = new KeyShortcuts({
    window,
    target
  });
  const onKey = once(shortcuts, "0", event => {
    is(event.key, "0");
    is(event.target, target);
  });
  EventUtils.synthesizeKey("0", {}, window);
  await onKey;

  target.remove();

  shortcuts.destroy();
}

function testInvalidShortcutString(shortcuts) {
  info("Test wrong shortcut string");

  const shortcut = KeyShortcuts.parseElectronKey(window, "Cmmd+F");
  ok(!shortcut, "Passing a invalid shortcut string should return a null object");

  shortcuts.on("Cmmd+F", function() {});
  ok(true, "on() shouldn't throw when passing invalid shortcut string");
}
