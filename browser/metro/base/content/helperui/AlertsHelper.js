/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var AlertsHelper = {
  _timeoutID: -1,
  _listener: null,
  _cookie: "",
  _clickable: false,
  get container() {
    delete this.container;
    let container = document.getElementById("alerts-container");

    let self = this;
    container.addEventListener("transitionend", function() {
      self.alertTransitionOver();
    }, true);

    return this.container = container;
  },

  showAlertNotification: function ah_show(aImageURL, aTitle, aText, aTextClickable, aCookie, aListener) {
    this._clickable = aTextClickable || false;
    this._listener = aListener || null;
    this._cookie = aCookie || "";

    // Reset the container settings from the last time so layout can happen naturally
    let container = this.container;
    container.removeAttribute("width");
    let alertText = document.getElementById("alerts-text");
    alertText.style.whiteSpace = "";

    document.getElementById("alerts-image").setAttribute("src", aImageURL);
    document.getElementById("alerts-title").value = aTitle;
    alertText.textContent = aText;

    container.hidden = false;
    let bcr = container.getBoundingClientRect();
    if (bcr.width > window.innerWidth - 50) {
      // If the window isn't wide enough, we need to re-layout
      container.setAttribute("width", window.innerWidth - 50); // force a max width
      alertText.style.whiteSpace = "pre-wrap"; // wrap text as needed
      bcr = container.getBoundingClientRect(); // recalculate the bcr
    }
    container.setAttribute("width", bcr.width); // redundant but cheap
    container.setAttribute("height", bcr.height);

    container.classList.add("showing");

    let timeout = Services.prefs.getIntPref("alerts.totalOpenTime");
    let self = this;
    if (this._timeoutID)
      clearTimeout(this._timeoutID);
    this._timeoutID = setTimeout(function() { self._timeoutAlert(); }, timeout);
  },

  _timeoutAlert: function ah__timeoutAlert() {
    this._timeoutID = -1;

    this.container.classList.remove("showing");
    if (this._listener)
      this._listener.observe(null, "alertfinished", this._cookie);
  },

  alertTransitionOver: function ah_alertTransitionOver() {
    let container = this.container;
    if (!container.classList.contains("showing")) {
      container.height = 0;
      container.hidden = true;
    }
  },

  click: function ah_click(aEvent) {
    if (this._clickable && this._listener)
      this._listener.observe(null, "alertclickcallback", this._cookie);

    if (this._timeoutID != -1) {
      clearTimeout(this._timeoutID);
      this._timeoutAlert();
    }
  }
};
