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

var gWizardType;

function checkInitialPage()
{
   gWizardType = document.getElementById( "initial-radiogroup" ).selectedItem.value;
   
   document.getElementsByAttribute( "pageid", "initialPage" )[0].setAttribute( "next", gWizardType );
}


function onPageShow( PageId )
{
   switch( PageId )
   {
      case "import":
      if( document.getElementById( "import-path-textbox" ).value.length == 0 )
      {
         document.getElementById( "calendar-wizard" ).canAdvance = false;
      }
      else
      {
         document.getElementById( "calendar-wizard" ).canAdvance = true;
      }
      break;

   case "import-2":
      buildCalendarsListbox( 'import-calendar-radiogroup' );
      
      if( document.getElementById( "import-calendar-radiogroup" ).selectedItem == null )
      {
         document.getElementById( "calendar-wizard" ).canAdvance = false;
      }
      else
      {
         document.getElementById( "calendar-wizard" ).canAdvance = true;
      }
      document.getElementById( "import-calendar-radiogroup" ).childNodes[0].setAttribute( "selected", "true" );
      break;
   }
}

function buildCalendarsListbox( ListBoxId )
{
   document.getElementById( ListBoxId ).database.AddDataSource( opener.gCalendarWindow.calendarManager.rdf.getDatasource() );

   document.getElementById( ListBoxId ).builder.rebuild();
}

function doWizardFinish( )
{
   window.setCursor( "wait" );

   switch( gWizardType )
   {
      case "import":
         return true;
         break;
   
      case "export":
         return doWizardExport();
         break;
   
      case "subscribe":
         return doWizardSubscribe();
         break;
   
      case "publish":
         return doWizardPublish();
         break;
   }
}


function doWizardImport()
{
   window.setCursor( "wait" );
   
   var calendarEventArray;

   var fileName = document.getElementById( "import-path-textbox" ).value;

   var aDataStream = readDataFromFile( fileName, "UTF-8" );

   if( fileName.indexOf( ".ics" ) != -1 )
      calendarEventArray = parseIcalData( aDataStream );
   else if( fileName.indexOf( ".xcs" ) != -1 )
      calendarEventArray = parseXCSData( aDataStream );
   
   if( document.getElementById( "import-2-radiogroup" ).selectedItem.value == "silent" )
   {
      var ServerName = document.getElementById( "import-calendar-radiogroup" ).selectedItem.value;

      doAddEventsToCalendar( calendarEventArray, true, ServerName );
   }
   else
   {
      doAddEventsToCalendar( calendarEventArray, true, null );
   }
}

function doAddEventsToCalendar( calendarEventArray, silent, ServerName )
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
         
         document.getElementById( "import-progress-meter" ).setAttribute( "value", ( (i/calendarEventArray.length)*100 )+"%" );
      }
      else
         editNewEvent( calendarEvent );
   }

   gICalLib.batchMode = false;
   window.setCursor( "auto" );

   document.getElementById( "importing-box" ).setAttribute( "collapsed", "true" );
   document.getElementById( "done-importing-box" ).removeAttribute( "collapsed" );
}

function doWizardExport()
{
   const UTF8 = "UTF-8";
   var aDataStream;
   var extension;
   var charset;
   
   var fileName = document.getElementById( "export-path-textbox" ).value;

   if( fileName.indexOf( ".ics" ) == -1 )
   {
      aDataStream = eventArrayToICalString( calendarEventArray, true );
      extension   = extensionCalendar;
      charset = "UTF-8";
   }
   else if( fileName.indexOf( ".rtf" ) == -1 )
   {
      aDataStream = eventArrayToRTF( calendarEventArray );
      extension   = extensionRtf;
   }
   else if( fileName.indexOf( ".htm" ) == -1 )
   {
      aDataStream = eventArrayToHTML( calendarEventArray );
      extension   = ".htm";
      charset = "UTF-8";
   }
   else if( fileName.indexOf( ".csv" ) == -1 )
   {
      aDataStream = eventArrayToCsv( calendarEventArray );
      extension   = extensionCsv;
      charset = "UTF-8";
   }
   else if( fileName.indexOf( ".xml" ) == -1 )
   {
      aDataStream = eventArrayToXCS( calendarEventArray );
      extension   = extensionXcs;
      charset = "UTF-8";
   }
   else if( fileName.indexOf( ".rdf" ) == -1 )
   {
      aDataStream = eventArrayToRdf( calendarEventArray );
      extension   = extensionRdf;
      charset = "UTF-8";
   }

   saveDataToFile( filePath, aDataStream, charset );
}

function launchFilePicker( Mode, ElementToGiveValueTo )
{
   // No show the 'Save As' dialog and ask for a filename to save to
   const nsIFilePicker = Components.interfaces.nsIFilePicker;

   var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);

   // caller can force disable of sand box, even if ON globally
   switch( Mode )
   {
   case "open":
      fp.defaultExtension = "ics";

      fp.appendFilter( filterCalendar, "*" + extensionCalendar );
      fp.appendFilter( filterXcs, "*" + extensionXcs );
   
      fp.init(window, "Open", nsIFilePicker.modeOpen);
      break;

   case "save":
      fp.init(window, "Save", nsIFilePicker.modeSave);
      //if(calendarEventArray.length == 1 && calendarEventArray[0].title)
      //   fp.defaultString = calendarEventArray[0].title;
      //else
         fp.defaultString = "Mozilla Calendar events";
   
      fp.defaultExtension = "ics";
   
      fp.appendFilter( filterCalendar, "*" + extensionCalendar );
      fp.appendFilter( filterRtf, "*" + extensionRtf );
      fp.appendFilters(nsIFilePicker.filterHTML);
      fp.appendFilter( filterCsv, "*" + extensionCsv );
      fp.appendFilter( filterXcs, "*" + extensionXcs );
      fp.appendFilter( filterRdf, "*" + extensionRdf );

      break;
   }
   
   fp.show();

   if (fp.file && fp.file.path.length > 0)
   {
      /*if(filePath.indexOf(".") == -1 )
          filePath += extension;
      */
      document.getElementById( ElementToGiveValueTo ).value = fp.file.path;

      document.getElementById( "calendar-wizard" ).canAdvance = true;
   }
}
