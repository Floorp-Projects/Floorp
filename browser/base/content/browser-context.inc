# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef XP_MACOSX
      <menuitem id="context-back"
                data-l10n-id="main-context-menu-back-mac"
                command="Browser:BackOrBackDuplicate"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-forward"
                data-l10n-id="main-context-menu-forward-mac"
                command="Browser:ForwardOrForwardDuplicate"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-reload"
                tooltip="dynamic-shortcut-tooltip"
                data-l10n-id="main-context-menu-reload-mac"
                command="Browser:ReloadOrDuplicate"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-stop"
                tooltip="dynamic-shortcut-tooltip"
                data-l10n-id="main-context-menu-stop-mac"
                command="Browser:Stop"/>
#else
      <menugroup id="context-navigation">
        <menuitem id="context-back"
                  data-l10n-id="main-context-menu-back-2"
                  data-l10n-args='{"shortcut":""}'
                  class="menuitem-iconic"
                  command="Browser:BackOrBackDuplicate"
                  onclick="checkForMiddleClick(this, event);"/>
        <menuitem id="context-forward"
                  data-l10n-id="main-context-menu-forward-2"
                  data-l10n-args='{"shortcut":""}'
                  class="menuitem-iconic"
                  command="Browser:ForwardOrForwardDuplicate"
                  onclick="checkForMiddleClick(this, event);"/>
        <menuitem id="context-reload"
                  class="menuitem-iconic"
                  tooltip="dynamic-shortcut-tooltip"
                  data-l10n-id="main-context-menu-reload"
                  command="Browser:ReloadOrDuplicate"
                  onclick="checkForMiddleClick(this, event);"/>
        <menuitem id="context-stop"
                  class="menuitem-iconic"
                  tooltip="dynamic-shortcut-tooltip"
                  data-l10n-id="main-context-menu-stop"
                  command="Browser:Stop"/>
        <menuitem id="context-bookmarkpage"
                  class="menuitem-iconic"
                  data-l10n-id="main-context-menu-bookmark-add"
                  oncommand="gContextMenu.bookmarkThisPage();"/>
      </menugroup>
#endif
      <menuseparator id="context-sep-navigation"/>
      <menuseparator id="page-menu-separator"/>
      <menuitem id="context-viewsource-goToLine"
                oncommand="gViewSourceUtils.getPageActor(gContextMenu.browser).promptAndGoToLine()"/>
      <menuitem id="context-viewsource-wrapLongLines"
                type="checkbox"
                oncommand="gViewSourceUtils.getPageActor(gContextMenu.browser).sendAsyncMessage('ViewSource:ToggleWrapping');"/>
      <menuitem id="context-viewsource-highlightSyntax"
                type="checkbox"
                oncommand="gViewSourceUtils.getPageActor(gContextMenu.browser).sendAsyncMessage('ViewSource:ToggleSyntaxHighlighting');"/>
      <menuseparator id="context-sep-viewsource-commands"/>
      <menuitem id="spell-no-suggestions"
                disabled="true"
                data-l10n-id="text-action-spell-no-suggestions"/>
      <menuitem id="spell-add-to-dictionary"
                label="&spellAddToDictionary.label;"
                accesskey="&spellAddToDictionary.accesskey;"
                oncommand="InlineSpellCheckerUI.addToDictionary();"/>
      <menuitem id="spell-undo-add-to-dictionary"
                label="&spellUndoAddToDictionary.label;"
                accesskey="&spellUndoAddToDictionary.accesskey;"
                oncommand="InlineSpellCheckerUI.undoAddToDictionary();" />
      <menuseparator id="spell-suggestions-separator"/>
      <menuitem id="context-openlinkincurrent"
                data-l10n-id="main-context-menu-open-link"
                oncommand="gContextMenu.openLinkInCurrent();"/>
# label and data-usercontextid are dynamically set.
      <menuitem id="context-openlinkincontainertab"
                accesskey="&openLinkCmdInTab.accesskey;"
                oncommand="gContextMenu.openLinkInTab(event);"/>
      <menuitem id="context-openlinkintab"
                data-l10n-id="main-context-menu-open-link-new-tab"
                data-usercontextid="0"
                oncommand="gContextMenu.openLinkInTab(event);"/>

      <menu id="context-openlinkinusercontext-menu"
            data-l10n-id="main-context-menu-open-link-container-tab"
            hidden="true">
        <menupopup oncommand="gContextMenu.openLinkInTab(event);"
                   onpopupshowing="return gContextMenu.createContainerMenu(event);" />
      </menu>

      <menuitem id="context-openlink"
                data-l10n-id="main-context-menu-open-link-new-window"
                oncommand="gContextMenu.openLink();"/>
      <menuitem id="context-openlinkprivate"
                data-l10n-id="main-context-menu-open-link-new-private-window"
                oncommand="gContextMenu.openLinkInPrivateWindow();"/>
      <menuseparator id="context-sep-open"/>
      <menuitem id="context-bookmarklink"
                data-l10n-id="main-context-menu-bookmark-link"
                oncommand="gContextMenu.bookmarkLink();"/>
      <menuitem id="context-savelink"
                data-l10n-id="main-context-menu-save-link"
                oncommand="gContextMenu.saveLink();"/>
      <menuitem id="context-savelinktopocket"
                data-l10n-id="main-context-menu-save-link-to-pocket"
                oncommand= "Pocket.savePage(gContextMenu.browser, gContextMenu.linkURL);"/>
      <menuitem id="context-copyemail"
                data-l10n-id="main-context-menu-copy-email"
                oncommand="gContextMenu.copyEmail();"/>
      <menuitem id="context-copylink"
                data-l10n-id="main-context-menu-copy-link-simple"
                oncommand="gContextMenu.copyLink();"/>
      <menu id="context-sendlinktodevice"
            class="sync-ui-item"
            data-l10n-id="main-context-menu-link-send-to-device"
            hidden="true">
            <menupopup id="context-sendlinktodevice-popup"
                  onpopupshowing="gSync.populateSendTabToDevicesMenu(event.target, gContextMenu.linkURL, gContextMenu.linkTextStr);"/>
      </menu>
      <menuseparator id="context-sep-sendlinktodevice" class="sync-ui-item"
                hidden="true"/>
      <menuseparator id="context-sep-copylink"/>
      <menuitem id="context-media-play"
                data-l10n-id="main-context-menu-media-play"
                oncommand="gContextMenu.mediaCommand('play');"/>
      <menuitem id="context-media-pause"
                data-l10n-id="main-context-menu-media-pause"
                oncommand="gContextMenu.mediaCommand('pause');"/>
      <menuitem id="context-media-mute"
                data-l10n-id="main-context-menu-media-mute"
                oncommand="gContextMenu.mediaCommand('mute');"/>
      <menuitem id="context-media-unmute"
                data-l10n-id="main-context-menu-media-unmute"
                oncommand="gContextMenu.mediaCommand('unmute');"/>
      <menu id="context-media-playbackrate" data-l10n-id="main-context-menu-media-play-speed-2">
        <menupopup>
          <menuitem id="context-media-playbackrate-050x"
                    data-l10n-id="main-context-menu-media-play-speed-slow-2"
                    type="radio"
                    name="playbackrate"
                    oncommand="gContextMenu.mediaCommand('playbackRate', 0.5);"/>
          <menuitem id="context-media-playbackrate-100x"
                    data-l10n-id="main-context-menu-media-play-speed-normal-2"
                    type="radio"
                    name="playbackrate"
                    checked="true"
                    oncommand="gContextMenu.mediaCommand('playbackRate', 1.0);"/>
          <menuitem id="context-media-playbackrate-125x"
                    data-l10n-id="main-context-menu-media-play-speed-fast-2"
                    type="radio"
                    name="playbackrate"
                    oncommand="gContextMenu.mediaCommand('playbackRate', 1.25);"/>
          <menuitem id="context-media-playbackrate-150x"
                    data-l10n-id="main-context-menu-media-play-speed-faster-2"
                    type="radio"
                    name="playbackrate"
                    oncommand="gContextMenu.mediaCommand('playbackRate', 1.5);"/>
          <menuitem id="context-media-playbackrate-200x"
                    data-l10n-id="main-context-menu-media-play-speed-fastest-2"
                    type="radio"
                    name="playbackrate"
                    oncommand="gContextMenu.mediaCommand('playbackRate', 2.0);"/>
        </menupopup>
      </menu>
      <menuitem id="context-media-loop"
                data-l10n-id="main-context-menu-media-loop"
                type="checkbox"
                oncommand="gContextMenu.mediaCommand('loop');"/>
      <menuitem id="context-leave-dom-fullscreen"
                data-l10n-id="main-context-menu-media-video-leave-fullscreen"
                oncommand="gContextMenu.leaveDOMFullScreen();"/>
      <menuitem id="context-video-fullscreen"
                data-l10n-id="main-context-menu-media-video-fullscreen"
                oncommand="gContextMenu.mediaCommand('fullscreen');"/>
      <menuitem id="context-media-hidecontrols"
                data-l10n-id="main-context-menu-media-hide-controls"
                oncommand="gContextMenu.mediaCommand('hidecontrols');"/>
      <menuitem id="context-media-showcontrols"
                data-l10n-id="main-context-menu-media-show-controls"
                oncommand="gContextMenu.mediaCommand('showcontrols');"/>
      <menuseparator id="context-media-sep-video-commands"/>
      <menuitem id="context-viewvideo"
                data-l10n-id="main-context-menu-video-view-new-tab"
                oncommand="gContextMenu.viewMedia(event);"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-video-pictureinpicture"
                data-l10n-id="main-context-menu-media-watch-pip"
                type="checkbox"
                oncommand="gContextMenu.mediaCommand('pictureinpicture');"/>
      <menuseparator id="context-media-sep-commands"/>
      <menuitem id="context-reloadimage"
                data-l10n-id="main-context-menu-image-reload"
                oncommand="gContextMenu.reloadImage();"/>
      <menuitem id="context-viewimage"
                data-l10n-id="main-context-menu-image-view-new-tab"
                oncommand="gContextMenu.viewMedia(event);"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-saveimage"
                data-l10n-id="main-context-menu-image-save-as"
                oncommand="gContextMenu.saveMedia();"/>
      <menuitem id="context-video-saveimage"
                data-l10n-id="main-context-menu-video-take-snapshot"
                oncommand="gContextMenu.saveVideoFrameAsImage();"/>
      <menuitem id="context-savevideo"
                data-l10n-id="main-context-menu-video-save-as"
                oncommand="gContextMenu.saveMedia();"/>
      <menuitem id="context-saveaudio"
                data-l10n-id="main-context-menu-audio-save-as"
                oncommand="gContextMenu.saveMedia();"/>
#ifdef CONTEXT_COPY_IMAGE_CONTENTS
      <menuitem id="context-copyimage-contents"
                data-l10n-id="main-context-menu-image-copy"
                oncommand="goDoCommand('cmd_copyImage');"/>
#endif
      <menuitem id="context-copyimage"
                data-l10n-id="main-context-menu-image-copy-link"
                oncommand="gContextMenu.copyMediaLocation();"/>
      <menuitem id="context-copyvideourl"
                data-l10n-id="main-context-menu-video-copy-link"
                oncommand="gContextMenu.copyMediaLocation();"/>
      <menuitem id="context-copyaudiourl"
                data-l10n-id="main-context-menu-audio-copy-link"
                oncommand="gContextMenu.copyMediaLocation();"/>
      <menuitem id="context-sendimage"
                data-l10n-id="main-context-menu-image-email"
                oncommand="gContextMenu.sendMedia();"/>
      <menuitem id="context-sendvideo"
                data-l10n-id="main-context-menu-video-email"
                oncommand="gContextMenu.sendMedia();"/>
      <menuitem id="context-sendaudio"
                data-l10n-id="main-context-menu-audio-email"
                oncommand="gContextMenu.sendMedia();"/>
      <menuitem id="context-viewimagedesc"
                data-l10n-id="main-context-menu-image-desc"
                oncommand="gContextMenu.viewImageDesc(event);"
                onclick="checkForMiddleClick(this, event);"/>
      <menuseparator id="context-sep-setbackground"/>
      <menuitem id="context-setDesktopBackground"
                data-l10n-id="main-context-menu-image-set-image-as-background"
                oncommand="gContextMenu.setDesktopBackground();"/>
      <menuitem id="context-ctp-play"
                data-l10n-id="main-context-menu-plugin-play"
                oncommand="gContextMenu.playPlugin();"/>
      <menuitem id="context-ctp-hide"
                data-l10n-id="main-context-menu-plugin-hide"
                oncommand="gContextMenu.hidePlugin();"/>
      <menuseparator id="context-sep-ctp"/>
#ifdef XP_MACOSX
      <menuitem id="context-bookmarkpage"
                data-l10n-id="main-context-menu-bookmark-add-mac"
                oncommand="gContextMenu.bookmarkThisPage();"/>
#endif
      <menuitem id="context-savepage"
                data-l10n-id="main-context-menu-page-save"
                oncommand="gContextMenu.savePageAs();"/>
      <menuitem id="context-pocket"
                data-l10n-id="main-context-menu-save-to-pocket"
                oncommand="Pocket.savePage(gContextMenu.browser, gContextMenu.browser.currentURI.spec, gContextMenu.browser.contentTitle);"/>
      <menu id="context-sendpagetodevice"
            class="sync-ui-item"
            data-l10n-id="main-context-menu-send-to-device"
            hidden="true">
        <menupopup id="context-sendpagetodevice-popup"
                   onpopupshowing="(() => { gSync.populateSendTabToDevicesMenu(event.target, gBrowser.currentURI.spec, gBrowser.contentTitle); })()"/>
      </menu>
      <menu id="fill-login" hidden="true">
        <menupopup id="fill-login-popup" />
      </menu>
      <menuitem id="fill-login-generated-password"
                data-l10n-id="main-context-menu-suggest-strong-password"
                hidden="true"
                oncommand="gContextMenu.useGeneratedPassword();"/>
      <menuitem id="manage-saved-logins"
                data-l10n-id="main-context-menu-manage-logins2"
                hidden="true"
                oncommand="gContextMenu.openPasswordManager();"/>
      <menuseparator id="passwordmgr-items-separator"/>
      <menuitem id="context-undo"
                data-l10n-id="text-action-undo"
                command="cmd_undo"/>
      <menuitem id="context-redo"
                data-l10n-id="text-action-redo"
                command="cmd_redo"/>
      <menuseparator id="context-sep-redo"/>
      <menuitem id="context-cut"
                data-l10n-id="text-action-cut"
                command="cmd_cut"/>
      <menuitem id="context-copy"
                data-l10n-id="text-action-copy"
                command="cmd_copy"/>
      <menuitem id="context-paste"
                data-l10n-id="text-action-paste"
                command="cmd_paste"/>
      <menuitem id="context-delete"
                data-l10n-id="text-action-delete"
                command="cmd_delete"/>
      <menuitem id="context-selectall"
                data-l10n-id="text-action-select-all"
                command="cmd_selectAll"/>
      <menuitem id="context-print-selection"
                data-l10n-id="main-context-menu-print-selection"
                oncommand="gContextMenu.printSelection();"/>
      <menuseparator id="context-sep-selectall"/>
      <menuitem id="context-take-screenshot"
                data-l10n-id="main-context-menu-take-screenshot"
                oncommand="gContextMenu.takeScreenshot();"/>
      <menuseparator id="context-sep-screenshots"/>
      <menuitem id="context-keywordfield"
                data-l10n-id="main-context-menu-keyword"
                oncommand="AddKeywordForSearchField();"/>
      <menuitem id="context-searchselect"
                oncommand="BrowserSearch.loadSearchFromContext(this.searchTerms, this.usePrivate, this.principal, this.csp, event);"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-searchselect-private"
                oncommand="BrowserSearch.loadSearchFromContext(this.searchTerms, true, this.principal, this.csp, event);"
                onclick="checkForMiddleClick(this, event);"/>

      <menuseparator id="frame-sep"/>
      <menu id="frame" data-l10n-id="main-context-menu-frame">
        <menupopup>
          <menuitem id="context-showonlythisframe"
                    data-l10n-id="main-context-menu-frame-show-this"
                    oncommand="gContextMenu.showOnlyThisFrame();"/>
          <menuitem id="context-openframeintab"
                    data-l10n-id="main-context-menu-frame-open-tab"
                    oncommand="gContextMenu.openFrameInTab();"/>
          <menuitem id="context-openframe"
                    data-l10n-id="main-context-menu-frame-open-window"
                    oncommand="gContextMenu.openFrame();"/>
          <menuseparator id="open-frame-sep"/>
          <menuitem id="context-reloadframe"
                    data-l10n-id="main-context-menu-frame-reload"
                    oncommand="gContextMenu.reloadFrame(event);"/>
          <menuseparator/>
          <menuitem id="context-bookmarkframe"
                    data-l10n-id="main-context-menu-frame-bookmark"
                    oncommand="gContextMenu.addBookmarkForFrame();"/>
          <menuitem id="context-saveframe"
                    data-l10n-id="main-context-menu-frame-save-as"
                    oncommand="gContextMenu.saveFrame();"/>
          <menuseparator/>
          <menuitem id="context-printframe"
                    data-l10n-id="main-context-menu-frame-print"
                    oncommand="gContextMenu.printFrame();"/>
          <menuseparator/>
          <menuitem id="context-viewframesource"
                    data-l10n-id="main-context-menu-frame-view-source"
                    oncommand="gContextMenu.viewFrameSource();"/>
          <menuitem id="context-viewframeinfo"
                    data-l10n-id="main-context-menu-frame-view-info"
                    oncommand="gContextMenu.viewFrameInfo();"/>
#ifdef NIGHTLY_BUILD
          <menuitem id="context-frameOsPid"
                    label="PID: Unknown"
                    disabled="true"/>
#endif
        </menupopup>
      </menu>
      <menuseparator id="spell-separator"/>
      <menuitem id="spell-check-enabled"
                label="&spellCheckToggle.label;"
                type="checkbox"
                accesskey="&spellCheckToggle.accesskey;"
                oncommand="InlineSpellCheckerUI.toggleEnabled(window);"/>
      <menuitem id="spell-add-dictionaries-main"
                label="&spellAddDictionaries.label;"
                accesskey="&spellAddDictionaries.accesskey;"
                oncommand="gContextMenu.addDictionaries();"/>
      <menu id="spell-dictionaries"
            label="&spellDictionaries.label;"
            accesskey="&spellDictionaries.accesskey;">
          <menupopup id="spell-dictionaries-menu">
              <menuseparator id="spell-language-separator"/>
              <menuitem id="spell-add-dictionaries"
                        label="&spellAddDictionaries.label;"
                        accesskey="&spellAddDictionaries.accesskey;"
                        oncommand="gContextMenu.addDictionaries();"/>
          </menupopup>
      </menu>
      <menuseparator hidden="true" id="context-sep-bidi"/>
      <menuitem hidden="true" id="context-bidi-text-direction-toggle"
                data-l10n-id="main-context-menu-bidi-switch-text"
                command="cmd_switchTextDirection"/>
      <menuitem hidden="true" id="context-bidi-page-direction-toggle"
                data-l10n-id="main-context-menu-bidi-switch-page"
                oncommand="gContextMenu.switchPageDirection();"/>
      <menuseparator id="inspect-separator" hidden="true"/>
      <menuitem id="context-viewpartialsource-selection"
                data-l10n-id="main-context-menu-view-selection-source"
                oncommand="gContextMenu.viewPartialSource();"/>
      <menuitem id="context-viewsource"
                data-l10n-id="main-context-menu-view-page-source"
                oncommand="BrowserViewSource(gContextMenu.browser);"/>
      <menuitem id="context-inspect-a11y"
                hidden="true"
                data-l10n-id="main-context-menu-inspect-a11y-properties"
                oncommand="gContextMenu.inspectA11Y();"/>
      <menuitem id="context-inspect"
                hidden="true"
                data-l10n-id="main-context-menu-inspect"
                oncommand="gContextMenu.inspectNode();"/>
      <menuseparator id="context-media-eme-separator" hidden="true"/>
      <menuitem id="context-media-eme-learnmore"
                class="menuitem-iconic"
                hidden="true"
                data-l10n-id="main-context-menu-eme-learn-more"
                oncommand="gContextMenu.drmLearnMore(event);"
                onclick="checkForMiddleClick(this, event);"/>
