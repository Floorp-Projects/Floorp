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
 
/**** calendarImportExport
 * Unit with functions to convert calendar events to and from different formats.
 *
 ****/


// File constants copied from file-utils.js
const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_RDWR     = 0x04;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;
const MODE_SYNC     = 0x40;
const MODE_EXCL     = 0x80;


/**** loadEventsFromFile
 * shows a file dialog, read the selected file and tries to parse events from it.
 */

function loadEventsFromFile()
{
   const nsIFilePicker = Components.interfaces.nsIFilePicker;
   
   var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
     
   fp.init(window, "Open", nsIFilePicker.modeOpen);
	
   // fp.defaultString = "*.ics";
   fp.defaultExtension = "ics"
   
   fp.appendFilter( "Calendar Files", "*.ics" );
   
   fp.show();

   if (fp.file && fp.file.path.length > 0) 
   {
      var aDataStream = readDataFromFile( fp.file.path );
      var calendarEventArray = parseIcalData( aDataStream );

      // Show a dialog with option to import events with or without dialogs
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(); 
      promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService); 
      var result = {value:0}; 
      
      var buttonPressed =      
      promptService.confirmEx(window, 
           "Import", "About to import " + calendarEventArray.length + " event(s). Do you want to open all events to import before importing?", 
           (promptService.BUTTON_TITLE_YES * promptService.BUTTON_POS_0) + 
           (promptService.BUTTON_TITLE_NO * promptService.BUTTON_POS_1) + 
           (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_2), 
            null,null,null,null, result); 
      if(buttonPressed == 0) // YES
      { 
      addEventsToCalendar( calendarEventArray );
   }
      else if(buttonPressed == 1) // NO
      { 
        addEventsToCalendar( calendarEventArray, true );
      } 
      else if(buttonPressed == 2) // CANCEL
      { 
         return false; 
      } 
   }
}


/**** createUniqueID
 *
 * Creates a new unique ID. Format copied from the oeICalImpl.cpp AddEvent function
 */

function createUniqueID()
{
   var newID = "";
   while( (newID == "") || (gICalLib.fetchEvent( newID ) != null) )
     newID = Math.round(900000000 + (Math.random() * 100000000));
   return newID;
}


/**** insertEvents
 *
 * Takes an array of events and adds the evens one by one to the calendar
 */
 
function addEventsToCalendar( calendarEventArray, silent )
{
   for(var i = 0; i < calendarEventArray.length; i++)
   {
      calendarEvent = calendarEventArray[i]
	       
      // Check if event with same ID already in Calendar. If so, import event with new ID.
      if( gICalLib.fetchEvent( calendarEvent.id ) != null ) {
         calendarEvent.id = createUniqueID( );
      }

      // the start time is in zulu time, need to convert to current time
      convertZuluToLocal( calendarEvent );

      // open the event dialog with the event to add
      if( silent )
         gICalLib.addEvent( calendarEvent );
      else
         editNewEvent( calendarEvent );
   }
}

const ZULU_OFFSET_MILLIS = new Date().getTimezoneOffset() * 60 * 1000;

function convertZuluToLocal( calendarEvent )
{
   var zuluStartTime = calendarEvent.start.getTime();
   var zuluEndTime = calendarEvent.end.getTime();
   calendarEvent.start.setTime( zuluStartTime  - ZULU_OFFSET_MILLIS );
   calendarEvent.end.setTime( zuluEndTime  - ZULU_OFFSET_MILLIS );
}

function convertLocalToZulu( calendarEvent )
{
   var zuluStartTime = calendarEvent.start.getTime();
   var zuluEndTime = calendarEvent.end.getTime();
   calendarEvent.start.setTime( zuluStartTime  + ZULU_OFFSET_MILLIS );
   calendarEvent.end.setTime( zuluEndTime  + ZULU_OFFSET_MILLIS );
}

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


/**** parseIcalData
 *
 * Takes a text block of iCalendar events and tries to split that into individual events.
 * Parses those events and returns an array of calendarEvents.
 */
 
function parseIcalData( icalStr )
{
   var calendarEventArray =  new Array();

   var i,j;
   while( icalStr.indexOf("BEGIN:VEVENT") != -1 )
   { 
      // try to find the begin and end of an event. ParseIcalString does not support VCALENDAR
      i = icalStr.indexOf("BEGIN:VEVENT");
      j = icalStr.indexOf("END:VEVENT") + 10;
      eventData = icalStr.substring(i, j);

      calendarEvent = createEvent();

      if ( !calendarEvent.parseIcalString(eventData) )
      // parsing import iCalendar failed. 
      {
         // initialize start and end dates.
         initCalendarEvent( calendarEvent );

         // Save clipboard text into description.
         calendarEvent.description = icalStr;      
      }

      calendarEventArray[ calendarEventArray.length ] = calendarEvent;
      // remove the parsed VEVENT from the calendar data to parse
      icalStr = icalStr.substring(j+1);
   }
   
   return calendarEventArray;
}


/**** readDataFromFile
 *
 * read data from a file. Returns the data read.
 */

function readDataFromFile( aFilePath )
{
   const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
   const FILEIN_CTRID = "@mozilla.org/network/file-input-stream;1";
   const SCRIPTSTREAM_CTRID = "@mozilla.org/scriptableinputstream;1";   
   const nsILocalFile = Components.interfaces.nsILocalFile;
   const nsIFileInputStream = Components.interfaces.nsIFileInputStream;
   const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

   var localFileInstance;
   var inputStream;
   var scriptableInputStream;
   var tmp; // not sure what the use is for this
   
   LocalFileInstance = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   LocalFileInstance.initWithPath( aFilePath );

   inputStream = Components.classes[FILEIN_CTRID].createInstance( nsIFileInputStream );
   try
   {
      inputStream.init( LocalFileInstance, MODE_RDONLY, 0444, tmp );
      
      scriptableInputStream = Components.classes[SCRIPTSTREAM_CTRID].createInstance( nsIScriptableInputStream);
      scriptableInputStream.init( inputStream );

      aDataStream = scriptableInputStream.read( -1 );
      scriptableInputStream.close();
      inputStream.close();
   }
   catch(ex)
   {
      alert("Unable to read from file: " + aFilePath );
   }

   return aDataStream;
}


/**** saveEventsToFile
 *
 * Save data to a file. Create the file or overwrite an existing file.
 * Input an array of calendar events, or no parameter for selected events.
 */

function saveEventsToFile( calendarEventArray )
{
   if( !calendarEventArray)
      var calendarEventArray = gCalendarWindow.EventSelection.selectedEvents;

   if (calendarEventArray.length == 0)
   {
      alert("No events selected to save");
      return;
   }

   // No show the 'Save As' dialog and ask for a filename to save to
   const nsIFilePicker = Components.interfaces.nsIFilePicker;
   
   var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
   
   // caller can force disable of sand box, even if ON globally
   
   fp.init(window, "Save As", nsIFilePicker.modeSave);
	
   fp.defaultString = "Mozilla Calendar events";
   fp.defaultExtension = "ics";

   fp.appendFilter( "Calendar Files", "*.ics" );
   fp.appendFilter( "Rich Text Format (RTF)", "*.rtf" );
   fp.appendFilters(nsIFilePicker.filterHTML);
   
   fp.show();

   // Now find out as what to save, convert the events and save to file.
   if (fp.file && fp.file.path.length > 0) 
   {
      var aDataStream;
      switch (fp.filterIndex) {
      case 0 : // ics
         aDataStream = eventArrayToICalString( calendarEventArray, true );
         break;
      case 1 : // rtf
         aDataStream = eventArrayToRTF( calendarEventArray );
         break;
      case 2 : // html
         aDataStream = eventArrayToHTML( calendarEventArray );
         break;
      }
      saveDataToFile(fp.file.path, aDataStream);
   }
}


/**** eventArrayToICalString
 * Converts a array of events to iCalendar text
 * Option to add events needed in other applications
 */

function eventArrayToICalString( calendarEventArray, doPatchForExport )
{   
   if( !calendarEventArray)
      calendarEventArray = gCalendarWindow.EventSelection.selectedEvents;

   var sTextiCalendar = "";
   for( var eventArrayIndex = 0;  eventArrayIndex < calendarEventArray.length; ++eventArrayIndex )
   {
      var calendarEvent = calendarEventArray[ eventArrayIndex ];

      // convert time to represent local to produce correct DTSTART and DTEND
      convertLocalToZulu( calendarEvent );
     
      if ( doPatchForExport )
        sTextiCalendar += patchICalStringForExport( calendarEvent.getIcalString() );
      else
        sTextiCalendar += calendarEvent.getIcalString() ;
   }

   return sTextiCalendar;
}


/**** patchICalStringForExport
 * Function to hack an iCalendar text block for use in other applications
 *  Adds METHOD and DTSTAMP fields, and patch TRIGGER field for Outlook
 */
 
function patchICalStringForExport( sTextiCalendar )
{
   // HACK: Remove this hack when Calendar supports METHOD properties
   var i = sTextiCalendar.indexOf("METHOD")
   if(i == -1) 
   {
      var i = sTextiCalendar.indexOf("VERSION")
      if(i != -1) {
         sTextiCalendar = 
            sTextiCalendar.substring(0,i) + "METHOD:PUBLISH\n" + sTextiCalendar.substring(i, sTextiCalendar.length);
      }
   }
   
   // HACK: Remove this hack when Calendar supports DTSTAMP properties
   var i = sTextiCalendar.indexOf("DTSTAMP")
   if(i == -1) 
   {
      var i = sTextiCalendar.indexOf("UID")
      if(i != -1) {
         sTextiCalendar = 
            sTextiCalendar.substring(0,i) + "DTSTAMP:20020430T114937Z\n" + sTextiCalendar.substring(i, sTextiCalendar.length);
      }
   }
   
   // HACK: TRIGGER patch hack for Outlook 2000
   var i = sTextiCalendar.indexOf("TRIGGER\n ;VALUE=DURATION\n :-");
   if(i != -1) {
      sTextiCalendar =
         sTextiCalendar.substring(0,i+27) + sTextiCalendar.substring(i+28, sTextiCalendar.length);
   }

  return sTextiCalendar;
}


/** 
* Converts a array of events to a block of HTML code
* Sample:
*    Summary: Phone Conference
*    Description: annual report
*    When: Thursday, November 09, 2000 11:00 PM-11:30 PM (GMT-08:00) Pacific Time (US & Canada); Tijuana.
*    Where: San Francisco
*    Organizer: foo1@example.com
*/

function eventArrayToHTML( calendarEventArray )
{
   sHTMLHeader = 
      "<html>\n" + "<head>\n" + "<title>Mozilla Calendar</title>\n" +
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">\n" +
      "</head>\n"+ "<body bgcolor=\"#FFFFFF\" text=\"#000000\">\n";
   sHTMLFooter =
      "\n</body>\n</html>\n";

   var sHTMLText = sHTMLHeader;

   for( var eventArrayIndex = 0;  eventArrayIndex < calendarEventArray.length; ++eventArrayIndex )
   {
      var calendarEvent = calendarEventArray[ eventArrayIndex ];
      sHTMLText += "<p>";
      sHTMLText += "<B>Summary: </B>\t" + calendarEvent.title + "<BR>\n";
      sHTMLText += "<B>Description: </B>\t" + calendarEvent.description + "<BR>\n";
      sHTMLText += "<B>When: </B>" + formatDateTimeInterval(calendarEvent.start, calendarEvent.end) + "<BR>\n";
      sHTMLText += "<B>Where: </B>" + calendarEvent.location + "<BR>\n";
      // sHTMLText += "<B>Organiser: </B>\t" + Event.???
      sHTMLText += "</p>";
   }
   sHTMLText += sHTMLFooter;
   return sHTMLText;
}


/**** eventArrayToRTF
* Converts a array of events to a block of text in Rich Text Format
*/

function eventArrayToRTF( calendarEventArray )
{
   sRTFHeader = 
      "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fswiss\\fcharset0 Arial;}{\\f1\\fmodern\\fcharset0 Courier New;}}" +
      "{\\colortbl ;\\red0\\green0\\blue0;}" +
      "\\viewkind4\\uc1\\pard\\fi-1800\\li1800\\tx1800\\cf1";
   sRTFFooter =
      "\\pard\\fi-1800\\li1800\\tx1800\\cf1\\f0\\par}";

   var sRTFText = sRTFHeader;

   for( var eventArrayIndex = 0;  eventArrayIndex < calendarEventArray.length; ++eventArrayIndex )
   {
      var calendarEvent = calendarEventArray[ eventArrayIndex ];
      sRTFText += "\\b\\f0\\fs20 " + "Summary:" + "\\b0\\tab " + calendarEvent.title + "\\par\n";
      sRTFText += "\\b\\f0\\fs20 " + "Description:" + "\\b0\\tab " + calendarEvent.description + "\\par\n";

      sRTFText += "\\b When:\\b0\\tab " + formatDateTimeInterval(calendarEvent.start, calendarEvent.end) + "\\par\n";

      sRTFText += "\\b Where:\\b0\\tab " + calendarEvent.location + "\\par\n";
      sRTFText += "\\par\n";
   }
   sRTFText += sRTFFooter;
   return sRTFText;
}


/**** saveDataToFile
 *
 * Save data to a file. Creates a new file or overwrites an existing file.
 */

function saveDataToFile(aFilePath, aDataStream)
{
   const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
   const FILEOUT_CTRID = "@mozilla.org/network/file-output-stream;1";
   const nsILocalFile = Components.interfaces.nsILocalFile;
   const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;

   var localFileInstance;
   var outputStream;
   
   LocalFileInstance = Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
   LocalFileInstance.initWithPath(aFilePath);

   outputStream = Components.classes[FILEOUT_CTRID].createInstance(nsIFileOutputStream);
   try
   {
      outputStream.init(LocalFileInstance, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, 0664, 0);
      outputStream.write(aDataStream, aDataStream.length);
      // outputStream.flush();
      outputStream.close();
   }
   catch(ex)
   {
      alert("Unable to write to file: " + aFilePath );
   }
}
