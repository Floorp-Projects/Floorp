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
 *						 Colin Phillips <colinp@oeone.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Eric Belhaire <eric.belhaire@ief.u-psud.fr>
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

var gCalendarEventTreeClicked = false; //set this to true when the calendar event tree is clicked
                                       //to allow for multiple selection

var gEventArray = new Array();

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

   dump( "\nCALENDAR unifinder.js->on selection changed" );
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
      /* selecting all events is taken care of in the selectAllEvents in calendar.js 
      ** Other than that, there's no other way to get in here. */
      if( gSelectAll === true )
      {
         SearchTree.view.selection.selectAll( );
         
         gSelectAll = false;
      }   
   }
   else
   {
      dump( "\n--->>>>unifinder.js selection callback :: Clear selection" );
      SearchTree.view.selection.clearSelection( );
   }
   
   /* This needs to be in a setTimeout */
   setTimeout( "resetAllowSelection()", 1 );
}

/**
*   Observer for the calendar event data source. This keeps the unifinder
*   display up to date when the calendar event data is changed
*/

var unifinderEventDataSourceObserver =
{
   onLoad   : function()
   {
        if( !gICalLib.batchMode )
        {
           refreshEventTree( getAndSetEventTable() );
        }
   },
   
   onStartBatch   : function()
   {
   },
    
   onEndBatch   : function()
   {
        refreshEventTree( getAndSetEventTable() );
   },
    
   onAddItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
            if( calendarEvent )
            {
               refreshEventTree( getAndSetEventTable() );
            }
        }
   },

   onModifyItem : function( calendarEvent, originalEvent )
   {
        if( !gICalLib.batchMode )
        {
            refreshEventTree( getAndSetEventTable() );
        }
   },

   onDeleteItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
           refreshEventTree( getAndSetEventTable() );
        }
   },

   onAlarm : function( calendarEvent )
   {
      
   },
   onError : function()
   {
   }
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
   gICalLib.addObserver( unifinderEventDataSourceObserver );
}

/**
*   Called when the calendar is unloaded
*/

function finishCalendarUnifinder( )
{
   gICalLib.removeObserver( unifinderEventDataSourceObserver  );
}


/**
*   Helper function to display event dates in the unifinder
*/

function formatUnifinderEventDate( date )
{
   return( gCalendarWindow.dateFormater.getFormatedDate( date ) );
}


/**
*   Helper function to display event times in the unifinder
*/

function formatUnifinderEventTime( time )
{
   return( gCalendarWindow.dateFormater.getFormatedTime( time ) );
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
      editEvent( calendarEvent );
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
   dump( "\n\nin unifinder onselect\n" );

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
            dump( "e is "+e );
            return;
         }
         ArrayOfEvents.push( calendarEvent );
      }
   }
   
   if( ArrayOfEvents.length == 1 )
   {
      /*start date is either the next or last occurence, or the start date of the event */
      var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
      
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
   if (event && event.keyCode == 13) 
   {
     clearSearchTimer();
     doSearch();
     return;
   }
    
    // always clear the old one first
    
   clearSearchTimer();
   
   // make a new timer
   
   gSearchTimeout = setTimeout( "doSearch()", 400 );
}

function clearSearchTimer( )
{
   if( gSearchTimeout )
   {
      clearTimeout( gSearchTimeout );
      gSearchTimeout = null;
   }
}

function doSearch( )
{
   var eventTable = new Array();

   var searchText = document.getElementById( "unifinder-search-field" ).value;
   
   if ( searchText.length <= 0 ) 
   {
      eventTable = gEventSource.currentEvents;
   }
   else if ( searchText == " " ) 
   {
      searchText = "";
      document.getElementById( "unifinder-search-field" ).value = '';
      return;
   }
   else
   {
      var FieldsToSearch = new Array( "title", "description", "location", "categories" );
      eventTable = gEventSource.search( searchText, FieldsToSearch );
   }
   
   if( document.getElementById( "erase_command" ) )
   {
      if( searchText.length <= 0 )
         document.getElementById( "erase_command" ).setAttribute( "disabled", "true" );
      else
         document.getElementById( "erase_command" ).removeAttribute( "disabled" );
   }
   refreshEventTree( eventTable );
}


/*
** This function returns the event table. The event table also gets set in the gEventSource
*/

function getAndSetEventTable( )
{
   var Today = new Date();
   //do this to allow all day events to show up all day long
   var StartDate = new Date( Today.getFullYear(), Today.getMonth(), Today.getDate(), 0, 0, 0 );
   var EndDate;

   switch( document.getElementById( "event-filter-menulist" ).selectedItem.value )
   {
      case "all":
         return( gEventSource.getAllEvents() );
         
      case "today":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1 );
         return( gEventSource.getEventsForRange( StartDate, EndDate ) );
         
      case "week":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 8 ) );
         return( gEventSource.getEventsForRange( StartDate, EndDate ) );
         
      case "2weeks":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 15 ) );
         return( gEventSource.getEventsForRange( StartDate, EndDate ) );
         
      case "month":
         EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 32 ) );
         return( gEventSource.getEventsForRange( StartDate, EndDate ) );
         
      case "future":
         return( gEventSource.getAllFutureEvents() );
      
      case "current":
         var SelectedDate = gCalendarWindow.getSelectedDate();
         MidnightSelectedDate = new Date( SelectedDate.getFullYear(), SelectedDate.getMonth(), SelectedDate.getDate(), 0, 0, 0 );
         EndDate = new Date( MidnightSelectedDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1000 );
         return( gEventSource.getEventsForRange( MidnightSelectedDate, EndDate ) );
      
      default: 
         dump( "there's no case for "+document.getElementById( "event-filter-menulist" ).selectedItem.value );
         return( eventTable = new Array() );
   }
}

function changeEventFilter( event )
{
   getAndSetEventTable()
   
   doSearch();

   /* The following isn't exactly right. It should actually reload after the next event happens. */

   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   setTimeout( "refreshEventTree( getAndSetEventTable() )", milliSecsTillTomorrow );
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
      //dump( "\nin cycle header" );
      var sortActive;
      var treeCols;
   
      // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
      this.selectedColumn = col.id || col;  
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
      switch( column.id || column )
      {
         case "unifinder-search-results-tree-col-title":
            return( calendarEvent.title );
         
         case "unifinder-search-results-tree-col-startdate":
            var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
            var startTime = formatUnifinderEventTime( eventStartDate );
            var startDate = formatUnifinderEventDate( eventStartDate );
            if( calendarEvent.allDay )
            {
	      return(gCalendarBundle.getFormattedString("unifinderAlldayEventDate", [startDate]));
            }
            else
            {
	      return(gCalendarBundle.getFormattedString("unifinderNormalEventDate", [startDate, startTime]));
            }
         
         case "unifinder-search-results-tree-col-enddate":
            var eventEndDate = getNextOrPreviousRecurrence( calendarEvent );
            var eventLength = calendarEvent.end.getTime() - calendarEvent.start.getTime();
            var actualEndDate = eventEndDate.getTime() + eventLength;
            var endDate, endTime;
            eventEndDate = new Date( actualEndDate );
            if( calendarEvent.allDay ) {
               //user-enddate is ical-enddate - 1
               eventEndDate.setDate( eventEndDate.getDate() - 1 );
               endDate = formatUnifinderEventDate( eventEndDate );
               return(gCalendarBundle.getFormattedString("unifinderAlldayEventDate", [endDate]));
            } else {
               endTime = formatUnifinderEventTime( eventEndDate );
               endDate = formatUnifinderEventDate( eventEndDate );
               return(gCalendarBundle.getFormattedString("unifinderNormalEventDate", [endDate, endTime]));
            }
         
         case "unifinder-search-results-tree-col-categories":
            return( calendarEvent.categories );
         
         case "unifinder-search-results-tree-col-location":
            return( calendarEvent.location );
         
         case "unifinder-search-results-tree-col-status":
            switch( calendarEvent.status )
            {
               case calendarEvent.ICAL_STATUS_TENTATIVE:
                  return( gCalendarBundle.getString( "Tentative" ) );
               case calendarEvent.ICAL_STATUS_CONFIRMED:
                  return( gCalendarBundle.getString( "Confirmed" ) );
               case calendarEvent.ICAL_STATUS_CANCELLED:
                  return( gCalendarBundle.getString( "Cancelled" ) );
               default: 
                  return false;
            }
            return false;

        case "unifinder-search-results-tree-col-filename":
            var thisCalendar = gCalendarWindow.calendarManager.getCalendarByName( calendarEvent.parent.server );
            if( thisCalendar )
                return( thisCalendar.getAttribute( "http://home.netscape.com/NC-rdf#name" ) );
            else
                return( "" );

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
         return compareString(eventA.categories, eventB.categories) * modifier;
   
      case "unifinder-search-results-tree-col-location":
         return compareString(eventA.location, eventB.location) * modifier;
   
      case "unifinder-search-results-tree-col-status":
         return compareString(eventA.status, eventB.status) * modifier;

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
  return ((a < b) ? -1 :      // avoid underflow problems of subtraction
          (a > b) ?  1 : 0); 
}

function msNextOrPreviousRecurrenceStart( calendarEvent ) {
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
  var msNextStart = msNextOrPreviousRecurrenceStart(event);
  var msDuration=dateToMilliseconds(event.end)-dateToMilliseconds(event.start);
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
   gEventArray = eventArray;

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

   document.getElementById( UnifinderTreeName ).eventView = new calendarEventView( eventArray );

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
         var eventStartDate = getNextOrPreviousRecurrence( SelectedEvent );
            
         /* you need this in case the current day is not visible. */
         gCalendarWindow.currentView.goToDay( eventStartDate, true);
      }
   }
}


function getNextOrPreviousRecurrence( calendarEvent )
{
   var isValid;

   if( calendarEvent.recur )
   {
      var now = new Date();

      var result = new Object();

      isValid = calendarEvent.getNextRecurrence( now.getTime(), result );

      if( isValid )
      {
         return( new Date( result.value ) );
      }
      else
      {
         isValid = calendarEvent.getPreviousOccurrence( now.getTime(), result );
         
         return( new Date( result.value ) );
      }
   }
   
   if( !isValid || !calendarEvent.recur )
   {
      return( new Date( calendarEvent.start.getTime() ) );
   }
   return( false );
}


function changeToolTipTextForEvent( event )
{
   var thisEvent = getCalendarEventFromEvent( event );
   
   var Html = document.getElementById( "eventTreeListTooltip" );

   while( Html.hasChildNodes() )
   {
      Html.removeChild( Html.firstChild ); 
   }
   
   if( !thisEvent )
   {
      showTooltip = false;
      return;
   }
      
   showTooltip = true;
   
   var HolderBox = getPreviewText( thisEvent );
   
   Html.appendChild( HolderBox );
}

function getPreviewText( calendarEvent )
{
	var HolderBox = document.createElement( "vbox" );
  var textString ;
   if (calendarEvent.title)
   {
      var TitleHtml = document.createElement( "description" );
      textString = gCalendarBundle.getFormattedString("tooltipTitleElement", [calendarEvent.title]);
      var TitleText = document.createTextNode( textString );
      TitleHtml.appendChild( TitleText );
      HolderBox.appendChild( TitleHtml );
   }

   var startDate = new Date( calendarEvent.start.getTime() );
   var endDate = new Date( calendarEvent.end.getTime() );
   var diff = DateUtils.getDifferenceInDays(startDate, endDate) ;

   var DateHtml = document.createElement( "description" );
   var DateText;
   if (calendarEvent.allDay) {
     if ( diff > 1 )  {
       textString = gCalendarBundle.getFormattedString("tooltipEventStart", 
						   [gCalendarWindow.dateFormater.getFormatedDate( startDate ),
						    ""]);
       DateText = document.createTextNode( textString );
       DateHtml.appendChild( DateText );
       HolderBox.appendChild( DateHtml );
       DateHtml = document.createElement( "description" );
       
       endDate.setDate( endDate.getDate() - 1 ); //allday enddate for user
       textString = gCalendarBundle.getFormattedString("tooltipEventEnd", 
						   [gCalendarWindow.dateFormater.getFormatedDate( endDate ),
						    ""]);
     }
     else {
       textString = gCalendarBundle.getFormattedString("tooltipAlldayEventDate", 
						   [gCalendarWindow.dateFormater.getFormatedDate( startDate ),
						    ""]);
     }
   }
   else {
     textString = gCalendarBundle.getFormattedString("tooltipEventStart", 
						  [gCalendarWindow.dateFormater.getFormatedDate( startDate ),
					           gCalendarWindow.dateFormater.getFormatedTime( startDate )]);
     DateText = document.createTextNode( textString );
     DateHtml.appendChild( DateText );
     HolderBox.appendChild( DateHtml );
     DateHtml = document.createElement( "description" );

     textString = gCalendarBundle.getFormattedString("tooltipEventEnd", 
						  [gCalendarWindow.dateFormater.getFormatedDate( endDate ),
						   gCalendarWindow.dateFormater.getFormatedTime( endDate )]);
   }
   DateText = document.createTextNode( textString );
   DateHtml.appendChild( DateText );
   HolderBox.appendChild( DateHtml );

   if (calendarEvent.location)
   {
      var LocationHtml = document.createElement( "description" );
      textString = gCalendarBundle.getFormattedString("tooltipEventLocation", [calendarEvent.location]);
      var LocationText = document.createTextNode( textString );
      LocationHtml.appendChild( LocationText );
      HolderBox.appendChild( LocationHtml );
   }

   if (calendarEvent.description)
   {
      textString = gCalendarBundle.getFormattedString("tooltipEventDescription", [calendarEvent.description]);
      var lines = textString.split("\n");
      var nbmaxlines = 5 ;
      var nblines = lines.length ;
      if( nblines > nbmaxlines ) {
	  nblines = nbmaxlines ;
	  lines[ nblines - 1 ] = "..." ;
      }
	
      for (var i = 0; i < nblines; i++) {
      var DescriptionHtml = document.createElement( "description" );
	var DescriptionText = document.createTextNode(lines[i]);
	DescriptionHtml.appendChild(DescriptionText);
	HolderBox.appendChild(DescriptionHtml);
	}
   }

   return ( HolderBox );
}

