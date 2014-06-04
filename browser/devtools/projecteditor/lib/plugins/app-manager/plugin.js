const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const promise = require("projecteditor/helpers/promise");
var { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { AppProjectEditor } = require("./app-project-editor");

var AppManagerRenderer = Class({
  extends: Plugin,

  isAppManagerProject: function() {
    return !!this.host.project.appManagerOpts;
  },
  editorForResource: function(resource) {
    if (!resource.parent && this.isAppManagerProject()) {
      return AppProjectEditor;
    }
  },
  onAnnotate: function(resource, editor, elt) {
    if (resource.parent || !this.isAppManagerProject()) {
      return;
    }

    let {appManagerOpts} = this.host.project;
    let doc = elt.ownerDocument;
    let image = doc.createElement("image");
    let label = doc.createElement("label");

    label.className = "project-name-label";
    image.className = "project-image";

    let name = appManagerOpts.name || resource.basename;
    let url = appManagerOpts.iconUrl || "icon-sample.png";

    label.textContent = name;
    image.setAttribute("src", url);

    elt.innerHTML = "";
    elt.appendChild(image);
    elt.appendChild(label);
    return true;
  }
});

exports.AppManagerRenderer = AppManagerRenderer;
registerPlugin(AppManagerRenderer);
