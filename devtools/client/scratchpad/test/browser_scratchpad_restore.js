/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Call the iterator for each item in the list,
   calling the final callback with all the results
   after every iterator call has sent its result */
function asyncMap(items, iterator, callback) {
  const expected = items.length;
  const results = [];

  items.forEach(function(item) {
    iterator(item, function(result) {
      results.push(result);
      if (results.length == expected) {
        callback(results);
      }
    });
  });
}

function test() {
  waitForExplicitFinish();
  testRestore();
}

function testRestore() {
  const states = [
    {
      filename: "testfile",
      text: "test1",
      executionContext: 2
    },
    {
      text: "text2",
      executionContext: 1
    },
    {
      text: "text3",
      executionContext: 1
    }
  ];

  asyncMap(states, function(state, done) {
    // Open some scratchpad windows
    openScratchpad(done, {state: state, noFocus: true});
  }, function(wins) {
    // Then save the windows to session store
    ScratchpadManager.saveOpenWindows();

    // Then get their states
    const session = ScratchpadManager.getSessionState();

    // Then close them
    wins.forEach(function(win) {
      win.close();
    });

    // Clear out session state for next tests
    ScratchpadManager.saveOpenWindows();

    // Then restore them
    const restoredWins = ScratchpadManager.restoreSession(session);

    is(restoredWins.length, 3, "Three scratchad windows restored");

    asyncMap(restoredWins, function(restoredWin, done) {
      openScratchpad(function(aWin) {
        const state = aWin.Scratchpad.getState();
        aWin.close();
        done(state);
      }, {window: restoredWin, noFocus: true});
    }, function(restoredStates) {
      // Then make sure they were restored with the right states
      ok(statesMatch(restoredStates, states),
        "All scratchpad window states restored correctly");

      // Yay, we're done!
      finish();
    });
  });
}

function statesMatch(restoredStates, states) {
  return states.every(function(state) {
    return restoredStates.some(function(restoredState) {
      return state.filename == restoredState.filename
        && state.text == restoredState.text
        && state.executionContext == restoredState.executionContext;
    });
  });
}
