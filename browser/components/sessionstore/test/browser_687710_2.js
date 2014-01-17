/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the fix for bug 687710 isn't too aggressive -- shentries which are
// cousins should be able to share bfcache entries.

let stateBackup = ss.getBrowserState();

let state = {entries:[
  {
    docIdentifier: 1,
    url: "http://example.com?1",
    children: [{ docIdentifier: 10,
                 url: "http://example.com?10" }]
  },
  {
    docIdentifier: 1,
    url: "http://example.com?1#a",
    children: [{ docIdentifier: 10,
                 url: "http://example.com?10#aa" }]
  }
]};

function test()
{
  waitForExplicitFinish();

  registerCleanupFunction(function () {
    ss.setBrowserState(stateBackup);
  });

  let tab = gBrowser.addTab("about:blank");
  waitForTabState(tab, state, function () {
    let history = tab.linkedBrowser.webNavigation.sessionHistory;

    is(history.count, 2, "history.count");
    for (let i = 0; i < history.count; i++) {
      for (let j = 0; j < history.count; j++) {
        compareEntries(i, j, history);
      }
    }

    finish();
  });
}

function compareEntries(i, j, history)
{
  let e1 = history.getEntryAtIndex(i, false)
                  .QueryInterface(Ci.nsISHEntry)
                  .QueryInterface(Ci.nsISHContainer);

  let e2 = history.getEntryAtIndex(j, false)
                  .QueryInterface(Ci.nsISHEntry)
                  .QueryInterface(Ci.nsISHContainer);

  ok(e1.sharesDocumentWith(e2),
     i + ' should share doc with ' + j);
  is(e1.childCount, e2.childCount,
     'Child count mismatch (' + i + ', ' + j + ')');

  for (let c = 0; c < e1.childCount; c++) {
    let c1 = e1.GetChildAt(c);
    let c2 = e2.GetChildAt(c);

    ok(c1.sharesDocumentWith(c2),
       'Cousins should share documents. (' + i + ', ' + j + ', ' + c + ')');
  }
}
