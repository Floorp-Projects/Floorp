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
 * Contributor(s): Garth Smedley <garths@oeone.com>
 *                 Mike Potter <mikep@oeone.com>
 *                 Chris Charabaruk <coldacid@meldstar.com>
 *                 Colin Phillips <colinp@oeone.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Eric Belhaire <eric.belhaire@ief.u-psud.fr>
 *                 Matthew Willis <mattwillis@gmail.com>
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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

/*-----------------------------------------------------------------
*   U N I F I N D E R 
*
*   This is a hacked in interface to the unifinder. We will need to 
*   improve this to make it usable in general.
*/
const UnifinderTreeName = "unifinder-search-results-listbox";
const kEventStatusOrder = ["TENTATIVE", "CONFIRMED", "CANCELLED"];

var gCalendarEventTreeClicked = false; //set this to true when the calendar event tree is clicked
                                       //to allow for multiple selection

var gEventArray = new Array();

var kDefaultTimezone;

function resetAllowSelection()
{
   /* 
   Do not change anything in the following lines, they are needed as described in the 
   selection observer above 
   */
   doingSelection = false;

   var SearchTree = document.getElementById( UnifinderTreeName );
   SearchTree.view.selection.selectEventsSuppressed = false;
   SearchTree.addEventListener( "select", unifinderOnSelect, true );
}  

var doingSelection = false;

function selectSelectedEventsInTree( EventsToSelect )
{
   if( doingSelection === true )
      return;

   doingSelection = true;

   if( EventsToSelect === false )
      EventsToSelect = gCalendarWindow.EventSelection.selectedEvents;
   var SearchTree = document.getElementById( UnifinderTreeName );
      
   /* The following is a brutal hack, caused by 
   http://lxr.mozilla.org/mozilla1.0/source/layout/xul/base/src/tree/src/nsTreeSelection.cpp#555
   and described in bug 168211
   http://bugzilla.mozilla.org/show_bug.cgi?id=168211
   Do NOT remove anything in the next 3 lines, or the selection in the tree will not work.
   */
   SearchTree.onselect = null;
   SearchTree.removeEventListener( "select", unifinderOnSelect, true );
   SearchTree.view.selection.selectEventsSuppressed = true;

   if( EventsToSelect.length == 1 )
   {
      var RowToScrollTo = SearchTree.eventView.getRowOfCalendarEvent( EventsToSelect[0] );
         
      if( RowToScrollTo != "null" )
      {
         SearchTree.view.selection.clearSelection( );
   
         SearchTree.treeBoxObject.ensureRowIsVisible( RowToScrollTo );

         SearchTree.view.selection.timedSelect( RowToScrollTo, 1 );
      }
      else
      {
         SearchTree.view.selection.clearSelection( );
      }
   }
   else if( EventsToSelect.length > 1 )
   {
      SearchTree.view.selection.clearSelection( );
      for (var i in EventsToSelect) {
         var row = SearchTree.eventView.getRowOfCalendarEvent(EventsToSelect[i]);
         SearchTree.view.selection.rangedSelect(row,row,true);
      }
   }
   else
   {
      SearchTree.view.selection.clearSelection( );
   }
   
   /* This needs to be in a setTimeout */
   setTimeout( "resetAllowSelection()", 1 );
}

/**
*   Observer for the calendar event data source. This keeps the unifinder
*   display up to date when the calendar event data is changed
*/

var unifinderObserver = {
    mInBatch: false,

    QueryInterface: function (aIID) {
        if (!aIID.equals(Components.interfaces.nsISupports) &&
            !aIID.equals(Components.interfaces.calICompositeObserver) &&
            !aIID.equals(Components.interfaces.calIObserver))
        {
            throw Components.results.NS_ERROR_NO_INTERFACE;
        }

        return this;
    },

    onStartBatch: function() {
        this.mInBatch = true;
    },
    onEndBatch: function() {
        this.mInBatch = false;
        refreshEventTree();
    },
    onLoad: function() {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onAddItem: function(aItem) {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onModifyItem: function(aNewItem, aOldItem) {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onDeleteItem: function(aDeletedItem) {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onAlarm: function(aAlarmItem) {},
    onError: function(aMessage) {},

    onCalendarAdded: function(aDeletedItem) {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onCalendarRemoved: function(aDeletedItem) {
        if (!this.mInBatch)
            refreshEventTree();
    },
    onDefaultCalendarChanged: function(aNewDefaultCalendar) {}
};


/**
*   Called when the calendar is loaded
*/

function prepareCalendarUnifinder( )
{
   // tell the unifinder to get ready
   var unifinderEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         selectSelectedEventsInTree( EventSelectionArray );
      }
   }
      
   gCalendarWindow.EventSelection.addObserver( unifinderEventSelectionObserver );
   
   // set up our calendar event observer
   
   var ccalendar = getDisplayComposite();
   ccalendar.addObserver(unifinderObserver);

   kDefaultTimezone = calendarDefaultTimezone();

   refreshEventTree(); //Display something upon first load. onLoad doesn't work properly for observers
}

/**
*   Called when the calendar is unloaded
*/

function finishCalendarUnifinder( )
{
   var ccalendar = getDisplayComposite();
   ccalendar.removeObserver(unifinderObserver);
}

/**
*   Helper function to display event datetimes in the unifinder
*/

function formatUnifinderEventDateTime( datetime, isAllDay )
{
  var d = datetime.getInTimezone(kDefaultTimezone).jsDate;
  return gCalendarWindow.dateFormater.formatDateTime( d, true, datetime.isDate );
}




/**
*  This is attached to the ondblclik attribute of the events shown in the unifinder
*/

function unifinderDoubleClickEvent( event )
{
   // we only care about button 0 (left click) events
   if (event.button != 0) return;
        
   // find event by id
   
   var calendarEvent = getCalendarEventFromEvent( event );
   
   if( calendarEvent != null )
      editEvent( calendarEvent.parentItem );
   else
      newEvent();
}


function getCalendarEventFromEvent( event )
{
   var tree = document.getElementById( UnifinderTreeName );

   var row = tree.treeBoxObject.getRowAt( event.clientX, event.clientY );

   if( row != -1 && row < tree.view.rowCount )
   { 
      return ( tree.eventView.getCalendarEventAtRow( row ) );
   } else {
      return ( null );
   }
}

/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderOnSelect( event )
{
   if( event.target.view.selection.getRangeCount() == 0 )
       return;

   var ArrayOfEvents = new Array( );
   
   gCalendarEventTreeClicked = true;
   
   var calendarEvent;

   //get the selected events from the tree
   var tree = document.getElementById( UnifinderTreeName );
   var start = new Object();
   var end = new Object();
   var numRanges = tree.view.selection.getRangeCount();
   
   for (var t=0; t<numRanges; t++){
      tree.view.selection.getRangeAt(t,start,end);
      
      for (var v=start.value; v<=end.value; v++){
         try {
            calendarEvent = tree.eventView.getCalendarEventAtRow( v );
         }
         catch( e )
         {
            dump( "e is "+e+"\n" );
            return;
         }
         ArrayOfEvents.push( calendarEvent );
      }
   }
   
   if( ArrayOfEvents.length == 1 )
   {
      /*start date is either the next or last occurence, or the start date of the event */
      var eventStartDate = calendarEvent.startDate.jsDate;
      
      /* you need this in case the current day is not visible. */
      if( gCalendarWindow.currentView.getVisibleEvent( calendarEvent ) == false )
         gCalendarWindow.currentView.goToDay( eventStartDate, true);
   }
   
   gCalendarWindow.EventSelection.setArrayToSelection( ArrayOfEvents );
}

function unifinderToDoHasFocus()
{
   return( document.getElementById( ToDoUnifinderTreeName ).treeBoxObject.focused );
}



/**
*  This is called from the unifinder when a key is pressed in the search field
*/
var gSearchTimeout = null;

function searchKeyPress( searchTextItem, event )
{
    // 13 == return
    if (event && event.keyCode == 13) {
        clearSearchTimer();
        refreshEventTree();
        return;
    }
    
    // always clear the old one first
    clearSearchTimer();
   
    // make a new timer
    gSearchTimeout = setTimeout( "refreshEventTree()", 400 );
}

function clearSearchTimer( )
{
   if( gSearchTimeout )
   {
      clearTimeout( gSearchTimeout );
      gSearchTimeout = null;
   }
}

/*
** This function returns the event table.
*/

function changeEventFilter( event )
{
   refreshEventTree();

   /* The following isn't exactly right. It should actually reload after the next event happens. */

   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   setTimeout( "refreshEventTree()", milliSecsTillTomorrow );
}

/**
*  Redraw the categories unifinder tree
*/
var treeView =
{
   rowCount : gEventArray.length,
   selectedColumn : null,
   sortDirection : null,
   sortStartedTime : new Date().getTime(), // updated just before sort
   outParameter : new Object(),  // used to obtain dates during sort

   isContainer : function(){return false;},
   getCellProperties : function(){return false;},
   getColumnProperties : function(){return false;},
   getRowProperties : function(){return false;},
   isSorted : function(){return false;},
   isEditable : function(){return true;},
   isSeparator : function(){return false;},
   getImageSrc : function(){return false;},
   cycleHeader : function(col, element) // element parameter used in Moz1.7-
   {                                    // not in Moz1.8+
      var sortActive;
      var treeCols;
   
      // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
      this.selectedColumn = (typeof(col)=="object" ? col.id : col);  
      if (!element) element = col.element;  // in Moz1.8+, get element from col
      sortActive = element.getAttribute("sortActive");
      this.sortDirection = element.getAttribute("sortDirection");
   
      if (sortActive != "true")
      {
         treeCols = document.getElementsByTagName("treecol");
         for (var i = 0; i < treeCols.length; i++)
         {
            treeCols[i].removeAttribute("sortActive");
            treeCols[i].removeAttribute("sortDirection");
         }
         sortActive = true;
         this.sortDirection = "ascending";
      }
      else
      {
         sortActive = true;
         if (this.sortDirection == "" || this.sortDirection == "descending")
         {
            this.sortDirection = "ascending";
         }
         else
         {
            this.sortDirection = "descending";
         }
      }
      element.setAttribute("sortActive", sortActive);
      element.setAttribute("sortDirection", this.sortDirection);
      this.sortStartedTime = new Date().getTime(); // for null/0 dates in sort
      gEventArray.sort( compareEvents );

      document.getElementById( UnifinderTreeName ).view = this;
   },
   setTree : function( tree ){this.tree = tree;},
   getCellText : function(row,column)
   {
      calendarEvent = gEventArray[row];

      // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
      switch( typeof(column)=="object" ? column.id : column )
      {
         case "unifinder-search-results-tree-col-title":
            return( calendarEvent.title );
         
         case "unifinder-search-results-tree-col-startdate":
            return formatUnifinderEventDateTime(calendarEvent.startDate);
         
         case "unifinder-search-results-tree-col-enddate":
            var eventEndDate = calendarEvent.endDate.clone();
            // XXX reimplement
            //var eventEndDate = getCurrentNextOrPreviousRecurrence( calendarEvent );
            if (calendarEvent.startDate.isDate) // display enddate is ical enddate - 1
               eventEndDate.day = eventEndDate.day - 1;
            return formatUnifinderEventDateTime(eventEndDate);         

         case "unifinder-search-results-tree-col-categories":
            return( calendarEvent.getProperty("CATEGORIES") );
         
         case "unifinder-search-results-tree-col-location":
            return( calendarEvent.getProperty("LOCATION") );
         
         case "unifinder-search-results-tree-col-status":
            return getEventStatusString(calendarEvent);

        case "unifinder-search-results-tree-col-calendarname":
          return calendarEvent.calendar.name;

         default: 
            return false;
      }
   }
}

function compareEvents( eventA, eventB )
{
   var modifier = 1;
   if (treeView.sortDirection == "descending")
	{
		modifier = -1;
	}
   
   switch(treeView.selectedColumn)
   {
      case "unifinder-search-results-tree-col-title":
         return compareString(eventA.title,  eventB.title) * modifier;
      
      case "unifinder-search-results-tree-col-startdate":
        var msNextStartA = msNextOrPreviousRecurrenceStart(eventA);
        var msNextStartB = msNextOrPreviousRecurrenceStart(eventB);
        return compareMSTime(msNextStartA, msNextStartB) * modifier;
   
      case "unifinder-search-results-tree-col-enddate":
        var msNextEndA = msNextOrPreviousRecurrenceEnd(eventA);
        var msNextEndB = msNextOrPreviousRecurrenceEnd(eventB);
        return compareMSTime(msNextEndA, msNextEndB) * modifier;
   
      case "unifinder-search-results-tree-col-categories":
         return compareString(eventA.getProperty("CATEGORIES"), 
                              eventB.getProperty("CATEGORIES")) * modifier;
   
      case "unifinder-search-results-tree-col-location":
        return compareString(eventA.getProperty("LOCATION"),
                             eventB.getProperty("LOCATION")) * modifier;
   
      case "unifinder-search-results-tree-col-status":
        return compareNumber(kEventStatusOrder.indexOf(eventA.status),
                             kEventStatusOrder.indexOf(eventB.status)) * modifier;

      case "unifinder-search-results-tree-col-calendarname":
        return compareString(eventA.calendar.name, eventB.calendar.name) * modifier;

      default:
         return 0;
   }
}

function compareString(a, b) {
  a = nullToEmpty(a);
  b = nullToEmpty(b);
  return ((a < b) ? -1 :
          (a > b) ?  1 : 0);
}

function nullToEmpty(value) {
  return value == null? "" : value;
}

function compareMSTime(a, b) {
  return ((a < b) ? -1 :
          (a > b) ?  1 : 0);
}

function msNextOrPreviousRecurrenceStart( calendarEvent ) {
  return calendarEvent.startDate.nativeTime;
  // XXX reimplement the following
  if (calendarEvent.recur && calendarEvent.start) {
    treeView.outParameter.value = null; // avoid creating objects during sort
    if (calendarEvent.getNextRecurrence(treeView.sortStartedTime,
                                         treeView.outParameter) ||
        calendarEvent.getPreviousOccurrence(treeView.sortStartedTime,
                                            treeView.outParameter))
      return treeView.outParameter.value;
  } 
  return dateToMilliseconds(calendarEvent.start);
}

function msNextOrPreviousRecurrenceEnd(event) {
  return event.endDate.nativeTime;
  //XXX reimplement the following
  var msNextStart = msNextOrPreviousRecurrenceStart(event);
  var msDuration=dateToMilliseconds(event.endDate)-dateToMilliseconds(event.startDate);
  return msNextStart + msDuration;
}

function dateToMilliseconds(date) {
  // Treat null/0 as 'now' when sort started, so incomplete tasks stay current.
  // Time is computed once per sort (just before sort) so sort is stable.
  if (date == null)
    return treeView.sortStartedTime;
  var ms = date.getTime();   // note: date is not a javascript date.
  if (ms == -62171262000000) // ms value for (0000/00/00 00:00:00)
    return treeView.sortStartedTime;
  return ms;
}


function calendarEventView( eventArray )
{
   this.eventArray = eventArray;   
}

calendarEventView.prototype.getCalendarEventAtRow = function( i )
{
   return( this.eventArray[ i ] );
}

calendarEventView.prototype.getRowOfCalendarEvent = function( Event )
{
   for( var i = 0; i < this.eventArray.length; i++ )
   {
      if( this.eventArray[i].id == Event.id )
         return( i );
   }
   return( "null" );
}


function refreshEventTree( eventArray )
{
   var savedThis = this;
   var refreshListener = {
       mEventArray: new Array(),

       onOperationComplete: function (aCalendar, aStatus, aOperationType, aId, aDateTime) {
           setTimeout(function () { refreshEventTreeInternal(refreshListener.mEventArray); }, 0);
       },

       onGetResult: function (aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
           for (var i = 0; i < aCount; i++) {
               refreshListener.mEventArray.push(aItems[i]);
           }
       }
   };

   var Today = new Date();
   //do this to allow all day events to show up all day long
   var StartDate = new Date( Today.getFullYear(), Today.getMonth(), Today.getDate(), 0, 0, 0 );
   var EndDate;

   var ccalendar = getDisplayComposite();
   var filter = 0;

   filter |= ccalendar.ITEM_FILTER_TYPE_EVENT;

   switch( document.getElementById( "event-filter-menulist" ).selectedItem.value )
   {
      case "all":
         StartDate = null;
         EndDate = null;
         break;
         
      case "today":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1 );
         break;
         
      case "next7Days":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 8 ) );
         break;
         
      case "next14Days":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 15 ) );
         break;
         
      case "next31Days":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 32 ) );
         break;
         
      case "thisCalendarMonth":
         // midnight on first day of this month
         var startOfMonth = new Date( Today.getFullYear(), Today.getMonth(), 1, 0, 0, 0 );
         // midnight on first day of next month
         var startOfNextMonth = new Date( Today.getFullYear(), (Today.getMonth() + 1), 1, 0, 0, 0 );
         // 23:59:59 on last day of this month
         EndDate = new Date( startOfNextMonth.getTime() - 1000 );
         StartDate = startOfMonth;
         break;

      case "future":
         EndDate = null;
         break;

      case "current":
         var SelectedDate = gCalendarWindow.getSelectedDate();
         StartDate = new Date( SelectedDate.getFullYear(), SelectedDate.getMonth(), SelectedDate.getDate(), 0, 0, 0 );
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1000 );
         break;
      
      default: 
         dump( "there's no case for "+document.getElementById( "event-filter-menulist" ).selectedItem.value+"\n" );
         EndDate = StartDate;
         break;
   }
   var s = StartDate ? jsDateToDateTime(StartDate).getInTimezone(calendarDefaultTimezone()) : null;
   var e = EndDate ? jsDateToDateTime(EndDate).getInTimezone(calendarDefaultTimezone()) : null;
   if (StartDate && EndDate) {
       filter |= ccalendar.ITEM_FILTER_CLASS_OCCURRENCES;
   }
   ccalendar.getItems (filter, 0, s, e, refreshListener);

}

function refreshEventTreeInternal(eventArray)
{
   var searchText = document.getElementById( "unifinder-search-field" ).value;

   // XXX match for strings with only whitespace. Skip those too
   if (searchText.length) {
       gEventArray = new Array();
       var fieldsToSearch = ["DESCRIPTION", "LOCATION", "CATEGORIES", "URL"];

       for (var j in eventArray) {
           var event = eventArray[j];
           if (event.title && 
               event.title.toLowerCase().indexOf(searchText) != -1 ) {
               gEventArray.push(event);
           } else { 
               for (var k in fieldsToSearch) {
                   var val = event.getProperty(fieldsToSearch[k]);
                   if (val && val.toLowerCase().indexOf(searchText) != -1 ) {
                       gEventArray.push(event);
                       break;
                   }
               }
           }
       }
   } else {
       gEventArray = eventArray;
   }

   treeView.rowCount = gEventArray.length;
      
   var ArrayOfTreeCols = document.getElementById( UnifinderTreeName ).getElementsByTagName( "treecol" );
   for( var i = 0; i < ArrayOfTreeCols.length; i++ )
   {
      if( ArrayOfTreeCols[i].getAttribute( "sortActive" ) == "true" )
      {
         treeView.selectedColumn = ArrayOfTreeCols[i].getAttribute( "id" );
         treeView.sortDirection = ArrayOfTreeCols[i].getAttribute("sortDirection");
         treeView.sortStartedTime = new Date().getTime(); //for null/0 dates
         gEventArray.sort(compareEvents);
         break;
      }
   }

   document.getElementById( UnifinderTreeName ).view = treeView;

   document.getElementById( UnifinderTreeName ).eventView = new calendarEventView( gEventArray );

   //select selected events in the tree.
   selectSelectedEventsInTree( false );
}

function focusFirstItemIfNoSelection()
{
   if( gCalendarWindow.EventSelection.selectedEvents.length == 0 )
   {
      //select the first event in the list.
      var ListBox = document.getElementById( UnifinderTreeName );

      if( ListBox.childNodes.length > 0 )
      {
         var SelectedEvent = ListBox.childNodes[0].event;

         var ArrayOfEvents = new Array();
   
         ArrayOfEvents[ ArrayOfEvents.length ] = SelectedEvent;
         
         gCalendarWindow.EventSelection.setArrayToSelection( ArrayOfEvents );
      
         /*start date is either the next or last occurence, or the start date of the event */
         var eventStartDate = getCurrentNextOrPreviousRecurrence( SelectedEvent );
            
         /* you need this in case the current day is not visible. */
         gCalendarWindow.currentView.goToDay( eventStartDate, true);
      }
   }
}

