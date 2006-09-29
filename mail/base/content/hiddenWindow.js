# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
# The Original Code is Mozilla Communicator client code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Don Crandall (macdoc@interx.net)
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

function hiddenWindowStartup()
{
  // Disable menus which are not appropriate
  var disabledItems = ['newNewMsgCmd', 'menu_newFolder', 'newAccountMenuItem', 'menu_close', 'menu_saveAs',
					   'menu_saveAsFile', 'menu_newVirtualFolder', 'menu_find', 'menu_findCmd', 'menu_findAgainCmd',
                       'menu_sendunsentmsgs', 'menu_subscribe', 'menu_renameFolder', 'menu_select', 
					   'menu_selectAll', 'menu_selectThread', 
					   'menu_favoriteFolder', 'menu_properties', 
					   'menu_Toolbars', 'menu_MessagePaneLayout', 'menu_showMessage', 'menu_FolderViews', 
					   'viewSortMenu', 'groupBySort', 'viewMessageViewMenu', 'mailviewCharsetMenu',
					   'viewMessagesMenu', 'menu_expandAllThreads', 'collapseAllThreads', 
					   'viewheadersmenu', 'viewBodyMenu', 'viewAttachmentsInlineMenuitem',
					   'viewTextSizeMenu', 'menu_textZoomEnlarge', 'menu_textZoomReduce',
					   'goNextMenu', 'menu_nextMsg', 'menu_nextUnreadMsg', 'menu_nextUnreadThread',
					   'goPreviousMenu', 'menu_prevMsg', 'menu_prevUnreadMsg', 'menu_goForward', 'menu_goBack',
					   'goStartPage', 'newMsgCmd', 'replyMainMenu', 'replySenderMainMenu', 'replyNewsgroupMainMenu', 
					   'menu_replyToAll', 'menu_forwardMsg', 'forwardAsMenu', 'menu_editMsgAsNew', 'openMessageWindowMenuitem',
					   'moveMenu', 'copyMenu', 'moveToFolderAgain', 'tagMenu', 'markMenu', 
					   'markReadMenuItem', 'menu_markThreadAsRead', 'menu_markReadByDate', 'menu_markAllRead',
					   'markFlaggedMenuItem', 'menu_markAsJunk', 'menu_markAsNotJunk', 'createFilter',
					   'killThread', 'watchThread', 'applyFilters', 'runJunkControls', 'deleteJunk', 'menu_import',
                       'searchMailCmd', 'searchAddressesCmd', 'filtersCmd', 'junkMailCmd',
					   'cmd_close', 'minimizeWindow', 'zoomWindow'];
  var id;
  var element;
  for (id in disabledItems) 
  {
    element = document.getElementById(disabledItems[id]);
    if (element)
      element.setAttribute("disabled", "true");
  }

  // also hide the window-list separator
  document.getElementById("sep-window-list").setAttribute("hidden", "true");
}
