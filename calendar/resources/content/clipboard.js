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
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is
 * ArentJan Banck <ajbanck@planet.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): ArentJan Banck <ajbanck@planet.nl>
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

/***** calendarClipboard
*
* NOTES 
*   TODO items
*     - Add a clipboard listener, to enable/disable menu-items depending if 
*       valid clipboard data is available.
*
******/


function getClipboard()
{
   const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
   const kClipboardIID = Components.interfaces.nsIClipboard;
   return Components.classes[kClipboardContractID].getService(kClipboardIID);
}

var Transferable = Components.Constructor("@mozilla.org/widget/transferable;1", Components.interfaces.nsITransferable);
var SupportsArray = Components.Constructor("@mozilla.org/supports-array;1", Components.interfaces.nsISupportsArray);
var SupportsCString = (("nsISupportsCString" in Components.interfaces)
                       ? Components.Constructor("@mozilla.org/supports-cstring;1", Components.interfaces.nsISupportsCString)
                       : Components.Constructor("@mozilla.org/supports-string;1", Components.interfaces.nsISupportsString)
                      );
var SupportsString = (("nsISupportsWString" in Components.interfaces)
                      ? Components.Constructor("@mozilla.org/supports-wstring;1", Components.interfaces.nsISupportsWString)
                      : Components.Constructor("@mozilla.org/supports-string;1", Components.interfaces.nsISupportsString)
                     );

/** 
* Test if the clipboard has items that can be pasted into Calendar.
* This must be of type "text/calendar" or "text/unicode"
*/

function canPaste()
{
   const kClipboardIID = Components.interfaces.nsIClipboard;

   var clipboard = getClipboard();
  var flavourArray = new SupportsArray;
   var flavours = ["text/calendar", "text/unicode"];
   
   for (var i = 0; i < flavours.length; ++i)
   {
    var kSuppString = new SupportsCString;
      kSuppString.data = flavours[i];
      flavourArray.AppendElement(kSuppString);
   }
   
   return clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
}


/** 
* Copy iCalendar data to the Clipboard, and delete the selected events.
* Does not use eventarray parameter, because DeletCcommand delete selected events.
*/

function cutToClipboard( /* calendarEventArray */)
{
  // if( !calendarEventArray)
  var calendarEventArray = gCalendarWindow.EventSelection.selectedEvents;

   if( copyToClipboard( calendarEventArray ) )
   {
      deleteEventCommand( true ); // deletes all selected events without prompting.
   }
}


/** 
* Copy iCalendar data to the Clipboard. The data is copied to three types:
* 1) text/calendar. Found that name somewhere in mail code. not used outside
*    Calendar as far as I know, so this can be customized for internal use.
* 2) text/unicode. Plaintext iCalendar data, tested on Windows for Outlook 2000 
*    and Lotus Organizer.
* 3) text/html. Not for parsing, only pretty looking calendar data.
*
**/

function copyToClipboard( calendarEventArray )
{  
   if( !calendarEventArray)
   {
      calendarEventArray = new Array( 0 );
      calendarEventArray = gCalendarWindow.EventSelection.selectedEvents;
   }

   if(calendarEventArray.length == 0)
      alert("No events selected");

   var calendarEvent;  
   var sTextiCalendar = eventArrayToICalString( calendarEventArray );
   var sTextiCalendarExport =  eventArrayToICalString( calendarEventArray, true );
   var sTextHTML = eventArrayToHTML( calendarEventArray ); 

   // 1. get the clipboard service
   var clipboard = getClipboard();

   // 2. create the transferable
  var trans = new Transferable;

   if ( trans && clipboard) {

      // 3. register the data flavors
      trans.addDataFlavor("text/calendar");
      trans.addDataFlavor("text/unicode");
      trans.addDataFlavor("text/html");

      // 4. create the data objects
    var icalWrapper = new SupportsString;
    var textWrapper = new SupportsString;
    var htmlWrapper = new SupportsString;

      if ( icalWrapper && textWrapper && htmlWrapper ) {
         // get the data
         icalWrapper.data = sTextiCalendar;        // plainTextRepresentation;
         textWrapper.data = sTextiCalendarExport;  // plainTextRepresentation;
         htmlWrapper.data = sTextHTML;             // htmlRepresentation;

         // 5. add data objects to transferable
         // Both Outlook 2000 client and Lotus Organizer use text/unicode when pasting iCalendar data
         trans.setTransferData ( "text/calendar", icalWrapper, icalWrapper.data.length*2 ); // double byte data
         trans.setTransferData ( "text/unicode", textWrapper, textWrapper.data.length*2 );
         trans.setTransferData ( "text/html", htmlWrapper, htmlWrapper.data.length*2 );

         clipboard.setData( trans, null, Components.interfaces.nsIClipboard.kGlobalClipboard );

         return true;         
      }
   }
   return true;
}


/** 
* Paste iCalendar events from the clipboard, 
* or paste clipboard text into description of new event
*/

function pasteFromClipboard()
{
   const kClipboardIID = Components.interfaces.nsIClipboard;

   if( canPaste() ) {   
      // 1. get the clipboard service
      var clipboard = getClipboard();

      // 2. create the transferable
    var trans = new Transferable;
  
      if ( trans && clipboard) {
                     
         // 3. register the data flavors you want, highest fidelity first!
         trans.addDataFlavor("text/calendar");
         trans.addDataFlavor("text/unicode");

         // 4. get transferable from clipboard
	 clipboard.getData ( trans, kClipboardIID.kGlobalClipboard);

	 // 5. ask transferable for the best flavor. Need to create new JS
         //    objects for the out params.
	 var flavour = { };
	 var data = { };
	 var length = { };
	 trans.getAnyTransferData(flavour, data, length);
	 data = data.value.QueryInterface(Components.interfaces.nsISupportsString).data;
	 //DEBUG alert("clipboard type: " + flavour.value);
    var calendarEventArray;
    var startDate;
    var endDateTime;
    var MinutesToAddOn;

	 switch (flavour.value) {
	 case "text/calendar":
            
            calendarEventArray = parseIcalData( data );
            
            //change the date of all the events to now
            startDate = gCalendarWindow.currentView.getNewEventDate();
            var categoriesStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
   
            MinutesToAddOn = getIntPref(gCalendarWindow.calendarPreferences.calendarPref, "event.defaultlength", categoriesStringBundle.GetStringFromName("defaultEventLength" ) );
   
            endDateTime = startDate.getTime() + ( 1000 * 60 * MinutesToAddOn );
   
            for( var i = 0; i < calendarEventArray.length; i++ )
            {
               calendarEventArray[i].start.setTime( startDate );
               calendarEventArray[i].end.setTime( endDateTime );
            }
	    // LINAGORA (We don't want to have to edit the event again)
            addEventsToCalendar( calendarEventArray, 1 );
            break;
	 case "text/unicode":
            if ( data.indexOf("BEGIN:VEVENT") == -1 )
            {
               // no iCalendar data, paste clipboard text into description of new event 
               calendarEvent = createEvent();
               initCalendarEvent( calendarEvent );
               calendarEvent.description = data;
               editNewEvent( calendarEvent );
            }
            else
            {
               calendarEventArray = parseIcalData( data );
               //change the date of all the events to now
               startDate = gCalendarWindow.currentView.getNewEventDate();
               MinutesToAddOn = getIntPref(gCalendarWindow.calendarPreferences.calendarPref, "event.defaultlength", 60 );
      
               endDateTime = startDate.getTime() + ( 1000 * 60 * MinutesToAddOn );
      
               for( i = 0; i < calendarEventArray.length; i++ )
               {
                  calendarEventArray[i].start.setTime( startDate );
                  calendarEventArray[i].end.setTime( endDateTime );
               }
               
               // LINAGORA (We don't want to have to edit the event again)
               addEventsToCalendar( calendarEventArray, 1 );
            }
            break;            
	 default: 
            alert("Unknown clipboard type: " + flavour.value);
	 }
      }
   }
   else
     alert( "No iCalendar or text on the clipboard." );
}
