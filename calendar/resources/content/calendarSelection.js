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
 * The Original Code is OEone Calendar Code, released October 31st, 2001.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Mike Potter <mikep@oeone.com>
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

function CalendarEventSelection( CalendarWindow )
{
   this.calendarWindow = CalendarWindow;

   this.selectedEvents = new Array();

   this.observerList = new Array();
}

CalendarEventSelection.prototype.addObserver = function calSel_addObserver( observer )
{
   if( observer )
   {
      // remove it first, that way no observer gets added twice
      this.removeObserver(  observer );
      
      // add it to the end.
      this.observerList[ this.observerList.length ] = observer;
      
   }
}


/** PUBLIC
*
*   /removeObserver.
*
*   Removes a previously added observer 
*
* PARAMETERS
*      observer     - Observer to remove.
*/

CalendarEventSelection.prototype.removeObserver = function calSel_removeObserver( observer )
{
   for( var index = 0; index < this.observerList.length; ++index )
   {
      if( observer === this.observerList[ index ] )
      {
         this.observerList.splice( index, 1 );
         return true;
      }
   }
   
   return false;
}


CalendarEventSelection.prototype.addToSelection = function calSel_addToSelection( Event )
{
   this.selectedEvents[ this.selectedEvents.length ] = Event;

   this.onSelectionChanged();
}

CalendarEventSelection.prototype.replaceSelection = function calSel_replaceSelection( Event )
{
   this.selectedEvents = new Array();

   this.selectedEvents[ this.selectedEvents.length ] = Event;

   this.onSelectionChanged();
   
}

CalendarEventSelection.prototype.emptySelection = function calSel_emptySelection( Event )
{
   this.selectedEvents = new Array();

   this.onSelectionChanged();
}

CalendarEventSelection.prototype.setArrayToSelection = function calSel_setArrayToSelection( ArrayOfEvents )
{
   this.selectedEvents = new Array();

   for( i = 0; i < ArrayOfEvents.length; i++ )
   {
      this.selectedEvents[ this.selectedEvents.length ] = ArrayOfEvents[i];
   }
   
   this.onSelectionChanged();
   
}

CalendarEventSelection.prototype.isSelectedEvent = function calSel_isSelectedEvent( Event )
{
   for( i = 0; i < this.selectedEvents.length; i++ )
   {
      if( this.selectedEvents[i] == Event )
         return true;
   }
   return false;
}

/** PUBLIC
*
*   onSelectionChanged.
*
*/

CalendarEventSelection.prototype.onSelectionChanged = function calSel_onSelectionChanged( )
{
   if( this.selectedEvents.length > 0 )
   {
      document.getElementById( "cut_command" ).removeAttribute( "disabled" );
      
      document.getElementById( "copy_command" ).removeAttribute( "disabled" );

      document.getElementById( "delete_command" ).removeAttribute( "disabled" );
      document.getElementById( "delete_command_no_confirm" ).removeAttribute( "disabled" );

      if( this.selectedEvents.length == 1 )
         document.getElementById( "modify_command" ).removeAttribute( "disabled" );
	   else
         document.getElementById( "modify_command" ).setAttribute( "disabled", "true" );

      if (gMailAccounts)
      {
         document.getElementById("send_event_command").removeAttribute("disabled");
      }

      document.getElementById( "print_command" ).removeAttribute( "disabled" );
   }
   else
   {
      document.getElementById( "cut_command" ).setAttribute( "disabled", "true" );
      
      document.getElementById( "copy_command" ).setAttribute( "disabled", "true" );

      document.getElementById( "delete_command" ).setAttribute( "disabled", "true" );
      document.getElementById( "delete_command_no_confirm" ).setAttribute( "disabled", "true" );

      document.getElementById( "modify_command" ).setAttribute( "disabled", "true" );

      document.getElementById("send_event_command").setAttribute("disabled", "true");

      document.getElementById("print_command").setAttribute("disabled", "true");
   }

   for( var index in this.observerList )
   {
      var observer = this.observerList[ index ];
      
      if( "onSelectionChanged" in observer )
      {
         observer.onSelectionChanged( this.selectedEvents );
      }
   }
}
