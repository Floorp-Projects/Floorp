# -*- Mode: HTML; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# vim: set ts=2 et sw=2 tw=80:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

<script type="application/javascript"
        src="chrome://browser/content/downloads/downloads.js"/>
<script type="application/javascript"
        src="chrome://browser/content/downloads/indicator.js"/>

<popupset>
  <!-- The downloads indicator is placed in its final toolbar location
       programmatically, and can be shown temporarily even when its
       placeholder is removed from the toolbars.  Its initial location within
       the document must not be a toolbar or the toolbar palette, otherwise the
       toolbar handling code could remove it from the document. -->
  <toolbarbutton id="downloads-indicator"
                 class="toolbarbutton-1 chromeclass-toolbar-additional"
                 tooltiptext="&indicator.tooltiptext;"
                 collapsed="true"
                 oncommand="DownloadsIndicatorView.onCommand(event);"
                 ondrop="DownloadsIndicatorView.onDrop(event);"
                 ondragover="DownloadsIndicatorView.onDragOver(event);"
                 ondragenter="DownloadsIndicatorView.onDragOver(event);"
                 ondragleave="DownloadsIndicatorView.onDragLeave(event);">
    <!-- The panel's anchor area is smaller than the outer button, but must
         always be visible and must not move or resize when the indicator
         state changes, otherwise the panel could change its position or lose
         its arrow unexpectedly. -->
    <stack id="downloads-indicator-anchor">
      <vbox id="downloads-indicator-progress-area"
            pack="center">
        <description id="downloads-indicator-counter"/>
        <progressmeter id="downloads-indicator-progress"
                       class="plain"
                       min="0"
                       max="100"/>
      </vbox>
      <vbox id="downloads-indicator-icon"/>
      <vbox id="downloads-indicator-notification"/>
    </stack>
  </toolbarbutton>
</popupset>
