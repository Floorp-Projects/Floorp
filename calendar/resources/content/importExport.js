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
 *                 Steve Hampton <mvgrad78@yahoo.com>
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

// XSL stylesheet directory
var convertersDirectory = "chrome://calendar/content/converters/";

// File constants copied from file-utils.js
const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_RDWR     = 0x04;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;
const MODE_SYNC     = 0x40;
const MODE_EXCL     = 0x80;

const filterCalendar    = "Calendar Files";
const extensionCalendar = ".ics";
const filterXcs         = "iCalendar XML Document";
const extensionXcs      = ".xcs";
const filterXml         = "XML Document";
const extensionXml      = ".xml";
const filterRtf         = "Rich Text Format (RTF)";
const extensionRtf      = ".rtf";
const filterHtml        = "HTML Files";
const extensionHtml     = ".html";
const filterCsv         = "Comma Separated";
const filterOutlookCsv  = "Outlook Comma Separated";
const extensionCsv      = ".csv";
const filterRdf         = "iCalendar RDF";
const extensionRdf      = ".rdf";

if( opener && opener.gICalLib )
   gICalLib = opener.gICalLib;

// convert to and from Unicode for file i/o
function convertFromUnicode( aCharset, aSrc )
{
   // http://lxr.mozilla.org/mozilla/source/intl/uconv/idl/nsIScriptableUConv.idl
   var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
   unicodeConverter.charset = aCharset;
   return unicodeConverter.ConvertFromUnicode( aSrc );
}

function convertToUnicode(aCharset, aSrc )
{
   // http://lxr.mozilla.org/mozilla/source/intl/uconv/idl/nsIScriptableUConv.idl
   var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
   unicodeConverter.charset = aCharset;
   return unicodeConverter.ConvertToUnicode( aSrc );
}

/**** loadEventsFromFile
 * shows a file dialog, read the selected file(s) and tries to parse events from it.
 */

function loadEventsFromFile()
{

  var dupResult = { cancelled: false, discard: true, prompt: false };

  openDialog("chrome://calendar/content/importDuplicatesDialog.xul", "caDuplicates", "chrome,modal,centerscreen", dupResult );
  if (dupResult.cancelled == true)
    return false;

//   dump("*******************\n");
//   dump("cancelled: " + dupResult.cancelled + "\n");
//   dump("discard:   " + dupResult.discard + "\n");
//   dump("prompt:    " + dupResult.prompt + "\n");
//   dump("*******************\n");

  const nsIFilePicker = Components.interfaces.nsIFilePicker;
  
  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, "Open", nsIFilePicker.modeOpenMultiple);
  fp.defaultExtension = "ics";
   
  fp.appendFilter( filterCalendar, "*" + extensionCalendar );
  fp.appendFilter( filterXcs, "*" + extensionXcs );
  fp.appendFilter( filterOutlookCsv, "*" + extensionCsv );
  fp.show();
  var filesToAppend = fp.files;

  if (filesToAppend && filesToAppend.hasMoreElements()) 
  {
      var calendarEventArray = new Array();
      var duplicateEventArray = new Array();
      var currentFile;
      var aDataStream;
      var i;
      var tempEventArray;

      while (filesToAppend.hasMoreElements())
      {
          currentFile = filesToAppend.getNext().QueryInterface(Components.interfaces.nsILocalFile);
          aDataStream = readDataFromFile( currentFile.path, "UTF-8" );

          switch (fp.filterIndex) {
          case 0 : // ics
            tempEventArray = parseIcalData( aDataStream );
            for (i=0; i < tempEventArray.length; i++)
              calendarEventArray[calendarEventArray.length]  = tempEventArray[i];    
            break;
          case 1 : // xcs
            tempEventArray = parseXCSData( aDataStream );
            for (i=0; i < tempEventArray.length; i++)
              calendarEventArray[calendarEventArray.length]  = tempEventArray[i];    
            break;
          case 2: // csv
            var ret = parseOutlookCSVData( aDataStream, dupResult.discard, dupResult.prompt );
            for (i=0; i < ret.calendarEventArray.length; i++)
               calendarEventArray[calendarEventArray.length]  = ret.calendarEventArray[i];
            for (i=0; i < ret.calendarDuplicateArray.length; i++)
	            duplicateEventArray[duplicateEventArray.length] = ret.calendarDuplicateArray[i];
            break;
          }
      }

      // If there are no events to import, let the user know
      //
      if (calendarEventArray.length == 0 && (duplicateEventArray.length == 0 || dupResult.discard == true) ) {
        alert("No events to import...");
        return false;
      }
      
      // Show a dialog with option to import events with or without dialogs
      var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(); 
      promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService); 
      var result = {value:0}; 

      if (calendarEventArray.length > 0) {

        var buttonPressed =      
          promptService.confirmEx(window, 
                                  "Import", "About to import " + calendarEventArray.length + " event(s).\nDo you want to open all new events to import before importing?", 
                                  (promptService.BUTTON_TITLE_YES * promptService.BUTTON_POS_0) + 
                                  (promptService.BUTTON_TITLE_NO * promptService.BUTTON_POS_1) + 
                                  (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_2), 
                                  null,null,null,null, result); 

        if(buttonPressed == 0) // YES
        { 
            addEventsToCalendar( calendarEventArray );
            return true;
        }
        else if(buttonPressed == 1) // NO
        { 
            addEventsToCalendar( calendarEventArray, true );
            return true;
        } 
        else if(buttonPressed == 2) // CANCEL
        { 
            return false; 
        } 
   }


      // Depending on how the user chose to deal with duplicates,
      // either add them blindly, or prompt then for each.
      //
      if (duplicateEventArray.length > 0) {

        if (dupResult.discard == false) {

          if (dupResult.prompt)
            alert("Some duplicate entries were imported. Each one will display in the New Event dialog,\n where you can skip it using Cancel, or edit and accept it using OK.");

          addEventsToCalendar( duplicateEventArray, !dupResult.prompt );
        }

      }
    }
  
  
  return false;
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
 
function addEventsToCalendar( calendarEventArray, silent, ServerName )
{
   gICalLib.batchMode = true;

   for(var i = 0; i < calendarEventArray.length; i++)
   {
      calendarEvent = calendarEventArray[i];
	       
      // Check if event with same ID already in Calendar. If so, import event with new ID.
      if( gICalLib.fetchEvent( calendarEvent.id ) != null ) {
         calendarEvent.id = createUniqueID( );
      }

      // the start time is in zulu time, need to convert to current time
      if(calendarEvent.allDay != true)
      convertZuluToLocal( calendarEvent );

      // open the event dialog with the event to add
      if( silent )
      {
         if( ServerName == null || ServerName == "" || ServerName == false )
            var ServerName = gCalendarWindow.calendarManager.getDefaultServer();
         
         gICalLib.addEvent( calendarEvent, ServerName );
      }
      else
         editNewEvent( calendarEvent );
   }

   gICalLib.batchMode = false;   
}

const ZULU_OFFSET_MILLIS = new Date().getTimezoneOffset() * 60 * 1000;

function convertZuluToLocal( calendarEvent )
{
   if( calendarEvent.start.utc == true )
   {
   var zuluStartTime = calendarEvent.start.getTime();
   calendarEvent.start.setTime( zuluStartTime  - ZULU_OFFSET_MILLIS );
      calendarEvent.start.utc = false;
   }
   if( calendarEvent.end.utc == true )
   {
      var zuluEndTime = calendarEvent.end.getTime();
   calendarEvent.end.setTime( zuluEndTime  - ZULU_OFFSET_MILLIS );
      calendarEvent.end.utc = false;
   }
}

function convertLocalToZulu( calendarEvent )
{
   if( calendarEvent.start.utc == false )
   {
   var zuluStartTime = calendarEvent.start.getTime();
   calendarEvent.start.setTime( zuluStartTime  + ZULU_OFFSET_MILLIS );
      calendarEvent.start.utc = true;
   }
   if( calendarEvent.end.utc == false )
   {
      var zuluEndTime = calendarEvent.end.getTime();
   calendarEvent.end.setTime( zuluEndTime  + ZULU_OFFSET_MILLIS );
      calendarEvent.end.utc = true;
   }
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
   
   var MinutesToAddOn = getIntPref(gCalendarWindow.calendarPreferences.calendarPref, "event.defaultlength", 60 );

   var endDateTime = startDate.getTime() + ( 1000 * 60 * MinutesToAddOn );

   calendarEvent.end.setTime( endDateTime );
}


function datesAreEqual(icalDate, date) {

  if (
      icalDate.month  == date.getMonth() &&
      icalDate.day    == date.getDate()  &&
      icalDate.year   == date.getFullYear() &&
      icalDate.hour   == date.getHours() &&
      icalDate.minute == date.getMinutes())
    return true;
  else
    return false;
}

function entryExists( date, subject) {

  var events = gCalendarWindow.eventSource.getEventsForDay( date );

  var ret = false;

  if (events.length == 0)
    return false;

  for (var i = 0; i < events.length; i++) {

    var event = events[i].event;

    if ( event.title == subject && datesAreEqual(event.start, date)) {
      ret = true;
      break;
    }
  }

  return ret;
}

function promptToKeepEntry(title, startTime, endTime) 
{
  return confirm(
                 "Add duplicate entry:\n\n" + 
                 "Title:      " + title + "\n" +
                 "Start Time: " + startTime.toString() + "\n" +
                 "End Time:   " + endTime.toString() + "\n" 
                 );

}


/**** parseOutlookCSVData
 *
 * Takes a text block of iCalendar events and tries to split that into individual events.
 * Parses those events and returns an array of calendarEvents.
 */
function parseOutlookCSVData( icalStr, discardDuplicates, promptEach )
{
  var lines = icalStr.split("\n");
  var calendarEvent;
  var eventArray = new Array();
  var dupArray = new Array();
  var lineIndex = 1;
  var totalLines = lines.length-1;
  var exists = false;

  while (lineIndex < totalLines) {

    var fields = lines[lineIndex].split('","');
    fields[0] = fields[0].substring(1); // strip off the leading quote...

    var title = fields[0];
    var stime = new Date(fields[1] + " " + fields[2]);
    var etime = new Date(fields[3] + " " + fields[4]);

    exists = entryExists(stime, title);
      
    calendarEvent = createEvent();
    calendarEvent.id = createUniqueID( );
    calendarEvent.title = title;
    calendarEvent.start.setTime(stime);
    calendarEvent.end.setTime(etime);

    if ( !exists )
      eventArray[ eventArray.length ] = calendarEvent;
    else
      dupArray[ dupArray.length ] = calendarEvent;


    ++lineIndex;
  }

//   dump("*** calendar entries : " + eventArray.length + "\n");
//   dump("*** duplicate entries: " + dupArray.length + "\n");

  return { calendarEventArray: eventArray, calendarDuplicateArray: dupArray };
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

      // if parsing import iCalendar failed, add date as description
      if ( !calendarEvent.parseIcalString(eventData) )
      {
         // initialize start and end dates.
         initCalendarEvent( calendarEvent );

         // Save the parsed text as description.
         calendarEvent.description = icalStr;      
      }

      calendarEventArray[ calendarEventArray.length ] = calendarEvent;
      // remove the parsed VEVENT from the calendar data to parse
      icalStr = icalStr.substring(j+1);
   }
   
   return calendarEventArray;
}

/**** parseXCSData
 *
 */
 
function parseXCSData( xcsString )
{
   var gParser = new DOMParser;
   var xmlDocument = gParser.parseFromString(xcsString, 'text/xml');   

   var result = serializeDocument(xmlDocument, "xcs2ics.xsl");

   return parseIcalData( result );
}


/**** readDataFromFile
 *
 * read data from a file. Returns the data read.
 */

function readDataFromFile( aFilePath, charset )
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
      
      if( charset)
         aDataStream = convertToUnicode( charset, aDataStream );
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
      calendarEventArray = gCalendarWindow.EventSelection.selectedEvents;

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

   if(calendarEventArray.length == 1 && calendarEventArray[0].title)
      fp.defaultString = calendarEventArray[0].title;
   else
      fp.defaultString = "Mozilla Calendar events";

   fp.defaultExtension = "ics";

   fp.appendFilter( filterCalendar, "*" + extensionCalendar );
   fp.appendFilter( filterRtf, "*" + extensionRtf );
   fp.appendFilters(nsIFilePicker.filterHTML);
   fp.appendFilter( filterCsv, "*" + extensionCsv );
   fp.appendFilter( filterXcs, "*" + extensionXcs );
   fp.appendFilter( filterRdf, "*" + extensionRdf );

   fp.show();

   // Now find out as what to save, convert the events and save to file.
   if (fp.file && fp.file.path.length > 0) 
   {
      const UTF8 = "UTF-8";
      var aDataStream;
      var extension;
      var charset;
      switch (fp.filterIndex) {
      case 0 : // ics
         aDataStream = eventArrayToICalString( calendarEventArray, true );
         extension   = extensionCalendar;
         charset = "UTF-8";
         break;
      case 1 : // rtf
         aDataStream = eventArrayToRTF( calendarEventArray );
         extension   = extensionRtf;
         break;
      case 2 : // html
         aDataStream = eventArrayToHTML( calendarEventArray );
         extension   = ".htm";
         charset = "UTF-8";
         break;
      case 3 : // csv
         aDataStream = eventArrayToCsv( calendarEventArray );
         extension   = extensionCsv;
         charset = "UTF-8";
         break;
      case 4 : // xCal
         aDataStream = eventArrayToXCS( calendarEventArray );
         extension   = extensionXcs;
         charset = "UTF-8";
         break;
      case 5 : // rdf
         aDataStream = eventArrayToRdf( calendarEventArray );
         extension   = extensionRdf;
         charset = "UTF-8";
         break;
      }
      var filePath = fp.file.path;
      if(filePath.indexOf(".") == -1 )
          filePath += extension;

      saveDataToFile( filePath, aDataStream, charset );
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
      var calendarEvent = calendarEventArray[ eventArrayIndex ].clone();

      // convert time to represent local to produce correct DTSTART and DTEND
      if(calendarEvent.allDay != true)
      convertLocalToZulu( calendarEvent );
     
      // check if all required properties are available
      if( calendarEvent.method == 0 )
         calendarEvent.method = calendarEvent.ICAL_METHOD_PUBLISH;
      if( calendarEvent.stamp.year ==  0 )
         calendarEvent.stamp.setTime( new Date() );

      if ( doPatchForExport )
        sTextiCalendar += patchICalStringForExport( calendarEvent.getIcalString() );
      else
        sTextiCalendar += calendarEvent.getIcalString() ;
   }

   return sTextiCalendar;
}


/**** patchICalStringForExport
 * Function to hack an iCalendar text block for use in other applications
 *  Patch TRIGGER field for Outlook
 */
 
function patchICalStringForExport( sTextiCalendar )
{
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
      "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n" +
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


/**** eventArrayToXML
* Converts a array of events to a XML string
*/
/*
function eventArrayToXML( calendarEventArray )
{
   var xmlDocument = getXmlDocument( calendarEventArray );
   var serializer = new XMLSerializer;
   
   return serializer.serializeToString (xmlDocument )
}
*/

/**** eventArrayToCsv
* Converts a array of events to comma delimited  text.
*/

function eventArrayToCsv( calendarEventArray )
{
   var xcsDocument = getXcsDocument( calendarEventArray );

   return serializeDocument( xcsDocument, "xcs2csv.xsl" );
}


/**** eventArrayToRdf
* Converts a array of events to RDF
*/

function eventArrayToRdf( calendarEventArray )
{
   var xcsDocument = getXcsDocument( calendarEventArray );

   return serializeDocument( xcsDocument, "xcs2rdf.xsl" );
}


/**** eventArrayToXCS
* Converts a array of events to a xCal string
*/

function eventArrayToXCS( calendarEventArray )
{
   var xmlDoc = getXmlDocument( calendarEventArray );
   var xcsDoc = transformXML( xmlDoc, "xml2xcs.xsl" );
   // add the doctype
/* Doctype uri blocks excel import
   var newdoctype = xcsDoc.implementation.createDocumentType(
       "iCalendar", 
       "-//IETF//DTD XCAL//iCalendar XML//EN",
       "http://www.ietf.org/internet-drafts/draft-ietf-calsch-many-xcal-02.txt");
   if (newdoctype)
      xcsDoc.insertBefore(newdoctype, xcsDoc.firstChild);
*/
   var serializer = new XMLSerializer;

   // XXX MAJOR UGLY HACK!! Serializer doesn't insert XML Declaration
   // http://bugzilla.mozilla.org/show_bug.cgi?id=63558
   var serialDocument = serializer.serializeToString ( xcsDoc );

   if( serialDocument.indexOf( "<?xml" ) == -1 )
      serialDocument = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + serialDocument;

   return serialDocument;
   // return serializer.serializeToString ( xcsDoc )
}


/**** saveDataToFile
 *
 * Save data to a file. Creates a new file or overwrites an existing file.
 */

function saveDataToFile(aFilePath, aDataStream, charset)
{
   const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
   const FILEOUT_CTRID = "@mozilla.org/network/file-output-stream;1";
   const nsILocalFile = Components.interfaces.nsILocalFile;
   const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;

   var localFileInstance;
   var outputStream;
   
   var LocalFileInstance = Components.classes[LOCALFILE_CTRID].createInstance(nsILocalFile);
   LocalFileInstance.initWithPath(aFilePath);

   outputStream = Components.classes[FILEOUT_CTRID].createInstance(nsIFileOutputStream);
   try
   {
      if(charset)
         aDataStream = convertFromUnicode( charset, aDataStream );

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


////////////////////////////////////
// XML/XSL functions              //
////////////////////////////////////

/**
*   Get the local path to the chrome directory
*/
/*
function getChromeDir()
{
    const JS_DIR_UTILS_FILE_DIR_CID = "@mozilla.org/file/directory_service;1";
    const JS_DIR_UTILS_I_PROPS      = "nsIProperties";
    const JS_DIR_UTILS_DIR          = new Components.Constructor(JS_DIR_UTILS_FILE_DIR_CID, JS_DIR_UTILS_I_PROPS);
    const JS_DIR_UTILS_CHROME_DIR                          = "AChrom";

    var rv;

    try
    {
      rv=(new JS_DIR_UTILS_DIR()).get(JS_DIR_UTILS_CHROME_DIR, Components.interfaces.nsIFile);
    }

    catch (e)
    {
       //jslibError(e, "(unexpected error)", "NS_ERROR_FAILURE", JS_DIR_UTILS_FILE+":getChromeDir");
       rv=null;
    }

    return rv;
}

function getTemplatesPath( templateName )
{
   var templatesDir = getChromeDir();
   templatesDir.append( "calendar" );
   templatesDir.append( "content" );
   templatesDir.append( "templates" );
   templatesDir.append( templateName );
   return templatesDir.path;
}

function xslt(xmlUri, xslUri)
{
   // TODO: Check uri's for CHROME:// and convert path to local path
   var xslProc = new XSLTProcessor();

   var result = document.implementation.createDocument("", "", null);
   var xmlDoc = document.implementation.createDocument("", "", null);
   var xslDoc = document.implementation.createDocument("", "", null);

   xmlDoc.load(xmlUri, "text/xml");
   xslDoc.load(xslUri, "text/xml");
   xslProc.transformDocument(xmlDoc, xslDoc, result, null);

   return result;
}

*/

/** PRIVATE
*
*   Opens a file and returns the content
*   It supports Uri's like chrome://
*/

function loadFile(aUriSpec)
{
    netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');
    var serv = Components.classes["@mozilla.org/network/io-service;1"].
        getService(Components.interfaces.nsIIOService);
    if (!serv) {
        throw Components.results.ERR_FAILURE;
    }
    var chan = serv.newChannel(aUriSpec, null, null);
    var instream = 
        Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance(Components.interfaces.nsIScriptableInputStream);
    instream.init(chan.open());

    return instream.read(instream.available());
}

/** PUBLIC GetXcsDocument
*
*/

function getXcsDocument( calendarEventArray )
{
   var xmlDocument = getXmlDocument( calendarEventArray );

   var xcsDocument = transformXML( xmlDocument, "xml2xcs.xsl" );

   var processingInstruction = xcsDocument.createProcessingInstruction(
      "xml", "version=\"1.0\" encoding=\"UTF-8\"");
   // xcsDocument.insertBefore(processingInstruction, xcsDocument.firstChild);
   
   return xcsDocument;
}


/** PUBLIC
*
*   Opens a xsl transformation file and applies it to xmlDocuments.
*   xslFile can be in the convertersDirectory, or a full Uri
*   Returns the resulting document
*/

function transformXML( xmlDocument, xslFilename )
{
   var xslProc = new XSLTProcessor();
   var gParser = new DOMParser;
   // .load isn't synchrone
   // var xslDoc = document.implementation.createDocument("", "", null);
   // xslDoc.load(path, "text/xml");

   // if only passsed a filename, assume it is a file in the default directory
   if( xslFilename.indexOf( ":" ) == -1 )
     xslFilename = convertersDirectory + xslFilename;

   var xslContent = loadFile( xslFilename );
   var xslDocument = gParser.parseFromString(xslContent, 'text/xml');
   var result = document.implementation.createDocument("", "", null);

   xslProc.transformDocument(xmlDocument, xslDocument, result, null);

   return result;
}

/** PUBLIC
*
*   Serializes a DOM document.if stylesheet is null, returns the document serialized
*   Applies the stylesheet when available, and return the serialized transformed document,
*   or the transformiix data
*/

function serializeDocument( xmlDocument, stylesheet )
{
   var serializer = new XMLSerializer;
   if( stylesheet )
   {
      var resultDocument = transformXML( xmlDocument, stylesheet );
      var transformiixResult = resultDocument.getElementById( "transformiixResult" );
      if( transformiixResult && transformiixResult.hasChildNodes() )
      {  // It's a document starting with:
         // <a0:html xmlns:a0="http://www.w3.org/1999/xhtml">
         // <a0:head/><a0:body><a0:pre id="transformiixResult">
         var textNode = transformiixResult.firstChild;
         if( textNode.nodeType == textNode.TEXT_NODE )
            return textNode.nodeValue;
         else
            return serializer.serializeString( transformiixResult );
      }
      else
         // No transformiixResult, return the serialized transformed document
         return serializer.serializeToString ( resultDocument );
   }
   else
      // No transformation, return the serialized xmlDocument
      return serializer.serializeToString ( xmlDocument );
}

/** PUBLIC
*
*   not used?
*/

function deserializeDocument(text, stylesheet )
{
   var gParser = new DOMParser;
   var xmlDocument = gParser.parseFromString(text, 'text/xml');
}

/** PRIVATE
*
*   transform a calendar event into a dom node.
*   hack, this is going to be part of libical
*/

function makeXmlNode( xmlDocument, calendarEvent )
{

   // Adds a property node to a event node
   //  do not add empty properties, valueType is optional
   var addPropertyNode = function( xmlDocument, eventNode, name, value, valueType )
   {
      if(!value)
         return;

      var propertyNode = xmlDocument.createElement( "property" );
      propertyNode.setAttribute( "name", name );

      var valueNode = xmlDocument.createElement( "value" );
      var textNode  = xmlDocument.createTextNode( value );

      if( valueType )
         valueNode.setAttribute( "value", valueType );

      valueNode.appendChild( textNode );
      propertyNode.appendChild( valueNode );
      eventNode.appendChild( propertyNode );
   }

   var addAlarmNode = function( xmlDocument, eventNode, triggerTime, triggerUnits )
   {
      if(triggerUnits == "minutes")
        triggerIcalUnits = "M";
      else  if(triggerUnits == "hours")
        triggerIcalUnits = "H";
      else  if(triggerUnits == "days")
        triggerIcalUnits = "D";

      var valarmNode = xmlDocument.createElement( "component" );
      valarmNode.setAttribute( "name", "VALARM" );

      var propertyNode = xmlDocument.createElement( "property" );
      propertyNode.setAttribute( "name", "TRIGGER" );

      var valueNode = xmlDocument.createElement( "value" );
      //valueNode.setAttribute( "value", "DURATION" );

      var textNode  = xmlDocument.createTextNode( "-PT" + triggerTime + triggerIcalUnits );

      valueNode.appendChild( textNode );
      propertyNode.appendChild( valueNode );
      valarmNode.appendChild( propertyNode )
      eventNode.appendChild( valarmNode );
   }
   
   var addRRuleNode = function( xmlDocument, eventNode )
   {
      // extremly ugly hack, but this will be done in libical soon
      var ruleText = "";
      var eventText = calendarEvent.getIcalString();
      var i = eventText.indexOf("RRULE");
      if( i > -1)
      {
         ruleText = eventText.substring(i+8);      
         ruleText = ruleText.substring(0, ruleText.indexOf("\n"));
      }
      if( ruleText && ruleText.length > 0)
      {
         var propertyNode = xmlDocument.createElement( "property" );
         propertyNode.setAttribute( "name", "RRULE" );

         var valueNode = xmlDocument.createElement( "value" );
         var textNode  = xmlDocument.createTextNode( ruleText );

         valueNode.appendChild( textNode );
         propertyNode.appendChild( valueNode );
         eventNode.appendChild( propertyNode );
      }
   }

     var checkString = function( str )
    {
        if( typeof( str ) == "string" )
            return str;
        else
            return ""
    }

    var checkNumber = function( num )
    {
        if( typeof( num ) == "undefined" || num == null )
            return "";
        else
            return num
    }

    var checkBoolean = function( bool )
    {
        if( bool == "false")
            return "false"
        else if( bool )      // this is false for: false, 0, undefined, null, ""
            return "true";
        else
            return "false"
    }

    // create a string in the iCalendar format, UTC time '20020412T121314Z'
    var checkDate = function ( dt, isDate )
    {   
        var dateObj = new Date( dt.getTime() );
        var result = "";

        if( isDate )
        {
            result += dateObj.getFullYear();
            if( dateObj.getMonth() + 1 < 10 )
               result += "0";
            result += dateObj.getMonth() + 1;

            if( dateObj.getDate() < 10 )
               result += "0";
            result += dateObj.getDate();
        }
        else
        {
           result += dateObj.getUTCFullYear();

           if( dateObj.getUTCMonth() + 1 < 10 )
              result += "0";
           result += dateObj.getUTCMonth() + 1;

           if( dateObj.getUTCDate() < 10 )
              result += "0";
           result += dateObj.getUTCDate();

           result += "T"

           if( dateObj.getUTCHours() < 10 )
              result += "0";
           result += dateObj.getUTCHours();

           if( dateObj.getUTCMinutes() < 10 )
              result += "0";
           result += dateObj.getUTCMinutes();

           if( dateObj.getUTCSeconds() < 10 )
              result += "0";
           result += dateObj.getUTCSeconds();
           result += "Z";
        }

        return result;
    }

    // make the event tag
    var calendarNode = xmlDocument.createElement( "component" );
    calendarNode.setAttribute( "name", "VCALENDAR" );

    addPropertyNode( xmlDocument, calendarNode, "PRODID", "-//Mozilla.org/NONSGML Mozilla Calendar V 1.0 //EN" );
    addPropertyNode( xmlDocument, calendarNode, "VERSION", "2.0" );
    if(calendarEvent.method == calendarEvent.ICAL_METHOD_PUBLISH)
       addPropertyNode( xmlDocument, calendarNode, "METHOD", "PUBLISH" );
    else if(calendarEvent.method == calendarEvent.ICAL_METHOD_REQUEST )
       addPropertyNode( xmlDocument, calendarNode, "METHOD", "REQUEST" );

    var eventNode = xmlDocument.createElement( "component" );
    eventNode.setAttribute( "name", "VEVENT" );

    addPropertyNode( xmlDocument, eventNode, "UID", calendarEvent.id );
    addPropertyNode( xmlDocument, eventNode, "SUMMARY", checkString( calendarEvent.title ) );
    addPropertyNode( xmlDocument, eventNode, "DTSTAMP", checkDate( calendarEvent.stamp ) );
    if( calendarEvent.allDay )
       addPropertyNode( xmlDocument, eventNode, "DTSTART", checkDate( calendarEvent.start, true ), "DATE" );
    else
    {
       addPropertyNode( xmlDocument, eventNode, "DTSTART", checkDate( calendarEvent.start ) );
       addPropertyNode( xmlDocument, eventNode, "DTEND", checkDate( calendarEvent.end ) );
    }

    addPropertyNode( xmlDocument, eventNode, "DESCRIPTION", checkString( calendarEvent.description ) );
    addPropertyNode( xmlDocument, eventNode, "CATEGORIES", checkString( calendarEvent.categories ) );
    addPropertyNode( xmlDocument, eventNode, "LOCATION", checkString( calendarEvent.location ) );
    addPropertyNode( xmlDocument, eventNode, "PRIVATEEVENT", checkString( calendarEvent.privateEvent ) );
    addPropertyNode( xmlDocument, eventNode, "URL", checkString( calendarEvent.url ) );
    addPropertyNode( xmlDocument, eventNode, "PRIORITY", checkNumber( calendarEvent.priority ) );

    addAlarmNode( xmlDocument, eventNode, calendarEvent.alarmLength, calendarEvent.alarmUnits );
    addRRuleNode( xmlDocument, eventNode );

    calendarNode.appendChild( eventNode );
    return calendarNode;
}

/** PUBLIC
*
*   Transforms an array of calendar events into a dom-document
*   hack, this is going to be part of libical
*/

function getXmlDocument ( eventList )
{
    // use the domparser to create the XML 
    var domParser = new DOMParser;
    // start with one tag
    var xmlDocument = domParser.parseFromString( "<libical/>", "text/xml" );
    
    // get the top tag, there will only be one.
    var topNodeList = xmlDocument.getElementsByTagName( "libical" );
    var topNode = topNodeList[0];


    // add each event as an element
 
    for( var index = 0; index < eventList.length; ++index )
    {
        var calendarEvent = eventList[ index ];
        
        var eventNode = this.makeXmlNode( xmlDocument, calendarEvent );
        
        topNode.appendChild( eventNode );
    }
    return xmlDocument;
}
