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
# The Original Code is the Shared Places Commands.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2005-2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Ben Goodger <beng@google.com>
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

<commandset id="placesCommands" 
            commandupdater="true" events="focus,select,mousedown,click,keypress" 
            oncommandupdate="PlacesController.onCommandUpdate()">
  <command id="placesCmd_select:all" 
           label="&cmd.select_all.label;" accesskey="&cmd.select_all.accesskey;"
           oncommand="PlacesController.selectAll();"/>
  <commandset readonly="true">
    <command id="placesCmd_edit:copy" 
             label="&cmd.edit_copy.label;" accesskey="&cmd.edit_copy.accesskey;"
             oncommand="PlacesController.copy();"/>
    <command id="placesCmd_show:info" 
             oncommand="PlacesController.showBookmarkPropertiesForSelection();"
#ifdef XP_MACOSX
             label="&cmd.show_infoMac.label;" 
             accesskey="&cmd.show_infoMac.accesskey;"/>
#else
             label="&cmd.show_infoWin.label;" 
             accesskey="&cmd.show_infoWin.accesskey;"/>
#endif

  </commandset>
  <commandset>
    <command id="placesCmd_edit:cut" 
             label="&cmd.edit_cut.label;" accesskey="&cmd.edit_cut.accesskey;"
             oncommand="PlacesController.cut();"/>
    <command id="placesCmd_edit:paste" 
             label="&cmd.edit_paste.label;" accesskey="&cmd.edit_paste.accesskey;"
             oncommand="PlacesController.paste();"/>
    <command id="placesCmd_edit:delete" 
             label="&cmd.edit_delete.label;" accesskey="&cmd.edit_delete.accesskey;"
             oncommand="PlacesController.remove('Remove Selection');"/>
    <command id="placesCmd_rename" label="&cmd.rename.label;" accesskey="&cmd.rename.accesskey;"/>
  </commandset>
  <commandset type="link" readonly="true">
    <command id="placesCmd_open" label="&cmd.open.label;" accesskey="&cmd.open.accesskey;"
             oncommand="PlacesController.openLinkInCurrentWindow();"/>
    <command id="placesCmd_open:window" label="&cmd.open_window.label;" accesskey="&cmd.open_window.accesskey;"
             oncommand="PlacesController.openLinkInNewWindow();"/>
    <command id="placesCmd_open:tab" label="&cmd.open_tab.label;" accesskey="&cmd.open_tab.accesskey;"
             oncommand="PlacesController.openLinkInNewTab();"/>
  </commandset>
  <commandset type="link">
  </commandset>
  <commandset type="container" readonly="true">
    <!-- The open:tabs command gets enabled/disabled based on the current 
         selection, and is used in the context menu for a node.  
         The open:tabsEnabled is always enabled, and is shown in bookmarks menus. -->
    <command id="placesCmd_open:tabs" label="&cmd.open_tabs.label;" accesskey="&cmd.open_tabs.accesskey;"
             oncommand="PlacesController.openLinksInTabs();"/>
  </commandset>
  <commandset type="container">
    <command id="placesCmd_sortby:name" label="&cmd.sortby_name.label;" accesskey="&cmd.sortby_name.accesskey;"/>
    <command id="placesCmd_new:bookmark" 
             label="&cmd.new_bookmark.label;" accesskey="&cmd.new_bookmark.accesskey;"
             oncommand="PlacesController.newBookmark()"/>
    <command id="placesCmd_new:folder" 
             label="&cmd.new_folder.label;" oncommand="PlacesController.newFolder()"/>
    <command id="placesCmd_new:separator" label="&cmd.new_separator.label;" accesskey="&cmd.new_separator.accesskey;"
             oncommand="PlacesController.newSeparator()"/>
  </commandset>
  <commandset type="container|feed" readonly="true">
    <command id="placesCmd_reload" 
             label="&cmd.reload.label;" accesskey="&cmd.reload.accesskey;"
             oncommand="PlacesController.reloadSelectedLivemarks();"
             disabled="true"/>
  </commandset>
</commandset>

