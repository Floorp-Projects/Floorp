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
*     - Move string constants so they can be localized.
*     - Cleanup workarounds if METHOD and DTSTAMP properties are implemented.
*     - Add a clipboard listener, to enable/disable menu-items depending if 
*       valid clipboard data is available.
*     - Currently only one item is copied or pasted. This could be expanded to 
*       support multiple events.
*
******/




// Can something from dateUtils be used here?
function formatDateTime( oeDateTime )
{
   var date = new Date(oeDateTime.getTime());
   return dateService.FormatDateTime("", dateService.dateFormatLong, 
          dateService.timeFormatSeconds, date.getFullYear(), date.getMonth()+1, 
          date.getDate(), date.getHours(), date.getMinutes(), date.getSeconds());  
//   return( gCalendarWindow.dateFormater.getFormatedDate( date ) + " " +
//           gCalendarWindow.dateFormater.getFormatedTime( date ) );
}


function formatDateTimeInterval( oeDateStart, oeDateEnd )
{
   // TODO: Extend this function to create pretty looking strings
   // When: Thursday, November 09, 2000 11:00 PM-11:30 PM (GMT-08:00) Pacific Time (US & Canada); Tijuana.
   // When: 7/1/1997 10:00AM PDT - 7/1/97 10:30AM PDT
   return formatDateTime(oeDateStart) + " - " + formatDateTime(oeDateEnd);
}


/** 
* Initialize an event with a start and end date.
*/

function initCalendarEvent( calendarEvent )
{
   var startDate = gCalendarWindow.currentView.getNewEventDate();

   var Minutes = Math.ceil( startDate.getMinutes() / 5 ) * 5 ;

   startDate = new Date( startDate.getFullYear(),
                         startDate.getMonth(),
                         startDate.getDate(),
                         startDate.getHours(),
                         Minutes,
                         0);

   calendarEvent.start.setTime( startDate );
   
   var MinutesToAddOn = gCalendarWindow.calendarPreferences.getPref( "defaulteventlength" );

   var endDateTime = startDate.getTime() + ( 1000 * 60 * MinutesToAddOn );

   calendarEvent.end.setTime( endDateTime );
}


function getClipboard()
{
   const kClipboardContractID = "@mozilla.org/widget/clipboard;1";
   const kClipboardIID = Components.interfaces.nsIClipboard;
   return Components.classes[kClipboardContractID].getService(kClipboardIID);
}


function createTransferable()
{
   const kTransferableContractID = "@mozilla.org/widget/transferable;1";
   const kTransferableIID = Components.interfaces.nsITransferable
   return Components.classes[kTransferableContractID].createInstance(kTransferableIID);
}


function createSupportsArray()
{
   const kSuppArrayContractID = "@mozilla.org/supports-array;1";
   const kSuppArrayIID = Components.interfaces.nsISupportsArray;
   return Components.classes[kSuppArrayContractID].createInstance(kSuppArrayIID);
}


// Should we use WString only, or no WStrings for compatibility?
function createSupportsString()
{
   const kSuppStringContractID = "@mozilla.org/supports-string;1";
   const kSuppStringIID = Components.interfaces.nsISupportsString;
   return Components.classes[kSuppStringContractID].createInstance(kSuppStringIID);
}


function createSupportsWString()
{
   const kSuppStringContractID = "@mozilla.org/supports-wstring;1";
   const kSuppStringIID = Components.interfaces.nsISupportsWString;
   return Components.classes[kSuppStringContractID].createInstance(kSuppStringIID);
}


/** 
* Test if the clipboard has items that can be pasted into Calendar.
* This must be type "text/calendar" or "text/unicode"
*/

function canPaste()
{
   const kClipboardIID = Components.interfaces.nsIClipboard;

   var clipboard = getClipboard();
   var flavourArray = createSupportsArray();   
   var flavours = ["text/calendar", "text/unicode"];
   for (var i = 0; i < flavours.length; ++i) {
      const kSuppString = createSupportsString();
      kSuppString.data = flavours[i];
      flavourArray.AppendElement(kSuppString);
   }
   return clipboard.hasDataMatchingFlavors(flavourArray, kClipboardIID.kGlobalClipboard);
}


/** 
* Convert an event to a block of HTML code, no headers
* Sample:
*    When: Thursday, November 09, 2000 11:00 PM-11:30 PM (GMT-08:00) Pacific Time (US & Canada); Tijuana.
*    Where: San Francisco
*    Organizer: foo1@example.com
*    Summary: Phone Conference
*
*/

function eventToHTML( calendarEvent )
{
   var sTextHTML = "";
   sTextHTML += "<B>When:</B>\t" + formatDateTimeInterval(calendarEvent.start, calendarEvent.end) + "<BR>";
   sTextHTML += "<B>Where:</B>\t" + calendarEvent.location + "<BR>";
   // sTextHTML += "<B>Organiser:</B>\t" + Event.???
   sTextHTML += "<B>Summary:</B>\t" + calendarEvent.description + "<BR>";
   return sTextHTML;
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

function copyToClipboard()
{
   var calendarEvent = gCalendarWindow.EventSelection.selectedEvents[0];
   if(!calendarEvent)
      alert("No events selected");

   var sTextiCalendar = calendarEvent.getIcalString();
   
   // Create a second ical data string for manipulation to export patched iCalendar to outlook.
   var sTextiCalendarPatched = sTextiCalendar;
   
   // TODO HACK: Remove this hack when Calendar supports METHOD properties
   var i = sTextiCalendarPatched.indexOf("METHOD")
   if(i = -1) 
   {
      var i = sTextiCalendarPatched.indexOf("VERSION")
      if(i != -1) {
         sTextiCalendarPatched = 
            sTextiCalendarPatched.substring(0,i) + "METHOD:PUBLISH\n" + sTextiCalendarPatched.substring(i, sTextiCalendarPatched.length);
      }
   }
   
   // TODO HACK: Remove this hack when Calendar supports DTSTAMP properties
   var i = sTextiCalendarPatched.indexOf("DTSTAMP")
   if(i = -1) 
   {
      var i = sTextiCalendarPatched.indexOf("UID")
      if(i != -1) {
         sTextiCalendarPatched = 
            sTextiCalendarPatched.substring(0,i) + "DTSTAMP:20020430T114937Z\n" + sTextiCalendarPatched.substring(i, sTextiCalendarPatched.length);
      }
   }
   
   // HACK: TRIGGER patch hack for Outlook 2000
   var i = sTextiCalendarPatched.indexOf("TRIGGER\n ;VALUE=DURATION\n :-");
   if(i != -1) {
      sTextiCalendarPatched =
         sTextiCalendarPatched.substring(0,i+27) + sTextiCalendarPatched.substring(i+28, sTextiCalendarPatched.length);
   }
 
   var sTextHTML = eventToHTML( calendarEvent ); 

   // 1. get the clipboard service
   var clipboard = getClipboard();

   // 2. create the transferable
   var trans = createTransferable(); 

   if ( trans && clipboard) {
                     
      // 3. register the data flavors
      trans.addDataFlavor("text/calendar");
      trans.addDataFlavor("text/unicode");
      trans.addDataFlavor("text/html");
                     
      // 4. create the data objects
      var icalWrapper = createSupportsWString();
      var textWrapper = createSupportsWString();
      var htmlWrapper = createSupportsWString();
                     
      if ( icalWrapper && textWrapper && htmlWrapper ) {
         // get the data
         icalWrapper.data = sTextiCalendar;        // plainTextRepresentation;
         textWrapper.data = sTextiCalendarPatched; // plainTextRepresentation;
         htmlWrapper.data = sTextHTML;             // htmlRepresentation;
                     
         // 5. add data objects to transferable
         // Both Outlook 2000 client and Lotus Organizer use text/unicode when pasting iCalendar data
         trans.setTransferData ( "text/calendar", icalWrapper, icalWrapper.data.length*2 ); // double byte data
         trans.setTransferData ( "text/unicode", textWrapper, textWrapper.data.length*2 );
         trans.setTransferData ( "text/html", htmlWrapper, htmlWrapper.data.length*2 );
            
         clipboard.setData( trans, null, Components.interfaces.nsIClipboard.kGlobalClipboard );
      }
          
   }   
}


/** 
* Paste an iCalendar event from the clipboard, or paste clipboard text into description
*/

function pasteFromClipboard()
{
   const kClipboardIID = Components.interfaces.nsIClipboard;

   if( canPaste() ) {   
      calendarEvent = createEvent();
 
      // 1. get the clipboard service
      var clipboard = getClipboard();

      // 2. create the transferable
      var trans = createTransferable();
  
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
	 data = data.value.QueryInterface(Components.interfaces.nsISupportsWString).data;
	 //DEBUG alert("clipboard type: " + flavour.value);
	 switch (flavour.value) {
	 case "text/calendar": // at the moment "text/calendar" only contains standard iCalendar text 
	 case "text/unicode":
         // strip VCALENDAR part. Workaround for parseIcalString
         // TODO: Clean this up if parseIcalString is fixed
         var i = data.indexOf("BEGIN:VEVENT");
         calendarEvent.parseIcalString(data.substring(i, data.length));
         if(calendarEvent.start.year == 0) // Is there a beter way to detect failed parsing?
         {
            // initialize start and end dates.
            initCalendarEvent( calendarEvent )

            // parsing import iCalendar failed. Save clipboard text into description.
            calendarEvent.description = data;      
         }
         break;
	 default: 
            alert("Unknown clipboard type: " + flavour.value);
	 }
      }
      editNewEvent( calendarEvent );
   }
   else
     alert( "No iCalendar or text on the clipboard." );
}
