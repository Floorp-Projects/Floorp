/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the DOM Template engine works properly

/*
 * These tests run both in Mozilla/Mochitest and plain browsers (as does
 * domtemplate)
 * We should endevour to keep the source in sync.
 */

const {template} = require("devtools/shared/gcli/templater");

const TEST_URI = TEST_URI_ROOT + "browser_templater_basic.html";

var test = Task.async(function* () {
  yield addTab("about:blank");
  let [host,, doc] = yield createHost("bottom", TEST_URI);

  info("Starting DOM Templater Tests");
  runTest(0, host, doc);
});

function runTest(index, host, doc) {
  let options = tests[index] = tests[index]();
  let holder = doc.createElement("div");
  holder.id = options.name;
  let body = doc.body;
  body.appendChild(holder);
  holder.innerHTML = options.template;

  info("Running " + options.name);
  template(holder, options.data, options.options);

  if (typeof options.result == "string") {
    is(holder.innerHTML, options.result, options.name);
  } else {
    ok(holder.innerHTML.match(options.result) != null,
       options.name + " result='" + holder.innerHTML + "'");
  }

  if (options.also) {
    options.also(options);
  }

  function runNextTest() {
    index++;
    if (index < tests.length) {
      runTest(index, host, doc);
    } else {
      finished(host);
    }
  }

  if (options.later) {
    let ais = is.bind(this);

    function createTester(testHolder, testOptions) {
      return () => {
        ais(testHolder.innerHTML, testOptions.later, testOptions.name + " later");
        runNextTest();
      };
    }

    executeSoon(createTester(holder, options));
  } else {
    runNextTest();
  }
}

function finished(host) {
  host.destroy();
  gBrowser.removeCurrentTab();
  info("Finishing DOM Templater Tests");
  tests = null;
  finish();
}

/**
 * Why have an array of functions that return data rather than just an array
 * of the data itself? Some of these tests contain calls to delayReply() which
 * sets up async processing using executeSoon(). Since the execution of these
 * tests is asynchronous, the delayed reply will probably arrive before the
 * test is executed, making the test be synchronous. So we wrap the data in a
 * function so we only set it up just before we use it.
 */
var tests = [
  () => ({
    name: "simpleNesting",
    template: '<div id="ex1">${nested.value}</div>',
    data: { nested: { value: "pass 1" } },
    result: '<div id="ex1">pass 1</div>'
  }),

  () => ({
    name: "returnDom",
    template: '<div id="ex2">${__element.ownerDocument.createTextNode(\'pass 2\')}</div>',
    options: { allowEval: true },
    data: {},
    result: '<div id="ex2">pass 2</div>'
  }),

  () => ({
    name: "srcChange",
    template: '<img _src="${fred}" id="ex3">',
    data: { fred: "green.png" },
    result: /<img( id="ex3")? src="green.png"( id="ex3")?>/
  }),

  () => ({
    name: "ifTrue",
    template: '<p if="${name !== \'jim\'}">hello ${name}</p>',
    options: { allowEval: true },
    data: { name: "fred" },
    result: "<p>hello fred</p>"
  }),

  () => ({
    name: "ifFalse",
    template: '<p if="${name !== \'jim\'}">hello ${name}</p>',
    options: { allowEval: true },
    data: { name: "jim" },
    result: ""
  }),

  () => ({
    name: "simpleLoop",
    template: '<p foreach="index in ${[ 1, 2, 3 ]}">${index}</p>',
    options: { allowEval: true },
    data: {},
    result: "<p>1</p><p>2</p><p>3</p>"
  }),

  () => ({
    name: "loopElement",
    template: '<loop foreach="i in ${array}">${i}</loop>',
    data: { array: [ 1, 2, 3 ] },
    result: "123"
  }),

  // Bug 692028: DOMTemplate memory leak with asynchronous arrays
  // Bug 692031: DOMTemplate async loops do not drop the loop element
  () => ({
    name: "asyncLoopElement",
    template: '<loop foreach="i in ${array}">${i}</loop>',
    data: { array: delayReply([1, 2, 3]) },
    result: "<span></span>",
    later: "123"
  }),

  () => ({
    name: "saveElement",
    template: '<p save="${element}">${name}</p>',
    data: { name: "pass 8" },
    result: "<p>pass 8</p>",
    also: function (options) {
      ok(options.data.element.innerHTML, "pass 9", "saveElement saved");
      delete options.data.element;
    }
  }),

  () => ({
    name: "useElement",
    template: '<p id="pass9">${adjust(__element)}</p>',
    options: { allowEval: true },
    data: {
      adjust: function (element) {
        is("pass9", element.id, "useElement adjust");
        return "pass 9b";
      }
    },
    result: '<p id="pass9">pass 9b</p>'
  }),

  () => ({
    name: "asyncInline",
    template: "${delayed}",
    data: { delayed: delayReply("inline") },
    result: "<span></span>",
    later: "inline"
  }),

  // Bug 692028: DOMTemplate memory leak with asynchronous arrays
  () => ({
    name: "asyncArray",
    template: '<p foreach="i in ${delayed}">${i}</p>',
    data: { delayed: delayReply([1, 2, 3]) },
    result: "<span></span>",
    later: "<p>1</p><p>2</p><p>3</p>"
  }),

  () => ({
    name: "asyncMember",
    template: '<p foreach="i in ${delayed}">${i}</p>',
    data: { delayed: [delayReply(4), delayReply(5), delayReply(6)] },
    result: "<span></span><span></span><span></span>",
    later: "<p>4</p><p>5</p><p>6</p>"
  }),

  // Bug 692028: DOMTemplate memory leak with asynchronous arrays
  () => ({
    name: "asyncBoth",
    template: '<p foreach="i in ${delayed}">${i}</p>',
    data: {
      delayed: delayReply([
        delayReply(4),
        delayReply(5),
        delayReply(6)
      ])
    },
    result: "<span></span>",
    later: "<p>4</p><p>5</p><p>6</p>"
  }),

  // Bug 701762: DOMTemplate fails when ${foo()} returns undefined
  () => ({
    name: "functionReturningUndefiend",
    template: "<p>${foo()}</p>",
    options: { allowEval: true },
    data: {
      foo: function () {}
    },
    result: "<p>undefined</p>"
  }),

  // Bug 702642: DOMTemplate is relatively slow when evaluating JS ${}
  () => ({
    name: "propertySimple",
    template: "<p>${a.b.c}</p>",
    data: { a: { b: { c: "hello" } } },
    result: "<p>hello</p>"
  }),

  () => ({
    name: "propertyPass",
    template: "<p>${Math.max(1, 2)}</p>",
    options: { allowEval: true },
    result: "<p>2</p>"
  }),

  () => ({
    name: "propertyFail",
    template: "<p>${Math.max(1, 2)}</p>",
    result: "<p>${Math.max(1, 2)}</p>"
  }),

  // Bug 723431: DOMTemplate should allow customisation of display of
  // null/undefined values
  () => ({
    name: "propertyUndefAttrFull",
    template: "<p>${nullvar}|${undefinedvar1}|${undefinedvar2}</p>",
    data: { nullvar: null, undefinedvar1: undefined },
    result: "<p>null|undefined|undefined</p>"
  }),

  () => ({
    name: "propertyUndefAttrBlank",
    template: "<p>${nullvar}|${undefinedvar1}|${undefinedvar2}</p>",
    data: { nullvar: null, undefinedvar1: undefined },
    options: { blankNullUndefined: true },
    result: "<p>||</p>"
  }),

  /* eslint-disable max-len */
  () => ({
    name: "propertyUndefAttrFull",
    template: '<div><p value="${nullvar}"></p><p value="${undefinedvar1}"></p><p value="${undefinedvar2}"></p></div>',
    data: { nullvar: null, undefinedvar1: undefined },
    result: '<div><p value="null"></p><p value="undefined"></p><p value="undefined"></p></div>'
  }),

  () => ({
    name: "propertyUndefAttrBlank",
    template: '<div><p value="${nullvar}"></p><p value="${undefinedvar1}"></p><p value="${undefinedvar2}"></p></div>',
    data: { nullvar: null, undefinedvar1: undefined },
    options: { blankNullUndefined: true },
    result: '<div><p value=""></p><p value=""></p><p value=""></p></div>'
  })
  /* eslint-enable max-len */
];

function delayReply(data) {
  return new Promise(resolve => resolve(data));
}
