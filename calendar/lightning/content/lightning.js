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
 * The Original Code is Lightning preferences code.
 *
 * The Initial Developer of the Original Code is 
 *   Joey Minta <jminta@gmail.com>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stefan Sitter <ssitter@googlemail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

// This file contains all of the default preference values for Lightning

// general settings
pref("calendar.date.format", 0);
pref("calendar.event.defaultlength", 60);

// alarm settings
pref("calendar.alarms.show", true);
pref("calendar.alarms.showmissed", true);
pref("calendar.alarms.playsound", false);
pref("calendar.alarms.soundURL", "");
pref("calendar.alarms.defaultsnoozelength", 5);

// default alarm settings for new event
pref("calendar.alarms.onforevents", 0);
pref("calendar.alarms.eventalarmlen", 15);
pref("calendar.alarms.eventalarmunit", "minutes");

// default alarm settings for new task
pref("calendar.alarms.onfortodos", 0);
pref("calendar.alarms.todoalarmlen", 15);
pref("calendar.alarms.todoalarmunit", "minutes");

// autorefresh settings
pref("calendar.autorefresh.enabled", true);
pref("calendar.autorefresh.timeout", 30);


// 0=Sunday, 1=Monday, 2=Tuesday, etc.  One day we might want to move this to
// a locale specific file.
pref("calendar.week.start", 0);
pref("calendar.weeks.inview", 4);
pref("calendar.previousweeks.inview", 0);

// Default days off
pref("calendar.week.d0sundaysoff", true);
pref("calendar.week.d1mondaysoff", false);
pref("calendar.week.d2tuesdaysoff", false);
pref("calendar.week.d3wednesdaysoff", false);
pref("calendar.week.d4thursdaysoff", false);
pref("calendar.week.d5fridaysoff", false);
pref("calendar.week.d6saturdaysoff", true);

// Do not set this!  If it's not there, then we guess the system timezone
//pref("calendar.timezone.local", "");

// categories settings
// XXX One day we might want to move this to a locale specific file
//     and include a list of locale specific default categories
pref("calendar.categories.names", "");
