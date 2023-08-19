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
    document.getElementById("backtogeneral__").addEventListener("command", () => {
      gotoPref("general");
    });
    getAllBackupedNotes().then(content => {
      const keys = Object.keys(content.data);
      for (let i = 0; i < keys.length; i++) {
        const elem = window.MozXULElement.parseXULToFragment(`
          <richlistitem>
            <label value="${convertToDateAndTime(Number(keys[i]))}" class="backup-date"/>
            <button class="restore-button" id="${keys[i]}" data-l10n-id="restore-button"/>
          </richlistitem>
        `);
        document.getElementById("backup-list").appendChild(elem);
        const elems = document.getElementsByClassName("restore-button");
        for (let j = 0; j < elems.length; j++) {
          elems[j].onclick = () => {
            restoreNote(elems[j].id);
          };
        }
      }
    });
  },
};
//convert timestamp to date and time
function convertToDateAndTime(timestamp) {
  const date = new Date(timestamp);
  const dateStr = date.toLocaleDateString();
  const timeStr = date.toLocaleTimeString();
  return `${dateStr} ${timeStr}`;
}

function getAllBackupedNotes() {
  const filePath = OS.Path.join(OS.Constants.Path.profileDir, "floorp_notes_backup.json");
  return OS.File.read(filePath, { encoding: "utf-8" })
    .then(content => {
      content = content.slice(0, -1) + "}}";
      return JSON.parse(content);
    });
}

async function restoreNote(timestamp) {
  const l10n = new Localization(["browser/floorp.ftl"], true);
  const prompts = Services.prompt;
  const check = { value: false };
  const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
  const result = prompts.confirmEx(null, l10n.formatValueSync("restore-from-backup-prompt-title"), `${l10n.formatValueSync("restore-from-this-backup")}\n${l10n.formatValueSync("backuped-time")}: ${convertToDateAndTime(Number(timestamp))}`, flags, "", null, "", null, check);
  if (result == 0) {
    const content = await getAllBackupedNotes();
    const note = `{${content.data[timestamp]}}`;
    Services.prefs.setCharPref("floorp.browser.note.memos", note);
  }
}