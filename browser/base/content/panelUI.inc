<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<panel id="PanelUI-popup"
       role="group"
       type="arrow"
       level="top"
       consumeoutsideclicks="true">
  <box id="PanelUI-container">
    <stack id="PanelUI-viewStack" view="main">
      <vbox id="PanelUI-mainView">
        <vbox class="PanelUI-pageControls" pack="center">
          <hbox class="PanelUI-editControls">
            <toolbarbutton id="PanelUI-cut-btn" flex="1"
                           label="&cutCmd.label;"
                           tooltiptext="&cutButton.tooltip;"
                           command="cmd_cut"/>
            <toolbarbutton id="PanelUI-copy-btn" flex="1"
                           label="&copyCmd.label;"
                           tooltiptext="&copyButton.tooltip;"
                           command="cmd_copy"/>
            <toolbarbutton id="PanelUI-paste-btn" flex="1"
                           label="&pasteCmd.label;"
                           tooltiptext="&pasteButton.tooltip;"
                           command="cmd_paste"/>
          </hbox>
          <hbox class="PanelUI-zoomControls">
            <toolbarbutton id="PanelUI-zoomOut-btn" flex="1"
                           noautoclose="true" label="&fullZoomReduceCmd.label;"
                           command="cmd_fullZoomReduce"
                           tooltiptext="&zoomOutButton.tooltip;"/>
            <toolbarbutton id="PanelUI-zoomReset-btn" flex="1"
                           noautoclose="true" label="&zoomReset.label;"
                           tooltiptext="&zoomReset.tooltip;"
                           command="cmd_fullZoomReset"/>
            <toolbarbutton id="PanelUI-zoomIn-btn" flex="1"
                           noautoclose="true" label="&fullZoomEnlargeCmd.label;"
                           command="cmd_fullZoomEnlarge"
                           tooltiptext="&zoomInButton.tooltip;"/>
          </hbox>
        </vbox>

        <vbox id="PanelUI-contents" type="grid"/>

        <vbox id="PanelUI-mainView-spring"/>

        <hbox class="PanelUI-footer" align="center">
          <toolbarbutton id="PanelUI-help-btn" label="&helpMenu.label;"
                         command="openHelpLink('firefox-help');"/>
          <spacer flex="1"/>
          <toolbarbutton id="PanelUI-customize-btn" label="&appMenuCustomize.label;"
                         command="cmd_CustomizeToolbars"/>
        </hbox>
      </vbox>

      <!-- Used to capture click events over the PanelUI-mainView if we're in
           subview mode. That way, any click on the PanelUI-mainView causes us
           to revert to the mainView mode, whereupon PanelUI-click-capture then
           allows click events to go through it. -->
      <box id="PanelUI-clickCapturer">
      </box>

      <!-- We manually set display: none (via a CSS attribute selector) on the
           subviews that are not being displayed. We're using this over a deck
           because a deck assumes the size of its largest child, regardless of
           whether or not it is shown. That's not good for our case, since we
           want to allow each subview to be uniquely sized. -->
      <box id="PanelUI-subViews">
        <vbox id="PanelUI-bookmarks" class="PanelUI-subView" flex="1">
          <label value="&appMenuBookmarks.label;"/>
          <vbox flex="1">
            <toolbarbutton label="&bookmarkThisPageCmd.label;"/>
            <toolbarbutton label="&viewBookmarksToolbar.label;"/>
            <toolbarbutton label="&showAllBookmarks2.label;"/>
            <spacer flex="1"/>
          </vbox>
          <vbox id="PanelUI-bookmarks-tall-maker"/>
        </vbox>
        <vbox id="PanelUI-history" flex="1">
          <label value="&appMenuHistory.label;"/>
          <vbox id="PanelUI-historyItems"/>
          <label value="&appMenuHistoryMore.label;"
                 id="PanelUI-historyMore"
                 class="text-link"
                 onclick="PlacesCommandHook.showPlacesOrganizer('History'); PanelUI.hide();"/>
        </vbox>
      </box>
    </stack>
  </box>
</panel>
