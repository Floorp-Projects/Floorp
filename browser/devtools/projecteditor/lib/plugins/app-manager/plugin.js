const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const promise = require("projecteditor/helpers/promise");
var { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { AppProjectEditor } = require("./app-project-editor");
const OPTION_URL = "chrome://browser/skin/devtools/tool-options.svg";

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
    let optionImage = doc.createElement("image");
    let flexElement = doc.createElement("div");
    let nameLabel = doc.createElement("span");
    let statusElement = doc.createElement("div");

    image.className = "project-image";
    optionImage.className = "project-options";
    nameLabel.className = "project-name-label";
    statusElement.className = "project-status";
    flexElement.className = "project-flex";

    let name = appManagerOpts.name || resource.basename;
    let url = appManagerOpts.iconUrl || "icon-sample.png";
    let status = appManagerOpts.validationStatus || "unknown";

    nameLabel.textContent = name;
    image.setAttribute("src", url);
    optionImage.setAttribute("src", OPTION_URL);
    statusElement.setAttribute("status", status)

    elt.innerHTML = "";
    elt.appendChild(image);
    elt.appendChild(nameLabel);
    elt.appendChild(flexElement);
    elt.appendChild(statusElement);
    elt.appendChild(optionImage);
    return true;
  }
});

exports.AppManagerRenderer = AppManagerRenderer;
registerPlugin(AppManagerRenderer);
