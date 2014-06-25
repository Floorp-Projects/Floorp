/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* Test that ensures DOM nodes are rendered correctly in VariablesView. */

function test() {
  const TEST_URI = 'data:text/html;charset=utf-8,                           \
                    <html>                                                  \
                      <head>                                                \
                        <title>Test for DOM nodes in variables view</title> \
                      </head>                                               \
                      <body>                                                \
                        <div></div>                                         \
                        <div id="testID"></div>                             \
                        <div class="single-class"></div>                    \
                        <div class="multiple-classes another-class"></div>  \
                        <div class="class-and-id" id="class-and-id"></div>  \
                        <div class="multiple-classes-and-id another-class"  \
                             id="multiple-classes-and-id"></div>            \
                        <div class="   whitespace-start"></div>             \
                        <div class="whitespace-end     "></div>             \
                        <div class="multiple    spaces"></div>              \
                      </body>                                               \
                    </html>';

  Task.spawn(runner).then(finishTest);

    function* runner() {
      const {tab} = yield loadTab(TEST_URI);
      const hud = yield openConsole(tab);
      const jsterm = hud.jsterm;

      let deferred = promise.defer();
      jsterm.once("variablesview-fetched", (_, aVar) => deferred.resolve(aVar));
      jsterm.execute("inspect(document.querySelectorAll('div'))");

      let variableScope = yield deferred.promise;
      ok(variableScope, "Variables view opened");

      yield findVariableViewProperties(variableScope, [
        { name: "0", value: "<div>"},
        { name: "1", value: "<div#testID>"},
        { name: "2", value: "<div.single-class>"},
        { name: "3", value: "<div.multiple-classes.another-class>"},
        { name: "4", value: "<div#class-and-id.class-and-id>"},
        { name: "5", value: "<div#multiple-classes-and-id.multiple-classes-and-id.another-class>"},
        { name: "6", value: "<div.whitespace-start>"},
        { name: "7", value: "<div.whitespace-end>"},
        { name: "8", value: "<div.multiple.spaces>"},
      ], { webconsole: hud});

    }
}
