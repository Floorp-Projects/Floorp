const { Cu } = require("chrome");
const { Class } = require("sdk/core/heritage");
const { EventTarget } = require("sdk/event/target");
const { emit } = require("sdk/event/core");
const promise = require("promise");
var { registerPlugin, Plugin } = require("projecteditor/plugins/core");
const { AppProjectEditor } = require("./app-project-editor");
const OPTION_URL = "chrome://browser/skin/devtools/tool-options.svg";
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/webide.properties");

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
  getUI: function(parent) {
    let doc = parent.ownerDocument;
    if (parent.childElementCount == 0) {
      let image = doc.createElement("image");
      let optionImage = doc.createElement("image");
      let flexElement = doc.createElement("div");
      let nameLabel = doc.createElement("span");
      let statusElement = doc.createElement("div");

      image.className = "project-image";
      optionImage.className = "project-options";
      optionImage.setAttribute("src", OPTION_URL);
      nameLabel.className = "project-name-label";
      statusElement.className = "project-status";
      flexElement.className = "project-flex";

      parent.appendChild(image);
      parent.appendChild(nameLabel);
      parent.appendChild(flexElement);
      parent.appendChild(statusElement);
      parent.appendChild(optionImage);
    }

    return {
      image: parent.querySelector(".project-image"),
      nameLabel: parent.querySelector(".project-name-label"),
      statusElement: parent.querySelector(".project-status")
    };
  },
  onAnnotate: function(resource, editor, elt) {
    if (resource.parent || !this.isAppManagerProject()) {
      return;
    }

    let {appManagerOpts} = this.host.project;
    let doc = elt.ownerDocument;

    let {image,nameLabel,statusElement} = this.getUI(elt);
    let name = appManagerOpts.name || resource.basename;
    let url = appManagerOpts.iconUrl || "icon-sample.png";
    let status = appManagerOpts.validationStatus || "unknown";
    let tooltip = Strings.formatStringFromName("status_tooltip",
      [Strings.GetStringFromName("status_" + status)], 1);

    nameLabel.textContent = name;
    image.setAttribute("src", url);
    statusElement.setAttribute("status", status);
    statusElement.setAttribute("tooltiptext", tooltip);

    return true;
  }
});

exports.AppManagerRenderer = AppManagerRenderer;
registerPlugin(AppManagerRenderer);
