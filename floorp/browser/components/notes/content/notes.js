/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

document.addEventListener('DOMContentLoaded', function(){

  let { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

  //Clear using pref first

  Services.prefs.clearUserPref("floorp.browser.note.memos.using");

  const memoInput = document.getElementById("memo-input");
  const memoSave = document.getElementById("memo-save");
  const memoList = document.getElementById("memo-list");
  const memoTitleInput = document.getElementById("memo-title-input");
  const createNewMemo = document.getElementById("memo-add");
  const deleteMemo = document.getElementById("memo-delete");
  let l10n = new Localization(["browser/floorp.ftl"], true);

  memoTitleInput.placeholder = l10n.formatValueSync("memo-title-input-placeholder");
  memoInput.placeholder = l10n.formatValueSync("memo-input-placeholder");

  let memos = Services.prefs.getStringPref("floorp.browser.note.memos"); 
  if (memos !== "") {
    memos = JSON.parse(memos);
    showMemos();
  } else {
    memos = {"titles": [l10n.formatValueSync("memo-welcome-title")], "contents": [`${l10n.formatValueSync("memo-first-tip")}\n\n${l10n.formatValueSync("memo-second-tip")}\n${l10n.formatValueSync("memo-third-tip")}\n\n${l10n.formatValueSync("memo-fourth-tip")}`] };
    memoInput.value = memos.contents[0];
    memoTitleInput.value = memos.titles[0];
    Services.prefs.setStringPref("floorp.browser.note.memos", JSON.stringify(memos));
    Services.prefs.setIntPref("floorp.browser.note.memos.using", 0);
    showMemos();
  }

  function showMemos() {
    memoList.innerHTML = "";
    for (let i = 0; i < memos.titles.length; i++) {
      const li = document.createElement("div");
      li.setAttribute("class", "memo-list-item");
      li.textContent = memos.titles[i];
      memoList.appendChild(li);
    }
    memoListItemClick();
  }

  function setNoteID(id) {
    Services.prefs.setIntPref("floorp.browser.note.memos.using", id);
  }

  function returnNoteID() {
    return Services.prefs.getIntPref("floorp.browser.note.memos.using");
  }

  // メモリストのアイテムをクリックしたときの処理
  function memoListItemClick() {
    let elements = document.getElementsByClassName("memo-list-item");
    for (let i = 0; i < elements.length; i++) {
      elements[i].addEventListener("click", function() {
        memoInput.value = memos.contents[i];
        memoTitleInput.value = memos.titles[i];
        setNoteID(i);
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
    showMemos();
  }

  // 新規メモ作成ボタンがクリックされたときの処理
  createNewMemo.onclick = function() {
    memoInput.value = "";
    memoTitleInput.value = l10n.formatValueSync("memo-new-title");
    setNoteID(-1);
  }
  
  // メモの保存ボタンがクリックされたときの処理
  memoSave.onclick = function() {
    const memo = memoInput.value;
    const memoTitle = memoTitleInput.value;
    const openningNoteID = returnNoteID();

    if (openningNoteID == -1) {
      memos.contents.push(memo);
      if (memoTitle) {
        memos.titles.push(memoTitle);
      } else {
         memos.titles.push(memo.substr(0, 10)); // タイトルがない場合、メモの先頭10文字をタイトルとする
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
});