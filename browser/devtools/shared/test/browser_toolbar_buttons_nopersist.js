/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 855108 - Commands toggled by the developer toolbar should not persist after the toolbar closes

function test() {
  waitForExplicitFinish();

  let Toolbox = Cu.import("resource:///modules/devtools/Toolbox.jsm", {}).Toolbox;
  let TargetFactory = Cu.import("resource:///modules/devtools/Target.jsm", {}).TargetFactory;
  let gDevTools = Cu.import("resource:///modules/devtools/gDevTools.jsm", {}).gDevTools;
  let CommandUtils = Cu.import("resource:///modules/devtools/DeveloperToolbar.jsm", {}).CommandUtils;
  let Requisition = (function () {
    let require = Cu.import("resource://gre/modules/devtools/Require.jsm", {}).require;
    Cu.import("resource:///modules/devtools/gcli.jsm", {});
    return require('gcli/cli').Requisition;
  })();
  let Services = Cu.import("resource://gre/modules/Services.jsm", {}).Services;
  let TiltGL = Cu.import("resource:///modules/devtools/TiltGL.jsm", {}).TiltGL;
  let Promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;

  // only test togglable items on the toolbar
  const commandsToTest =
          CommandUtils.getCommandbarSpec("devtools.toolbox.toolbarSpec")
          .filter(function (command) {
            if ( !(typeof command == "string" &&
                   command.indexOf("toggle") >= 0))
              return false;
            if (command == "tilt toggle") {
              // Is it possible to run the tilt tool?
              let XHTML_NS = "http://www.w3.org/1999/xhtml";
              let canvas = document.createElementNS(XHTML_NS, "canvas");
              return (TiltGL.isWebGLSupported() &&
                      TiltGL.create3DContext(canvas));
            }
            return true;
          });

  const URL = "data:text/html;charset=UTF-8," + encodeURIComponent(
    [ "<!DOCTYPE html>",
      "<html>",
      "  <head>",
      "    <title>Bug 855108</title>",
      "  </head>",
      "  <body>",
      "    <p>content</p>",
      "  </body>",
      "</html>"
    ].join("\n"));

  function clearHostPref() {
    let pref = Toolbox.prototype._prefs.LAST_HOST;
    Services.prefs.getBranch("").clearUserPref(pref);
  }

  registerCleanupFunction(clearHostPref);

  let requisition;

  function testNextCommand(tab) {
    if (commandsToTest.length ==  0) {
      finish();
      return null;
    }
    let commandSpec = commandsToTest.pop();
    requisition.update(commandSpec);
    let command = requisition.commandAssignment.value;

    let target = TargetFactory.forTab(tab);
    function checkToggle(expected, msg) {
      is(!!command.state.isChecked(target), expected, commandSpec+" "+msg);
    }
    let toolbox;

    clearHostPref();

    // Actions: Toggle the command on using the toolbox button, then
    //          close the toolbox.
    // Expected result: The command should no longer be toggled on.
    return gDevTools.showToolbox(target)
      .then(catchFail(function (aToolbox) {
        toolbox = aToolbox;
        let button = toolbox.doc.getElementById(command.buttonId);
        return clickElement(button);
      })).then(catchFail(function () {
        checkToggle(true, "was toggled on by toolbox");
        return toolbox.destroy();
      }))
      .then(catchFail(function () {
        target = TargetFactory.forTab(tab);
        checkToggle(false, "is untoggled after toobox closed");
      }))

    // Actions: Open the toolbox, toggle the command on use means
    //          OTHER than the toolbox, then close the toolbox.
    // Expected result: The command should still be toggled.
    // Cleanup: Toggle the command off again.
      .then(function () gDevTools.showToolbox(target))
      .then(catchFail(function (toolbox) {
        requisition.update(commandSpec);
        requisition.exec();
        checkToggle(true, "was toggled on by command");
        return toolbox.destroy();
      }))
      .then(catchFail(function () {
        target = TargetFactory.forTab(tab);
        checkToggle(true, "is still toggled after toolbox closed");
        requisition.update(commandSpec);
        requisition.exec();
      }))

    // Actions: Toggle the command on using the button on a docked
    //          toolbox, detach the toolbox into a window, then close
    //          the toolbox.
    // Expected result: The command should no longer be toggled on.
      .then(function () gDevTools.showToolbox(target, null,
                                              Toolbox.HostType.BOTTOM))
      .then(catchFail(function (aToolbox) {
        toolbox = aToolbox;
        let button = toolbox.doc.getElementById(command.buttonId);
        return clickElement(button);
      })).then(catchFail(function () {
        checkToggle(true, "was toggled by docked toolbox");
        return toolbox.switchHost(Toolbox.HostType.WINDOW);
      }))
      .then(function () toolbox.destroy())
      .then(catchFail(function () {
        target = TargetFactory.forTab(tab);
        checkToggle(false, "is untoggled after detached toobox closed");
      }))

      .then(function () target.destroy())
      .then(function () testNextCommand(tab));
  }

  function clickElement(el) {
    let deferred = Promise.defer();
    let window = el.ownerDocument.defaultView;
    waitForFocus(function () {
      EventUtils.synthesizeMouseAtCenter(el, {}, window);
      deferred.resolve();
    }, window);
    return deferred.promise;
  }

  addTab(URL, function (browser, tab) {
    let environment = { chromeDocument: tab.ownerDocument };
    requisition = new Requisition(environment);
    testNextCommand(tab);
  });
}
