/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*---------------------------------------------------------------- Context Menu ----------------------------------------------------------------*/
let checkItems = []
let contextMenuObserver = new MutationObserver(contextMenuObserverFunc)
function addContextBox(id,l10n,insert,runFunction,checkID,checkedFunction){
  let contextMenu = document.createXULElement("menuitem");
  contextMenu.setAttribute("data-l10n-id",l10n);
  contextMenu.id = id;
  contextMenu.setAttribute("oncommand",runFunction);

  document.getElementById("contentAreaContextMenu").insertBefore(contextMenu,document.getElementById(insert));
  contextMenuObserver.observe(document.getElementById(checkID), {attributes:true})
  checkItems.push(checkedFunction)

  contextMenuObserverFunc();
}

function contextMenuObserverFunc(){
  for(const elem of checkItems) elem()
}

/********************* Share mode *********************************/

let beforeElem = document.getElementById("menu_openFirefoxView");
let addElem = window.MozXULElement.parseXULToFragment(`
    <menuitem data-l10n-id="sharemode-menuitem" type="checkbox" id="toggle_sharemode" checked="false"
          oncommand="addOrRemoveShareModeCSS();" accesskey="S">
    </menuitem>
`);
beforeElem.after(addElem);

function addOrRemoveShareModeCSS(){
  let CSSexist = document.getElementById("sharemode");

  if(CSSexist == null){
    let CSS = document.createElement("style");
    CSS.id = "sharemode";
    CSS.innerText = "@import url(chrome://browser/skin/options/sharemode.css)";
    document.head.appendChild(CSS);
  } else {
    document.getElementById("sharemode").remove();
  }
}