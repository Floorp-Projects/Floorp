// NOTE: This only turns on 1.8.5 in shell builds.  The browser requires the
//       futzing in js/src/tests/browser.js (which only turns on 1.8, the most
//       the browser supports).
if (typeof version != 'undefined')
  version(185);

function assertThrownErrorContains(thunk, substr) {
    try {
        thunk();
    } catch (e) {
        if (e.message.indexOf(substr) !== -1)
            return;
        throw new Error("Expected error containing " + substr + ", got " + e);
    }
    throw new Error("Expected error containing " + substr + ", no exception thrown");
}
