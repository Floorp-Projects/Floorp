/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

if(Services.prefs.getBoolPref("floorp.browser.native.verticaltabs.enabled", false)) {
    Services.prefs.setBoolPref("floorp.enable.multitab", true);
window.setTimeout(function () {
    document.getElementById("browser").insertBefore(document.getElementsByClassName("toolbar-items")[0], document.getElementById("browser").firstChild);
  
    document.getElementsByClassName("toolbar-items")[0].setAttribute("align", "start");
    document.getElementsByClassName("toolbar-items")[0].setAttribute("style", "display: block; max-width: 25em; min-width: 5em; overflow-y: scroll; overflow-x: hidden; min-height: 0px;");
  
    document.getElementById("tabbrowser-arrowscrollbox").setAttribute("orient", "vertical");
    document.getElementById("tabbrowser-arrowscrollbox").removeAttribute("overflowing");
    document.getElementById("tabbrowser-arrowscrollbox").removeAttribute("scrolledtostart")
    document.getElementById("tabbrowser-arrowscrollbox").disabled = true;
  
    document.getElementById("tabbrowser-tabs").setAttribute("orient", "vertical");
  
    document.getElementById("tabbrowser-arrowscrollbox").shadowRoot.querySelector(`[part="scrollbox"]`).setAttribute("orient", "vertical");
    let observer = new MutationObserver(function(){ document.getElementById("tabbrowser-arrowscrollbox").shadowRoot.querySelector(`[part="scrollbox"]`).removeAttribute("style");})
    observer.observe(document.getElementById("tabbrowser-arrowscrollbox").shadowRoot.querySelector(`[part="scrollbox"]`),{attributes:true})
  
    const splitterNode = window.MozXULElement.parseXULToFragment(
      `
      <splitter id="sidebar-splitter3" class="chromeclass-extrachrome" style="background: var(--toolbar-bgcolor);
                border: var(--win-sidebar-bgcolor) 0.1px solid;" hidden="false"/>
      `
   );
    document.getElementById("sidebar-box").before(splitterNode);
  }, 500);

var Tag = document.createElement("style");
Tag.textContent = `@import url("chrome://browser/content/browser-verticaltabs.css");`;
document.head.appendChild(Tag);

Services.prefs.addObserver("floorp.browser.native.verticaltabs.enabled", function () {
    if("floorp.browser.native.verticaltabs.enabled"){
        Services.prefs.setBoolPref("floorp.enable.multitab", false);
    }
}, false);
}