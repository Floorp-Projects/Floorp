/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.addEventListener('DOMContentLoaded', function(){

  let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
  const memoInput = document.getElementById("memo-input");
  const memoSave = document.getElementById("memo-save");
  const memoList = document.getElementById("memo-list");
  const memoTitleInput = document.getElementById("memo-title-input");
  const createNewMemo = document.getElementById("memo-add");
  const deleteMemo = document.getElementById("memo-delete");
  const HTMLPreview = document.getElementById("html-output");
  const memoMarkDownPreViewButton = document.getElementById("memo-markdown-preview");
  const l10n = new Localization(["browser/floorp.ftl"], true);
  const offlineLabel = document.getElementById("offline-label");

  //Clear using pref first
  Services.prefs.clearUserPref("floorp.browser.note.memos.using");
  memoInput.value = "";
  memoTitleInput.value = "";

  memoTitleInput.placeholder = l10n.formatValueSync("memo-title-input-placeholder");
  memoInput.placeholder = l10n.formatValueSync("memo-input-placeholder");

  let memos = Services.prefs.getStringPref("floorp.browser.note.memos");
  if (memos !== "") {
    memos = JSON.parse(memos);
    showMemos();
  } else {
    memos = {"titles": [l10n.formatValueSync("memo-welcome-title")], "contents": [`${l10n.formatValueSync("memo-first-tip")}\n\n${l10n.formatValueSync("memo-second-tip")}\n${l10n.formatValueSync("memo-third-tip")}`] };
    memoInput.value = memos.contents[0];
    memoTitleInput.value = memos.titles[0];
    Services.prefs.setStringPref("floorp.browser.note.memos", JSON.stringify(memos));
    Services.prefs.setIntPref("floorp.browser.note.memos.using", 0);
    showMemos();
  }

  if (!window.navigator.onLine) {
    whenBrowserIsOffline();
  }

  window.addEventListener('online', (e)=>{
    whenBrowserIsOnline();
  })
  
  window.addEventListener('offline', (e)=>{
    whenBrowserIsOffline();
  })

  function showMemos() {
    memoList.innerHTML = "";
    for (let i = 0; i < memos.titles.length; i++) {
      const li = document.createElement("div");
      li.setAttribute("class", "memo-list-item");
      li.textContent = memos.titles[i];
      memoList.appendChild(li);
    }
    updateSelectedItem(returnNoteID());
    memoListItemClick();
  }

  function setNoteID(id) {
    Services.prefs.setIntPref("floorp.browser.note.memos.using", id);
    updateSelectedItem(id);
  }

  function returnNoteID() {
    return Services.prefs.getIntPref("floorp.browser.note.memos.using");
  }

  function updateSelectedItem(id) {
    const oldSelectedItem = document.querySelector(".memo-list-item.selected, #memo-add.selected");
    if (oldSelectedItem != null) oldSelectedItem.classList.remove("selected");
    if (id != -1) {
      const newSelectedItem = document.querySelector(`.memo-list-item:nth-child(${id + 1})`);
      if (newSelectedItem != null) newSelectedItem.classList.add("selected");
    } else {
      const memoNewButton = document.querySelector("#memo-add");
      memoNewButton.classList.add("selected");
    }
  }

  // メモリストのアイテムをクリックしたときの処理
  function memoListItemClick() {
    let elements = document.getElementsByClassName("memo-list-item");
    for (let i = 0; i < elements.length; i++) {
      elements[i].addEventListener("click", function() {
        memoInput.value = memos.contents[i];
        memoTitleInput.value = memos.titles[i];
        setNoteID(i);
        hideMarkDownPreview();
      });
    }
  }

  //メモを削除する
  deleteMemo.onclick = function() {
    let openningNoteID = returnNoteID();
    memos.contents.splice(openningNoteID, 1);
    memos.titles.splice(openningNoteID, 1);
    Services.prefs.setStringPref("floorp.browser.note.memos", JSON.stringify(memos));
    memoInput.value = "";
    memoTitleInput.value = "";
    hideMarkDownPreview();
    setNoteID(-1);
    showMemos();
  }

  // 新規メモ作成ボタンがクリックされたときの処理
  createNewMemo.onclick = function() {
    memoInput.value = "";
    let DefaultNewTitle = l10n.formatValueSync("memo-new-title");
    if (memos.titles.includes(DefaultNewTitle)) {
      let i = 1;
      while (memos.titles.includes(DefaultNewTitle + ` (${i})`)) {
        i++;
      }
      memoTitleInput.value = DefaultNewTitle + ` (${i})`;
    } else {
      memoTitleInput.value = DefaultNewTitle;
    }
    setNoteID(-1);
  }
  
  // メモの保存ボタンがクリックされたときの処理
  memoSave.onclick = function() {
    saveNotes();
  }

  // メモのマークダウンプレビューボタンがクリックされたときの処理
  memoMarkDownPreViewButton.onclick = function() {
    if (HTMLPreview.style.display == "none") {
      showMarkDownPreview();
    } else {
      hideMarkDownPreview();
    }
  }
  memoInput.addEventListener("input", saveNotes);
  memoTitleInput.addEventListener("input", saveNotes);

  function saveNotes() {
    const memo = memoInput.value;
    const memoTitle = memoTitleInput.value;
    const openningNoteID = returnNoteID();
  
    if (openningNoteID == -1) {
      memos.contents.push(memo);
      if (memoTitle) {
        memos.titles.push(memoTitle);
      } else {
         memos.titles.push(l10n.formatValueSync("memo-new-title"));
      }
      Services.prefs.setStringPref("floorp.browser.note.memos", JSON.stringify(memos));
      let currentNoteID = memos.contents.length - 1;
      setNoteID(currentNoteID);
    } else {
      memos.contents[openningNoteID] = memo;
      if (memoTitle) {
        memos.titles[openningNoteID] = memoTitle;
      } else {
        memos.titles[openningNoteID] = memoInput.value.substr(0, 10);
      }
      Services.prefs.setStringPref("floorp.browser.note.memos", JSON.stringify(memos));
    }
    showMemos();
  };

  function convertMarkdownToHtml(markdown) {
    const converter = new showdown.Converter();
    return converter.makeHtml(markdown);
  }

  function showMarkDownPreview() {
    const memo = memoInput.value;
    if (!memo == "") {
      HTMLPreview.style.display = "block";
      memoInput.style.display = "none";
      memoSave.style.display = "none";
      HTMLPreview.innerHTML = convertMarkdownToHtml(memo);
    }
  }

  function hideMarkDownPreview() {
    HTMLPreview.style.display = "none";
    memoInput.style.display = "block";
    memoSave.style.display = "block";
    HTMLPreview.innerHTML = "";
  }

  function whenBrowserIsOffline() {
    memoInput.readOnly = true;
    memoTitleInput.readOnly = true;
    memoSave.disabled = true;
    createNewMemo.disabled = true;
    deleteMemo.disabled = true;
    offlineLabel.hidden = false;
  }

  function whenBrowserIsOnline() {
    memoInput.readOnly = false;
    memoTitleInput.readOnly = false;
    memoSave.disabled = false;
    createNewMemo.disabled = false;
    deleteMemo.disabled = false;
    offlineLabel.hidden = true;
  }
});