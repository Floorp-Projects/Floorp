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
# The Original Code is the Shared Places Context Menu.
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

<!-- if you make changes to this file you need to rebuild in both browser/base
     and browser/components/places for the effect to appear in both the toolbar/
     menu and the places organizer. -->
<popup id="placesContext" 
       onpopupshowing="return PlacesController.buildContextMenu(this);">
  <!-- the rules defined in the selection attribute control whether or not 
       the menu item is _visible_ regardless of whether or not the command
       is enabled. -->
  <menuitem id="placesContext_open" command="placesCmd_open" default="true"
            selection="link"/>
  <menuitem id="placesContext_open:tabs" command="placesCmd_open:tabs"
            selection="folder|links"/>
  <menuitem id="placesContext_open:newwindow" command="placesCmd_open:window"
            selection="link"/>
  <menuitem id="placesContext_open:newtab" command="placesCmd_open:tab"
            selection="link"/>
  <menuseparator id="placesContext_openSeparator"
            selection="link|links|folder"/>
  <menuitem id="placesContext_new:folder" command="placesCmd_new:folder" accesskey="&cmd.context_new_folder.accesskey;"
            selection="mutable"/>
  <menuitem id="placesContext_new:separator" command="placesCmd_new:separator"
            selection="mutable"/>
  <menuseparator id="placesContext_newSeparator"
            selection="mutable"/>
  <menuitem id="placesContext_edit:cut" command="placesCmd_edit:cut"
            selection="separator|link|links|folder|mixed"/>
  <menuitem id="placesContext_edit:copy" command="placesCmd_edit:copy"
            selection="separator|link|links|folder|mixed"/>
  <menuitem id="placesContext_edit:paste" command="placesCmd_edit:paste"
            selection="mutable"/>
  <menuitem id="placesContext_edit:delete" command="placesCmd_edit:delete"
            selection="host|separator|link|links|folder|mixed"/>
  <menuseparator id="placesContext_editSeparator"
            selection="link|links|folder|mixed|mutable"/>
  <menuitem id="placesContext_select:all" command="placesCmd_select:all"
            selection="multiselect"/>
  <menuseparator id="placesContext_selectSeparator"
            selection="multiselect"/>
  <menuitem id="placesContext_reload" command="placesCmd_reload"
            selection="livemark/feedURI|livemark/bookmarkFeedURI|allLivemarks"/>
  <menuitem id="placesContext_sortby:name" command="placesCmd_sortby:name"
            accesskey="&cmd.context_sortby_name.accesskey;"
            selection="mutable"/>
  <menuseparator id="placesContext_sortSeparator"
            selection="mutable|livemark/feedURI|livemark/bookmarkFeedURI|allLivemarks"/>
  <menuitem id="placesContext_show:info" command="placesCmd_show:info"
            selection="link|folder|query"/>
</popup>

