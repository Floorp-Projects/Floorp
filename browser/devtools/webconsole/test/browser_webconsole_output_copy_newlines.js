/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that multiple messages are copied into the clipboard and that they are
// separated by new lines. See bug 916997.

function test()
{
  const TEST_URI = "data:text/html;charset=utf8,<p>hello world, bug 916997";
  let clipboardValue = "";

  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);

  function consoleOpened(hud)
  {
    hud.jsterm.clearOutput();

    let controller = top.document.commandDispatcher.
                     getControllerForCommand("cmd_copy");
    is(controller.isCommandEnabled("cmd_copy"), false, "cmd_copy is disabled");

    content.console.log("Hello world! bug916997a");
    content.console.log("Hello world 2! bug916997b");

    waitForMessages({
      webconsole: hud,
      messages: [{
        text: "Hello world! bug916997a",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }, {
        text: "Hello world 2! bug916997b",
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    }).then(() => {
      hud.ui.output.selectAllMessages();
      hud.outputNode.focus();

      goUpdateCommand("cmd_copy");
      controller = top.document.commandDispatcher.getControllerForCommand("cmd_copy");
      is(controller.isCommandEnabled("cmd_copy"), true, "cmd_copy is enabled");

      let selection = hud.iframeWindow.getSelection() + "";
      info("selection '" + selection + "'");
      waitForClipboard((str) => {
          clipboardValue = str;
          return str.indexOf("bug916997a") > -1 && str.indexOf("bug916997b") > -1;
        },
        () => { goDoCommand("cmd_copy"); },
        checkClipboard.bind(null, hud),
        () => {
          info("last clipboard value: '" + clipboardValue + "'");
          finishTest();
        });
    });
  }

  function checkClipboard(hud)
  {
    info("clipboard value '" + clipboardValue + "'");
    let lines = clipboardValue.trim().split("\n");
    is(hud.outputNode.children.length, 2, "number of messages");
    is(lines.length, hud.outputNode.children.length, "number of lines");
    isnot(lines[0].indexOf("bug916997a"), -1,
          "first message text includes 'bug916997a'");
    isnot(lines[1].indexOf("bug916997b"), -1,
          "second message text includes 'bug916997b'");
    is(lines[0].indexOf("bug916997b"), -1,
       "first message text does not include 'bug916997b'");
    finishTest();
  }
}
