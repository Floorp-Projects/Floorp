# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
<html:div id="fullscreen-and-pointerlock-wrapper" renderroot="popover">
  <html:div id="fullscreen-warning" class="pointerlockfswarning" hidden="true">
    <html:div class="pointerlockfswarning-domain-text">
      &fullscreenWarning.beforeDomain.label;
      <html:span class="pointerlockfswarning-domain"/>
      &fullscreenWarning.afterDomain.label;
    </html:div>
    <html:div class="pointerlockfswarning-generic-text">
      &fullscreenWarning.generic.label;
    </html:div>
    <html:button id="fullscreen-exit-button"
                 onclick="FullScreen.exitDomFullScreen();">
#ifdef XP_MACOSX
            &exitDOMFullscreenMac.button;
#else
            &exitDOMFullscreen.button;
#endif
    </html:button>
  </html:div>

  <html:div id="pointerlock-warning" class="pointerlockfswarning" hidden="true">
    <html:div class="pointerlockfswarning-domain-text">
      &pointerlockWarning.beforeDomain.label;
      <html:span class="pointerlockfswarning-domain"/>
      &pointerlockWarning.afterDomain.label;
    </html:div>
    <html:div class="pointerlockfswarning-generic-text">
      &pointerlockWarning.generic.label;
    </html:div>
  </html:div>
</html:div>
