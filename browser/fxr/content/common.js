/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Creates a modal container, if it doesn't exist, and adds the provided
// content element to it
function showModalContainer(content) {
  var container = document.getElementById("eModalContainer");
  if (container == null) {
    container = document.createElement("div");
    container.id = "eModalContainer";
    container.classList.add("modal_container");

    var mask = document.createElement("div");
    mask.id = "eModalMask";
    mask.classList.add("modal_mask");

    document.body.appendChild(mask);
    document.body.appendChild(container);
  } else {
    container.hidden = false;
    document.getElementById("eModalMask").hidden = false;
  }

  container.appendChild(content);
  if (content.classList.contains("modal_hide")) {
    content.classList.replace("modal_hide", "modal_content");
  } else {
    content.classList.add("modal_content");
  }
}

// Hides the modal container, and returns the contents back to the caller.
// The caller can choose to use the return value to move the contents to
// another part of the DOM, or ignore the return value so that the nodes
// can be garbage collected.
function clearModalContainer() {
  var container = document.getElementById("eModalContainer");
  container.hidden = true;
  document.getElementById("eModalMask").hidden = true;

  var content = container.firstElementChild;
  container.removeChild(content);
  content.classList.replace("modal_content", "modal_hide");

  return content;
}
