/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } =  ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp", ]);
});

const gNotesPane = {
  _pane: null,
  init() {
    this._pane = document.getElementById("paneNotes");
    document.getElementById("backtogeneral__").addEventListener("command", function () {
      gotoPref("general")
    });
    getAllBackupedNotes().then(content => {
      for (let i = 0; i < Object.keys(content.data).length; i++) {
        if(i > 9) {
          document.querySelectorAll(".backup-item")[0].remove();
        }
        let elem = window.MozXULElement.parseXULToFragment(`
              <richlistitem class="backup-item">
                <label value="${coventToDateAndTime(Number(Object.keys(content.data)[i]))}" class="backup-date"/>
                <button class="restore-button" id="${Object.keys(content.data)[i]}" data-l10n-id="restore-button"/>
              </richlistitem>
            `);
        document.getElementById("backup-list").appendChild(elem);
        let elems = document.getElementsByClassName("restore-button");
        for (let i = 0; i < elems.length; i++) {
          elems[i].onclick = function () {
            restoreNote(elems[i].id);
          }
        }
      }
    });
  },
};
//convert timestamp to date and time
function coventToDateAndTime(timestamp) {
  let date = new Date(timestamp);
  let dateStr = date.toLocaleDateString();
  let timeStr = date.toLocaleTimeString();
  return dateStr + " " + timeStr;
}

function getAllBackupedNotes() {
  const filePath = PathUtils.join(
    Services.dirsvc.get(
      "ProfD", Ci.nsIFile).path,
    "floorp_notes_backup.json"
  );
  const content = IOUtils.readUTF8(filePath).then(
    content => {
      content = content.slice(0, -1) + "}}";
      return JSON.parse(content);
    }
  );
  return content;
}

async function restoreNote(timestamp) {
  let l10n = new Localization(["browser/floorp.ftl"], true);
  const prompts = Services.prompt;
  const check = {
    value: false
  };
  const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
  let result = prompts.confirmEx(null, l10n.formatValueSync("restore-from-backup-prompt-title"), `${l10n.formatValueSync("restore-from-this-backup")}\n${l10n.formatValueSync("backuped-time")}: ${coventToDateAndTime(Number(timestamp))}`, flags, "", null, "", null, check);
  if (result == 0) {
    let content = await getAllBackupedNotes();
    let note = `{${content.data[timestamp]}}`;
    Services.prefs.setCharPref("floorp.browser.note.memos", note);
  }
}
