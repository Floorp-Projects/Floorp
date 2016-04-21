/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1163961 - Test search API

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {

  let iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.src = 'data:text/html,foo bar foo XXX Foo BAR foobar foobar';

  const once = (eventName) => {
    return new Promise((resolve) => {
      iframe.addEventListener(eventName, function onEvent(...args) {
        iframe.removeEventListener(eventName, onEvent);
        resolve(...args);
      });
    });
  }

  // Test if all key=>value pairs in o1 are present in o2.
  const c = (o1, o2) => Object.keys(o1).every(k => o1[k] == o2[k]);

  let testCount = 0;

  once('mozbrowserloadend').then(() => {
    iframe.findAll('foo', 'case-insensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 5,
    }), `test ${testCount++}`);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 2,
      numberOfMatches: 5,
    }), `test ${testCount++}`);
    iframe.findNext('backward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 5,
    }), `test ${testCount++}`);
    iframe.findAll('xxx', 'case-sensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'xxx',
      searchLimit: 1000,
      activeMatchOrdinal: 0,
      numberOfMatches: 0,
    }), `test ${testCount++}`);
    iframe.findAll('bar', 'case-insensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 4,
    }), `test ${testCount++}`);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 2,
      numberOfMatches: 4,
    }), `test ${testCount++}`);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 3,
      numberOfMatches: 4,
    }), `test ${testCount++}`);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 4,
      numberOfMatches: 4,
    }), `test ${testCount++}`);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 4,
    }), `test ${testCount++}`);
    iframe.clearMatch();
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    ok(c(detail, {
      msg_name: "findchange",
      active: false
    }), `test ${testCount++}`);
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);

}

addEventListener('testready', runTest);
