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
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Matthew Buckett <buckett@bumph.org>
 *                 Mike Loll <michaelloll@hotmail.com>
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

var gNextSubNodeToRefresh=0;
var gModifiedTime = new Array();
//var gModifiedTimeRemote = new Array(); 
var gAutoreloadTimerId = new Array(); //stores Timer Id for each autoreload

const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
const nsILocalFile = Components.interfaces.nsILocalFile;

function CalendarObject()
{
   this.path = "";
   this.serverNumber = 0;
   this.name = "";
   this.remote = false;
   this.remotePath = "";
   this.active = false;
   this.color = ""; 
   this.publishAutomatically = false;
   this.shared = false;
   this.autoreload = 0;
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
      thisDir.create();
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
      profileFile = this.getProfileDirectory();
      profileFile.append("Calendar");
      profileFile.append("CalendarDataFile.ics");
                         
      var node = this.rootContainer.addNode( "calendar0" );
   
      node.setAttribute( "http://home.netscape.com/NC-rdf#name", defaultCalendarFileName );
      node.setAttribute( "http://home.netscape.com/NC-rdf#path", profileFile.path );
      node.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#remote", "false" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#remotePath", "" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#color", "#F9F4FF"); //default color
      node.setAttribute( "http://home.netscape.com/NC-rdf#shared", "false" );
      node.setAttribute( "http://home.netscape.com/NC-rdf#autoreload", "0" );
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
      var Calendar = this.rootContainer.getSubNodes()[i]; 
      if( Calendar.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" )
      {
         this.addCalendar( Calendar );
      }
      // Add autoreload timers for all calendars
      var autoreload = Calendar.getAttribute( "http://home.netscape.com/NC-rdf#autoreload" );
      if ( autoreload > 0 ) 
      {
         function closure( ThisObject,ThisCalendar ) {
            function autoreloadCmd(){
               if( ThisCalendar.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" )
                  ThisObject.reloadCalendar( ThisCalendar ); 
            }
            return autoreloadCmd;
         }
         var X = closure( this, Calendar );
         var serverNumber = Calendar.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" );
         gAutoreloadTimerId[serverNumber] = setInterval( X, autoreload ); 
      }
   }
   
   /* Refresh remote calendars */

   var RefreshServers = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.reloadonlaunch", gCalendarBundle.getString("reloadServersOnLaunch" ) );
   
   if( RefreshServers == true )
      this.refreshAllRemoteCalendars( );
}

/*
** Launch the new calendar file dialog
*/
calendarManager.prototype.launchNewCalendarFileDialog = function calMan_launchNewCalendarFileDialog( aName, aPath )
{
  this.launchNewOrOpenCalendarFileDialog(aName, aPath, "new");
}

/*
** Launch the open calendar file dialog
*/
calendarManager.prototype.launchOpenCalendarFileDialog = function calMan_launchOpenCalendarFileDialog( aName, aPath )
{
  this.launchNewOrOpenCalendarFileDialog(aName, aPath, "open");
}

/*
** PRIVATE: Launch the new file dialog or open calendar file dialog
*/
calendarManager.prototype.launchNewOrOpenCalendarFileDialog = function calMan_launchNewOrOpenCalendarFileDialog( aName, aPath, aMode )
{
   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();
   
   if( aName )
      ThisCalendarObject.name = aName;

   if( aPath )
      ThisCalendarObject.path = aPath;

   var args = new Object();
   args.mode = aMode;

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.addServerDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/localCalDialog.xul", "caAddServer", "chrome,titlebar,modal", args );
}

/*
     ** Launch the edit calendar file dialog
*/
calendarManager.prototype.launchEditCalendarDialog = function calMan_launchEditCalendarDialog( )
{
   //get the currently selected calendar

   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();

   var SelectedCalendarId = this.getSelectedCalendarId();
   
   var SelectedCalendar = this.rdf.getNode( SelectedCalendarId );
   ThisCalendarObject.Id = SelectedCalendarId;
   
   ThisCalendarObject.name = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#name" );
   ThisCalendarObject.path = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   ThisCalendarObject.active = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#active" );
   ThisCalendarObject.remote = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remote" );
   ThisCalendarObject.color = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#color"); 
   ThisCalendarObject.remotePath = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" );
   ThisCalendarObject.publishAutomatically = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#publishAutomatically" );
   ThisCalendarObject.shared = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#shared" );
   ThisCalendarObject.autoreload = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#autoreload" );
   
   var args = new Object();
   args.mode = "edit";

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.editLocalCalendarDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/localCalDialog.xul", "caEditServer", "chrome,titlebar,modal", args );
}


/*
** Launch the new calendar file dialog
*/
calendarManager.prototype.launchAddRemoteCalendarDialog = function calMan_launchAddRemoteCalendarDialog( aName, aUrl )
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
   openDialog("chrome://calendar/content/serverDialog.xul", "caAddServer", "chrome,titlebar,modal", args );

}

/*
     ** Launch the edit calendar file dialog
*/
calendarManager.prototype.launchEditRemoteCalendarDialog = function calMan_launchEditCalendarDialog( )
{
   //get the currently selected calendar

   // set up a bunch of args to pass to the dialog
   var ThisCalendarObject = new CalendarObject();

   var SelectedCalendarId = this.getSelectedCalendarId();
   
   var SelectedCalendar = this.rdf.getNode( SelectedCalendarId );
   ThisCalendarObject.Id = SelectedCalendarId;
   
   ThisCalendarObject.name = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#name" );
   ThisCalendarObject.path = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   ThisCalendarObject.active = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#active" );
   ThisCalendarObject.remote = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remote" );
   ThisCalendarObject.remotePath = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" );
   ThisCalendarObject.publishAutomatically = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#publishAutomatically" );
   ThisCalendarObject.color = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#color" );
   ThisCalendarObject.shared = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#shared" );
   ThisCalendarObject.autoreload = SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#autoreload" );
   
   var args = new Object();
   args.mode = "edit";

   var thisManager = this;

   var callback = function( ThisCalendarObject ) { thisManager.editServerDialogResponse( ThisCalendarObject ) };

   args.onOk =  callback;
   args.CalendarObject = ThisCalendarObject;

   // open the dialog modally
   openDialog("chrome://calendar/content/serverDialog.xul", "caEditServer", "chrome,titlebar,modal", args );
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.addServerDialogResponse = function calMan_addServerDialogResponse( CalendarObject )
{
   var next = this.nextCalendar();
   var name = "calendar"+next;

   CalendarObject.active = true;
   CalendarObject.remotePath = CalendarObject.remotePath.replace( "webcal:", "http:" );

   var node = this.rootContainer.addNode(name);
   node.setAttribute("http://home.netscape.com/NC-rdf#active", "true");
   node.setAttribute("http://home.netscape.com/NC-rdf#serverNumber", next);
   node.setAttribute("http://home.netscape.com/NC-rdf#name", CalendarObject.name);
   node.setAttribute( "http://home.netscape.com/NC-rdf#shared",  CalendarObject.shared);
   node.setAttribute( "http://home.netscape.com/NC-rdf#autoreload",  CalendarObject.autoreload);
   
   var profileFile;

   if( CalendarObject.path == "" )
   {
      //they didn't set a path in the box, that's OK, its not required.
      profileFile = this.getProfileDirectory();
      profileFile.append("Calendar");
      profileFile.append("CalendarDataFile"+ next+ ".ics");
      CalendarObject.path = profileFile.path;
   }
   
   node.setAttribute("http://home.netscape.com/NC-rdf#path", CalendarObject.path);

   // CofC save off the color of the new calendar
    // Add the default color for when a user does not select a calendar color.
    if( CalendarObject.color == '' )
    {
        node.setAttribute("http://home.netscape.com/NC-rdf#color", "#F9F4FF");
    }
    else
    {
        node.setAttribute("http://home.netscape.com/NC-rdf#color", CalendarObject.color);
    }
   
   if( CalendarObject.remotePath.indexOf( "http://" ) != -1 ||
       CalendarObject.remotePath.indexOf( "https://" ) != -1 ||
       CalendarObject.remotePath.indexOf( "ftp://" ) != -1 )
   {
      profileFile = this.getProfileDirectory();
      profileFile.append( "Calendar" );
      profileFile.append("RemoteCalendar"+ next+ ".ics");

      node.setAttribute("http://home.netscape.com/NC-rdf#remote", "true");
      
      node.setAttribute("http://home.netscape.com/NC-rdf#remotePath", CalendarObject.remotePath);
      node.setAttribute("http://home.netscape.com/NC-rdf#publishAutomatically", CalendarObject.publishAutomatically);
      this.retrieveAndSaveRemoteCalendar( node );
      
      dump( "Remote Calendar Number "+ next+ " Added\n" );
   }
   else
   {
      node.setAttribute("http://home.netscape.com/NC-rdf#remote", "false");
      this.reloadCalendar( node ); 
      dump( "Calendar Number "+CalendarObject.serverNumber+" Added\n" );
   }
   
   this.rdf.flush();

   // change made by PAB... new calendars don't have their Id field set because
   // it does not exist until after the node is created. This causes trouble downstream
   // because the calendar coloring code forms the name of the color style from the Id.
   // So... set the CalendarObjects Id here
   CalendarObject.Id = node.resource.Value;
   
   // call the calendar color update function with the calendar object
   // NOTE: this call was moved
   calendarColorStyleRuleUpdate( CalendarObject );
}

/**
 * Finds the maximum calendar id used in the RDF datasource.
 */
calendarManager.prototype.nextCalendar = function calMan_getNextCalendar()
{
    var seq = this.rootContainer.getSubNodes();
    var max = -1;
    var subject;

    for (var count = 0; count < seq.length; count++ ) {
        subject = seq[count].getSubject();
        subject = subject.replace(/^.*calendar(\d+)$/, "$1");
        if (Number(subject) > max) max = Number(subject);
    }

    return ++max;
}


/*
** Called when OK is clicked in the edit localCalendar dialog.
*/
calendarManager.prototype.editLocalCalendarDialogResponse = function calMan_editServerDialogResponse( CalendarObject )
{
   var name = CalendarObject.Id;
   
   //get the node
   var node = this.rdf.getNode( name );
   
   node.setAttribute( "http://home.netscape.com/NC-rdf#name", CalendarObject.name );
   node.setAttribute( "http://home.netscape.com/NC-rdf#remotePath", CalendarObject.remotePath );
   node.setAttribute("http://home.netscape.com/NC-rdf#publishAutomatically", CalendarObject.publishAutomatically);
   node.setAttribute("http://home.netscape.com/NC-rdf#color", CalendarObject.color);
   //node.setAttribute( "http://home.netscape.com/NC-rdf#shared",  CalendarObject.shared); //TBD uncomment once the calendar dialog is changed
   //node.setAttribute( "http://home.netscape.com/NC-rdf#autoreload",  CalendarObject.autoreload); //TBD uncomment once the calendar dialog is changed 

   this.rdf.flush();
   
   // CofC
   // call the calendar color update function with the calendar object
   calendarColorStyleRuleUpdate( CalendarObject );
}

/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.editServerDialogResponse = function calMan_editServerDialogResponse( CalendarObject )
{
   var name = CalendarObject.Id;
   
   //get the node
   var node = this.rdf.getNode( name );
   
   node.setAttribute( "http://home.netscape.com/NC-rdf#name", CalendarObject.name );
   node.setAttribute("http://home.netscape.com/NC-rdf#publishAutomatically", CalendarObject.publishAutomatically);
   node.setAttribute("http://home.netscape.com/NC-rdf#color", CalendarObject.color);
   //node.setAttribute( "http://home.netscape.com/NC-rdf#shared",  CalendarObject.shared); //TBD uncomment once the calendar dialog is changed
   //node.setAttribute( "http://home.netscape.com/NC-rdf#autoreload",  CalendarObject.autoreload); //TBD uncomment once the calendar dialog is changed
   
   this.rdf.flush();
   
   // CofC
   // call the calendar color update function with the calendar object
   calendarColorStyleRuleUpdate( CalendarObject );
}


/*
** Add the calendar so it is included in searches
*/
calendarManager.prototype.addCalendar = function calMan_addCalendar( ThisCalendarObject )
{
  dump(Components.stack.toString() + "\n");
  var x = Components.stack.caller;
  while (x) {
    dump(x.toString() + "\n");
    x = x.caller;
  }

   gICalLib.addCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ),
                         ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#type" ) );
   var id = ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" );
   gModifiedTime[id] = this.getLocalModifiedTime( ThisCalendarObject );
}


/* 
** Remove the calendar, so it doesn't get included in searches 
*/
calendarManager.prototype.removeCalendar = function calMan_removeCalendar( ThisCalendarObject )
{
   dump( "\n calendarManager-> remove calendar "+ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   gICalLib.removeCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );

   //ThisCalendarObject.setAttribute( "http://home.netscape.com/NC-rdf#active", "false" );

   //this.rdf.flush();
}


/* 
** Reload the calendar
*
* skipRefresh will reparse the file, without refreshing the view
* return true if the calendar was actually reloaded i.e. if there were external changes
*/

calendarManager.prototype.reloadCalendar = function calMan_reloadCalendar( ThisCalendarObject, skipRefresh )
{
   skipRefresh = (calMan_reloadCalendar.arguments.length == 1) ? false : skipRefresh;
   //TODO implement reloadCalendar inside gICalLib
   var active = ( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" );
   var path = ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   var id = ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber");

   var localModifiedTime = this.getLocalModifiedTime( ThisCalendarObject );
   if ( gModifiedTime[id] < localModifiedTime ) {
      gICalLib.removeCalendar( path );
      this.addCalendar( ThisCalendarObject );
      if ( !skipRefresh ) {
         refreshView();
      }
      return true;
   }
   return false;
}



calendarManager.prototype.getLocalModifiedTime = function calMan_getModifiedTime( Calendar )
{   
   var calendarFile = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   var path = Calendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   calendarFile.initWithPath( path );
   
   if ( calendarFile.exists() ){
      return calendarFile.lastModifiedTime;
   } else {
      return 0;
   }   
}


calendarManager.prototype.startLocalLock = function calMan_startLocalLock( Calendar )
{
   var lockTimeOut = 5000; //millisecs after which a lock expires
   var maxWaitTime = 10000; //millisecs waited before giving up
   var loopWait = 500; //millisecs to wait before rechecking lock
   
   const NORMAL_FILE_TYPE = 0;
   const PERMISSIONS = 777;
   
   var CalendarFile = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   var LockFile = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   var DummyFile = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   var path = Calendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   CalendarFile.initWithPath( path );
   LockFile.initWithPath( path + ".lock" );
   DummyFile.initWithPath( path + ".dummy" );
   
   //Make TimeOut depend on file size
   var fileSize =  CalendarFile.fileSize;
   lockTimeOut = lockTimeOut + fileSize / 1000; //~+1 sec per MB
   maxWaitTime = maxWaitTime + fileSize / 500; //~+2 sec per MB
         
    //Get time of server hosting remote file, simulating _nix touch command
   if (DummyFile.exists()){
     DummyFile.remove(false);
   }
   try{
      DummyFile.create(NORMAL_FILE_TYPE,PERMISSIONS);
   } catch(er) {
      return false;
   }
   var timeServer = DummyFile.lastModifiedTime;
   DummyFile.remove(false);
   
   //Check lock
   var timeStart = new Date().getTime();
   var timeOffset = timeServer - timeStart; 
   var timeLoop = 0; //used to replicate wait() 
   var timeLock = 0; //modified time of locked file
   while(1){
      timeNow = new Date().getTime(); 
      //TODO add proper wait function (using xpcom?)
      if ( timeNow > timeLoop + loopWait ) {
         //only check every half second
         timeLoop = timeNow;
         if ( ! LockFile.exists() ) {
            //if there is no lock, we assume it is ok to proceed
            break;
         } else {
            //Check if the lock is an old leftover, if so delete it
            try {
               timeLock = LockFile.lastModifiedTime;
            } catch(er) {
               timeLock = null;
            }
            if ( ( timeLock != null ) &&
                 ( timeNow + timeOffset > timeLock + lockTimeOut ) ) {
               LockFile.remove(false);
               break;
            }
         }
      }
      //Give up if we have been looping for more than maxWaitTime
      if ( timeNow > timeStart + maxWaitTime ) {
         return false;
      }
   }
   //Start new lock 
   try{
      LockFile.create(NORMAL_FILE_TYPE,PERMISSIONS);
      return LockFile.exists();
   } catch(er) {
      return false;
   }
}


calendarManager.prototype.removeLocalLock = function calMan_removeLocalLock( Calendar )
{
   var LockFile = Components.classes[LOCALFILE_CTRID].createInstance( nsILocalFile );
   var path = Calendar.getAttribute( "http://home.netscape.com/NC-rdf#path" );
   LockFile.initWithPath( path + ".lock" );
   try{
      LockFile.remove(false);
   } catch(er) {
   }
}


/*
** Delete the calendar. Remove the file, it won't be used any longer.
*/
calendarManager.prototype.deleteCalendar = function calMan_deleteCalendar( ThisCalendarObject, deleteFile )
{
   var serverNumber = ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#serverNumber" );
   if( serverNumber == 0 )
      return;

   gICalLib.removeCalendar( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
   var tid = gAutoreloadTimerId[serverNumber];
   if ( tid != null ){
      clearInterval(gAutoreloadTimerId[serverNumber]);
   }

   //I think deleting calendar and file is a bad idea, particularly if the calendar is shared
   //TODO modify GUI and eliminate "Delete Calendar and File" button
   if( deleteFile == true && ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#shared" ) != "true" ) 
   {
      var FileToRemove = new File( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
      FileToRemove.remove();
   }
   
   ThisCalendarObject.remove();

   this.rdf.flush();
}

calendarManager.prototype.publishCalendar = function calMan_publishCalendar( SelectedCalendar )
{
   if( !SelectedCalendar )
   {
      var SelectedCalendarId = this.getSelectedCalendarId();
      SelectedCalendar = this.rdf.getNode( SelectedCalendarId );
   }
   
   calendarUploadFile(SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#path" ), 
                      SelectedCalendar.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" ), 
                      "text/calendar");
}


calendarManager.prototype.getSelectedCalendarId = function calMan_getSelectedCalendarId( )
{
   var calendarListBox = document.getElementById( "list-calendars-listbox" );
   
   return( calendarListBox.selectedItem.getAttribute( "id" ) );
}


calendarManager.prototype.getCalendarByName = function calMan_getCalendarByName( Name )
{
   for( var i = 0; i < this.rootContainer.getSubNodes().length; i++ )
   {
      if( this.rootContainer.getSubNodes()[i].getAttribute( "http://home.netscape.com/NC-rdf#path" ) == Name )
      {
         return( this.rootContainer.getSubNodes()[i] );
      }                
   }
   return( false );
}

calendarManager.prototype.retrieveAndSaveRemoteCalendar = function calMan_retrieveAndSaveRemoteCalendar( ThisCalendarObject, onResponseExtra )
{
   //the image doesn't always exist. If it doesn't exist, it causes problems, so check for it here
   document.getElementById( ThisCalendarObject.getSubject() ).childNodes[1].childNodes[0].setAttribute( "synching", "true" );

   var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
   
   var Path = ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#remotePath" );

   var Channel = ioService.newChannel( Path, null, null );
   Channel.loadFlags |= Components.interfaces.nsIRequest.LOAD_BYPASS_CACHE;

   var CalendarManager = this;
   
   var onResponse = function( CalendarData )
   {
      //save the stream to a file.
      //saveDataToFile( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ), CalendarData, "UTF-8" );
       saveDataToFile( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#path" ), CalendarData, null );
      
      if( onResponseExtra )
         onResponseExtra();
      
      CalendarManager.removeCalendar( ThisCalendarObject );
   
      if( ThisCalendarObject.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "true" )
      {
         CalendarManager.addCalendar( ThisCalendarObject );
      }
      
      refreshView();
      
      document.getElementById( ThisCalendarObject.getSubject() ).childNodes[1].childNodes[0].removeAttribute( "synching" );
   }

   var CalendarData = this.getRemoteCalendarText( Channel, onResponse, null );
}

//this function will do the refreshing in turn for all calendars
//so once refreshing one is finished refreshing the other will be
//invoked.
calendarManager.prototype.refreshAllRemoteCalendars = function calMan_refreshAllRemoteCalendars()
{
   var throbberElement = document.getElementById("navigator-throbber");
   
   //get all the calendars.
   //get all the other calendars
   var SubNodes = this.rootContainer.getSubNodes();

   for( var i = gNextSubNodeToRefresh; i < SubNodes.length; i++ )
   {
      //check their remote attribute, if its true, call retrieveAndSaveRemoteCalendar()
      if( SubNodes[i].getAttribute( "http://home.netscape.com/NC-rdf#remote" ) == "true" )
      {
         if (throbberElement)
            throbberElement.setAttribute("busy", "true")
         this.retrieveAndSaveRemoteCalendar( SubNodes[i] );
         gNextSubNodeToRefresh = i+1;
         return;
      }
   }
   gNextSubNodeToRefresh = 0;
   if (throbberElement)
      throbberElement.setAttribute("busy", "false")
}

/*
** Checks if the URL is already in the list.
** If so, it returns the calendar object.
** Otherwise, returns false.
*/

calendarManager.prototype.isURLAlreadySubscribed = function calMan_isCalendarSubscribed( CalendarURI )
{
   CalendarURL = CalendarURI.spec.replace( "webcal:", "http:" );

   var subNodes = this.rootContainer.getSubNodes();
   for( var i = 0; i < subNodes.length; i++ )
   {
      if( subNodes[i].getAttribute( "http://home.netscape.com/NC-rdf#remotePath" ) == CalendarURL )
      {
         return( subNodes[i] );
      }
   }
   if( CalendarURI.scheme == "file" ) {
     for( i = 0; i < subNodes.length; i++ ) {
       if( makeURLFromPath( subNodes[i].getAttribute( "http://home.netscape.com/NC-rdf#path" ) ).equals( CalendarURI ) ) {
         return( subNodes[i] );
       }
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
   var calendarSubscribed = this.isURLAlreadySubscribed( Channel.URI );
   
   if( calendarSubscribed === false )
   {
      if( Channel.URI.spec.indexOf( "http://" ) != -1 || Channel.URI.spec.indexOf( "https://" ) != -1
          || Channel.URI.spec.indexOf( "ftp://" ) != -1 || Channel.URI.spec.indexOf( "webcal://" ) != -1 )
      {
         //not subscribed, prompt the user to do so.
         var arrayForNames = Channel.URI.spec.split( "/" );
         var CalendarNameWithExtension = arrayForNames[ arrayForNames.length - 1 ];
         var CalendarName = CalendarNameWithExtension.replace( ".ics", "" );

         this.launchAddRemoteCalendarDialog( CalendarName, Channel.URI.spec );

      } else {
         var CalendarManager = this;
      
         var onResponse = function( CalendarData )
         {
            var BeginEventText = "BEGIN:VEVENT";
            var firstMatchLocation = CalendarData.indexOf( BeginEventText );
            if( CalendarData.indexOf( BeginEventText, firstMatchLocation + BeginEventText.length + 1 ) == -1 )
            {
               var iCalEventComponent = Components.classes["@mozilla.org/icalevent;1"].createInstance();
               var calendarEvent = iCalEventComponent.QueryInterface(Components.interfaces.oeIICalEvent);
               calendarEvent.parseIcalString( CalendarData );
               
               /* EDITING AN EXISTING EVENT IS NOT WORKING
               
               //only 1 event
               var Result = gICalLib.fetchEvent( calendarEvent.id );
                                    
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
               var profileFile = CalendarManager.getProfileDirectory();
      
               profileFile.append("Calendar");
               var CalendarName = "Local"+ CalendarManager.nextCalendar();
               profileFile.append( CalendarName + ".ics");
                
               FilePath = profileFile.path;
               saveDataToFile(FilePath, CalendarData, null);
      
               CalendarManager.launchOpenCalendarFileDialog( CalendarName, FilePath );
            }
         }
         var result = this.getRemoteCalendarText( Channel, onResponse, null );
      }
   }
   else
   {
      //calendarSubscribed is the subscribed calendar object.
      if( calendarSubscribed.getAttribute( "http://home.netscape.com/NC-rdf#active" ) == "false" ) {
         calendarSubscribed.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );

         this.addCalendar( calendarSubscribed );
      
         //document.getElementById( "calendar-list-item-"+calendarSubscribed.serverNumber ).setAttribute( "checked", "true" );

         refreshView();
      } else {
        this.reloadCalendar( calendarSubscribed );
      }
   }
}


calendarManager.prototype.getRemoteCalendarText = function calMan_getRemoteCalendarText( Channel, onResponse, onError )
{
   var Listener =
   {
      onStreamComplete: function(loader, ctxt, status, resultLength, result)
      {
         // revert from "wait" cursor set by getRemoteCalendarText (below)
         window.setCursor( "auto" );

         var calendarStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
         var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                       .getService(Components.interfaces.nsIPromptService);

         var retval = false;
         if( typeof( result ) != "string" ) { //for 1.7 compatibility
            // Result is an array of integer unicode values.

            try {
               var unicodeConverter = Components.classes["@mozilla.org/intl/scriptableunicodeconverter"]
                                                .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
               // ics files are always utf8
               unicodeConverter.charset = "UTF-8";
               result = unicodeConverter.convertFromByteArray( result, result.length );
            } catch(e) {
               // Now try the pre-1.8a5 method, which might have problems
               // with utf8 chars

               // Convert to string by applying String.fromCharCode static function.
               // Function.apply can take array of length < expt(2, 16),
               // so convert slices half that length to strings, then join strings.
               var sliceStringArray = new Array();
               for (var start = 0, end;
                    start < (end = Math.min(result.length, start + 32768));
                    start = end) { 
                 var slice = result.slice(start, end);
                 var sliceString = String.fromCharCode.apply(null, slice);
                 sliceStringArray[sliceStringArray.length] = sliceString;
               }
               result = sliceStringArray.join("");
            }
         }

         var ch;
         try {
           ch = loader.request.QueryInterface(Components.interfaces.nsIHttpChannel);
         } catch(e) {
         }
         if (ch && !ch.requestSucceeded) {
           promptService.alert(null, calendarStringBundle.GetStringFromName('errorTitle'),
                               calendarStringBundle.formatStringFromName('httpError',[ch.responseStatus, ch.responseStatusText],2));
         }

         else if (ch && !Components.isSuccessCode(loader.request.status)) {
           // XXX this should be made human-readable.
           promptService.alert(null, calendarStringBundle.GetStringFromName('errorTitle'),
                               calendarStringBundle.formatStringFromName('otherError',[loader.request.status.toString(16)],1));
         }

         //check to make sure its actually a calendar file, if not return.
         else if( result.indexOf( "BEGIN:VCALENDAR" ) == -1 )
         {
           promptService.alert(null, calendarStringBundle.GetStringFromName('errorTitle'),
                               calendarStringBundle.formatStringFromName('contentError',[Channel.URI.spec, result],2));
         } else {
             onResponse( result );
             retval = true;
         }
         if( gNextSubNodeToRefresh )
            gCalendarWindow.calendarManager.refreshAllRemoteCalendars();
         return retval;
      }
   }

   var notificationCallbacks =
   {
     // nsIInterfaceRequestor interface
     getInterface: function(iid, instance) {
       if (iid.equals(Components.interfaces.nsIAuthPrompt)) {
         // use the window watcher service to get a nsIAuthPrompt impl
         return Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                          .getService(Components.interfaces.nsIWindowWatcher)
                          .getNewAuthPrompter(window);
       }
       else if (iid.equals(Components.interfaces.nsIPrompt)) {
         // use the window watcher service to get a nsIPrompt impl
         return Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
                          .getService(Components.interfaces.nsIWindowWatcher)
                          .getNewPrompter(window);
       }
       Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
       return null;
     }
   }

   Channel.notificationCallbacks = notificationCallbacks;
   var myInstance = Components.classes["@mozilla.org/network/stream-loader;1"].createInstance(Components.interfaces.nsIStreamLoader);
   dump( "init channel, \nChannel is "+Channel+"\nURL is "+Channel.URI.spec+"\n" );
   window.setCursor( "wait" );
   try {
     myInstance.init( Channel, Listener, null );
   } catch (e) {
     window.setCursor( "auto" );
   }
}

calendarManager.prototype.getProfileDirectory = function calMan_getProfileDirectory()
{
   var dirService;
   if( "@mozilla.org/directory_service;1" in Components.classes ) {
       dirService = Components.classes["@mozilla.org/directory_service;1"]
             .getService(Components.interfaces.nsIProperties);
   } else {
       dirService = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties);
   }
   var profileDir = dirService.get("ProfD", Components.interfaces.nsIFile);
   return profileDir;
}


calendarManager.prototype.getDefaultServer = function calMan_getDefaultServer()
{
   return( this.rootContainer.getSubNodes()[0].getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
}

calendarManager.prototype.getDefaultCalendarName = function calMan_getDefaultName()
{
   return( this.rootContainer.getSubNodes()[0].getAttribute( "http://home.netscape.com/NC-rdf#name" ) );
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
         var Branch = prefService.getBranch( "calendar." );
         thisCalendar.serverNumber = ArrayOfCalendars[i];
         var serverString = "server" + thisCalendar.serverNumber;
         
         thisCalendar.name = getCharPref(Branch, serverString + ".name", "" );
         thisCalendar.path = getCharPref(Branch, serverString + ".path", "" );
         thisCalendar.active = getBoolPref(Branch, serverString + ".active", false );
         thisCalendar.remote = getBoolPref(Branch, serverString + ".remote", false );
         thisCalendar.remotePath = getCharPref(Branch, serverString + ".remotePath", "" );
         thisCalendar.shared = getCharPref(Branch, serverString + ".shared", false );
         thisCalendar.autoreload = getCharPref(Branch, serverString + ".autoreload", false );
      }
      catch ( e )
      {
         dump( "error: could not get calendar information from preferences\n"+e );
      }

      var nameId = this.nextCalendar();
      var name = "calendar"+ nameId;
        
      //now convert it, and put it in the RDF file.
      var node = this.rootContainer.addNode(name);
      node.setAttribute("http://home.netscape.com/NC-rdf#active", thisCalendar.name);
      node.setAttribute("http://home.netscape.com/NC-rdf#serverNumber", nameId);
      node.setAttribute("http://home.netscape.com/NC-rdf#name", thisCalendar.name);
      
      if( thisCalendar.remote == true )
      {
         var profileFile = this.getProfileDirectory();
         profileFile.append( "Calendar" );
         profileFile.append("RemoteCalendar"+ nameId);
         var CalendarPath  = profileFile.path;
      }
      else
      {
         CalendarPath = thisCalendar.remotePath;
      }

      node.setAttribute("http://home.netscape.com/NC-rdf#path", CalendarPath);
      node.setAttribute("http://home.netscape.com/NC-rdf#remote", thisCalendar.remote);
      node.setAttribute("http://home.netscape.com/NC-rdf#remotePath", thisCalendar.remotePath);

      this.rdf.flush();
      //if the file CalendarDataFile.ics exists in the users profile directory, move it to Calendar/CalendarDataFile.ics
      newCalendarFile = this.getProfileDirectory();
      newCalendarFile.append( "Calendar" );
      newCalendarFile.append( "RemoteCalendar"+this.nextCalendar()+".ics" );
   
      oldCalendarDataFile = new File( thisCalendar.path );
      newCalendarDataFile = new File( newCalendarFile.path );
   
      if( oldCalendarDataFile.exists() && thisCalendar.remote == true )
      {
         alert( "moving "+oldCalendarDataFile.path+" to "+newCalendarDataFile.path );
         oldCalendarDataFile.copy( newCalendarDataFile.path );
         oldCalendarDataFile.remove( );
      }
      try{
         Branch.clearUserPref( serverString + ".name" );
         Branch.clearUserPref( serverString + ".path" );
         Branch.clearUserPref( serverString + ".active" );
         Branch.clearUserPref( serverString + ".remote" );
         Branch.clearUserPref( serverString + ".remotePath" );
      }catch(e){
   }
   }
   try
   {
      Branch.clearUserPref( "server0.active" );
      Branch.clearUserPref( "servers.count" );
      Branch.clearUserPref( "servers.array" );
   } catch( e ) {
   }
}

function makeURLFromPath( path ) {
  var localFile = Components.classes["@mozilla.org/file/local;1"].createInstance().
                                    QueryInterface(Components.interfaces.nsILocalFile);
  localFile.initWithPath( path );
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                 .getService(Components.interfaces.nsIIOService);
  return ioService.newFileURI(localFile); 
}

function refreshView() {
  refreshEventTree();
  refreshToDoTree( false );
  gCalendarWindow.currentView.refreshEvents();
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
   
   //16 is the width of the checkbox
   if( ( event.clientX - event.currentTarget.boxObject.x ) > 16 )
   {
      return;
   }
   
   var calendarNode = gCalendarWindow.calendarManager.rdf.getNode( event.currentTarget.getAttribute( "id" ) );

   if( event.currentTarget.childNodes[0].getAttribute( "checked" ) != "true" )
   {
      window.setCursor( "wait" );

      gICalLib.addCalendar( calendarNode.getAttribute( "http://home.netscape.com/NC-rdf#path" ),
                            calendarNode.getAttribute( "http://home.netscape.com/NC-rdf#type" ));
      
      window.setCursor( "auto" );
      
      calendarNode.setAttribute( "http://home.netscape.com/NC-rdf#active", "true" );
      
      event.currentTarget.childNodes[0].setAttribute( "checked", "true" );
   }
   else
   {
      window.setCursor( "wait" );

      gICalLib.removeCalendar( calendarNode.getAttribute( "http://home.netscape.com/NC-rdf#path" ) );
      
      window.setCursor( "auto" );
      
      calendarNode.setAttribute( "http://home.netscape.com/NC-rdf#active", "false" );
      
      event.currentTarget.childNodes[0].removeAttribute( "checked" );
   }
      
   gCalendarWindow.calendarManager.rdf.flush();
   
   refreshView();
}


function deleteCalendar( )
{
   // Show a dialog with option to import events with or without dialogs
   var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(); 
   promptService = promptService.QueryInterface(Components.interfaces.nsIPromptService); 
   var result = {value:0}; 

   var buttonPressed =      
      promptService.confirmEx(window, 
                            gCalendarBundle.getString( "deleteCalendarTitle" ), gCalendarBundle.getString( "deleteCalendarMessage" ), 
                            (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_0) + 
                            (promptService.BUTTON_TITLE_CANCEL * promptService.BUTTON_POS_1) + 
                            (promptService.BUTTON_TITLE_IS_STRING * promptService.BUTTON_POS_2), 
                            gCalendarBundle.getString( "deleteCalendarOnly" ),null,gCalendarBundle.getString( "deleteCalendarAndFile" ),null, result); 
   
   var IdToDelete = gCalendarWindow.calendarManager.getSelectedCalendarId()
   
   var calendarObjectToDelete = gCalendarWindow.calendarManager.rdf.getNode( IdToDelete );

   if(buttonPressed == 0) // Delete calendar
   { 
      gCalendarWindow.calendarManager.deleteCalendar( calendarObjectToDelete, false );
   }
   else if(buttonPressed == 2) //delete calendar and file
   { 
      gCalendarWindow.calendarManager.deleteCalendar( calendarObjectToDelete, true );
   } 
   else if(buttonPressed == 1) // CANCEL
   { 
      return false; 
   } 
   
   refreshView();

   return true;
}

// CofC
// College of Charleston calendar color change code... when returning from the dialog
// update the calendar's background color style in the event it was changed.
// Author(s): Dallas Vaughan, Paul Buhler
function calendarColorStyleRuleUpdate( ThisCalendarObject )
{
   var j = -1;
   var i;
   
   // obtain calendar name from the Id
   var containerName = ThisCalendarObject.Id.split(':')[2];

   var tempStyleSheets = document.styleSheets;
   for (i=0; i<tempStyleSheets.length; i++)
   {
      if (tempStyleSheets[i].href.match(/chrome.*\/skin.*\/calendar.css$/))
	  {
          j = i;
          break;
	  }
   }

   // check that the calendar.css stylesheet was found
   if( j != -1 )
   {
	   var ruleList = tempStyleSheets[j].cssRules;
	   var ruleName;

	   for (i=0; i < ruleList.length; i++)
	   {
		  ruleName = ruleList[i].cssText.split(' ');

		  // find the existing rule so that it can be deleted
	      if (ruleName[0] == "." + containerName)
		  {
	         tempStyleSheets[j].deleteRule(i);
	         break;
	      }
	   }

	   // insert the new calendar color rule
	   tempStyleSheets[j].insertRule("." + containerName + " { background-color:" +ThisCalendarObject.color + " !important;}",1);

           var calcColor = ThisCalendarObject.color.replace( /#/g, "" );
           var red = parseInt( calcColor.substring( 0, 2 ), 16 );
           var green = parseInt( calcColor.substring( 2, 4 ), 16 );
           var blue = parseInt( calcColor.substring( 4, 6 ), 16 );

           // Calculate the L(ightness) value of the HSL color system.
           // L = ( max( R, G, B ) + min( R, G, B ) ) / 2
           var max = Math.max( Math.max( red, green ), blue );
           var min = Math.min( Math.min( red, green ), blue );
           var lightness = ( max + min ) / 2;

           // Consider all colors with less than 50% Lightness as dark colors
           // and use white as the foreground color.
           // Actually we use a treshold a bit below 50%, so colors like
           // #FF0000, #00FF00 and #0000FF still get black text which looked
           // better when we tested this.
           if ( lightness < 120 )
              tempStyleSheets[j].insertRule( "." + containerName + " { color:" + " white" + "!important;}", 1 );
	}
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
         gCalendarWindow.calendarManager.launchOpenCalendarFileDialog(url.fileBaseName, url.spec );

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
