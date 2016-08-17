/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that proxy objects get their internal state added as pseudo properties.
 */

const TAB_URL = EXAMPLE_URL + "doc_proxy.html";

var test = Task.async(function* () {
  let options = {
    source: TAB_URL,
    line: 1
  };
  var dbg = initDebugger(TAB_URL, options);
  const [tab,, panel] = yield dbg;
  const debuggerLineNumber = 34;
  const scopes = waitForCaretAndScopes(panel, debuggerLineNumber);
  callInTab(tab, "doPause");
  yield scopes;

  const variables = panel.panelWin.DebuggerView.Variables;
  ok(variables, "Should get the variables view.");

  const scope = [...variables][0];
  ok(scope, "Should get the current function's scope.");

  let proxy;
  for (let [name, value] of scope) {
    if (name === "proxy") {
      proxy = value;
    }
  }
  ok(proxy, "Should have found the proxy variable");

  info("Expanding variable 'proxy'");
  let expanded = once(variables, "fetched");
  proxy.expand();
  yield expanded;

  let foundTarget = false;
  let foundHandler = false;
  for (let [property, data] of proxy) {
    info("Expanding property '" + property + "'");
    let expanded = once(variables, "fetched");
    data.expand();
    yield expanded;
    if (property === "<target>") {
      for(let [subprop, subdata] of data) if(subprop === "name") {
        is(subdata.value, "target", "The value of '<target>' should be the [[ProxyTarget]]");
        foundTarget = true;
      }
    } else {
      is(property, "<handler>", "There shouldn't be properties other than <target> and <handler>");
      for (let [subprop, subdata] of data) {
        if(subprop === "name") {
          is(subdata.value, "handler", "The value of '<handler>' should be the [[ProxyHandler]]");
          foundHandler = true;
        }
      }
    }
  }
  ok(foundTarget, "Should have found the '<target>' property containing the [[ProxyTarget]]");
  ok(foundHandler, "Should have found the '<handler>' property containing the [[ProxyHandler]]");

  resumeDebuggerThenCloseAndFinish(panel);
});
