/*
 * ***** BEGIN LICENSE BLOCK *****
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * The Original Code is Mozilla Calendar Code.
 * 
 * The Initial Developer of the Original Code is
 * Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 1999-2002
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s): Eric Belhaire (belhaire@ief.u-psud.fr)
 * 
 * ***** END LICENSE BLOCK *****
 */
 

//Used by Mozilla Firebird
function openCalendarInFirebird() 
{
  //window.openDialog("chrome://calendar/content", "_blank", "chrome,all,dialog=no");
  calendarWindow = window.open("chrome://calendar/content/calendar.xul", "calendar", "chrome,extrachrome,menubar,resizable,scrollbars,status,toolbar");
  //openCalendar();
}

