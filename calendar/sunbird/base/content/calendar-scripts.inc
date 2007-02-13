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

  <script type="application/x-javascript">
    var confirmDeleteAllEvents = "&calendar.confirm.deleteallevents;";
    var confirmDeleteEvent = "&calendar.confirm.deleteevent;";
    var confirmDeleteUntitledEvent = "&calendar.confirm.deleteuntitledevent;";
    var defaultCalendarFileName = "&calendar.file.default.name;";
  </script>

#ifdef XP_MACOSX
  <!-- Needed for Mac OS X "Window" menu -->
  <script type="application/x-javascript" src="chrome://calendar/content/hiddenWindow.js"/>
#endif

  <!-- NEEDED FOR MULTIPLE CALENDAR SUPPORT -->
  <script type="application/x-javascript" src="chrome://calendar/content/calendarManagement.js"/>

  <!-- NEEDED FOR CLIPBOARD SUPPORT -->
  <script type="application/x-javascript" src="chrome://calendar/content/clipboard.js"/>

  <!-- NEEDED FOR IMPORT / EXPORT SUPPORT -->
  <script type="application/x-javascript" src="chrome://calendar/content/import-export.js"/>

  <!-- NEEDED FOR PUBLICATION SUPPORT -->
  <script type="application/x-javascript" src="chrome://calendar/content/publish.js"/>

  <script type="application/x-javascript" src="chrome://calendar/content/calendar.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/calendar-item-editing.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/calUtils.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/calendarWindow.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/mouseoverPreviews.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/unifinder.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/unifinderToDo.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/calendar-views.js"/>
  <script type="application/x-javascript" src="chrome://calendar/content/migration.js"/>

  <!-- NEEDED FOR APPLICATION SUPPORT -->
  <script type="application/x-javascript" src="chrome://calendar/content/applicationUtil.js"/>
  <script type="application/x-javascript" src="chrome://global/content/globalOverlay.js"/>

  <script type="application/x-javascript" src="chrome://global/content/printUtils.js"/>
