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
      iframe.addEventListener(eventName, function(...args) {
        resolve(...args);
      }, {once: true});
    });
  }

  // Test if all key=>value pairs in o1 are present in o2.
  const c = (o1, o2, i) => {
    for (let k of Object.keys(o1)) {
      is(o1[k], o2[k], `Test ${i} should match for key ${k}`);
    }
  }

  let testCount = 0;

  once('mozbrowserloadend').then(() => {
    iframe.findAll('foo', 'case-insensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 5,
    }, testCount++);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 2,
      numberOfMatches: 5,
    }, testCount++);
    iframe.findNext('backward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'foo',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 5,
    }, testCount++);
    iframe.findAll('xxx', 'case-sensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'xxx',
      searchLimit: 1000,
      activeMatchOrdinal: 0,
      numberOfMatches: 0,
    }, testCount++);
    iframe.findAll('bar', 'case-insensitive');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 4,
    }, testCount++);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 2,
      numberOfMatches: 4,
    }, testCount++);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 3,
      numberOfMatches: 4,
    }, testCount++);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 4,
      numberOfMatches: 4,
    }, testCount++);
    iframe.findNext('forward');
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: true,
      searchString: 'bar',
      searchLimit: 1000,
      activeMatchOrdinal: 1,
      numberOfMatches: 4,
    }, testCount++);
    iframe.clearMatch();
    return once('mozbrowserfindchange');
  }).then(({detail}) => {
    c(detail, {
      msg_name: "findchange",
      active: false
    }, testCount++);
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);

}

addEventListener('testready', runTest);
