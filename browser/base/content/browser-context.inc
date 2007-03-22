# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla.org Code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

      <menuitem id="spell-no-suggestions"
                disabled="true"
                label="&spellNoSuggestions.label;"/>
      <menuitem id="spell-add-to-dictionary"
                label="&spellAddToDictionary.label;"
                accesskey="&spellAddToDictionary.accesskey;"
                oncommand="InlineSpellCheckerUI.addToDictionary();"/>
      <menuseparator id="spell-suggestions-separator"/>
      <menuitem id="context-openlink"
                label="&openLinkCmd.label;"
                accesskey="&openLinkCmd.accesskey;"
                oncommand="gContextMenu.openLink();"/>
      <menuitem id="context-openlinkintab"
                label="&openLinkCmdInTab.label;"
                accesskey="&openLinkCmdInTab.accesskey;"
                oncommand="gContextMenu.openLinkInTab();"/>
      <menuseparator id="context-sep-open"/>
      <menuitem id="context-bookmarklink"
                label="&bookmarkLinkCmd.label;"
                accesskey="&bookmarkLinkCmd.accesskey;"
                oncommand="gContextMenu.bookmarkLink();"/>
      <menuitem id="context-savelink"
                label="&saveLinkCmd.label;"
                accesskey="&saveLinkCmd.accesskey;"
                oncommand="gContextMenu.saveLink();"/>
      <menuitem id="context-sendlink"
                label="&sendLinkCmd.label;"
                accesskey="&sendLinkCmd.accesskey;"
                oncommand="gContextMenu.sendLink();"/>
      <menuitem id="context-copyemail"
                label="&copyEmailCmd.label;"
                accesskey="&copyEmailCmd.accesskey;"
                oncommand="gContextMenu.copyEmail();"/>
      <menuitem id="context-copylink"
                label="&copyLinkCmd.label;"
                accesskey="&copyLinkCmd.accesskey;"
                oncommand="goDoCommand('cmd_copyLink');"/>
      <menuseparator id="context-sep-copylink"/>
      <menuitem id="context-viewimage"
                label="&viewImageCmd.label;"
                accesskey="&viewImageCmd.accesskey;"
                oncommand="gContextMenu.viewImage(event);"
                onclick="checkForMiddleClick(this, event);"/>
#ifdef CONTEXT_COPY_IMAGE_CONTENTS
      <menuitem id="context-copyimage-contents"
                label="&copyImageContentsCmd.label;"
                accesskey="&copyImageContentsCmd.accesskey;"
                oncommand="goDoCommand('cmd_copyImageContents');"/>
#endif
      <menuitem id="context-copyimage"
                label="&copyImageCmd.label;"
                accesskey="&copyImageCmd.accesskey;"
                oncommand="goDoCommand('cmd_copyImageLocation');"/>
      <menuseparator id="context-sep-copyimage"/>
      <menuitem id="context-saveimage"
                label="&saveImageCmd.label;"
                accesskey="&saveImageCmd.accesskey;"
                oncommand="gContextMenu.saveImage();"/>
      <menuitem id="context-sendimage"  
                label="&sendImageCmd.label;" 
                accesskey="&sendImageCmd.accesskey;" 
                oncommand="gContextMenu.sendImage();"/>
      <menuitem id="context-setDesktopBackground"
                label="&setDesktopBackgroundCmd.label;"
                accesskey="&setDesktopBackgroundCmd.accesskey;"
                oncommand="gContextMenu.setDesktopBackground();"/>
      <menuitem id="context-blockimage"
                type="checkbox"
                accesskey="&blockImageCmd.accesskey;"
                oncommand="gContextMenu.toggleImageBlocking(this.getAttribute('checked') == 'true');"/>
      <menuitem id="context-back"
                label="&backCmd.label;"
                accesskey="&backCmd.accesskey;"
                command="Browser:Back"/>
      <menuitem id="context-forward"
                label="&forwardCmd.label;"
                accesskey="&forwardCmd.accesskey;"
                command="Browser:Forward"/>
      <menuitem id="context-reload"
                label="&reloadCmd.label;"
                accesskey="&reloadCmd.accesskey;"
                command="Browser:Reload"/>
      <menuitem id="context-stop"
                label="&stopCmd.label;"
                accesskey="&stopCmd.accesskey;"
                command="Browser:Stop"/>
      <menuseparator id="context-sep-stop"/>
      <menuitem id="context-bookmarkpage"
                label="&bookmarkPageCmd.label;"
                accesskey="&bookmarkPageCmd.accesskey;"
                oncommand="gContextMenu.bookmarkThisPage();"/>
      <menuitem id="context-savepage"
                label="&savePageCmd.label;"
                accesskey="&savePageCmd.accesskey2;"
                oncommand="gContextMenu.savePageAs();"/>
      <menuitem id="context-sendpage"  
                label="&sendPageCmd.label;" 
                accesskey="&sendPageCmd.accesskey;" 
                oncommand="gContextMenu.sendPage();"/>
      <menuseparator id="context-sep-viewbgimage"/>  
      <menuitem id="context-viewbgimage"
                label="&viewBGImageCmd.label;"
                accesskey="&viewBGImageCmd.accesskey;"
                oncommand="gContextMenu.viewBGImage(event);"
                onclick="checkForMiddleClick(this, event);"/>
      <menuitem id="context-undo"
                label="&undoCmd.label;"
                accesskey="&undoCmd.accesskey;"
                command="cmd_undo"/>
      <menuseparator id="context-sep-undo"/>
      <menuitem id="context-cut"
                label="&cutCmd.label;"
                accesskey="&cutCmd.accesskey;"
                command="cmd_cut"/>
      <menuitem id="context-copy"
                label="&copyCmd.label;"
                accesskey="&copyCmd.accesskey;"
                command="cmd_copy"/>
      <menuitem id="context-paste"
                label="&pasteCmd.label;"
                accesskey="&pasteCmd.accesskey;"
                command="cmd_paste"/>
      <menuitem id="context-delete"
                label="&deleteCmd.label;"
                accesskey="&deleteCmd.accesskey;"
                command="cmd_delete"/>
      <menuseparator id="context-sep-paste"/>
      <menuitem id="context-selectall"
                label="&selectAllCmd.label;"
                accesskey="&selectAllCmd.accesskey;"
                command="cmd_selectAll"/>      
      <menuseparator id="context-sep-selectall"/>
      <menuitem id="context-keywordfield"
                label="&keywordfield.label;"
                accesskey="&keywordfield.accesskey;"
                oncommand="AddKeywordForSearchField();"/>
      <menuitem id="context-searchselect"
                accesskey="&search.accesskey;"
                oncommand="BrowserSearch.loadSearch(getBrowserSelection(), true);"/>
      <menuseparator id="frame-sep"/>
      <menu id="frame" label="&thisFrameMenu.label;" accesskey="&thisFrameMenu.accesskey;">
        <menupopup>
          <menuitem label="&showOnlyThisFrameCmd.label;"
                    accesskey="&showOnlyThisFrameCmd.accesskey;"
                    oncommand="gContextMenu.showOnlyThisFrame();"/>
          <menuitem label="&openFrameCmd.label;"
                    accesskey="&openFrameCmd.accesskey;"
                    oncommand="gContextMenu.openFrame();"/>
          <menuitem label="&openFrameCmdInTab.label;"
                    accesskey="&openFrameCmdInTab.accesskey;"
                    oncommand="gContextMenu.openFrameInTab();"/>
          <menuseparator/>
          <menuitem label="&reloadFrameCmd.label;"
                    accesskey="&reloadFrameCmd.accesskey;"
                    oncommand="gContextMenu.reloadFrame();"/>
          <menuseparator/>
          <menuitem label="&bookmarkFrameCmd.label;"
                    accesskey="&bookmarkFrameCmd.accesskey;"
                    oncommand="gContextMenu.addBookmarkForFrame();"/>
          <menuitem label="&saveFrameCmd.label;"
                    accesskey="&saveFrameCmd.accesskey;"
                    oncommand="saveDocument(gContextMenu.target.ownerDocument);"/>
          <menuseparator/>
          <menuitem label="&printFrameCmd.label;"
                    accesskey="&printFrameCmd.accesskey;"
                    oncommand="gContextMenu.printFrame();"/>
          <menuseparator/>
          <menuitem label="&viewFrameSourceCmd.label;"
                    accesskey="&viewFrameSourceCmd.accesskey;"
                    oncommand="gContextMenu.viewFrameSource();"/>
          <menuitem label="&viewFrameInfoCmd.label;"
                    accesskey="&viewFrameInfoCmd.accesskey;"
                    oncommand="gContextMenu.viewFrameInfo();"/>
        </menupopup>
      </menu>
      <menuseparator id="context-sep-properties"/>
      <menuitem id="context-viewpartialsource-selection"
                label="&viewPartialSourceForSelectionCmd.label;"
                accesskey="&viewPartialSourceCmd.accesskey;"
                oncommand="gContextMenu.viewPartialSource('selection');"/>
      <menuitem id="context-viewpartialsource-mathml"
                label="&viewPartialSourceForMathMLCmd.label;"
                accesskey="&viewPartialSourceCmd.accesskey;"
                oncommand="gContextMenu.viewPartialSource('mathml');"/>
      <menuitem id="context-viewsource"
                label="&viewPageSourceCmd.label;"
                accesskey="&viewPageSourceCmd.accesskey;"
                oncommand="BrowserViewSourceOfDocument(content.document);"/>
      <menuitem id="context-viewinfo"
                label="&viewPageInfoCmd.label;"
                accesskey="&viewPageInfoCmd.accesskey;"
                oncommand="gContextMenu.viewInfo();"/>
      <menuitem id="context-metadata"
                label="&metadataCmd.label;"
                accesskey="&metadataCmd.accesskey;"
                oncommand="gContextMenu.showMetadata();"/>
      <menuseparator id="spell-separator"/>
      <menuitem id="spell-check-enabled"
                label="&spellEnable.label;"
                type="checkbox"
                accesskey="&spellEnable.accesskey;"
                oncommand="InlineSpellCheckerUI.toggleEnabled();"/>
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
                label="&bidiSwitchTextDirectionItem.label;"
                accesskey="&bidiSwitchTextDirectionItem.accesskey;"
                command="cmd_switchTextDirection"/>
      <menuitem hidden="true" id="context-bidi-page-direction-toggle"
                label="&bidiSwitchPageDirectionItem.label;"
                accesskey="&bidiSwitchPageDirectionItem.accesskey;"
                oncommand="SwitchDocumentDirection(window.content);"/>

