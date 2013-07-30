/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Loader } = require("sdk/test/loader");
const utils = require("sdk/tabs/utils");
const { open, close } = require("sdk/window/helpers");
const { getMostRecentBrowserWindow } = require("sdk/window/utils");
const { events } = require("sdk/tab/events");
const { on, off } = require("sdk/event/core");
const { resolve, defer } = require("sdk/core/promise");

let isFennec = require("sdk/system/xul-app").is("Fennec");

function test(options) {
  return function(assert, done) {
    let tabEvents = [];
    let tabs = [];
    let { promise, resolve: resolveP } = defer();
    let win = isFennec ? resolve(getMostRecentBrowserWindow()) :
      open(null, {
        features: { private: true, toolbar:true, chrome: true }
      });
    let window = null;

    // Firefox events are fired sync; Fennec events async
    // this normalizes the tests
    function handler (event) {
      tabEvents.push(event);
      runIfReady();
    }

    function runIfReady () {
      let releventEvents = getRelatedEvents(tabEvents, tabs);
      if (options.readyWhen(releventEvents))
        options.end({
          tabs: tabs,
          events: releventEvents,
          assert: assert,
          done: resolveP
        });
    }

    win.then(function(w) {
      window = w;
      on(events, "data", handler);
      options.start({ tabs: tabs, window: window });

      // Execute here for synchronous FF events, as the handlers
      // were called before tabs were pushed to `tabs`
      runIfReady(); 
      return promise;
    }).then(function() {
      off(events, "data", handler);
      return isFennec ? null : close(window);
    }).then(done, assert.fail);
  };
}

// Just making sure that tab events work for already opened tabs not only
// for new windows.
exports["test current window"] = test({
  readyWhen: events => events.length === 3,
  start: ({ tabs, window }) => {
    let tab = utils.openTab(window, 'data:text/plain,open');
    tabs.push(tab);
    utils.closeTab(tab);
  },
  end: ({ tabs, events, assert, done }) => {
    let [open, select, close] = events;
    let tab = tabs[0];

    assert.equal(open.type, "TabOpen");
    assert.equal(open.target, tab);

    assert.equal(select.type, "TabSelect");
    assert.equal(select.target, tab);

    assert.equal(close.type, "TabClose");
    assert.equal(close.target, tab);
    done();
  }
});

exports["test open"] = test({
  readyWhen: events => events.length === 2,
  start: ({ tabs, window }) => {
    tabs.push(utils.openTab(window, 'data:text/plain,open'));
  },
  end: ({ tabs, events, assert, done }) => {
    let [open, select] = events;
    let tab = tabs[0];

    assert.equal(open.type, "TabOpen");
    assert.equal(open.target, tab);

    assert.equal(select.type, "TabSelect");
    assert.equal(select.target, tab);
    done();
  }
});

exports["test open -> close"] = test({
  readyWhen: events => events.length === 3,
  start: ({ tabs, window }) => {
    // First tab is useless we just open it so that closing second tab won't
    // close window on some platforms.
    utils.openTab(window, 'data:text/plain,ignore');
    let tab = utils.openTab(window, 'data:text/plain,open-close');
    tabs.push(tab);
    utils.closeTab(tab);
  },
  end: ({ tabs, events, assert, done }) => {
    let [open, select, close] = events;
    let tab = tabs[0];

    assert.equal(open.type, "TabOpen");
    assert.equal(open.target, tab);

    assert.equal(select.type, "TabSelect");
    assert.equal(select.target, tab);

    assert.equal(close.type, "TabClose");
    assert.equal(close.target, tab);
    done();
  }
});

exports["test open -> open -> select"] = test({
  readyWhen: events => events.length === 5,
  start: ({tabs, window}) => {
    tabs.push(utils.openTab(window, 'data:text/plain,Tab-1'));
    tabs.push(utils.openTab(window, 'data:text/plain,Tab-2'));
    utils.activateTab(tabs[0], window);
  },
  end: ({ tabs, events, assert, done }) => {
    let [ tab1, tab2 ] = tabs;
    let tab1Events = 0;
    getRelatedEvents(events, tab1).map(event => {
      tab1Events++;
      if (tab1Events === 1)
        assert.equal(event.type, "TabOpen", "first tab opened");
      else
        assert.equal(event.type, "TabSelect", "first tab selected");
      assert.equal(event.target, tab1);
    });
    assert.equal(tab1Events, 3, "first tab has 3 events");

    let tab2Opened;
    getRelatedEvents(events, tab2).map(event => {
      if (!tab2Opened)
        assert.equal(event.type, "TabOpen", "second tab opened");
      else
        assert.equal(event.type, "TabSelect", "second tab selected");
      tab2Opened = true;
      assert.equal(event.target, tab2);
    });
    done();
  }
});

exports["test open -> pin -> unpin"] = test({
  readyWhen: events => events.length === (isFennec ? 2 : 5),
  start: ({ tabs, window }) => {
    tabs.push(utils.openTab(window, 'data:text/plain,pin-unpin'));
    utils.pin(tabs[0]);
    utils.unpin(tabs[0]);
  },
  end: ({ tabs, events, assert, done }) => {
    let [open, select, move, pin, unpin] = events;
    let tab = tabs[0];

    assert.equal(open.type, "TabOpen");
    assert.equal(open.target, tab);

    assert.equal(select.type, "TabSelect");
    assert.equal(select.target, tab);

    if (isFennec) {
      assert.pass("Tab pin / unpin is not supported by Fennec");
    }
    else {
      assert.equal(move.type, "TabMove");
      assert.equal(move.target, tab);

      assert.equal(pin.type, "TabPinned");
      assert.equal(pin.target, tab);

      assert.equal(unpin.type, "TabUnpinned");
      assert.equal(unpin.target, tab);
    }
    done();
  }
});

exports["test open -> open -> move "] = test({
  readyWhen: events => events.length === (isFennec ? 4 : 5),
  start: ({tabs, window}) => {
    tabs.push(utils.openTab(window, 'data:text/plain,Tab-1'));
    tabs.push(utils.openTab(window, 'data:text/plain,Tab-2'));
    utils.move(tabs[0], 2);
  },
  end: ({ tabs, events, assert, done }) => {
    let [ tab1, tab2 ] = tabs;
    let tab1Events = 0;
    getRelatedEvents(events, tab1).map(event => {
      tab1Events++;
      if (tab1Events === 1)
        assert.equal(event.type, "TabOpen", "first tab opened");
      else if (tab1Events === 2)
        assert.equal(event.type, "TabSelect", "first tab selected");
      else if (tab1Events === 3 && isFennec)
        assert.equal(event.type, "TabMove", "first tab moved");
      assert.equal(event.target, tab1);
    });
    assert.equal(tab1Events, isFennec ? 2 : 3,
      "correct number of events for first tab");

    let tab2Events = 0;
    getRelatedEvents(events, tab2).map(event => {
      tab2Events++;
      if (tab2Events === 1)
        assert.equal(event.type, "TabOpen", "second tab opened");
      else
        assert.equal(event.type, "TabSelect", "second tab selected");
      assert.equal(event.target, tab2);
    });
    done();
  }
});

function getRelatedEvents (events, tabs) {
  return events.filter(({target}) => ~([].concat(tabs)).indexOf(target));
}

require("sdk/test").run(exports);
