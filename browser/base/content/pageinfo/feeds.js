/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Via pageInfo.xul -> utilityOverlay.js
/* import-globals-from ../utilityOverlay.js */
/* import-globals-from ./pageInfo.js */

function initFeedTab(feeds) {
  for (const [name, type, url] of feeds) {
    addRow(name, type, url);
  }

  const feedListbox = document.getElementById("feedListbox");
  document.getElementById("feedTab").hidden = feedListbox.getRowCount() == 0;
}

function addRow(name, type, url) {
  const item = document.createElement("richlistitem");

  const top = document.createElement("hbox");
  top.setAttribute("flex", "1");
  item.appendChild(top);

  const bottom = document.createElement("hbox");
  bottom.setAttribute("flex", "1");
  item.appendChild(bottom);

  const nameLabel = document.createElement("label");
  nameLabel.className = "feedTitle";
  nameLabel.textContent = name;
  nameLabel.setAttribute("flex", "1");
  top.appendChild(nameLabel);

  const typeLabel = document.createElement("label");
  typeLabel.textContent = type;
  top.appendChild(typeLabel);

  const urlContainer = document.createElement("hbox");
  urlContainer.setAttribute("flex", "1");
  bottom.appendChild(urlContainer);

  const urlLabel = document.createElement("label");
  urlLabel.className = "text-link";
  urlLabel.textContent = url;
  urlLabel.setAttribute("tooltiptext", url);
  urlLabel.addEventListener("click", ev => openUILink(this.value, ev, {triggeringPrincipal: Services.scriptSecurityManager.createNullPrincipal({})}));
  urlContainer.appendChild(urlLabel);

  const subscribeButton = document.createElement("button");
  subscribeButton.className = "feed-subscribe";
  subscribeButton.addEventListener("click",
    () => openWebLinkIn(url, "current", { ignoreAlt: true }));
  subscribeButton.setAttribute("label", gBundle.getString("feedSubscribe"));
  subscribeButton.setAttribute("accesskey", gBundle.getString("feedSubscribe.accesskey"));
  bottom.appendChild(subscribeButton);

  document.getElementById("feedListbox").appendChild(item);
}
