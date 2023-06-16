/* global PKT_PANEL:false */

function onDOMLoaded() {
  if (!window.thePKT_PANEL) {
    var thePKT_PANEL = new PKT_PANEL();
    /* global thePKT_PANEL */
    window.thePKT_PANEL = thePKT_PANEL;
    thePKT_PANEL.initStyleGuide();
  }
  window.thePKT_PANEL.overlay.create();

  setupDarkModeUI();
}

function setupDarkModeUI() {
  let isDarkModeEnabled = window?.matchMedia(
    `(prefers-color-scheme: dark)`
  ).matches;
  let elDarkModeToggle = document.querySelector(`#dark_mode_toggle input`);
  let elBody = document.querySelector(`body`);

  function setTheme() {
    if (isDarkModeEnabled) {
      elBody.classList.add(`theme_dark`);
      elDarkModeToggle.checked = true;
    } else {
      elBody.classList.remove(`theme_dark`);
      elDarkModeToggle.checked = false;
    }
  }

  setTheme();

  elDarkModeToggle.addEventListener(`click`, function (e) {
    e.preventDefault;
    isDarkModeEnabled = !isDarkModeEnabled;
    setTheme();
  });
}

if (document.readyState != `loading`) {
  onDOMLoaded();
} else {
  document.addEventListener(`DOMContentLoaded`, onDOMLoaded);
}
