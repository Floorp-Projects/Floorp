/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = [
  "onSpellCheck",
];

const SPELL_CHECK_ENDED_TOPIC = "inlineSpellChecker-spellCheck-ended";
const SPELL_CHECK_STARTED_TOPIC = "inlineSpellChecker-spellCheck-started";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

/**
 * Waits until spell checking has stopped on the given element.
 *
 * When a spell check is pending, this waits indefinitely until the spell check
 * ends.  When a spell check is not pending, it waits a small number of turns of
 * the event loop: if a spell check begins, it resumes waiting indefinitely for
 * the end, and otherwise it stops waiting and calls the callback.
 *
 * This this can therefore trap spell checks that have not started at the time
 * of calling, spell checks that have already started, multiple consecutive
 * spell checks, and the absence of spell checks altogether.
 *
 * @param editableElement  The element being spell checked.
 * @param callback         Called when spell check has completed or enough turns
 *                         of the event loop have passed to determine it has not
 *                         started.
 */
function onSpellCheck(editableElement, callback) {
  let editor = editableElement.editor;
  if (!editor) {
    let win = editableElement.ownerGlobal;
    editor = win.QueryInterface(Ci.nsIInterfaceRequestor).
                 getInterface(Ci.nsIWebNavigation).
                 QueryInterface(Ci.nsIInterfaceRequestor).
                 getInterface(Ci.nsIEditingSession).
                 getEditorForWindow(win);
  }
  if (!editor)
    throw new Error("Unable to find editor for element " + editableElement);

  try {
    // False is important here.  Pass false so that the inline spell checker
    // isn't created if it doesn't already exist.
    var isc = editor.getInlineSpellChecker(false);
  }
  catch (err) {
    // getInlineSpellChecker throws if spell checking is not enabled instead of
    // just returning null, which seems kind of lame.  (Spell checking is not
    // enabled on Android.)  The point here is only to determine whether spell
    // check is pending, and if getInlineSpellChecker throws, then it's not
    // pending.
  }
  let waitingForEnded = isc && isc.spellCheckPending;
  let count = 0;

  function observe(subj, topic, data) {
    if (subj != editor)
      return;
    count = 0;
    let expectedTopic = waitingForEnded ? SPELL_CHECK_ENDED_TOPIC :
                        SPELL_CHECK_STARTED_TOPIC;
    if (topic != expectedTopic)
      Cu.reportError("Expected " + expectedTopic + " but got " + topic + "!");
    waitingForEnded = !waitingForEnded;
  }

  let os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.addObserver(observe, SPELL_CHECK_STARTED_TOPIC, false);
  os.addObserver(observe, SPELL_CHECK_ENDED_TOPIC, false);

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.init(function tick() {
    // Wait an arbitrarily large number -- 50 -- turns of the event loop before
    // declaring that no spell checks will start.
    if (waitingForEnded || ++count < 50)
      return;
    timer.cancel();
    os.removeObserver(observe, SPELL_CHECK_STARTED_TOPIC);
    os.removeObserver(observe, SPELL_CHECK_ENDED_TOPIC);
    callback();
  }, 0, Ci.nsITimer.TYPE_REPEATING_SLACK);
};
