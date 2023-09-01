/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

var { AppConstants } =  ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
var { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
var WorkspaceUtils = ChromeUtils.importESModule(
  "resource:///modules/WorkspaceUtils.sys.mjs"
);


XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp", ]);
});

Preferences.addAll([
  { id: WorkspaceUtils.workspacesPreferences.WORKSPACE_CLOSE_POPUP_AFTER_CLICK_PREF, type: "bool" },
  { id: WorkspaceUtils.workspacesPreferences.WORKSPACE_MANAGE_ON_BMS_PREF, type: "bool" },
  { id: WorkspaceUtils.workspacesPreferences.WORKSPACE_SHOW_WORKSPACE_NAME_PREF, type: "bool" },
  { id: WorkspaceUtils.workspacesPreferences.WORKSPACE_CHANGE_WORKSPACE_WITH_DEFAULT_KEY_PREF, type: "bool"},
])

function convertToDateAndTime(timestamp) {
  const date = new Date(timestamp);
  const dateStr = date.toLocaleDateString();
  const timeStr = date.toLocaleTimeString();
  return `${dateStr} ${timeStr}`;
}

async function resetWorkspaces() {
  const l10n = new Localization(["browser/floorp.ftl"], true);
  const prompts = Services.prompt;
  const check = { value: false };
  const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
  const result = prompts.confirmEx(null, l10n.formatValueSync("workspaces-reset-service-title"), l10n.formatValueSync("workspaces-reset-warning"), flags, "", null, "", null, check);
  if (result == 0) {
    await Promise.all([
      Services.prefs.clearUserPref(WorkspaceUtils.workspacesPreferences.WORKSPACE_ALL_PREF),
      Services.prefs.clearUserPref(WorkspaceUtils.workspacesPreferences.WORKSPACE_CURRENT_PREF),
      Services.prefs.clearUserPref(WorkspaceUtils.workspacesPreferences.WORKSPACE_TABS_PREF),
    ]);
    Services.obs.notifyObservers([], "floorp-restart-browser");
  }
}

const gWorkspacesPane = {
  _pane: null,
  init() {
    this._pane = document.getElementById("paneWorkspaces");
    document.getElementById("backtogeneral-workspaces").addEventListener("command", () => {
      gotoPref("general")
    });

    const needreboot = document.getElementsByClassName("needreboot");
    for (let i = 0; i < needreboot.length; i++) {
      needreboot[i].addEventListener("click", () => {
        if (!Services.prefs.getBoolPref("floorp.enable.auto.restart", false)) {
          (async () => {
            let userConfirm = await confirmRestartPrompt(null)
            if (userConfirm == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
              Services.startup.quit(
                Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
              );
            }
          })()
        } else {
          window.setTimeout(() => {
            Services.startup.quit(Services.startup.eAttemptQuit | Services.startup.eRestart);
          }, 500);
        }
      });
    }

    document.getElementById("manageWorkspace-button").addEventListener("command", () => {
      gSubDialog.open(
        "chrome://browser/content/preferences/dialogs/manageWorkspace.xhtml",
      );
    });

    const resetButton = document.getElementById("reset-workspaces-button");
    resetButton.addEventListener("command", resetWorkspaces);

    // get workspace backups.
    const file = FileUtils.getFile("ProfD", ["floorp-workspace-backup.json"]);

    if (!file.exists()) {
      return;
    }

    const fstream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    fstream.init(file, -1, 0, 0);
    const inputStream = NetUtil.readInputStreamToString(
      fstream,
      fstream.available()
    );
    fstream.close();
    //Workspace saved in json format. a line save 1 json object.
    let lines = inputStream.split("\r");

    for(let i = 0; i < lines.length; i++) {
      let workspace = JSON.parse(lines[i]);

      let element = window.MozXULElement.parseXULToFragment(`
      <richlistitem>
        <label class="backup-date" value="${coventToDateAndTime(Number(Object.keys(workspace)[0]))}"></label>
        <spacer flex="1"/> 
        <button class="restore-workspaces-button" linenum="${i}"  data-l10n-id="restore-button"></button>
      </richlistitem>
     `);

      function insetElement() {
       const parentElement = document.getElementById("workspaces-backup-list");
       if (parentElement) {
         parentElement.appendChild(element);
         applyFunctions();
       } else {
         setTimeout(insertElement, 100);
       }
     }
      
     function applyFunctions() {
       const targets = document.getElementsByClassName("restore-workspaces-button");
       for (const target of targets) {
         target.onclick = () => {
           restoreWorkspaces(target.getAttribute("linenum"));
         };
       }
     }

     async function restoreWorkspaces(i) {
      const l10n = new Localization(["browser/floorp.ftl", "branding/brand.ftl"], true);
      const prompts = Services.prompt;
      const check = { value: false };
      const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
      const result = prompts.confirmEx(null, l10n.formatValueSync("workspaces-restore-service-title"), l10n.formatValueSync("workspaces-restore-warning"), flags, "", null, "", null, check);
      if (result == 0) {
        Services.obs.notifyObservers({lineNum: i}, 'backupWorkspace');
        await new Promise(resolve => setTimeout(resolve, 4000));
        Services.obs.notifyObservers([], "floorp-restart-browser");
      }
    }
      insetElement();
    }
  },  
};
