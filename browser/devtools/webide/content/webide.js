/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource:///modules/devtools/gDevTools.jsm");

const {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {require} = devtools;
const {Services} = Cu.import("resource://gre/modules/Services.jsm");
const {AppProjects} = require("devtools/app-manager/app-projects");
const {Connection} = require("devtools/client/connection-manager");
const {AppManager} = require("devtools/app-manager");

const Strings = Services.strings.createBundle("chrome://webide/content/webide.properties");

const HTML = "http://www.w3.org/1999/xhtml";

window.addEventListener("load", function onLoad() {
  window.removeEventListener("load", onLoad);
  UI.init();
});

window.addEventListener("unload", function onUnload() {
  window.removeEventListener("unload", onUnload);
  UI.uninit();
});

let UI = {
  init: function() {
    AppManager.init();

    this.onMessage = this.onMessage.bind(this);
    window.addEventListener("message", this.onMessage);

    this.appManagerUpdate = this.appManagerUpdate.bind(this);
    AppManager.on("app-manager-update", this.appManagerUpdate);

    this.logNode = document.querySelector("#logs");

    this.updateCommands();
    this.updateRuntimeList();

    this.onfocus = this.onfocus.bind(this);
    window.addEventListener("focus", this.onfocus, true);

    try {
      let lastProjectLocation = Services.prefs.getCharPref("devtools.webide.lastprojectlocation");
      AppProjects.load().then(() => {
        let lastProject = AppProjects.get(lastProjectLocation);
        if (lastProject) {
          AppManager.selectedProject = lastProject;
        } else {
          AppManager.selectedProject = null;
        }
      });
    } catch(e) {
      AppManager.selectedProject = null;
    }

    document.querySelector("#toggle-logs").addEventListener("click", function() {
      document.querySelector("#logs").classList.toggle("expand");
      UI.logNode.scrollTop = UI.logNode.scrollTopMax;
    });
  },

  uninit: function() {
    window.removeEventListener("focus", this.onfocus, true);
    AppManager.off("app-manager-update", this.appManagerUpdate);
    AppManager.uninit();
    window.removeEventListener("message", this.onMessage);
  },

  onfocus: function() {
    // Because we can't track the activity in the folder project,
    // we need to validate the project regularly. Let's assume that
    // if a modification happened, it happened when the window was
    // not focused.
    if (AppManager.selectedProject &&
        AppManager.selectedProject.type != "runtimeApp") {
      AppManager.validateProject(AppManager.selectedProject);
    }
  },

  appManagerUpdate: function(event, what, details) {
    // Got a message from app-manager.js
    switch (what) {
      case "console":
        if (details.level == "log")     this.console.log(details.message);
        if (details.level == "warning") this.console.warning(details.message);
        if (details.level == "error")   this.console.error(details.message);
        if (details.level == "success") this.console.success(details.message);
        break;
      case "runtimelist":
        this.updateRuntimeList();
        break;
      case "connection":
        this.updateRuntimeButton();
        this.updateCommands();
        break;
      case "project":
        this.updateTitle();
        this.closeToolbox();
        this.updateCommands();
        this.updateProjectButton();
        this.openProject();
        break;
      case "project-is-not-running":
      case "project-is-running":
        this.updateCommands();
        break;
      case "runtime":
        this.updateRuntimeButton();
        break;
      case "project-validated":
        this.updateTitle();
        this.updateCommands();
        this.updateProjectButton();
        break;
    };
  },

  openInBrowser: function(url) {
    // Open a URL in a Firefox window
    let browserWin = Services.wm.getMostRecentWindow("navigator:browser");
    if (browserWin) {
      let gBrowser = browserWin.gBrowser;
      gBrowser.selectedTab = gBrowser.addTab(url);
      browserWin.focus();
    } else {
      window.open(url);
    }
  },

  updateTitle: function() {
    let project = AppManager.selectedProject;
    if (project) {
      window.document.title = Strings.formatStringFromName("title_app", [project.name], 1);
    } else {
      window.document.title = Strings.GetStringFromName("title_noApp");
    }
  },

  hidePanels: function() {
    let panels = document.querySelectorAll("panel");
    for (let p of panels) {
      p.hidePopup();
    }
  },

  busy: function() {
    document.querySelector("window").classList.add("busy")
    this.updateCommands();
  },

  unbusy: function() {
    document.querySelector("window").classList.remove("busy")
    this.updateCommands();
  },

  busyUntil: function(promise, operationDescription) {
    // Freeze the UI until the promise is resolved. A 30s timeout
    // will unfreeze the UI, just in case the promise never gets
    // resolved.
    let timeout = setTimeout(() => {
      this.unbusy();
      this.console.error("Operation timeout: " + operationDescription);
    }, 30000);
    this.busy();
    promise.then(() => {
      clearTimeout(timeout);
      this.unbusy();
    }, () => {
      clearTimeout(timeout);
      this.unbusy();
    });
  },

  /********** RUNTIME **********/

  updateRuntimeList: function() {
    let USBListNode = document.querySelector("#runtime-panel-usbruntime");
    let simulatorListNode = document.querySelector("#runtime-panel-simulators");
    while (USBListNode.hasChildNodes()) {
      USBListNode.firstChild.remove();
    }

    this.console.log("Found " + AppManager.runtimeList.usb.length + " USB devices.");
    this.console.log("Found " + AppManager.runtimeList.simulators.length + " simulators.");
    for (let runtime of AppManager.runtimeList.usb) {
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item runtime-panel-item-usbruntime";
      panelItemNode.setAttribute("label", runtime.getName());
      USBListNode.appendChild(panelItemNode);
      let r = runtime;
      panelItemNode.addEventListener("click", () => {
        this.hidePanels();
        this.connectToRuntime(r);
      }, true);
    }

    while (simulatorListNode.hasChildNodes()) {
      simulatorListNode.firstChild.remove();
    }
    for (let runtime of AppManager.runtimeList.simulators) {
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item runtime-panel-item-simulator";
      panelItemNode.setAttribute("label", runtime.getName());
      simulatorListNode.appendChild(panelItemNode);
      let r = runtime;
      panelItemNode.addEventListener("click", () => {
        this.hidePanels();
        this.connectToRuntime(r);
      }, true);
    }

  },

  connectToRuntime: function(runtime) {
    let name = runtime.getName();
    let promise = AppManager.connectToRuntime(runtime);
    this.busyUntil(promise, "connecting to runtime");
    promise.then(
      () => {this.console.success("Connected to " + name)},
      () => {this.console.error("Can't connect to " + name)});
  },

  updateRuntimeButton: function() {
    let buttonNode = document.querySelector("#runtime-panel-button");
    let labelNode = buttonNode.querySelector(".panel-button-label");
    if (!AppManager.selectedRuntime) {
      labelNode.setAttribute("value", Strings.GetStringFromName("runtimeButton_label"));
    } else {
      let name = AppManager.selectedRuntime.getName();
      labelNode.setAttribute("value", name);
    }
  },

  /********** PROJECTS **********/

  // Panel & button

  updateProjectButton: function() {
    let buttonNode = document.querySelector("#project-panel-button");
    let labelNode = buttonNode.querySelector(".panel-button-label");
    let imageNode = buttonNode.querySelector(".panel-button-image");

    let project = AppManager.selectedProject;

    if (!project) {
      buttonNode.classList.add("no-project");
      labelNode.setAttribute("value", Strings.GetStringFromName("projectButton_label"));
      imageNode.removeAttribute("src");
    } else {
      buttonNode.classList.remove("no-project");
      labelNode.setAttribute("value", project.name);
      imageNode.setAttribute("src", project.icon);
    }
  },

  // details.xhtml

  openProject: function() {
    let details = document.querySelector("#details");
    let project = AppManager.selectedProject;

    if (!project) {
      details.setAttribute("hidden", "true");
      return;
    }

    if (project.location) {
      Services.prefs.setCharPref("devtools.webide.lastprojectlocation", project.location);
    }

    details.removeAttribute("hidden");
  },

  /********** COMMANDS **********/

  updateCommands: function() {

    if (document.querySelector("window").classList.contains("busy")) {
      document.querySelector("#cmd_newApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_importPackagedApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_importHostedApp").setAttribute("disabled", "true");
      document.querySelector("#cmd_showProjectPanel").setAttribute("disabled", "true");
      document.querySelector("#cmd_showRuntimePanel").setAttribute("disabled", "true");
      document.querySelector("#cmd_removeProject").setAttribute("disabled", "true");
      document.querySelector("#cmd_disconnectRuntime").setAttribute("disabled", "true");
      document.querySelector("#cmd_showPermissionsTable").setAttribute("disabled", "true");
      document.querySelector("#cmd_takeScreenshot").setAttribute("disabled", "true");
      document.querySelector("#cmd_showRuntimeDetails").setAttribute("disabled", "true");
      document.querySelector("#cmd_play").setAttribute("disabled", "true");
      document.querySelector("#cmd_stop").setAttribute("disabled", "true");
      document.querySelector("#cmd_toggleToolbox").setAttribute("disabled", "true");
      return;
    }

    document.querySelector("#cmd_newApp").removeAttribute("disabled");
    document.querySelector("#cmd_importPackagedApp").removeAttribute("disabled");
    document.querySelector("#cmd_importHostedApp").removeAttribute("disabled");
    document.querySelector("#cmd_showProjectPanel").removeAttribute("disabled");
    document.querySelector("#cmd_showRuntimePanel").removeAttribute("disabled");

    document.querySelector("#runtime-panel-button").removeAttribute("active");

    // Action commands
    let playCmd = document.querySelector("#cmd_play");
    let stopCmd = document.querySelector("#cmd_stop");
    let debugCmd = document.querySelector("#cmd_toggleToolbox");

    if (!AppManager.selectedProject || AppManager.connection.status != Connection.Status.CONNECTED) {
      playCmd.setAttribute("disabled", "true");
      stopCmd.setAttribute("disabled", "true");
      debugCmd.setAttribute("disabled", "true");
    } else {
      let isProjectRunning = AppManager.isProjectRunning();
      if (isProjectRunning) {
        stopCmd.removeAttribute("disabled");
        debugCmd.removeAttribute("disabled");
      } else {
        stopCmd.setAttribute("disabled", "true");
        debugCmd.setAttribute("disabled", "true");
      }

      // If connected and a project is selected
      if (AppManager.selectedProject.type == "runtimeApp") {
        if (isProjectRunning) {
          playCmd.setAttribute("disabled", "true");
        } else {
          playCmd.removeAttribute("disabled");
        }
      } else {
        if (AppManager.selectedProject.errorsCount == 0) {
          playCmd.removeAttribute("disabled");
        } else {
          playCmd.setAttribute("disabled", "true");
        }
      }
      document.querySelector("#runtime-panel-button").setAttribute("active", "true");
    }

    // Remove command
    let removeCmdNode = document.querySelector("#cmd_removeProject");
    if (AppManager.selectedProject) {
      removeCmdNode.removeAttribute("disabled");
    } else {
      removeCmdNode.setAttribute("disabled", "true");
    }

    // Runtime commands
    let screenshotCmd = document.querySelector("#cmd_takeScreenshot");
    let permissionsCmd = document.querySelector("#cmd_showPermissionsTable");
    let detailsCmd = document.querySelector("#cmd_showRuntimeDetails");
    let disconnectCmd = document.querySelector("#cmd_disconnectRuntime");

    let box = document.querySelector("#runtime-actions");

    if (AppManager.connection.status == Connection.Status.CONNECTED) {
      screenshotCmd.removeAttribute("disabled");
      permissionsCmd.removeAttribute("disabled");
      disconnectCmd.removeAttribute("disabled");
      detailsCmd.removeAttribute("disabled");
      box.removeAttribute("hidden");
    } else {
      screenshotCmd.setAttribute("disabled", "true");
      permissionsCmd.setAttribute("disabled", "true");
      disconnectCmd.setAttribute("disabled", "true");
      detailsCmd.setAttribute("disabled", "true");
      box.setAttribute("hidden", "true");
    }

  },

  /********** TOOLBOX **********/

  onMessage: function(event) {
    // The custom toolbox sends a message to its parent
    // window.
    try {
      let json = JSON.parse(event.data);
      switch (json.name) {
        case "toolbox-close":
          this.closeToolboxUI();
          break;
      }
    } catch(e) { Cu.reportError(e); }
  },

  closeToolbox: function() {
    if (this.toolboxPromise) {
      this.toolboxPromise.then(toolbox => {
        toolbox.destroy();
        this.toolboxPromise = null;
      }, this.console.error);
    }
  },

  showToolbox: function(target) {
    if (this.toolboxIframe) {
      return;
    }

    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.removeAttribute("hidden");

    let iframe = document.createElement("iframe");
    document.querySelector("window").insertBefore(iframe, splitter.nextSibling);
    let host = devtools.Toolbox.HostType.CUSTOM;
    let options = { customIframe: iframe };
    this.toolboxIframe = iframe;

    let height = Services.prefs.getIntPref("devtools.toolbox.footer.height");
    iframe.height = height;

    document.querySelector("#action-button-debug").setAttribute("active", "true");

    return gDevTools.showToolbox(target, null, host, options);
  },

  closeToolboxUI: function() {
    let body = document.querySelector("#body");
    body.removeAttribute("hidden");

    Services.prefs.setIntPref("devtools.toolbox.footer.height", this.toolboxIframe.height);

    // We have to destroy the iframe, otherwise, the keybindings of webide don't work
    // properly anymore.
    this.toolboxIframe.remove();
    this.toolboxIframe = null;

    let splitter = document.querySelector(".devtools-horizontal-splitter");
    splitter.setAttribute("hidden", "true");
    document.querySelector("#action-button-debug").removeAttribute("active");
  },

  console: {
    _log: function(msg, classname) {
      let li = document.createElementNS(HTML, "p");
      li.textContent = msg;
      li.className = classname;
      UI.logNode.appendChild(li);
      UI.logNode.scrollTop = UI.logNode.scrollTopMax;
    },
    log: function(msg) {
      UI.console._log(msg, "log");
      console.log(msg);
    },
    warning: function(msg) {
      UI.console._log(msg, "warning");
      console.warning(msg);
    },
    error: function(msg) {
      UI.console._log(msg, "error");
      console.error(msg);
    },
    success: function(msg) {
      UI.console._log(msg, "success");
      console.log(msg);
    },
  },
}


let Cmds = {
  quit: function() {
    window.close();
  },

  newApp: function() {
    UI.hidePanels();
    let ret = {location:null};
    window.openDialog("chrome://webide/content/newapp.xul", "newapp", "chrome,modal", ret);
    if (!ret.location)
      return;
    let project = AppProjects.get(ret.location);
    UI.busyUntil(AppManager.validateProject(project).then(() => {
      UI.console.success("New project created at " + ret.location);
      AppManager.selectedProject = project;
    }, (e) => UI.console.error("Error while create new app: " + e)), "creating new app");;
  },

  importPackagedApp: function() {
    UI.hidePanels();
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, Strings.GetStringFromName("importPackagedApp_title"), Ci.nsIFilePicker.modeGetFolder);
    let res = fp.show();
    if (res == Ci.nsIFilePicker.returnCancel)
      return;
    UI.busyUntil(AppProjects.addPackaged(fp.file)
                            .then(project => AppManager.validateProject(project))
                            .then(project => AppManager.selectedProject = project)
                            .then(( ) => { UI.console.log("New project successfuly added") },
                                  (e) => { UI.console.error("Error while importing project: " + e) }),
                            "importing packaged app");
  },


  importHostedApp: function() {
    UI.hidePanels();
    let promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].getService(Ci.nsIPromptService);
    let ret = {value:null};
    promptService.prompt(window,
                         Strings.GetStringFromName("importHostedApp_title"),
                         Strings.GetStringFromName("importHostedApp_header"),
                         ret, null, {});
    let url = ret.value;
    if (!url)
      return;
    UI.busyUntil(AppProjects.addHosted(url)
                            .then(project => AppManager.validateProject(project))
                            .then(project => AppManager.selectedProject = project)
                            .then(( ) => { UI.console.log("New project successfuly added") },
                                  (e) => { UI.console.error("Error while importing project: " + e) }),
                            "importing hosted app");
  },


  showProjectPanel: function() {
    let panelNode = document.querySelector("#project-panel");
    let panelVboxNode = document.querySelector("#project-panel > vbox");
    let anchorNode = document.querySelector("#project-panel-button > .panel-button-anchor");
    let projectsNode = document.querySelector("#project-panel-projects");

    while (projectsNode.hasChildNodes()) {
      projectsNode.firstChild.remove();
    }

    AppProjects.load().then(() => {
      let projects = AppProjects.store.object.projects;
      for (let i = 0; i < projects.length; i++) {
        let project = projects[i];
        let panelItemNode = document.createElement("toolbarbutton");
        panelItemNode.className = "panel-item";
        projectsNode.appendChild(panelItemNode);
        panelItemNode.setAttribute("label", project.name || AppManager.DEFAULT_PROJECT_NAME);
        panelItemNode.setAttribute("image", project.icon || AppManager.DEFAULT_PROJECT_ICON);
        if (!project.validationStatus) {
          // The result of the validation process (storing names, icons, …) has never been
          // stored in the IndexedDB database. This happens when the project has been created
          // from the old app manager. We need to run the validation again and update the name
          // and icon of the app
          AppManager.validateProject(project).then(() => {
            panelItemNode.setAttribute("label", project.name);
            panelItemNode.setAttribute("image", project.icon);
          });
        }
        panelItemNode.addEventListener("click", () => {
          UI.hidePanels();
          AppManager.selectedProject = project;
        }, true);
      }

      window.setTimeout(() => {
        // Open the popup only when the projects are added.
        // Not doing it in the next tick can cause mis-calculations
        // of the size of the panel.
        panelNode.openPopup(anchorNode);
        panelVboxNode.scrollTop = 0;
      }, 0);
    }, UI.console.error);


    let runtimeappsHeaderNode = document.querySelector("#panel-header-runtimeapps");
    if (AppManager.connection.status == Connection.Status.CONNECTED) {
      runtimeappsHeaderNode.removeAttribute("hidden");
    } else {
      runtimeappsHeaderNode.setAttribute("hidden", "true");
    }

    let runtimeAppsNode = document.querySelector("#project-panel-runtimeapps");
    while (runtimeAppsNode.hasChildNodes()) {
      runtimeAppsNode.firstChild.remove();
    }

    UI.console.log("Found " + AppManager.webAppsStore.object.all.length + " apps");

    for (let i = 0; i < AppManager.webAppsStore.object.all.length; i++) {
      let app = AppManager.webAppsStore.object.all[i];
      let panelItemNode = document.createElement("toolbarbutton");
      panelItemNode.className = "panel-item";
      panelItemNode.setAttribute("label", app.name);
      panelItemNode.setAttribute("image", app.iconURL);
      runtimeAppsNode.appendChild(panelItemNode);
      panelItemNode.addEventListener("click", () => {
        UI.hidePanels();
        AppManager.selectedProject = {
          type: "runtimeApp",
          app: app,
          icon: app.iconURL,
          name: app.name
        };
      }, true);
    }
  },

  showRuntimePanel: function() {
    let panel = document.querySelector("#runtime-panel");
    let anchor = document.querySelector("#runtime-panel-button > .panel-button-anchor");
    panel.openPopup(anchor);
  },

  disconnectRuntime: function() {
    UI.busyUntil(AppManager.disconnectRuntime());
  },

  takeScreenshot: function() {
    UI.hidePanels();
    UI.busyUntil(AppManager.deviceFront.screenshotToDataURL().then(longstr => {
       return longstr.string().then(dataURL => {
         longstr.release().then(null, UI.console.error);
         UI.openInBrowser(dataURL);
       });
    }));
  },

  showPermissionsTable: function() {
    UI.hidePanels();
    UI.busyUntil(AppManager.deviceFront.getRawPermissionsTable().then(json => {
      let styleContent = "";
      styleContent += "body {background:white; font-family: monospace}";
      styleContent += "table {border-collapse: collapse}";
      styleContent += "th, td {padding: 5px; border: 1px solid #EEE}";
      styleContent += "th {min-width: 130px}";
      styleContent += "td {text-align: center}";
      styleContent += "th:first-of-type, td:first-of-type {text-align:left}";
      styleContent += ".permallow  {color:rgb(152, 207, 57)}";
      styleContent += ".permprompt {color:rgb(0,158,237)}";
      styleContent += ".permdeny   {color:rgb(204,73,8)}";
      let style = document.createElementNS(HTML, "style");
      style.textContent = styleContent;
      let table = document.createElementNS(HTML, "table");
      table.innerHTML = "<tr><th>Name</th><th>type:web</th><th>type:privileged</th><th>type:certified</th></tr>";
      let permissionsTable = json.rawPermissionsTable;
      for (let name in permissionsTable) {
        let tr = document.createElementNS(HTML, "tr");
        let td = document.createElementNS(HTML, "td");
        td.textContent = name;
        tr.appendChild(td);
        for (let type of ["app","privileged","certified"]) {
          let td = document.createElementNS(HTML, "td");
          if (permissionsTable[name][type] == json.ALLOW_ACTION) {
            td.textContent = "✓";
            td.className = "permallow";
          }
          if (permissionsTable[name][type] == json.PROMPT_ACTION) {
            td.textContent = "!";
            td.className = "permprompt";
          }
          if (permissionsTable[name][type] == json.DENY_ACTION) {
            td.textContent = "✕";
            td.className = "permdeny"
          }
          tr.appendChild(td);
        }
        table.appendChild(tr);
      }
      let body = document.createElementNS(HTML, "body");
      body.appendChild(style);
      body.appendChild(table);
      let url = "data:text/html;charset=utf-8,";
      url += encodeURIComponent(body.outerHTML);
      UI.openInBrowser(url);
    }), "showing permission table");
  },

  showRuntimeDetails: function() {
    UI.hidePanels();
    UI.busyUntil(AppManager.deviceFront.getDescription().then(json => {
      let styleContent = "";
      styleContent += "body {background:white; font-family: monospace}";
      styleContent += "table {border-collapse: collapse}";
      styleContent += "th, td {padding: 5px; border: 1px solid #EEE}";
      let style = document.createElementNS(HTML, "style");
      style.textContent = styleContent;
      let table = document.createElementNS(HTML, "table");
      for (let name in json) {
        let tr = document.createElementNS(HTML, "tr");
        let td = document.createElementNS(HTML, "td");
        td.textContent = name;
        tr.appendChild(td);
        td = document.createElementNS(HTML, "td");
        td.textContent = json[name];
        tr.appendChild(td);
        table.appendChild(tr);
      }
      let body = document.createElementNS(HTML, "body");
      body.appendChild(style);
      body.appendChild(table);
      let url = "data:text/html;charset=utf-8,";
      url += encodeURIComponent(body.outerHTML);
      UI.openInBrowser(url);
    }), "showing runtime details");

  },

  play: function() {
    switch(AppManager.selectedProject.type) {
      case "packaged":
      case "hosted":
        UI.busyUntil(AppManager.installAndRunProject(), "installing and running app");
        break;
      case "runtimeApp":
        UI.busyUntil(AppManager.runRuntimeApp(), "running app");
        break;
    }
  },

  stop: function() {
    UI.busyUntil(AppManager.stopRunningApp(), "stopping app");
  },

  toggleToolbox: function() {
    if (UI.toolboxIframe) {
      UI.closeToolbox();
    } else {
      UI.toolboxPromise = AppManager.getTarget().then((target) => {
        return UI.showToolbox(target);
      }, UI.console.error);
      UI.busyUntil(UI.toolboxPromise, "opening toolbox");
    }
  },

  removeProject: function() {
    AppManager.removeSelectedProject();
  },

  toggleEditors: function() {
    // Toggle Itchpad
  },
}
