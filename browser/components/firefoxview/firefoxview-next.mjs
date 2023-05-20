/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let navigation = document.querySelector("#navigation > ul");
let pageList = [];

for (const item of navigation.children) {
  pageList.push(item.getAttribute("page"));
  item.addEventListener("click", function (event) {
    location.hash = event.target.getAttribute("page");
  });
}

function onHashChange() {
  changePage(document.location.hash.substring(1));
}

function changePage(page) {
  if (pageList.includes(page)) {
    document.getElementById("pages").selectedViewName = page;
  } else {
    document.location.hash = pageList[0];
  }
}

window.addEventListener("DOMContentLoaded", async () => {
  if (document.location.hash) {
    changePage(document.location.hash.substring(1));
  }
  window.addEventListener("hashchange", onHashChange);
});

document
  .getElementById("pages")
  .addEventListener("view-changed", async event => {
    for (const child of event.target.children) {
      if (child.getAttribute("name") == event.target.selectedViewName) {
        child.enter();
      } else {
        child.exit();
      }
    }
  });

window.addEventListener("unload", () => {});
