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
  <command id="new_command" oncommand="newEventCommand( event )"/>
  <command id="delete_command" oncommand="deleteEventCommand( )" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="delete_command_no_confirm" oncommand="deleteEventCommand( true )" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="modify_command" oncommand="editEventCommand()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="send_event_command" oncommand="sendEvent()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="print_command" oncommand="print()"/>
  <command id="cut_command" oncommand="cutToClipboard()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="copy_command" oncommand="copyToClipboard()" disabled="true" disabledwhennoeventsselected="true"/>
  <command id="paste_command" oncommand="pasteFromClipboard()"/>
  <command id="select_all_command" oncommand="selectAllEvents()"/>

  <command id="new_todo_command" oncommand="newToDoCommand()"/>
  <command id="modify_todo_command" oncommand="modifyToDoCommand( event )"/>
  <command id="delete_todo_command" oncommand="deleteToDoCommand()" disabled="true"/>

  <command id="new_local_calendar_command" oncommand="gCalendarWindow.calendarManager.launchNewCalendarFileDialog()"/>
  <command id="open_local_calendar_command" oncommand="gCalendarWindow.calendarManager.launchOpenCalendarFileDialog()"/>
  <command id="edit_local_calendar_command" oncommand="gCalendarWindow.calendarManager.launchEditCalendarDialog()"/>
  <command id="new_server_command" oncommand="gCalendarWindow.calendarManager.launchAddRemoteCalendarDialog()"/>
  <command id="edit_server_command" oncommand="gCalendarWindow.calendarManager.launchEditRemoteCalendarDialog()"/>
  <command id="delete_server_command" oncommand="deleteCalendar()"/>
  <command id="find_new_calendar_command" oncommand="goFindNewCalendars()"/>

  <command id="next_command" oncommand="gCalendarWindow.currentView.goToNext()"/>
  <command id="previous_command" oncommand="gCalendarWindow.currentView.goToPrevious()"/>
  <command id="go_today_command" oncommand="gCalendarWindow.goToToday()"/>
  <command id="go_date_command" oncommand="gCalendarWindow.pickAndGoToDate()"/>

  <command id="day_view_command"  oncommand="gCalendarWindow.switchToDayView()" disabled="true"/>
  <command id="week_view_command"  oncommand="gCalendarWindow.switchToWeekView()" disabled="true"/>
  <command id="month_view_command" oncommand="gCalendarWindow.switchToMonthView()" disabled="true"/>
  <command id="multiweek_view_command" oncommand="gCalendarWindow.switchToMultiweekView()" disabled="false"/>

  <command id="import_command" oncommand="startImport()"/>
  <command id="export_command" oncommand="saveEventsToFile()" disabled="true" disabledwhennoeventsselected="true"/>

  <command id="refresh_remote_calendars" oncommand="gCalendarWindow.calendarManager.refreshAllRemoteCalendars()"/>
  <command id="publish_calendar" oncommand="publishEntireCalendar()"/>
  <command id="publish_events_command" oncommand="publishCalendarData()" disabled="true" disabledwhennoeventsselected="true"/>

<!-- <command id="change_display_todo_in_view" oncommand="" checked="true"/> -->

  <command id="close_calendar_command" oncommand="closeCalendar()"/>

  <command id="wizard_command" oncommand="launchWizard()" collapsed="true"/>

  <command id="cmd_CustomizeToolbars" oncommand="CalendarCustomizeToolbar()"/>
#ifdef XP_MACOSX
  <command id="minimizeWindowCmd" oncommand="window.minimize();"/>
  <command id="zoomWindowCmd" oncommand="zoomWindow();"/>
#endif
</commandset>

<keyset>
  <key id="new_event_key"        key="&event.new.event.key;"             observes="new_command"                 modifiers="accel"/>
  <key id="new_todo_key"         key="&event.new.todo.key;"              observes="new_todo_command"            modifiers="accel"/>
  <key id="modify_event_key"     key="&event.edit.event.key;"            observes="modify_command"              modifiers="accel"/>
  <key id="new_calendar_key"     key="&calendar.new.server.key;"         observes="new_local_calendar_command"  modifiers="accel"/>
  <key id="open_calendar_key"    key="&calendar.open.file.key;"          observes="open_local_calendar_command" modifiers="accel"/>
  <key id="key_close"            key="&calendar.menu.options.close.key;" observes="close_calendar_command"      modifiers="accel"/>
#ifdef XP_MACOSX
  <key id="send_event_key"       key="&event.mail.event.key;"            observes="send_event_command"          modifiers="shift, accel"/>
#else
  <key id="send_event_key"       key="&event.mail.event.key;"            observes="send_event_command"          modifiers="accel"/>
#endif
  <key id="print_key"            key="&calendar.print.key;"              observes="print_command"               modifiers="accel"/>

  <key id="key_cut"              key="&calendar.cut.key;"                observes="cut_command"                 modifiers="accel"/>
  <key id="key_copy"             key="&calendar.copy.key;"               observes="copy_command"                modifiers="accel"/>
  <key id="key_paste"            key="&calendar.paste.key;"              observes="paste_command"               modifiers="accel"/>
  <key id="delete_key"           keycode="VK_DELETE"                     observes="delete_command_no_confirm"/>
  <key id="key_selectAll"        key="&calendar.selectall.key;"          observes="select_all_command"          modifiers="accel"/>

  <key id="view_day_key"         keycode="VK_F1"                         observes="day_view_command"/>
  <key id="view_week_key"        keycode="VK_F2"                         observes="week_view_command"/>
  <key id="view_multiweek_key"   keycode="VK_F3"                         observes="multiweek_view_command"/>
  <key id="view_month_key"       keycode="VK_F4"                         observes="month_view_command"/>

  <key id="go_to_today_key"      keycode="VK_DOWN"                       observes="go_today_command"            modifiers="accel"/>
  <key id="go_to_date_key"       keycode="VK_UP"                         observes="go_date_command"             modifiers="accel"/>
  <key id="go_backward_key"      keycode="VK_PAGE_UP"                    observes="previous_command"/>
  <key id="go_forward_key"       keycode="VK_PAGE_DOWN"                  observes="next_command"/>

  <key id="import_key"           key="&calendar.import.key;"             observes="import_command"              modifiers="accel"/>

#ifdef XP_MACOSX
  <key id="key_minimizeWindow"   key="&minimizeWindow.key;"              command="minimizeWindow"               modifiers="accel"/>
#endif
</keyset>

<keyset id="tasksKeys"/>

<!-- STRING BUNDLE for calendar properties -->
<stringbundleset id="stringbundleset">
  <stringbundle id="bundle_calendar" src="chrome://calendar/locale/calendar.properties"/>
  <stringbundle id="bundle_calendar_email" src="chrome://calendar/locale/email.properties"/>
</stringbundleset>
<script type="application/x-javascript">
  var gCalendarBundle = document.getElementById("bundle_calendar");
  var gCalendarEmailBundle = document.getElementById("bundle_calendar_email");
</script>
