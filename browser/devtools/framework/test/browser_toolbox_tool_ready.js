/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  addTab().then(function(data) {
    let toolIds = [ "jsdebugger", "styleeditor", "webconsole", "inspector" ];

    let open = function(index) {
      let toolId = toolIds[index];

      info("About to open " + index + "/" + toolId);
      gDevTools.showToolbox(data.target, toolId).then(function(toolbox) {
        ok(toolbox, "toolbox exists for " + toolId);
        is(toolbox.currentToolId, toolId, "currentToolId should be " + toolId);

        let nextIndex = index + 1;
        if (nextIndex >= toolIds.length) {
          toolbox.destroy();
          finish();
        }
        else {
          open(nextIndex);
        }
      }, console.error);
    };

    open(0);
  }).then(null, console.error);
}
