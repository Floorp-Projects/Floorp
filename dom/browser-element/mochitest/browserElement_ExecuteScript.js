/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1174733 - Browser API: iframe.executeScript

'use strict';

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {

  const origin = 'http://example.org';
  const url = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_ExecuteScript.html';

  // Test if all key=>value pairs in o1 are present in o2.
  const c = (o1, o2) => Object.keys(o1).every(k => o1[k] == o2[k]);

  let scriptId = 0;

  const bail = () => {
    ok(false, `scriptId: ${scriptId++}`);
  }

  SpecialPowers.pushPermissions([
    {type: 'browser', allow: 1, context: document},
    {type: 'browser:universalxss', allow: 1, context: document}
  ], function() {
    let iframe = document.createElement('iframe');
    iframe.setAttribute('mozbrowser', 'true');
    iframe.addEventListener('mozbrowserloadend', function onload() {
      iframe.removeEventListener('mozbrowserloadend', onload);
      onReady(iframe);
    });
    iframe.src = url;
    document.body.appendChild(iframe);
  });


  function onReady(iframe) {
    iframe.executeScript('4 + 4', {url}).then(rv => {
      is(rv, 8, `scriptId: ${scriptId++}`);
      return iframe.executeScript('(() => {return {a:42}})()', {url})
    }, bail).then(rv => {
      ok(c(rv, {a:42}), `scriptId: ${scriptId++}`);
      return iframe.executeScript('(() => {return {a:42}})()', {origin})
    }, bail).then(rv => {
      ok(c(rv, {a:42}), `scriptId: ${scriptId++}`);
      return iframe.executeScript('(() => {return {a:42}})()', {origin, url})
    }, bail).then(rv => {
      ok(c(rv, {a:42}), `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
          new Promise((resolve, reject) => {
            resolve(document.body.textContent.trim());
          });
      `, {url})
    }, bail).then(rv => {
      is(rv, 'foo', `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
          new Promise((resolve, reject) => {
            resolve({a:43,b:34});
          });
      `, {url})
    }, bail).then(rv => {
      ok(c(rv, {a:43,b:34}), `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
        … syntax error
      `, {url});
    }, bail).then(bail, (error) => {
      is(error.name, 'SyntaxError: illegal character', `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
        window
      `, {url});
    }).then(bail, (error) => {
      is(error.name, 'Script last expression must be a promise or a JSON object', `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
          new Promise((resolve, reject) => {
            reject('BOOM');
          });
      `, {url});
    }).then(bail, (error) => {
      is(error.name, 'BOOM', `scriptId: ${scriptId++}`);
      return iframe.executeScript(`
          new Promise((resolve, reject) => {
            resolve(window);
          });
      `, {url});
    }).then(bail, (error) => {
      is(error.name, 'Value returned (resolve) by promise is not a valid JSON object', `scriptId: ${scriptId++}`);
      return iframe.executeScript('window.btoa("a")', {url})
    }, bail).then(rv => {
      ok(c(rv, 'YQ=='), `scriptId: ${scriptId++}`);
      return iframe.executeScript('window.wrappedJSObject.btoa("a")', {url})
    }, bail).then(bail, (error) => {
      is(error.name, 'TypeError: window.wrappedJSObject is undefined', `scriptId: ${scriptId++}`);
      return iframe.executeScript('42', {})
    }).then(bail, error => {
      is(error.name, 'InvalidAccessError', `scriptId: ${scriptId++}`);
      return iframe.executeScript('42');
    }).then(bail, error => {
      is(error.name, 'InvalidAccessError', `scriptId: ${scriptId++}`);
      return iframe.executeScript('43', { url: 'http://foo.com' });
    }).then(bail, (error) => {
      is(error.name, 'URL mismatches', `scriptId: ${scriptId++}`);
      return iframe.executeScript('43', { url: '_' });
    }, bail).then(bail, (error) => {
      is(error.name, 'Malformed URL', `scriptId: ${scriptId++}`);
      return iframe.executeScript('43', { origin: 'http://foo.com' });
    }, bail).then(bail, (error) => {
      is(error.name, 'Origin mismatches', `scriptId: ${scriptId++}`);
      return iframe.executeScript('43', { origin: 'https://example.org' });
    }, bail).then(bail, (error) => {
      is(error.name, 'Origin mismatches', `scriptId: ${scriptId++}`);
      SimpleTest.finish();
    });
  }
}

addEventListener('testready', runTest);
