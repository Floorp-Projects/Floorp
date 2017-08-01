'use strict';

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;
function scopedCuImport(path) {
  const scope = {};
  Cu.import(path, scope);
  return scope;
}
const {loader, require} = scopedCuImport("resource://devtools/shared/Loader.jsm");
const {TargetFactory} = require("devtools/client/framework/target");
const {Utils: WebConsoleUtils} =
  require("devtools/client/webconsole/utils");
let { gDevTools } = require("devtools/client/framework/devtools");
loader.lazyGetter(this, "HUDService", () => require("devtools/client/webconsole/webconsole"));
loader.lazyGetter(this, "HUDService", () => require("devtools/client/webconsole/hudservice"));
let promise = require("promise");

/**
 * Open the toolbox in a given tab.
 * @param {XULNode} tab The tab the toolbox should be opened in.
 * @param {String} toolId Optional. The ID of the tool to be selected.
 * @param {String} hostType Optional. The type of toolbox host to be used.
 * @return {Promise} Resolves with the toolbox, when it has been opened.
 */
var openToolboxForTab = Task.async(function* (tab, toolId, hostType) {
  info("Opening the toolbox");

  let toolbox;
  let target = TargetFactory.forTab(tab);
  yield target.makeRemote();

  // Check if the toolbox is already loaded.
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    if (!toolId || (toolId && toolbox.getPanel(toolId))) {
      info("Toolbox is already opened");
      return toolbox;
    }
  }

  // If not, load it now.
  toolbox = yield gDevTools.showToolbox(target, toolId, hostType);

  // Make sure that the toolbox frame is focused.
  yield new Promise(resolve => waitForFocus(resolve, toolbox.win));

  info("Toolbox opened and focused");

  return toolbox;
});

/**
 * Find multiple messages in the output.
 *
 * @param object hud
 *        The web console.
 * @param string text
 *        A substring that can be found in the message.
 * @param selector [optional]
 *        The selector to use in finding the message.
 */
function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.experimentalOutputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(
    messages,
    (el) => el.textContent.includes(text)
  );
  return elements;
}
