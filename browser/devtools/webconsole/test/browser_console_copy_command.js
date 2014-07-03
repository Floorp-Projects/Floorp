/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the $0 console helper works as intended.

let gWebConsole, gJSTerm;

let TEXT =  "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum." +
    new Date();

let ID = "select-me";

add_task(function* init() {
  let deferredFocus = Promise.defer();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    waitForFocus(deferredFocus.resolve, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,test for copy helper in web console";

  yield deferredFocus.promise;

  let doc = content.document;
  let div = doc.createElement("div");
  let h1 = doc.createElement("h1");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  let div2 = doc.createElement("div");
  let p3 = doc.createElement("p");
  doc.title = "Testing copy command";
  h1.textContent = "Testing copy command";
  p1.textContent = "This is some example text";
  p2.textContent = TEXT;
  p2.id = "" + ID;
  div.appendChild(h1);
  div.appendChild(p1);
  div.appendChild(p2);
  div2.appendChild(p3);
  doc.body.appendChild(div);
  doc.body.appendChild(div2);

  let deferredConsole = Promise.defer();
  openConsole(gBrowser.selectedTab, deferredConsole.resolve);

  gWebConsole = yield deferredConsole.promise;
  gJSTerm = gWebConsole.jsterm;
});

add_task(function* test_copy() {
  let RANDOM = Math.random();
  let string = "Text: " + RANDOM;
  let obj = {a: 1, b: "foo", c: RANDOM};

  let samples = [[RANDOM, RANDOM],
                 [JSON.stringify(string), string],
                 [obj.toSource(),  JSON.stringify(obj, null, "  ")],
                 ["$('#" + ID + "')", content.document.getElementById(ID).outerHTML]
                ];
  for (let [source, reference] of samples) {
    let deferredResult = Promise.defer();

    SimpleTest.waitForClipboard(
      "" + reference,
      () => {
        let command = "copy(" + source + ")";
        info("Attempting to copy: " + source);
        info("Executing command: " + command);
        gJSTerm.execute(command, msg => {
          is (msg, undefined, "Command success: " + command);
        });
      },
      deferredResult.resolve,
      deferredResult.reject);

    yield deferredResult.promise;
  }
});

