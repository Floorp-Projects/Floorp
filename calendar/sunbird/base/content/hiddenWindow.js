/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Don Crandall (macdoc@interx.net)
 *                 Matthew Willis (mattwillis@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* 'new_todo_command', 
*/

#ifdef XP_MACOSX
function hiddenWindowStartup()
{
  // focus the hidden window
  window.focus();

  // Disable menus which are not appropriate
  var disabledItems = ['new_command', 'new_local_calendar_command', 
                       'modify_command', 'close_calendar_command', 'send_event_command', 
                       'print_command', 'cut_command', 'copy_command', 'paste_command', 
                       'delete_command', 'delete_todo_command', 'select_all_command', 
                       'day_view_command', 'week_view_command', 'multiweek_view_command', 
                       'month_view_command', 'cmd_CustomizeToolbars', 'go_today_command', 
                       'go_date_command', 'previous_command', 'next_command', 
                       'wizard_command', 'import_command', 'export_command', 
                       'new_server_command', 'publish_events_command', 'modify_todo_command', 
                       'edit_local_calendar_command', 'edit_server_command', 
                       'delete_server_command', 'find_new_calendar_command', 
                       'refresh_remote_calendars', 'publish_calendar'];
  for (var id in disabledItems) 
  {
    var broadcaster = document.getElementById(disabledItems[id]);
    if (broadcaster)
      broadcaster.setAttribute("disabled", "true");
  };
}

// Mac OS X "Window" menu functions
const nsIWindowDataSource = Components.interfaces.nsIWindowDataSource;

function checkFocusedWindow()
{
  var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);

  var sep = document.getElementById("sep-window-list");
  // Using double parens to avoid warning
  while ((sep = sep.nextSibling)) {
    var url = sep.getAttribute('id');
    var win = windowManagerDS.getWindowForResource(url);
    if (win == window) {
      sep.setAttribute("checked", "true");
      break;
    }
  }
}

function toOpenWindow( aWindow )
{
  aWindow.document.commandDispatcher.focusedWindow.focus();
}

function ShowWindowFromResource( node )
{
  var windowManagerDS = Components.classes['@mozilla.org/rdf/datasource;1?name=window-mediator'].getService(nsIWindowDataSource);

  var desiredWindow = null;
  var url = node.getAttribute('id');
  desiredWindow = windowManagerDS.getWindowForResource( url );
  if ( desiredWindow )
  {
    toOpenWindow(desiredWindow);
  }
}
#endif
