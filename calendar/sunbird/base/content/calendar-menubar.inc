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

     <menubar id="main-menubar" style="border:0px;padding:0px;margin:0px;-moz-appearance:none">
       <menu id="menu_File"
             label="&fileMenu.label;"
             accesskey="&fileMenu.accesskey;">
         <menupopup id="menu_FilePopup">

           <!-- create new event or task (top of 1st menu, for new users) -->
           <menuitem id="calendar-new-event-menu"
                     key="new_event_key"
                     label="&event.new.event;"
                     accesskey="&event.new.event.accesskey;"
                     observes="new_command"/>
           <menuitem id="calendar-new-todo-menu"
                     key="new_todo_key"
                     label="&event.new.todo;"
                     accesskey="&event.new.todo.accesskey;"
                     observes="new_todo_command"/>
           <menuitem id="unifinder-modify-menu"
                     key="modify_event_key"
                     label="&event.edit.event;"
                     accesskey="&event.edit.event.accesskey;"
                     observes="modify_command"/>
           <menuseparator/>

           <!-- files -->
           <menuitem id="calendar-new-calendar-menu"
                     key="new_calendar_key"
                     label="&calendar.new.server.label;"
                     accesskey="&calendar.new.server.accesskey;"
                     observes="new_local_calendar_command"/>
           <menuitem id="calendar-open-calendar-menu"
                     key="open_calendar_key"
                     label="&calendar.open.file.label;"
                     accesskey="&calendar.open.file.accesskey;"
                     observes="open_local_calendar_command"/>
           <menuitem id="calendar-addserver-menu"
                     label="&calendar.subscribe.label;"
                     accesskey="&calendar.subscribe.accesskey;"
                     observes="new_server_command"/>
           <menuitem id="calendar-refreshserver-menu"
                     label="&calendar.context.refreshserver.label;"
                     accesskey="&calendar.context.refreshserver.accesskey;"
                     observes="refresh_remote_calendars"/>
           <menuseparator/>

           <!-- import: pulling data into existing files -->
           <menuitem id="calendar-import-menu"
                     key="import_key"
                     label="&calendar.import.label;"
                     accesskey="&calendar.import.accesskey;"
                     observes="import_command"/>
           <menuseparator/>

           <!-- export: getting data out of existing files -->
           <menuitem id="calendar-export-menu"
                     label="&calendar.export.label;"
                     accesskey="&calendar.export.accesskey;"
                     observes="export_command"/>
           <menuitem id="calendar-mail-event"
                     key="send_event_key"
                     label="&event.mail.event;"
                     accesskey="&event.mail.event.accesskey;"
                     observes="send_event_command"/>
           <menuitem id="calendar-publish-menu"
                     label="&calendar.publish.label;"
                     accesskey="&calendar.publish.accesskey;"
                     observes="publish_events_command"/>
           <menuitem id="calendar-print-menu"
                     key="print_key"
                     label="&calendar.print.label;"
                     accesskey="&calendar.print.accesskey;"
                     observes="print_command"/>
           <menuseparator id="calendar-print-sep"/>

           <!-- window/Application -->
           <menuitem id="calendar-close"
                     key="key_close"
                     label="&calendar.menu.options.close;"
                     accesskey="&calendar.menu.options.close.accesskey;"
                     observes="close_calendar_command"/>
         </menupopup>
       </menu>

       <menu id="menu_Edit"
             label="&editMenu.label;"
              accesskey="&editMenu.accesskey;">
         <menupopup id="mppEdit">
           <menuitem id="calendar-cut-menu"
                     key="key_cut"
                     label="&calendar.cut.label;"
                     accesskey="&calendar.cut.accesskey;"
                     observes="cut_command" />
           <menuitem id="calendar-copy-menu"
                     key="key_copy"
                     label="&calendar.copy.label;"
                     accesskey="&calendar.copy.accesskey;"
                     observes="copy_command" />
           <menuitem id="calendar-paste-menu"
                     key="key_paste"
                     label="&calendar.paste.label;"
                     accesskey="&calendar.paste.accesskey;"
                     observes="paste_command"/>
           <menuitem id="unifinder-remove-menu"
                     key="delete_key"
                     label="&event.delete.event;"
                     accesskey="&event.delete.event.accesskey;"
                     observes="delete_command" />
           <menuitem id="unifinder-remove-menu"
                     label="&event.delete.tasks;"
                     accesskey="&event.delete.tasks.accesskey;"
                     observes="delete_todo_command" />
           <menuseparator/>
           <menuitem id="calendar-selectall-menu"
                     key="key_selectAll"
                     label="&calendar.selectall.label;"
                     accesskey="&calendar.selectall.accesskey;"
                     observes="select_all_command"/>
         </menupopup>
       </menu>

       <menu id="menu_View"
             label="&viewMenu.label;"
             accesskey="&viewMenu.accesskey;">
         <menupopup id="mppView">
           <menuitem id="calendar-view-menu-day"
                     key="view_day_key"
                     label="&calendar.flat.topbar.dayview;"
                     accesskey="&calendar.dayview.accesskey;"
                     observes="day_view_command"/>
           <menuitem id="calendar-view-menu-week"
                     key="view_week_key"
                     label="&calendar.flat.topbar.weekview;"
                     accesskey="&calendar.weekview.accesskey;"
                     observes="week_view_command"/>
           <menuitem id="calendar-view-menu-multiweek"
                     key="view_multiweek_key"
                     label="&calendar.flat.topbar.multiweekview;"
                     accesskey="&calendar.multiweekview.accesskey;"
                     observes="multiweek_view_command"/>
           <menuitem id="calendar-view-menu-month"
                     key="view_month_key"
                     label="&calendar.flat.topbar.monthview;"
                     accesskey="&calendar.monthview.accesskey;"
                     observes="month_view_command"/>
           <menuseparator/>
           <menuitem id="only-workday-checkbox-1"
                     type="checkbox"
                     label="&calendar.onlyworkday.checkbox.label;"
                     accesskey="&calendar.onlyworkday.checkbox.accesskey;"
                     oncommand="changeOnlyWorkdayCheckbox(1);"
                     persist="checked"
                     disabled="true"/>
           <menuitem id="display-todo-inview-checkbox-1"
                     type="checkbox"
                     label="&calendar.displaytodos.checkbox.label;"
                     accesskey="&calendar.displaytodos.checkbox.accesskey;"
                     oncommand="changeDisplayToDoInViewCheckbox(1);"
                     persist="checked"
                     disabled="true"/>
           <menu label="&calendar.menu.numberofweeks.label;"
                 oncommand="gCalendarWindow.currentView.changeNumberOfWeeks(event.target)"
                 id="menu-numberofweeks-inview"
                 disabled="true">
           <menupopup>
             <menuitem type="radio"
                       value="1"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.1;"/>
             <menuitem type="radio"
                       value="2"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.2;"/>
             <menuitem type="radio"
                       value="3"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.3;"/>
             <menuitem type="radio"
                       value="4"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.4;"/>
             <menuitem type="radio"
                       value="5"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.5;"/>
             <menuitem type="radio"
                       value="6"
                       name="menunbofweeks"
                       label="&calendar.menu.numberofweeks.6;"/>
           </menupopup>
           </menu>
           <menuseparator/>
           <menuitem id="calendar-view-menu-customize"
                     command="cmd_CustomizeToolbars"
                     label="&calendar.menu.customize.label;"
                     accesskey="&calendar.menu.customize.accesskey;"/>
         </menupopup>
       </menu>

       <menu id="menu_Go"
             label="&goMenu.label;"
             accesskey="&goMenu.accesskey;">
         <menupopup id="mppView">
           <menuitem id="calendar-go-menu-today"
                     key="go_to_today_key"
                     label="&goTodayCmd.label;"
                     accesskey="&goTodayCmd.accesskey;"
                     observes="go_today_command"/>
           <menuitem id="calendar-go-menu-date"
                     key="go_to_date_key"
                     label="&goDateCmd.label;"
                     accesskey="&goDateCmd.accesskey;"
                     observes="go_date_command"/>
           <menuitem id="calendar-go-menu-previous"
                     key="go_backward_key"
                     label="&goPreviousCmd.label;"
                     accesskey="&goPreviousCmd.accesskey;"
                     observes="previous_command"/>
           <menuitem id="calendar-go-menu-next"
                     key="go_forward_key"
                     label="&goNextCmd.label;"
                     accesskey="&goNextCmd.accesskey;"
                     observes="next_command"/>
         </menupopup>
       </menu>

       <menu id="menu_Tools"
             label="&toolsMenu.label;"
             accesskey="&toolsMenu.accesskey;">
         <menupopup id="toolsPopup">
#           <menuitem id="calendar-wizard-menu"
#                     label="&calendar.wizard.label;"
#                     accesskey="&calendar.wizard.accesskey;"
#                     observes="wizard_command"/>
           <menuseparator/>
               <menuitem id="javascriptConsole"
                         label="&javaScriptConsoleCmd.label;"
                         accesskey="&javaScriptConsoleCmd.accesskey;"
                         oncommand="toJavaScriptConsole();"/>
<!-- The ids of the following menuseparator and menuitem must remain as they are to be
   - automatically hidden on Mac OS X - See mozilla/widget/src/mac/nsMenuBarX.cpp -->
           <menuseparator id="menu_PrefsSeparator"/>
               <menuitem id="menu_preferences"
                         label="&preferencesCmd.label;"
                         accesskey="&preferencesCmd.accesskey;"
                         oncommand="openPreferences();"/>

         </menupopup>
       </menu>

#ifdef XP_MACOSX
<!-- Mac OS X only "Window" menu -->
       <menu id="windowMenu"
             label="&windowMenu.label;"
             datasources="rdf:window-mediator"
             ref="NC:WindowMediatorRoot"
             onpopupshown="checkFocusedWindow();">
         <template>
           <rule>
             <menupopup>
               <menuitem uri="rdf:*"
                         label="rdf:http://home.netscape.com/NC-rdf#Name"
                         type="radio"
                         name="windowList"
                         oncommand="ShowWindowFromResource(event.target)"/>
             </menupopup>
           </rule>
         </template>
         <menupopup id="windowPopup">
           <menuitem id="minimizeWindow"
                     key="key_minimizeWindow"
                     label="&minimizeWindow.label;"
                     command="minimizeWindowCmd"/>
           <menuitem id="zoomWindow"
                     label="&zoomWindow.label;"
                     command="zoomWindowCmd"/>
           <menuseparator/>
           <menuitem id="bringAllToFront"
                     label="&bringAllToFront.label;"
                     command="bringAllToFront"
                     disabled="true"/>
           <menuseparator id="sep-window-list"/>
         </menupopup>
       </menu>
#endif

       <menu id="menu_Help"
             label="&helpMenu.label;"
             accesskey="&helpMenu.accesskey;">
         <menupopup id="menu_HelpPopup">
           <menuitem accesskey="&releaseCmd.accesskey;"
                     label="&releaseCmd.label;"
                     oncommand="openTopWin('&releaseURL;');"
                     disabled="true"/>
           <menuseparator id="menu_HelpAboutSeparator"/>
           <menuitem id="aboutName"
                     label="&aboutCmd.label;"
                     accesskey="&aboutCmd.accesskey;"
                     oncommand="openAboutDialog();"/>
         </menupopup>
       </menu>
     </menubar>
