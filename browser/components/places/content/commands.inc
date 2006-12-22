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
            commandupdater="true"
            events="focus,select"
            oncommandupdate="goUpdatePlacesCommands();">>
    <command id="placesCmd_open"
             label="&cmd.open.label;"
             accesskey="&cmd.open.accesskey;"
             oncommand="goDoCommand('placesCmd_open');"/>
    <command id="placesCmd_open:window"
             label="&cmd.open_window.label;"
             accesskey="&cmd.open_window.accesskey;"
             oncommand="goDoCommand('placesCmd_open:window');"/>
    <command id="placesCmd_open:tab"
             label="&cmd.open_tab.label;"
             accesskey="&cmd.open_tab.accesskey;"
             oncommand="goDoCommand('placesCmd_open:tab');"/>
    <command id="placesCmd_open:tabs"
             label="&cmd.open_tabs.label;"
             accesskey="&cmd.open_tabs.accesskey;"
             oncommand="goDoCommand('placesCmd_open:tabs');"/>
#ifdef MOZ_PLACES_BOOKMARKS
    <command id="placesCmd_new:bookmark" 
             label="&cmd.new_bookmark.label;"
             accesskey="&cmd.new_bookmark.accesskey;"
             oncommand="goDoCommand('placesCmd_new:bookmark');"/>
    <command id="placesCmd_new:folder" 
             label="&cmd.new_folder.label;"
             oncommand="goDoCommand('placesCmd_new:folder');"/>
    <command id="placesCmd_new:separator"
             label="&cmd.new_separator.label;"
             accesskey="&cmd.new_separator.accesskey;"
             oncommand="goDoCommand('placesCmd_new:separator');"/>
    <command id="placesCmd_show:info" 
             oncommand="goDoCommand('placesCmd_show:info');"
#ifdef XP_MACOSX
             label="&cmd.show_infoMac.label;" 
             accesskey="&cmd.show_infoMac.accesskey;"/>
#else
             label="&cmd.show_infoWin.label;" 
             accesskey="&cmd.show_infoWin.accesskey;"/>
#endif
    <!-- XXXmano bug 364644: not yet supported -->
    <command id="placesCmd_reload" 
             label="&cmd.reload.label;" accesskey="&cmd.reload.accesskey;"
             oncommand="PlacesController.reloadSelectedLivemarks();"
             disabled="true"/>
    <command id="placesCmd_sortby:name"
             label="&cmd.sortby_name.label;"/>

    <!-- XXX bug 364643: not implemented -->
    <command id="placesCmd_rename"
             label="&cmd.rename.label;"
             accesskey="&cmd.rename.accesskey;"/>
#endif
</commandset>
