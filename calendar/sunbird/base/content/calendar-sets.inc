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
# The Original Code is OEone Calendar Code, released October 31st, 2001.
#
# The Initial Developer of the Original Code is
# OEone Corporation.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s): Garth Smedley <garths@oeone.com>
#                 Mike Potter <mikep@oeone.com>
#                 Colin Phillips <colinp@oeone.com>
#                 Chris Charabaruk <coldacid@djfly.org>
#                 Karl Guertin <grayrest@grayrest.com>
#                 Dan Parent <danp@oeone.com>
#                 ArentJan Banck <ajbanck@planet.nl>
#                 Eric Belhaire <belhaire@ief.u-psud.fr>
#                 Matthew Willis <mattwillis@gmail.com>
#                 Stefan Sitter <ssitter@googlemail.com>
#                 Dan Mosedale <dan.mosedale@oracle.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the LGPL or the GPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

<broadcasterset id="calendar_broadcasters">
  <broadcaster id="is_editable" hidden="false"/>
</broadcasterset>

<commandset id="calendar_commands">
  <command id="new_command" oncommand="createEventWithDialog()"/>
  <command id="delete_command" oncommand="deleteEventCommand( )" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="delete_command_no_confirm" oncommand="deleteEventCommand( true )" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="modify_command" oncommand="modifyEventWithDialog(currentView().getSelectedItems({})[0])" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="cmd_pageSetup" oncommand="PrintUtils.showPageSetup();"/>
  <command id="print_command" oncommand="calPrint()"/>
  
  <command id="undo_command" oncommand="undo()" disabled="true"/>
  <command id="redo_command" oncommand="redo()" disabled="true"/>
  <command id="cut_command" oncommand="cutToClipboard()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="copy_command" oncommand="copyToClipboard()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="paste_command" oncommand="pasteFromClipboard()"/>
  <command id="select_all_command" oncommand="selectAllEvents()"/>

  <command id="new_todo_command" oncommand="createTodoWithDialog()"/>
  <command id="modify_todo_command" oncommand="editToDo()"/>
  <command id="delete_todo_command" oncommand="deleteToDoCommand()" disabled="true"/>

  <command id="new_calendar_command" oncommand="newCalendarDialog()"/>
  <command id="open_local_calendar_command" oncommand="openLocalCalendar()"/>
  <command id="edit_calendar_command" oncommand="editCalendarDialog(event)"/>
  <command id="delete_calendar_command" oncommand="deleteCalendar(event)"/>
  <command id="find_new_calendar_command" oncommand="goFindNewCalendars()"/>

  <command id="next_command" oncommand="moveView(1)"/>
  <command id="previous_command" oncommand="moveView(-1)"/>
  <command id="go_today_command" oncommand="gCalendarWindow.goToToday()"/>
  <command id="go_date_command" oncommand="gCalendarWindow.pickAndGoToDate()"/>

  <command id="day_view_command"  oncommand="gCalendarWindow.switchToView('day')"/>
  <command id="week_view_command"  oncommand="gCalendarWindow.switchToView('week')"/>
  <command id="month_view_command" oncommand="gCalendarWindow.switchToView('month')"/>
  <command id="multiweek_view_command" oncommand="gCalendarWindow.switchToView('multiweek')"/>

  <command id="import_command" oncommand="loadEventsFromFile()"/>
  <command id="export_command" oncommand="saveEventsToFile(currentView().getSelectedItems({}))" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="ask_and_export_calendar" oncommand="exportEntireCalendar()"/>

  <command id="reload_remote_calendars" oncommand="reloadCalendars()"/>
  <command id="publish_calendar" oncommand="publishEntireCalendar(getDefaultCalendar())"/>
  <command id="ask_and_publish_calendar" oncommand="publishEntireCalendar()"/>
  <command id="publish_events_command" oncommand="publishCalendarData()" disabled="true" disabledwhennoeventsselected="true"/>

  <command id="toggle_tasks_in_view" 
           oncommand="changeDisplayToDoInViewCheckbox()" 
           persist="checked"/>
  <command id="toggle_workdays_only" 
           oncommand="changeOnlyWorkdayCheckbox()"
           persist="checked"/>

  <command id="close_calendar_command" oncommand="closeCalendar()"/>

  <command id="wizard_command" oncommand="launchWizard()" collapsed="true"/>

  <command id="cmd_CustomizeToolbars"   oncommand="CalendarCustomizeToolbar()"/>
  <command id="cmd_showSearchToolbar"   oncommand="goToggleToolbar('searchBox', 'menu_showSearchToolbar');"/>
  <command id="cmd_showStatusBar"       oncommand="goToggleToolbar('status-bar', 'menu_showStatusBar');"/>
  <command id="cmd_showTaskBox"         oncommand="goToggleToolbar('taskBox', 'menu_showTaskBox'); goToggleToolbar('calendar-todo-splitter');"/>

  <command id="Tools:Addons" oncommand="goOpenAddons();"/>

#ifdef XP_MACOSX
  <command id="minimizeWindowCmd" oncommand="window.minimize();"/>
  <command id="zoomWindowCmd" oncommand="zoomWindow();"/>
#endif
</commandset>

<keyset>
  <key id="new_event_key"        key="&event.new.event.key;"             observes="new_command"                 modifiers="accel"/>
  <key id="new_todo_key"         key="&event.new.todo.key;"              observes="new_todo_command"            modifiers="accel"/>
#XXX lilmatt: Finish this!
  <key id="new_journal_key"      key="&calendar.new.journal.key;"        modifiers="accel"/>

  <key id="modify_event_key"     key="&event.edit.event.key;"            observes="modify_command"              modifiers="accel"/>
  <key id="new_calendar_key"     key="&calendar.new.server.key;"         observes="new_calendar_command"  modifiers="accel"/>
  <key id="open_calendar_key"    key="&calendar.open.file.key;"          observes="open_local_calendar_command" modifiers="accel"/>
  <key id="key_close"            key="&calendar.menu.options.close.key;" observes="close_calendar_command"      modifiers="accel"/>
  <key id="print_key"            key="&calendar.print.key;"              observes="print_command"               modifiers="accel"/>

  <key id="undo_key"             key="&calendar.undo.key;"               observes="undo_command"                modifiers="accel"/>
#ifdef XP_UNIX
  <key id="redo_key"             key="&calendar.undo.key;"               observes="redo_command"                modifiers="shift, accel"/>
#else
  <key id="redo_key"             key="&calendar.redo.key;"               observes="redo_command"                modifiers="accel"/>
#endif

  <key id="key_cut"              key="&calendar.cut.key;"                observes="cut_command"                 modifiers="accel"/>
  <key id="key_copy"             key="&calendar.copy.key;"               observes="copy_command"                modifiers="accel"/>
  <key id="key_paste"            key="&calendar.paste.key;"              observes="paste_command"               modifiers="accel"/>
  <key id="key_selectAll"        key="&calendar.selectall.key;"          observes="select_all_command"          modifiers="accel"/>

  <key id="view_day_key"         key="&calendar.dayView.key;"            observes="day_view_command"            modifiers="accel"/>
  <key id="view_week_key"        key="&calendar.weekView.key;"           observes="week_view_command"           modifiers="accel"/>
  <key id="view_multiweek_key"   key="&calendar.multiweekView.key;"      observes="multiweek_view_command"      modifiers="accel"/>
  <key id="view_month_key"       key="&calendar.monthView.key;"          observes="month_view_command"          modifiers="accel"/>

  <key id="go_to_today_key"      keycode="VK_DOWN"                       observes="go_today_command"            modifiers="accel"/>
  <key id="go_to_date_key"       keycode="VK_UP"                         observes="go_date_command"             modifiers="accel"/>

  <key id="import_key"           key="&calendar.import.key;"             observes="import_command"              modifiers="accel"/>
  <key id="reload_server_key"    key="&calendar.reloadserver.key;"       observes="reload_remote_calendars"     modifiers="accel"/>

  <key id="find_key"
       key="&calendar.find.key;"
       modifiers="accel"
       oncommand="focusSearch();"/>

#ifdef XP_MACOSX
  <key id="key_minimizeWindow"   key="&minimizeWindow.key;"              command="minimizeWindow"               modifiers="accel"/>
  <key id="key_preferencesCmdMac" key="&preferencesCmdMac.commandkey;" modifiers="&preferencesCmdMac.modifiers;"/>
  <key id="key_quitCmdMac" key="&calendar.quit.key.mac;" modifiers="&calendar.quit.modifiers.mac;"/>
  <key id="key_hideThisAppCmdMac" key="&hideThisAppCmdMac.commandkey;" modifiers="&hideThisAppCmdMac.modifiers;"/>
  <key id="key_hideOtherAppsCmdMac" key="&hideOtherAppsCmdMac.commandkey;" modifiers="&hideOtherAppsCmdMac.modifiers;"/>
#endif
</keyset>

<keyset id="tasksKeys"/>

<!-- STRING BUNDLE for calendar properties -->
<stringbundleset id="stringbundleset">
  <stringbundle id="bundle_branding" src="chrome://branding/locale/brand.properties"/>
  <stringbundle id="bundle_calendar" src="chrome://calendar/locale/calendar.properties"/>
</stringbundleset>
<script type="application/x-javascript">
  var gCalendarBundle = document.getElementById("bundle_calendar");
</script>
