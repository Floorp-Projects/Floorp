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
 * The Original Code is Mozilla Calendar Code.
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
include('chrome://calendar/content/jslib/io/io.js');
include('chrome://calendar/content/jslib/rdf/rdf.js');
include('chrome://calendar/content/jslib/rdf/rdfFile.js');

function CalendarObject()
{
   this.path = "";
   this.serverNumber = 0;
   this.name = "";
   this.remote = false;
   this.remotePath = "";
   this.active = false;
}

function calendarManager( CalendarWindow )
{
   this.CalendarWindow = CalendarWindow;

   /* We need a Calendar directory in our profile directory */
   var CalendarDirectory = new File( this.getProfileDirectory().path );

   CalendarDirectory.append( "Calendar" );
   
   /* make sure the calendar directory exists, create it if it doesn't */
   if( !CalendarDirectory.exists() )
   {
      var thisDir = new Dir( CalendarDirectory.path );
      thisDir.create( 0755 );
   }

   var profileFile = this.getProfileDirectory();

   profileFile.append("Calendar");
   profileFile.append("CalendarManager.rdf");
                      
   this.rdf = new RDFFile( profileFile.path, null);
   
   /* make sure we have a root node, if not we probably have an empty file */
   if( this.rdf.getRootContainers().length == 0 )
   {
      this.rootContainer = this.rdf.addRootSeq( "urn:calendarcontainer" );

      //add the default calendar
      var profileFile = this.getProfileDirectory();
      profileFile.append("Calendar");
      profileFile.append("CalendarDataFile.ics");
                         
      var node = this.rootContainer.addNode( "calendar0" );
   
      node.setAttribute( "http://home.netscape.com/NC-rdf#name", "My Calendar" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#path", profileFile.path );
      node.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#remote", "false" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#remotePath", "" );
   }
   else
   {
      this.rootContainer = this.rdf.getRootContainers("seq")[0];
   }
   
   this.rdf.flush();

   this.getAndConvertAllOldCalendars();

   document.getElementById( "list-calendars-listbox" ).database.AddDataSource( this.rdf.getDatasource() );

   document.getElementById( "list-calendars-listbox" ).builder.rebuild();

   /* add active calendars */
   for( var i = 0; i < this.rootContainer.getSubNodes().length; i++ )
   {
      if( this.rootContainer.getSubNodes()[i].getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" )
      {
         this.addCalendar( this.rootContainer.getSubNodes()[i] );
      }                
   }
   
   /* Refresh remote calendars */
   /*
   var RefreshServers = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.reloadonlaunch", false );
   
   if( RefreshServers == true )
      this.refreshAllRemoteCalendars( );
   */
}

/*
** Launch the new calendar file dialog
*/
calendarManager.prototype.launchAddCalendarDialog = function calMan_launchAddCalendarDialog( aName, aUrl )
{
   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();
   
   if( aName )
      ThisCalendarObject.name = aName;

   if( aUrl )
   {
      ThisCalendarObject.remote = true;
      ThisCalendarObject.remotePath = aUrl;
   }

   var args = new Object();
   args.mode = "new";

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.addServerDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarServerDialog.xul", "caAddServer", "chrome,modal", args );
}

/*
** Launch the edit calendar file dialog
*/
calendarManager.prototype.launchEditCalendarDialog = function calMan_launchEditCalendarDialog( )
{
   //get the currently selected calendar

   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();

   var TempCalendar = this.getSelectedCalendar();
   ThisCalendarObject.name = TempCalendar.getAttribute( "http://home.netscape.com/NC-rdf#name" );
   ThisCalendarObject.path = TempCalendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   ThisCalendarObject.active = TempCalendar.getAttribute( "http://home.netscape.com/NC-rdf#active" );
   ThisCalendarObject.remote = TempCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remote" );
   ThisCalendarObject.remotePath = TempCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" );
   
   var args = new Object();
   args.mode = "edit";

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.editServerDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/calendarServerDialog.xul", "caEditServer", "chrome,modal", args );
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.addServerDialogResponse = function calMan_addServerDialogResponse( CalendarObject )
{
   var name = "calendar"+this.rootContainer.getSubNodes().length;
        
   CalendarObject.active = true;
   CalendarObject.path = CalendarObject.path.replace( "webcal:", "http:" );

   var node = this.rootContainer.addNode(name);
   node.setAttribute("http://home.netscape.com/NC-rdf#active", "true");
   node.setAttribute("http://home.netscape.com/NC-rdf#serverNumber", this.rootContainer.getSubNodes().length);
   node.setAttribute("http://home.netscape.com/NC-rdf#name", CalendarObject.name);
   
   if( CalendarObject.path == "" )
   {
      //they didn't set a path in the box, that's OK, its not required.
      var profileFile = this.getProfileDirectory();
      profileFile.append("Calendar");
      profileFile.append("CalendarDataFile"+this.rootContainer.getSubNodes().length+".ics");
      CalendarObject.path = profileFile.path;
   }
   
   node.setAttribute("http://home.netscape.com/NC-rdf#path", CalendarObject.path)

   if( CalendarObject.path.indexOf( "http://" ) != -1 )
   {
      var profileFile = this.getProfileDirectory();
      profileFile.append( "Calendar" );
      profileFile.append("RemoteCalendar"+this.rootContainer.getSubNodes().length+".ics");

      node.setAttribute("http://home.netscape.com/NC-rdf#remote", "true");
      node.setAttribute("http://home.netscape.com/NC-rdf#remotePath", CalendarObject.path);
      node.setAttribute("http://home.netscape.com/NC-rdf#path", profileFile.path);
      
      this.retrieveAndSaveRemoteCalendar( node, onResponseAndRefresh );
      
      dump( "Remote Calendar Number "+this.rootContainer.getSubNodes().length+" Added" );
   }
   else
   {
      node.setAttribute("http://home.netscape.com/NC-rdf#remote", "false");
      
      dump( "Calendar Number "+CalendarObject.serverNumber+" Added" );
   }
   
   this.rdf.flush();
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.editServerDialogResponse = function calMan_editServerDialogResponse( CalendarObject )
{
   var name = "calendar"+CalendarObject.serverNumber;

   //get the node
   var node = this.rootContainer.getNode( name );
   
   node.setAttribute( "http://home.netscape.com/NC-rdf#name", CalendarObject.name );

   this.rdf.flush();
}


/*
** Add the calendar so it is included in searches
*/
calendarManager.prototype.addCalendar = function calMan_addCalendar( ThisCalendarObject )
{
   dump( "\n CALENDAR MANANGER-> add calendar with path "+ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path"+"\n\n" ) );

   this.CalendarWindow.eventSource.gICalLib.addCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   ThisCalendarObject.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );

   this.rdf.flush();
}


/* 
** Remove the calendar, so it doesn't get included in searches 
*/
calendarManager.prototype.removeCalendar = function calMan_removeCalendar( ThisCalendarObject )
{
   dump( "\n calendarManager-> remove calendar "+ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   this.CalendarWindow.eventSource.gICalLib.removeCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   ThisCalendarObject.setAttribute( "http://home.netscape.com/NC-rdf#active", "false" );

   this.rdf.flush();
}


/*
** Delete the calendar. Remove the file, it won't be used any longer.
*/
calendarManager.prototype.deleteCalendar = function calMan_deleteCalendar( ThisCalendarObject )
{
   //DISABLE DELETE FOR NOW
   return;

   if( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" ) == 0 )
      return;

   this.CalendarWindow.eventSource.gICalLib.removeCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   var FileToRemove = new File( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
   FileToRemove.remove();

   ThisCalendarObject.remove();

   this.rdf.flush();
}


calendarManager.prototype.getSelectedCalendar = function calMan_getSelectedCalendar( )
{
   var calendarListBox = document.getElementById( "list-calendars-listbox" );
   
   return( this.rdf.getNode( calendarListBox.selectedItem.getAttribute( "id" ) ) );
}


var xmlhttprequest;
var request;
var calendarToGet = null;

calendarManager.prototype.retrieveAndSaveRemoteCalendar = function calMan_retrieveAndSaveRemoteCalendar( ThisCalendarObject, onResponse )
{
   //the image doesn't always exist. If it doesn't exist, it causes problems, so check for it here
   if( document.getElementById( "calendar-list-image-"+ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" ) ) )
      document.getElementById( "calendar-list-image-"+ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" ) ).setAttribute( "synching", "true" );

   calendarToGet = ThisCalendarObject;
   // make a request
   xmlhttprequest = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
   request =  xmlhttprequest.createInstance( Components.interfaces.nsIXMLHttpRequest );
    
   request.addEventListener( "load", onResponse, false );
   request.addEventListener( "error", onError, false );
   
   request.open( "GET", ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" ), false );    // true means async
   
   request.send( null );
}


calendarManager.prototype.refreshAllRemoteCalendars = function calMan_refreshAllRemoteCalendars()
{
   //get all the calendars.
   //get all the other calendars
   var SubNodes = this.rootContainer.getSubNodes();

   for( var i = 0; i < SubNodes.length; i++ )
   {
      //check their remote attribute, if its true, call retrieveAndSaveRemoteCalendar()
      if( SubNodes[i].getAttribute( "http://home.netscape.com/NC-rdf#remote" ) == "true" )
      {
         this.retrieveAndSaveRemoteCalendar( SubNodes[i], onResponseAndRefresh );
      }
   }
}

/*
** Checks if the URL is already in the list.
** If so, it returns the calendar object.
** Otherwise, returns false.
*/

calendarManager.prototype.isURLAlreadySubscribed = function calMan_isCalendarSubscribed( CalendarURL )
{
   CalendarURL = CalendarURL.replace( "webcal:", "http:" );

   this.rdf.getRootContainers().length == 0
      
   for( var i = 0; i < this.rootContainer.getSubNodes().length; i++ )
   {
      if( this.rootContainer.getSubNodes()[i].getAttribute( "http://home.netscape.com/NC-rdf#remotePath" ) == CalendarURL )
      {
         return( this.rootContainer.getSubNodes()[i] );
      }
   }
   return( false );
}

/*
** This function is called when clicking on a file with mime type "text/calendar"
** It first checks to see if the calendar is already subscribed. If so, it disables all other calendars
** and then adds that calendar.
** If not, then it opens up the dialog for users to add the calendar to their subscribed list.
*/

calendarManager.prototype.checkCalendarURL = function calMan_checkCalendarURL( Channel )
{
   var calendarSubscribed = this.isURLAlreadySubscribed( Channel.URI.spec );

   if( calendarSubscribed === false )
   {
      if( Channel.URI.spec.indexOf( "imap://" ) != -1 )
      {
         var Listener =
         {
            onStreamComplete: function(loader, ctxt, status, resultLength, result)
            {
               //check to make sure its actually a calendar file, if not return.
               if( result.indexOf( "BEGIN:VCALENDAR" ) == -1 )
                  return false;

               //if we have only one event, open the event dialog.
               var BeginEventText = "BEGIN:VEVENT";
               var firstMatchLocation = result.indexOf( BeginEventText );
               if( firstMatchLocation == -1 )
                  return false;
               else
               {
                  if( result.indexOf( BeginEventText, firstMatchLocation + BeginEventText.length + 1 ) == -1 )
                  {
                     var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
                     var calendarEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
                     calendarEvent.parseIcalString( result );
                     
                     /* EDITING AN EXISTING EVENT IS NOT WORKING
                     
                     //only 1 event
                     var Result = gCalendarWindow.eventSource.gICalLib.fetchEvent( calendarEvent.id );
                                          
                     if( Result )
                     {
                        editEvent( calendarEvent );
                     }
                     else
                     {
                        //if we have the event already, edit the event
                        //otherwise, open up a new event dialog
                        */
                     editNewEvent( calendarEvent, null );
                     //}
                  }
                  else
                  {
                     //if we have > 1 event, prompt the user to subscribe to the event.
                     var profileFile = gCalendarWindow.calendarManager.getProfileDirectory();

                     profileFile.append("Calendar");
                     profileFile.append("Email"+gCalendarWindow.calendarManager.rootContainer.getSubNodes().length+".ics");
                      
                     FilePath = profileFile.path;
                     saveDataToFile(FilePath, result, null);

                     gCalendarWindow.calendarManager.launchAddCalendarDialog( CalendarName, FilePath );
                  }
               }
            }
         }
          
         var myInstance = Components.classes["@mozilla.org/network/stream-loader;1"].createInstance(Components.interfaces.nsIStreamLoader);
        
         myInstance.init( Channel, Listener, null );
      }
      else
      {
         //not subscribed, prompt the user to do so.
         var arrayForNames = Channel.URI.spec.split( "/" );
         var CalendarNameWithExtension = arrayForNames[ arrayForNames.length - 1 ];
         var CalendarName = CalendarNameWithExtension.replace( ".ics", "" );

         this.launchAddCalendarDialog( CalendarName, Channel.URI.spec );
      }
   }
   else
   {
      //calendarSubscribed is the subscribed calendar object.
      if( calendarSubscribed.active == false )
      {
         calendarSubscribed.active = true;

         gCalendarWindow.calendarManager.addCalendar( calendarSubscribed );
      
         document.getElementById( "calendar-list-item-"+calendarSubscribed.serverNumber ).setAttribute( "checked", "true" );

         refreshEventTree( false );

         refreshToDoTree( false );
   
         gCalendarWindow.currentView.refreshEvents();
      }
   }
}


calendarManager.prototype.getProfileDirectory = function calMan_getProfileDirectory()
{
   var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
   var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
   var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
   return( profileFile );
}


calendarManager.prototype.getDefaultServer = function calMan_getDefaultServer()
{
   return( this.rootContainer.getSubNodes()[0].getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
}


calendarManager.prototype.getAndConvertAllOldCalendars = function calMan_getAllCalendars()
{
   //if the file CalendarDataFile.ics exists in the users profile directory, move it to Calendar/CalendarDataFile.ics
   var oldCalendarFile = this.getProfileDirectory();
   oldCalendarFile.append( "CalendarDataFile.ics" );

   var newCalendarFile = this.getProfileDirectory();
   newCalendarFile.append( "Calendar" );
   newCalendarFile.append( "CalendarDataFile.ics" );

   var oldCalendarDataFile = new File( oldCalendarFile.path );
   var newCalendarDataFile = new File( newCalendarFile.path );
   
   if( oldCalendarDataFile.exists() )
   {
      alert( "moving "+oldCalendarDataFile.path+" to "+newCalendarDataFile.path );
      oldCalendarDataFile.copy( newCalendarDataFile.path );
      oldCalendarDataFile.remove( );
   }

   //go through the prefs file, calendars are stored in there.
   
   var ServerArray = getCharPref(prefService.getBranch( "calendar." ), "servers.array", "" );
   
   var ArrayOfCalendars = ServerArray.split( "," );
   
   //don't count the default server, so this starts at 1
   for( var i = 1; i < ArrayOfCalendars.length; i++ )
   {
      if( ArrayOfCalendars[i] >= this.nextCalendarNumber )
         this.nextCalendarNumber = parseInt( ArrayOfCalendars[i] )+1;

      thisCalendar = new CalendarObject();
      
      try { 
         thisCalendar.serverNumber = ArrayOfCalendars[i];
         thisCalendar.name = getCharPref(prefService.getBranch( "calendar." ), "server"+ArrayOfCalendars[i]+".name", "" );
         thisCalendar.path = getCharPref(prefService.getBranch( "calendar." ), "server"+ArrayOfCalendars[i]+".path", "" );
         thisCalendar.active = getBoolPref(prefService.getBranch( "calendar." ), "server"+ArrayOfCalendars[i]+".active", false );
         thisCalendar.remote = getBoolPref(prefService.getBranch( "calendar." ), "server"+ArrayOfCalendars[i]+".remote", false );
         thisCalendar.remotePath = getCharPref(prefService.getBranch( "calendar." ), "server"+ArrayOfCalendars[i]+".remotePath", "" );
         
         this.calendars[ this.calendars.length ] = thisCalendar;
      }
      catch ( e )
      {
         dump( "error: could not get calendar information from preferences\n"+e );
      }

      //now convert it, and put it in the RDF file.
      var node = this.rootContainer.addNode(name);
      node.setAttribute("http://home.netscape.com/NC-rdf#active", thisCalendar.name);
      node.setAttribute("http://home.netscape.com/NC-rdf#serverNumber", this.rootContainer.getSubNodes().length);
      node.setAttribute("http://home.netscape.com/NC-rdf#name", thisCalendar.name);
      node.setAttribute("http://home.netscape.com/NC-rdf#path", "RemoteCalendar"+this.rootContainer.getSubNodes().length+".ics");
      node.setAttribute("http://home.netscape.com/NC-rdf#remote", thisCalendar.remote);
      node.setAttribute("http://home.netscape.com/NC-rdf#remotePath", thisCalendar.remotePath);

      this.rdf.flush();
      //if the file CalendarDataFile.ics exists in the users profile directory, move it to Calendar/CalendarDataFile.ics
      var newCalendarFile = this.getProfileDirectory();
      newCalendarFile.append( "Calendar" );
      newCalendarFile.append( "RemoteCalendar"+this.rootContainer.getSubNodes().length+".ics" );
   
      var oldCalendarDataFile = new File( thisCalendar.path );
      var newCalendarDataFile = new File( newCalendarFile.path );
   
      if( oldCalendarDataFile.exists() )
      {
         alert( "moving "+oldCalendarDataFile.path+" to "+newCalendarDataFile.path );
         oldCalendarDataFile.copy( newCalendarDataFile.path );
         oldCalendarDataFile.remove( );
      }

      prefService.getBranch( "calendar." ).clearUserPref( "server"+thisCalendar.serverNumber+".name" );
      prefService.getBranch( "calendar." ).clearUserPref( "server"+thisCalendar.serverNumber+".path" );
      prefService.getBranch( "calendar." ).clearUserPref( "server"+thisCalendar.serverNumber+".active" );
      prefService.getBranch( "calendar." ).clearUserPref( "server"+thisCalendar.serverNumber+".remote" );
      prefService.getBranch( "calendar." ).clearUserPref( "server"+thisCalendar.serverNumber+".remotePath" );
   }
   try
   {
      prefService.getBranch( "calendar." ).clearUserPref( "server0.active" );
   } catch( e ) {
   }
   
   try
   {
      prefService.getBranch( "calendar." ).clearUserPref( "servers.array" );
   } catch( e ) {
   }

   try
   {
      prefService.getBranch( "calendar." ).clearUserPref( "servers.count" );
   } catch( e ) {
   }
}

function onResponseAndRefresh( )
{
   //check to make sure this is a real calendar file first.
   //if its not, it causes Mozilla to crash.
   if( request.responseText.indexOf( "BEGIN:VCALENDAR" ) == -1 )
   {
      return;
   }

   //save the stream to a file.
   saveDataToFile( calendarToGet.getAttribute( "http://home.netscape.com/NC-rdf#path" ), request.responseText, "UTF-8" );
   
   gCalendarWindow.calendarManager.removeCalendar( calendarToGet );

   if( calendarToGet.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" )
   {
      gCalendarWindow.calendarManager.addCalendar( calendarToGet.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
      calendarToGet = null;
   }

   refreshEventTree( false );

   refreshToDoTree( false );

   gCalendarWindow.currentView.refreshEvents();

   //document.getElementById( "calendar-list-image-"+calendarToGet.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" ) ).removeAttribute( "synching" );
}

function onError( )
{
   alert( "error: Could not load remote calendar" );
}

/*
** swithces the calendar from on to off and off to on
*/

function switchCalendar( event )
{
   dump( "\nRemoveCalendar in calendarManager.js: button is "+event.button );
   if (event.button != 0) 
   {
      return;
   }
   
   var calendarNode = gCalendarWindow.calendarManager.rdf.getNode( event.currentTarget.getAttribute( "id" ) );

   if( event.currentTarget.childNodes[0].getAttribute( "checked" ) != "true" )
   {
      gCalendarWindow.eventSource.gICalLib.addCalendar( calendarNode.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
      
      calendarNode.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );
      
      event.currentTarget.childNodes[0].setAttribute( "checked", "true" );
   }
   else
   {
      gCalendarWindow.eventSource.gICalLib.removeCalendar( calendarNode.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
      
      calendarNode.setAttribute( "http://home.netscape.com/NC-rdf#active", "false" );
      
      event.currentTarget.childNodes[0].removeAttribute( "checked" );
   }
      
   gCalendarWindow.calendarManager.rdf.flush();

   refreshEventTree( false );

   refreshToDoTree( false );
   
   gCalendarWindow.currentView.refreshEvents();
}


function deleteCalendar( )
{
   var calendarObjectToDelete = gCalendarWindow.calendarManager.getSelectedCalendar();

   gCalendarWindow.calendarManager.deleteCalendar( calendarObjectToDelete );

   refreshEventTree( false );

   refreshToDoTree( false );

   gCalendarWindow.currentView.refreshEvents();
}


const nsIDragService = Components.interfaces.nsIDragService;

var calendarManagerDNDObserver = {

   // This function should return a list of flavours that the object being dragged over can accept.
   getSupportedFlavours : function ()
   {
      var flavourSet = new FlavourSet();
      // flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      flavourSet.appendFlavour("text/x-moz-url");
      return flavourSet;
   },

   //Define this function to have something happen when a drag starts
   onDragStart: function (aEvent, aXferData, aDragAction)
   {
   },


   // The onDragOver function defines what happens when an object is dragged over.
   onDragOver: function (aEvent, aFlavour, aDragSession)
   {
   },


   onDrop: function (aEvent, aXferData, aDragSession)
   {
      var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
      // trans.addDataFlavor("application/x-moz-file");
      trans.addDataFlavor("text/x-moz-url");

      aDragSession.getData (trans, i);

      var dataObj = new Object();
      var bestFlavor = new Object();
      var len = new Object();
      trans.getAnyTransferData(bestFlavor, dataObj, len);

      switch (bestFlavor.value)
      {

      case "text/x-moz-url":
         var droppedUrl = this.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
         if (!droppedUrl)
             return;

         // url has spec, fileName, fileBaseName, fileExtension and others
         var url = Components.classes["@mozilla.org/network/standard-url;1"].createInstance();
         url = url.QueryInterface(Components.interfaces.nsIURL);
	      url.spec = droppedUrl;
	      gCalendarWindow.calendarManager.launchAddCalendarDialog(url.fileBaseName, url.spec )

         break;

      case "application/x-moz-file": // file from OS, we expect it to be iCalendar data
         try {
            var fileObj = dataObj.value.QueryInterface(Components.interfaces.nsIFile);
            var aDataStream = readDataFromFile( fileObj.path );
         }
         catch(ex) {
            alert(ex.message);
         }
	 break;
      }
      
  },

  onDragExit: function (aEvent, aDragSession)
  {
     // nothing, doesn't fire for cancel? needed for interval-drag cleanup
  },

  retrieveURLFromData: function(aData, flavour)
  {
     switch (flavour) 
     {
         case "text/unicode":
            if (aData.search(client.linkRE) != -1)
                 return aData;
             else
                 return null;
 
         case "text/x-moz-url":
             var data = aData.toString();
             var separator = data.indexOf("\n");
             if (separator != -1)
                 data = data.substr(0, separator);
             return data;
 
         case "application/x-moz-file":
             return aData.URL;
     }
 
    return null;                                                   
  }

};
