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
 *                 James Maidment <james@mouseboks.org.uk>
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

// TODO move nsIDragService definition
//const nsIDragService = Components.interfaces.nsIDragService;

// function to select whatever what is clicked to start dragging, Mozilla doesn't do this
function calendarViewClick ( aEvent ) {
   if(aEvent.target.className == "day-view-hour-box-class")
      dayViewHourClick( aEvent );

   if(aEvent.target.parentNode.className == "day-view-event-class")
      gCalendarWindow.EventSelection.replaceSelection( aEvent.target.parentNode.calendarEventDisplay.event );


   if(aEvent.target.className == "day-view-event-class")
      gCalendarWindow.EventSelection.replaceSelection( aEvent.target.calendarEventDisplay.event );
     //dayEventItemClick( aEvent.target, aEvent );

   if(aEvent.target.className == "week-view-event-class")
      gCalendarWindow.EventSelection.replaceSelection( aEvent.target.calendarEventDisplay.event );

   if(aEvent.target.className == "week-view-event-label-class")
      gCalendarWindow.EventSelection.replaceSelection( aEvent.target.parentNode.calendarEventDisplay.event );

   //if (gCalendarWindow.currentView == gCalendarWindow.weekView)
     //weekViewHourClick( aEvent );
}

//var startDateIndex;

var calendarViewDNDObserver = {

   getSupportedFlavours : function () {
      var flavourSet = new FlavourSet();
      flavourSet.appendFlavour("text/unicode");
      flavourSet.appendFlavour("text/calendar");
      flavourSet.appendFlavour("text/calendar-interval");
      // flavourSet.appendFlavour("text/x-moz-message-or-folder"); // not implemented
      flavourSet.appendFlavour("application/x-moz-file", "nsIFile");
      return flavourSet;
   },


   onDragStart: function (aEvent, aXferData, aDragAction){

      //Clear any dragged over events left from last drag selection.
      var liveList = document.getElementsByAttribute( "draggedover", "true" );
      // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
      // 'live', so when attribute is deleted the indexes of later elements
      // change, but Moz1.7- is not.  Reversed order works with both.
      for (var i = liveList.length - 1; i >= 0; i--) 
      {
         liveList.item(i).removeAttribute( "draggedover" );
      }

      // select clicked object, Mozilla doesn't do this.
      calendarViewClick( aEvent );

      // aEvent.currentTarget;
      if( aEvent.target.localName == "splitter" || aEvent.target.localName == "menu")
         throw Components.results.NS_OK; // not a draggable item

      var eventIsDragged ;
      switch (aEvent.target.className) {
      case "day-view-event-label-class" :
      case "day-view-event-class" :
	eventIsDragged = true ;
	this.startDragDayIndex = 0 ;
	break;
      case "week-view-event-class" :
	eventIsDragged = true ;
	this.startDragDayIndex = aEvent.target.getAttribute( "dayindex" );
	break;
      case "week-view-event-label-class" :
        eventIsDragged = true ;
	this.startDragDayIndex = aEvent.target.parentNode.getAttribute( "dayindex" );
	break;
      default :
	eventIsDragged = false ;
      }

      if(eventIsDragged == true)
      {
         // We are going to drag an event
         var dragEvents = gCalendarWindow.EventSelection.selectedEvents;
         aXferData.data= new TransferData();
         aXferData.data.addDataForFlavour("text/calendar", eventArrayToICalString( dragEvents ) );
         aXferData.data.addDataForFlavour("text/unicode", eventArrayToICalString( dragEvents, true ) );
         //if (aEvent.ctrlKey) {
 	 //  action.action = nsIDragService.DRAGDROP_ACTION_COPY ;
 	 //}
         aEvent.preventBubble();
      }
      else
      {
         // Dragging on the calendar canvas to select an event period
         // var newDate = gHeaderDateItemArray[dayIndex].getAttribute( "date" );

         //var gStartDate = new Date( gCalendarWindow.getSelectedDate() );
         
         //The date the drap action initiated on.
         var gStartDate;
         
         this.startDateIndex = aEvent.target.getAttribute( "day" );

         //In the week view, the drag can start on a date different from the currently 
         //selected one. However, in the day view, the event target does not have a 'day'
         //attribute.
         if( gCalendarWindow.currentView == gCalendarWindow.weekView )
         {
            //Week view.
            gStartDate = new Date ( gHeaderDateItemArray[this.startDateIndex].getAttribute( "date" ));
         }
         else
         {
            //Day view.
            gStartDate = new Date ( gCalendarWindow.getSelectedDate() );
         }

         gStartDate.setHours( aEvent.target.getAttribute( "hour" ) );
         gStartDate.setMinutes( 0 );
         gStartDate.setSeconds( 0 );

         aXferData.data=new TransferData();
         aXferData.data.addDataForFlavour("text/calendar-interval", gStartDate.getTime() );

         // aDragAction.action = nsIDragService.DRAGDROP_ACTION_MOVE;
         aEvent.preventBubble();
      }
   },


   onDragOver: function calendarViewOnDragOver(aEvent, aFlavour, aDragSession)
   {
      if (aDragSession.isDataFlavorSupported("text/calendar-interval")) 
      {
         // multiday events not supported. In week view, do not allow more than one day in selection
         if (gCalendarWindow.currentView == gCalendarWindow.weekView )
         {
            if (aEvent.target.getAttribute( "day" ) != this.startDateIndex)
            {
               aDragSession.canDrop = false;
               return false;
            }
         }

         // if (aDragSession.isDataFlavorSupported("text/calendar-interval"))
         aEvent.target.setAttribute( "draggedover", "true" );
      }
      return true;
   },


   onDrop: function (aEvent, aXferData, aDragSession)
   {
      var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
      trans.addDataFlavor("text/calendar");
      trans.addDataFlavor("text/calendar-interval");
      //trans.addDataFlavor("text/x-moz-message-or-folder"); // not implemented
      //trans.addDataFlavor("text/x-moz-url");               // not implemented
      trans.addDataFlavor("application/x-moz-file");
      trans.addDataFlavor("text/unicode");

      aDragSession.getData (trans, i);

      var dataObj = new Object();
      var bestFlavor = new Object();
      var len = new Object();
      trans.getAnyTransferData(bestFlavor, dataObj, len);

      switch (bestFlavor.value)
      {
      case "text/calendar": // A calendar object is dropped. ical text data.
         if (dataObj)
            // XXX For MOZILLA10 1.0 this is nsISupportWString
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);

         var dropEvent = createEvent();
         var newEventId = dropEvent.id; // can be used when copying to new event

         // XXX TODO: Check if there is a function in calendarImportExport to do this
         icalStr = dataObj.data.substring(0, len.value);
         i = icalStr.indexOf("BEGIN:VEVENT");
         j = icalStr.indexOf("END:VEVENT") + 10;
         eventData = icalStr.substring(i, j);
         dropEvent.parseIcalString( eventData );

         // calculate new start/end time for droplocation
         // use the minutes from the event, and change the hour to the hour dropped in
         var gDropzoneStartTime = new Date( dropEvent.start.getTime() );
	 
 	 if(aEvent.target.getAttribute( "hour" ))
	   {
         gDropzoneStartTime.setHours( aEvent.target.getAttribute( "hour" ) );
	   }
	 else
	   {
	     gDropzoneStartTime.setHours( aEvent.target.parentNode.calendarEventDisplay.event.start.hour );
	   }
	 if(aEvent.target.getAttribute( "day" )) 
	   { //We are is the week view, so we check the drop day
	     var theNewDate = gDropzoneStartTime.getDate() - this.startDragDayIndex + parseInt(aEvent.target.getAttribute( "day" ));
	     gDropzoneStartTime.setDate(theNewDate);
	   }

         var draggedTime = gDropzoneStartTime - dropEvent.start ;
         var eventDuration = dropEvent.end.getTime() - dropEvent.start.getTime();

         if(aDragSession.dragAction == nsIDragService.DRAGDROP_ACTION_MOVE)
         {
            var calendarEvent = gICalLib.fetchEvent( dropEvent.id );
	       
	         if( calendarEvent != null )
	         {
               calendarEvent.start.setTime(gDropzoneStartTime.getTime() );
               calendarEvent.end.setTime ( calendarEvent.start.getTime() + eventDuration );

               // LINAGORA: Needed to update remote calendar
               modifyEventDialogResponse( calendarEvent, calendarEvent.parent.server );
               // gICalLib.modifyEvent( calendarEvent );
            } 
            else
               alert(" Event with id: " + dropEvent.id + " not found");
         }
         else
         {
            // create a copy of the dragged event on the droplocation
            dropEvent.id = newEventId;
            // XXX TODO Check if stamp/DTSTAMP is correclty set to current time
            dropEvent.start.setTime(gDropzoneStartTime.getTime() );
            dropEvent.end.setTime ( dropEvent.start.getTime() + eventDuration );

            editNewEvent( dropEvent );
         }
	 break;

      case "text/calendar-interval": // A start time is dragged as start of an event duration
         // no copy for interval drag
         // if (dragSession.dragAction == nsIDragService.DRAGDROP_ACTION_COPY)
         //    return false;
         if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
         var dragTime = dataObj.data.substring(0, len.value);

         gStartDate = new Date(  );
         gStartDate.setTime( dragTime );
         gEndDate = new Date( gStartDate.getTime() );
         gEndDate.setHours( aEvent.target.getAttribute( "hour" ) );
         if( gEndDate.getTime() < gStartDate.getTime() )
         {
            var Temp = gEndDate;
            gEndDate = gStartDate;
            gStartDate = Temp;
         }
         newEvent( gStartDate, gEndDate );
	 break;

      case "text/x-moz-url":
         var url = transferUtils.retrieveURLFromData(aXferData.data, aXferData.flavour.contentType);
         if (!url)
             return;
          alert(url);
      
         break;

      case "text/unicode": // text, create new event with text as description
         if (dataObj)
            dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);

         // calculate star and and time's for new event, default event lenght is 1 hour
         var gStartDate = new Date( gCalendarWindow.getSelectedDate() );
         gStartDate.setHours( aEvent.target.getAttribute( "hour" ) );
         gStartDate.setMinutes( 0 );
         gStartDate.setSeconds( 0 );
         gEndDate = new Date( gStartDate.getTime() );
         gEndDate.setHours(gStartDate.getHours() + 1);

         newEvent = createEvent();
         newEvent.start.setTime( gStartDate );
         newEvent.end.setTime( gEndDate );
         newEvent.description = dataObj.data.substring(0, len.value);
         editNewEvent( newEvent );
         break;

      case "text/x-moz-message-or-folder": // Mozilla mail, work in progress
         try {
            if (dataObj)
               dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
            // pull the URL out of the data object

            // sourceDocument nul if from other app

            var sourceUri = dataObj.data.substring(0, len.value);

            if (! sourceUri)
               break;

            alert("import not implemented " + sourceUri);

            /*
            try
            {
                sourceResource = RDF.GetResource(sourceUri, true);
                var folder = sourceResource.QueryInterface(Components.interfaces.nsIFolder);
                if (folder)
                    dragFolder = true;
            }
            catch(ex)
            {
                sourceResource = null;
                var isServer = GetFolderAttribute(folderTree, targetResource, "IsServer");
                if (isServer == "true")
                {
                    debugDump("***isServer == true\n");
                    return false;
                }
                // canFileMessages checks no select, and acl, for imap.
                var canFileMessages = GetFolderAttribute(folderTree, targetResource, "CanFileMessages");
                if (canFileMessages != "true")
                {
                    debugDump("***canFileMessages == false\n");
                    return false;
                }

                var hdr = messenger.messageServiceFromURI(sourceUri).messageURIToMsgHdr(sourceUri);
                alert(hdr);
                if (hdr.folder == targetFolder)
                    return false;
                break;
            }
         */
                  } 
         catch(ex) {
            alert(ex.message);
         }
	 break;

      case "application/x-moz-file": // file from OS, we expect it to be iCalendar data
         try {
            var fileObj = dataObj.value.QueryInterface(Components.interfaces.nsIFile);
            var aDataStream = readDataFromFile( fileObj.path );
            var calendarEventArray = parseIcalData( aDataStream );
            // LINAGORA (- TODO Move to calendarImportExport to have the option to turn off dialogs)
            addEventsToCalendar( calendarEventArray, 1 );
         }
         catch(ex) {
            alert(ex.message);
         }
	 break;
      }
      
      // cleanup
      var liveList = document.getElementsByAttribute( "draggedover", "true" );
      // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
      // 'live', so when attribute is deleted the indexes of later elements
      // change, but Moz1.7- is not.  Reversed order works with both.
      for (var i = liveList.length - 1; i >= 0; i--)
      {
         liveList.item(i).removeAttribute( "draggedover" );
      }
  },


  onDragExit: function (aEvent, aDragSession)
  {
     // nothing, doesn't fire for cancel? needed for interval-drag cleanup
  }


};
