/* global PKT_PANEL:false */

function onDOMLoaded() {
  if (!window.thePKT_PANEL) {
    var thePKT_PANEL = new PKT_PANEL();
    /* global thePKT_PANEL */
    window.thePKT_PANEL = thePKT_PANEL;
    thePKT_PANEL.initSaved();
  }
  window.thePKT_PANEL.create();
}

if (document.readyState != `loading`) {
  onDOMLoaded();
} else {
  document.addEventListener(`DOMContentLoaded`, onDOMLoaded);
}
