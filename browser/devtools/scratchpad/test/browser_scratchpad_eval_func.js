/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    openScratchpad(runTests);
  }, true);

  content.location = "data:text/html;charset=utf8,test Scratchpad eval function.";
}

function reportErrorAndQuit(error) {
  DevToolsUtils.reportException("browser_scratchpad_eval_func.js", error);
  ok(false);
  finish();
}

function runTests(sw)
{
  const sp = sw.Scratchpad;

  let foo = "" + function main() { console.log(1); };
  let bar = "var bar = " + (() => { console.log(2); });

  const fullText =
    foo + "\n" +
    "\n" +
    bar + "\n"

  sp.setText(fullText);

  // On the function declaration.
  sp.editor.setCursor({ line: 0, ch: 18 });
  sp.evalTopLevelFunction()
    .then(([text, error, result]) => {
      is(text, foo, "Should re-eval foo.");
      ok(!error, "Should not have got an error.");
      ok(result, "Should have got a result.");
    })

    // On the arrow function.
    .then(() => {
      sp.editor.setCursor({ line: 2, ch: 18 });
      return sp.evalTopLevelFunction();
    })
    .then(([text, error, result]) => {
      is(text, bar.replace("var ", ""), "Should re-eval bar.");
      ok(!error, "Should not have got an error.");
      ok(result, "Should have got a result.");
    })

    // On the empty line.
    .then(() => {
      sp.editor.setCursor({ line: 1, ch: 0 });
      return sp.evalTopLevelFunction();
    })
    .then(([text, error, result]) => {
      is(text, fullText,
         "Should get full text back since we didn't find a specific function.");
      ok(!error, "Should not have got an error.");
      ok(!result, "Should not have got a result.");
    })

    // Syntax error.
    .then(() => {
      sp.setText("function {}");
      sp.editor.setCursor({ line: 0, ch: 9 });
      return sp.evalTopLevelFunction();
    })
    .then(([text, error, result]) => {
      is(text, "function {}",
         "Should get the full text back since there was a parse error.");
      ok(!error, "Should not have got an error");
      ok(!result, "Should not have got a result");
      ok(sp.getText().contains("SyntaxError"),
         "We should have written the syntax error to the scratchpad.");
    })

    .then(finish, reportErrorAndQuit);
}
