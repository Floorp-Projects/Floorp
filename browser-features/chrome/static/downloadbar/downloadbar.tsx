// SPDX-License-Identifier: MPL-2.0

import downloadbarStyle from "./downloadbar.css?inline";

export function DonwloadBar() {
  return (
    <>
      <xul:vbox
        id="downloadsPanel"
        data-l10n-id="downloads-panel"
        class="cui-widget-panel panel-no-padding"
        role="group"
        type="arrow"
        orient="horizontal"
      >
        <xul:linkset>
          <link rel="localization" href="browser/downloads.ftl" />
        </xul:linkset>

        <xul:menupopup id="downloadsContextMenu" class="download-state">
          <xul:menuitem
            command="downloadsCmd_pauseResume"
            class="downloadPauseMenuItem"
            data-l10n-id="downloads-cmd-pause"
          />
          <xul:menuitem
            command="downloadsCmd_pauseResume"
            class="downloadResumeMenuItem"
            data-l10n-id="downloads-cmd-resume"
          />
          <xul:menuitem
            command="downloadsCmd_unblock"
            class="downloadUnblockMenuItem"
            data-l10n-id="downloads-cmd-unblock"
          />
          <xul:menuitem
            command="downloadsCmd_openInSystemViewer"
            class="downloadUseSystemDefaultMenuItem"
            data-l10n-id="downloads-cmd-use-system-default"
          />
          <xul:menuitem
            command="downloadsCmd_alwaysOpenInSystemViewer"
            type="checkbox"
            class="downloadAlwaysUseSystemDefaultMenuItem"
            data-l10n-id="downloads-cmd-always-use-system-default"
          />
          <xul:menuitem
            command="downloadsCmd_alwaysOpenSimilarFiles"
            type="checkbox"
            class="downloadAlwaysOpenSimilarFilesMenuItem"
            data-l10n-id="downloads-cmd-always-open-similar-files"
          />
          <xul:menuitem
            command="downloadsCmd_show"
            class="downloadShowMenuItem"
            data-l10n-id="downloads-cmd-show-menuitem-2"
          />

          <xul:menuseparator class="downloadCommandsSeparator" />

          <xul:menuitem
            command="downloadsCmd_openReferrer"
            class="downloadOpenReferrerMenuItem"
            data-l10n-id="downloads-cmd-go-to-download-page"
          />
          <xul:menuitem
            command="downloadsCmd_copyLocation"
            class="downloadCopyLocationMenuItem"
            data-l10n-id="downloads-cmd-copy-download-link"
          />

          <xul:menuseparator />

          <xul:menuitem
            command="downloadsCmd_deleteFile"
            class="downloadDeleteFileMenuItem"
            data-l10n-id="downloads-cmd-delete-file"
          />
          <xul:menuitem
            command="cmd_delete"
            class="downloadRemoveFromHistoryMenuItem"
            data-l10n-id="downloads-cmd-remove-from-history"
          />
          <xul:menuitem
            command="downloadsCmd_clearList"
            data-l10n-id="downloads-cmd-clear-list"
          />
          <xul:menuitem
            command="downloadsCmd_clearDownloads"
            hidden
            data-l10n-id="downloads-cmd-clear-downloads"
          />
        </xul:menupopup>
        <xul:vbox
          id="downloadsPanel-multiView"
          mainViewId="downloadsPanel-mainView"
          disablekeynav="true"
        >
          <xul:hbox id="downloadsPanel-mainView">
            <xul:vbox class="panel-view-body-unscrollable">
              <div id="downloadsPanel-list">
                <xul:richlistbox
                  id="downloadsListBox"
                  data-l10n-id="downloads-panel-items"
                  data-l10n-attrs="style"
                  context="downloadsContextMenu"
                  onmouseover="DownloadsView.onDownloadMouseOver(event);"
                  onmouseout="DownloadsView.onDownloadMouseOut(event);"
                  oncontextmenu="DownloadsView.onDownloadContextMenu(event);"
                  ondragstart="DownloadsView.onDownloadDragStart(event);"
                />
                <xul:description
                  id="emptyDownloads"
                  data-l10n-id="downloads-panel-empty"
                />
                <div id="downloadsPanel-button">
                  <xul:toolbarbutton
                    oncommand="DownloadsPanel.showDownloadsHistory();"
                    class="toolbarbutton-1 subviewbutton"
                    id="downloadIcon"
                  />
                  <xul:toolbarbutton
                    command="downloadsCmd_clearList"
                    class="toolbarbutton-1 subviewbutton"
                    id="closeIcon"
                  />
                </div>
              </div>
            </xul:vbox>
            <xul:vbox id="downloadsFooter">
              <xul:stack>
                <xul:hbox
                  id="downloadsSummary"
                  align="center"
                  orient="horizontal"
                  onkeydown="DownloadsSummary.onKeyDown(event);"
                  onclick="DownloadsSummary.onClick(event);"
                >
                  <xul:image class="downloadTypeIcon" />
                  <xul:vbox pack="center" flex="1" class="downloadContainer">
                    <xul:description id="downloadsSummaryDescription" />
                    <progress
                      id="downloadsSummaryProgress"
                      class="downloadProgress"
                      max="100"
                    />
                    <xul:description id="downloadsSummaryDetails" crop="end" />
                  </xul:vbox>
                </xul:hbox>
                <xul:vbox id="downloadsFooterButtons">
                  <xul:toolbarseparator />
                  <xul:button
                    type="button"
                    id="downloadsHistory"
                    data-l10n-id="downloads-history"
                    class="downloadsPanelFooterButton subviewbutton panel-subview-footer-button toolbarbutton-1"
                    flex="1"
                    oncommand="DownloadsPanel.showDownloadsHistory();"
                    pack="start"
                  />
                </xul:vbox>
              </xul:stack>
            </xul:vbox>
          </xul:hbox>
        </xul:vbox>
      </xul:vbox>
      <style class="nora-statusbar">
        {downloadbarStyle}
      </style>
    </>
  );
}
