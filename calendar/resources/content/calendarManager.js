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
   this.nextCalendarNumber = 1;

   this.CalendarWindow = CalendarWindow;

   this.calendars = new Array();

   this.getAllCalendars();

   for( var i = 0; i < this.calendars.length; i++ )
   {
      this.addCalendarToListBox( this.calendars[i] );
   }

   //if the calendars are being used, add them.
   this.addActiveCalendars();
}


calendarManager.prototype.addActiveCalendars = function calMan_addActiveCalendars()
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i].active == true )
      {
         this.addCalendar( this.calendars[i] );
      }                
   }
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
   var ThisCalendarObject = this.getSelectedCalendar();
   
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
   CalendarObject.active = true;

   CalendarObject.serverNumber = this.nextCalendarNumber;
   this.nextCalendarNumber++;

   CalendarObject.path = CalendarObject.path.replace( "webcal:", "http:" );

   if( CalendarObject.path.indexOf( "http://" ) != -1 )
   {
      var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
      var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
      var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
      profileFile.append("RemoteCalendar"+CalendarObject.serverNumber+".ics");

      CalendarObject.remotePath = CalendarObject.path;
      CalendarObject.path = profileFile.path;
      
      this.retrieveAndSaveRemoteCalendar( CalendarObject, onResponseAndRefresh );

      dump( "Remote Calendar Number "+CalendarObject.serverNumber+" Added" );
   }
   else
   {
      dump( "Calendar Number "+CalendarObject.serverNumber+" Added" );
   }
  
   
   //add the information to the preferences.
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CalendarObject.serverNumber+".name", CalendarObject.name );
   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+CalendarObject.serverNumber+".remote", true );
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CalendarObject.serverNumber+".remotePath", CalendarObject.remotePath );
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CalendarObject.serverNumber+".path", CalendarObject.path );
   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+CalendarObject.serverNumber+".active", true );
   this.CalendarWindow.calendarPreferences.calendarPref.setIntPref(  "servers.count", (parseInt( CalendarObject.serverNumber )+1) );
   
   //add this to the global calendars array
   this.calendars[ this.calendars.length ] = CalendarObject;

   this.updateServerArrayText();

   this.addCalendarToListBox( CalendarObject );
}


/*
** Called when OK is clicked in the new server dialog.
*/
calendarManager.prototype.editServerDialogResponse = function calMan_editServerDialogResponse( CalendarObject )
{
   //the only thing they could have changed is the name.
   //get the server number of the calendar
   //edit its name.
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "server"+CalendarObject.serverNumber+".name", CalendarObject.name );

   //get the element and change its label
   document.getElementById( "calendar-list-item-"+CalendarObject.serverNumber ).setAttribute( "label", CalendarObject.name );
   document.getElementById( "calendar-list-item-"+CalendarObject.serverNumber ).calendarObject = CalendarObject;
}


/*
** Add the calendar so it is included in searches
*/
calendarManager.prototype.addCalendar = function calMan_addCalendar( ThisCalendarObject )
{
   this.CalendarWindow.eventSource.gICalLib.addCalendar( ThisCalendarObject.path );

   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( ThisCalendarObject )+".active", true );
}


/* 
** Remove the calendar, so it doesn't get included in searches 
*/
calendarManager.prototype.removeCalendar = function calMan_removeCalendar( ThisCalendarObject )
{
   this.CalendarWindow.eventSource.gICalLib.removeCalendar( ThisCalendarObject.path );

   this.CalendarWindow.calendarPreferences.calendarPref.setBoolPref( "server"+this.getCalendarIndex( ThisCalendarObject )+".active", false );
}


/*
** Delete the calendar. Remove the file, it won't be used any longer.
*/
calendarManager.prototype.deleteCalendar = function calMan_deleteCalendar( ThisCalendarObject )
{
   this.removeCalendar( ThisCalendarObject );
   
   //remove it from the array
   var index = 0;
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i] == ThisCalendarObject )
      {
         index = i;
         break;
      }
   }

   this.calendars.splice( index, 1 );

   //remove it from the prefs
   this.updateServerArrayText();
   //http://lxr.mozilla.org/seamonkey/source/modules/libpref/public/nsIPrefBranch.idl#205
   this.CalendarWindow.calendarPreferences.calendarPref.clearUserPref( "server"+ThisCalendarObject.serverNumber+".name" );
   this.CalendarWindow.calendarPreferences.calendarPref.clearUserPref( "server"+ThisCalendarObject.serverNumber+".path" );
   this.CalendarWindow.calendarPreferences.calendarPref.clearUserPref( "server"+ThisCalendarObject.serverNumber+".remote" );
   this.CalendarWindow.calendarPreferences.calendarPref.clearUserPref( "server"+ThisCalendarObject.serverNumber+".remotePath" );
   this.CalendarWindow.calendarPreferences.calendarPref.clearUserPref( "server"+ThisCalendarObject.serverNumber+".active" );


   //remove the listitem
   document.getElementById( "calendar-list-item-"+ThisCalendarObject.serverNumber ).parentNode.removeChild( document.getElementById( "calendar-list-item-"+ThisCalendarObject.serverNumber ) );
   
   //TODO: remove the file completely
}


calendarManager.prototype.getAllCalendars = function calMan_getAllCalendars()
{
   this.calendars = new Array();

   var profileComponent = Components.classes["@mozilla.org/profile/manager;1"].createInstance();
      
   var profileInternal = profileComponent.QueryInterface(Components.interfaces.nsIProfileInternal);
 
   var profileFile = profileInternal.getProfileDir(profileInternal.currentProfile);
      
   profileFile.append("CalendarDataFile.ics");
                      
   //the root calendar is not stored in the prefs file.
   var thisCalendar = new CalendarObject;
   thisCalendar.name = "Default";
   thisCalendar.path = profileFile.path;
   var active;

   active = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server0.active", true );

   thisCalendar.active = active;
   thisCalendar.remote = false;
   this.calendars[ this.calendars.length ] = thisCalendar;
   
   //go through the prefs file, calendars are stored in there.
   //var NumberOfCalendars = getIntPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.count", 1 );
   
   var ServerArray = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.array", "" );
   
   var ArrayOfCalendars = ServerArray.split( "," );
   
   //don't count the default server, so this starts at 1
   for( var i = 1; i < ArrayOfCalendars.length; i++ )
   {
      if( ArrayOfCalendars[i] >= this.nextCalendarNumber )
         this.nextCalendarNumber = parseInt( ArrayOfCalendars[i] )+1;

      thisCalendar = new CalendarObject();
      
      try { 
         thisCalendar.serverNumber = ArrayOfCalendars[i];
         thisCalendar.name = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+ArrayOfCalendars[i]+".name", "" );
         thisCalendar.path = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+ArrayOfCalendars[i]+".path", "" );
         thisCalendar.active = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+ArrayOfCalendars[i]+".active", false );
         thisCalendar.remote = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+ArrayOfCalendars[i]+".remote", false );
         thisCalendar.remotePath = getCharPref(this.CalendarWindow.calendarPreferences.calendarPref, "server"+ArrayOfCalendars[i]+".remotePath", "" );
         
         this.calendars[ this.calendars.length ] = thisCalendar;
      }
      catch ( e )
      {
         dump( "error: could not get calendar information from preferences\n"+e );
      }
   }

   var RefreshServers = getBoolPref(this.CalendarWindow.calendarPreferences.calendarPref, "servers.reloadonlaunch", false );
   
   if( RefreshServers == true )
      this.refreshAllRemoteCalendars( );

}


calendarManager.prototype.addCalendarToListBox = function calMan_addCalendarToListBox( ThisCalendarObject )
{
   var calendarListItem = document.createElement( "listitem" );
   calendarListItem.setAttribute( "id", "calendar-list-item-"+ThisCalendarObject.serverNumber );
   calendarListItem.setAttribute( "onclick", "switchCalendar( event );" );
   calendarListItem.calendarObject = ThisCalendarObject;
   calendarListItem.setAttribute( "label", ThisCalendarObject.name );
   calendarListItem.setAttribute( "flex", "1" );
   
   calendarListItem.setAttribute( "calendarPath", ThisCalendarObject.path );

   calendarListItem.setAttribute( "type", "checkbox" );
   calendarListItem.setAttribute( "checked", ThisCalendarObject.active );
   
   document.getElementById( "list-calendars-listbox" ).appendChild( calendarListItem );
}


calendarManager.prototype.getCalendarIndex = function calMan_getCalendarIndex( ThisCalendarObject )
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i] === ThisCalendarObject )
      {
         return( i );
      }
   }
   return( false );
}


calendarManager.prototype.getSelectedCalendar = function calMan_getSelectedCalendar( )
{
   var calendarListBox = document.getElementById( "list-calendars-listbox" );
   
   return( calendarListBox.selectedItem.calendarObject );
}


calendarManager.prototype.updateServerArrayText = function calMan_updateServerArrayText( )
{
   var ArrayText = "";
   var Seperator = "";
   for ( i = 0; i < this.calendars.length; i++ ) {
      ArrayText += Seperator;
      ArrayText += this.calendars[i].serverNumber;
      Seperator = ",";
   }
   this.CalendarWindow.calendarPreferences.calendarPref.setCharPref( "servers.array", ArrayText );

}

var xmlhttprequest;
var request;
var calendarToGet = null;

calendarManager.prototype.retrieveAndSaveRemoteCalendar = function calMan_retrieveAndSaveRemoteCalendar( ThisCalendarObject, onResponse )
{
   calendarToGet = ThisCalendarObject;
   // make a request
   xmlhttprequest = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
   request =  xmlhttprequest.createInstance( Components.interfaces.nsIXMLHttpRequest );
    
   request.addEventListener( "load", onResponse, false );
   request.addEventListener( "error", onError, false );
   
   request.open( "GET", ThisCalendarObject.remotePath, true );    // true means async
   
   request.send( null );
}


calendarManager.prototype.refreshAllRemoteCalendars = function calMan_refreshAllRemoteCalendars()
{
   for( var i = 0; i < this.calendars.length; i++ )
   {
      if( this.calendars[i].remote == true && this.calendars[i].remotePath != "" )
      {
         this.retrieveAndSaveRemoteCalendar( this.calendars[i], onResponseAndRefresh );
      }
   }
}

calendarManager.prototype.getDefaultServer = function calMan_getDefaultServer()
{
   return( this.calendars[0] );
}


function onResponseAndRefresh( )
{
   //save the stream to a file.
   saveDataToFile( calendarToGet.path, request.responseText, "UTF-8" );
   
   gCalendarWindow.calendarManager.removeCalendar( calendarToGet );

   gCalendarWindow.calendarManager.addCalendar( calendarToGet );

   refreshEventTree( false );

   gCalendarWindow.currentView.refreshEvents();
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
      
   if( event.currentTarget.checked )
      gCalendarWindow.calendarManager.addCalendar( event.currentTarget.calendarObject );
   else
      gCalendarWindow.calendarManager.removeCalendar( event.currentTarget.calendarObject );

   refreshEventTree( false );

   gCalendarWindow.currentView.refreshEvents();
}


function deleteCalendar( )
{
   var calendarObjectToDelete = gCalendarWindow.calendarManager.getSelectedCalendar();

   gCalendarWindow.calendarManager.deleteCalendar( calendarObjectToDelete );

   refreshEventTree( false );

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
