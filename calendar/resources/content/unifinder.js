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

function calendarUnifinderInit( )
{
   var unifinderEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         selectSelectedEventsInTree( EventSelectionArray );
      }
   }
      
   gCalendarWindow.EventSelection.addObserver( unifinderEventSelectionObserver );
}

function resetAllowSelection()
{
   /* 
   Do not change anything in the following lines, they are needed as described in the 
   selection observer above 
   */
   var SearchTree = document.getElementById( UnifinderTreeName );
   
   SearchTree.treeBoxObject.selection.selectEventsSuppressed = false;
   
   SearchTree.addEventListener( "select", unifinderOnSelect, true );
}


function selectSelectedEventsInTree( EventsToSelect )
{
   if( EventsToSelect === false )
      EventsToSelect = gCalendarWindow.EventSelection.selectedEvents;

   //dump( "\nCALENDAR unifinder.js->on selection changed" );
   var SearchTree = document.getElementById( UnifinderTreeName );
      
   /* The following is a brutal hack, caused by 
   http://lxr.mozilla.org/mozilla1.0/source/layout/xul/base/src/tree/src/nsTreeSelection.cpp#555
   and described in bug 168211
   http://bugzilla.mozilla.org/show_bug.cgi?id=168211
   Do NOT remove anything in the next 3 lines, or the selection in the tree will not work.
   */

   SearchTree.treeBoxObject.selection.selectEventsSuppressed = true;
   SearchTree.onselect = null;
   SearchTree.removeEventListener( "select", unifinderOnSelect, true );
   
   if( EventsToSelect.length == 1 )
   {
      var RowToScrollTo = SearchTree.eventView.getRowOfCalendarEvent( EventsToSelect[0] );
         
      if( RowToScrollTo != "null" )
      {
         SearchTree.treeBoxObject.selection.clearSelection( );
   
         SearchTree.treeBoxObject.ensureRowIsVisible( RowToScrollTo );

         SearchTree.treeBoxObject.selection.timedSelect( RowToScrollTo, 1 );
      }
      else
      {
         SearchTree.treeBoxObject.selection.clearSelection( );
      }
   }
   else if( EventsToSelect.length > 1 )
   {
      /* selecting all events is taken care of in the selectAllEvents in calendar.js 
      ** Other than that, there's no other way to get in here. */
      if( gSelectAll === true )
      {
         SearchTree.treeBoxObject.selection.selectAll( );
         
         gSelectAll = false;
      }   
   }
   else
   {
      dump( "\n--->>>>unifinder.js selection callback :: Clear selection" );
      SearchTree.treeBoxObject.selection.clearSelection( );
   }
   
   /* This needs to be in a setTimeout */
   setTimeout( "resetAllowSelection()", 0 );
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
            unifinderRefresh();
        }
   },
   
   onStartBatch   : function()
   {
   },
    
   onEndBatch   : function()
   {
        unifinderRefresh();
   },
    
   onAddItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
            if( calendarEvent )
            {
                unifinderRefresh();
            }
        }
   },

   onModifyItem : function( calendarEvent, originalEvent )
   {
        if( !gICalLib.batchMode )
        {
            unifinderRefresh();
        }
   },

   onDeleteItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
           unifinderRefresh();
        }
   },

   onAlarm : function( calendarEvent )
   {
      
   }

};


/**
*   Called when the calendar is loaded
*/

function prepareCalendarUnifinder( )
{
   // tell the unifinder to get ready
   calendarUnifinderInit( );
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
*   Called by event observers to update the display
*/

function unifinderRefresh()
{
   eventTable = getEventTable();

   unifinderSearchKeyPress( document.getElementById( 'unifinder-search-field' ), null );
}


/**
*  This is attached to the ondblclik attribute of the events shown in the unifinder
*/

function unifinderDoubleClickEvent( event )
{
   // find event by id
   
   var calendarEvent = getCalendarEventFromEvent( event );
   
   if( calendarEvent != null )
   {
      // go to day view, of the day of the event, select the event
      
      editEvent( calendarEvent );
   }
}


function getCalendarEventFromEvent( event )
{
   var tree = document.getElementById( UnifinderTreeName );
   var row = new Object();

   tree.treeBoxObject.getCellAt( event.clientX, event.clientY, row, {}, {} );

   if( row.value != -1 && row.value < tree.view.rowCount )
   { 
      var event = tree.eventView.getCalendarEventAtRow( row.value );
      return event;
   }
   return( false );
}

/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderOnSelect( event )
{
   dump( "\n\nin unifinder onselect" );

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
      gCalendarWindow.currentView.goToDay( eventStartDate, true);
   }
   
   gCalendarWindow.EventSelection.setArrayToSelection( ArrayOfEvents );
}

function unifinderToDoHasFocus()
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   return( tree.treeBoxObject.focused );
}



/**
*  This is called from the unifinder when a key is pressed in the search field
*/
var gSearchTimeout = null;

function unifinderSearchKeyPress( searchTextItem, event )
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
   
   if ( searchText == '' ) 
   {
      eventTable = getEventTable();
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


function getEventTable( )
{
   var Today = new Date();
   //do this to allow all day events to show up all day long
   var StartDate = new Date( Today.getFullYear(), Today.getMonth(), Today.getDate(), 0, 0, 0 );
   
   gEventSource.onlyFutureEvents = false;
   
   switch( document.getElementById( "event-filter-menulist" ).selectedItem.value )
   {
      case "all":
         eventTable = gEventSource.getCurrentEvents();
         break;
   
      case "today":
         var EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1 );
         eventTable = gEventSource.getEventsForRange( StartDate, EndDate );
         break;
      case "week":
         var EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 8 ) );
         eventTable = gEventSource.getEventsForRange( StartDate, EndDate );
         break;
      case "2weeks":
         var EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 15 ) );
         eventTable = gEventSource.getEventsForRange( StartDate, EndDate );
         break;
      case "month":
         var EndDate = new Date( StartDate.getTime() + ( 1000 * 60 * 60 * 24 * 32 ) );
         eventTable = gEventSource.getEventsForRange( StartDate, EndDate );
         break;
      case "future":
         gEventSource.onlyFutureEvents = true;
         eventTable = gEventSource.getCurrentEvents();
         break;
      case "current":
         var SelectedDate = gCalendarWindow.getSelectedDate();
         MidnightSelectedDate = new Date( SelectedDate.getFullYear(), SelectedDate.getMonth(), SelectedDate.getDate(), 0, 0, 0 );
         var EndDate = new Date( MidnightSelectedDate.getTime() + ( 1000 * 60 * 60 * 24 ) - 1000 );
         eventTable = gEventSource.getEventsForRange( MidnightSelectedDate, EndDate );
         break;
      default: 
         eventTable = new Array();
         dump( "there's no case for "+document.getElementById( "event-filter-menulist" ).selectedItem.value );
         break;
   }

   return( eventTable );
}

function unifinderDoFilterEvents( event )
{
   refreshEventTree( false );

   /* The following isn't exactly right. It should actually reload after the next event happens. */

   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   setTimeout( "refreshEventTree( eventTable )", milliSecsTillTomorrow );
}

/**
*  Redraw the categories unifinder tree
*/
var treeView =
{
   rowCount : gEventArray.length,
   selectedColumn : null,
   sortDirection : null,

   isContainer : function(){return false;},
   getCellProperties : function(){return false;},
   getColumnProperties : function(){return false;},
   getRowProperties : function(){return false;},
   isSorted : function(){return false;},
   isEditable : function(){return true;},
   isSeparator : function(){return false;},
   getImageSrc : function(){return false;},
   cycleHeader : function( ColId, element )
   {
      var sortActive;
      var treeCols;
   
      this.selectedColumn = ColId;
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
      gEventArray.sort( sortEvents );
      document.getElementById( UnifinderTreeName ).view = this;
   },
   setTree : function( tree ){this.tree = tree;},
   getCellText : function(row,column)
   {
      var columnElement = document.getElementById(column);
      var sortActive = columnElement.getAttribute("sortActive");
      if (sortActive == "true")
      {
         this.selectedColumn = column;
         this.sortDirection = columnElement.getAttribute("sortDirection");
         gEventArray.sort(sortEvents);
      }
   
      calendarEvent = gEventArray[row];
      var calendarStringBundle = srGetStrBundle("chrome://calendar/locale/calendar.properties");
      switch( column )
      {
         case "unifinder-search-results-tree-col-title":
            if( calendarEvent.title == "" )
               var titleText = "Untitled";
            else  
               var titleText = calendarEvent.title;
            return( titleText );
         break;
         case "unifinder-search-results-tree-col-startdate":
            var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
            var startTime = formatUnifinderEventTime( eventStartDate );
            var startDate = formatUnifinderEventDate( eventStartDate );
            if( calendarEvent.allDay )
            {
               startText = "All day " + startDate;
            }
            else
            {
               startText = startDate + " " + startTime;
            }
            return( startText );
         break;
         case "unifinder-search-results-tree-col-enddate":
            var eventEndDate = new Date( calendarEvent.end.getTime() );
            var endTime = formatUnifinderEventTime( eventEndDate );
            var endDate = formatUnifinderEventDate( eventEndDate );
            if( calendarEvent.allDay )
            {
               endText = "All day " + endDate;
            }
            else
            {
               endText = endDate + " " + endTime;
            }
            return( endText );
         break;
         case "unifinder-search-results-tree-col-categories":
            return( calendarEvent.categories );
         break;
         case "unifinder-search-results-tree-col-location":
            return( calendarEvent.location );
         break;
         case "unifinder-search-results-tree-col-status":
            switch( calendarEvent.status )
            {
               case calendarEvent.ICAL_STATUS_TENTATIVE:
                  return( calendarStringBundle.GetStringFromName( "Tentative" ) );
               case calendarEvent.ICAL_STATUS_CONFIRMED:
                  return( calendarStringBundle.GetStringFromName( "Confirmed" ) );
               case calendarEvent.ICAL_STATUS_CANCELLED:
                  return( calendarStringBundle.GetStringFromName( "Cancelled" ) );
            }
         break;
      }
   }
}

function sortEvents( EventA, EventB )
{
   var modifier = 1;
   if (treeView.sortDirection == "descending")
	{
		modifier = -1;
	}

   switch(treeView.selectedColumn)
   {
   case "unifinder-search-results-tree-col-title":
      return( ((EventA.title > EventB.title) ? 1 : -1) * modifier );
   
   case "unifinder-search-results-tree-col-startdate":
      return( ((EventA.start.getTime() > EventB.start.getTime()) ? 1 : -1) * modifier );

   case "unifinder-search-results-tree-col-enddate":
         return( ((EventA.end.getTime() > EventB.end.getTime()) ? 1 : -1) * modifier );

   case "unifinder-search-results-tree-col-categories":
         return( ((EventA.categories > EventB.categories) ? 1 : -1) * modifier );

   case "unifinder-search-results-tree-col-location":
         return( ((EventA.location > EventB.location) ? 1 : -1) * modifier );

   case "unifinder-search-results-tree-col-status":
         return( ((EventA.status > EventB.status) ? 1 : -1) * modifier );

   }
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
   if( eventArray === false )
   {
      eventArray = getEventTable();
   }
   
   gEventArray = new Array();

   gEventArray = eventArray;

   treeView.rowCount = gEventArray.length;
      
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
   if( calendarEvent.recur )
   {
      var now = new Date();

      var result = new Object();

      var isValid = calendarEvent.getNextRecurrence( now.getTime(), result );

      if( isValid )
      {
         var eventStartDate = new Date( result.value );
      }
      else
      {
         isValid = calendarEvent.getPreviousOccurrence( now.getTime(), result );
         
         var eventStartDate = new Date( result.value );
      }
   }
   
   if( !isValid || !calendarEvent.recur )
   {
      var eventStartDate = new Date( calendarEvent.start.getTime() );
   }
      
   return eventStartDate;
}


function changeToolTipTextForEvent( event )
{
   var thisEvent = getCalendarEventFromEvent( event );
   
   var Html = document.getElementById( "eventTooltip" );

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

   if (calendarEvent.title)
   {
      var TitleHtml = document.createElement( "description" );
      var TitleText = document.createTextNode( "Title: "+calendarEvent.title );
      TitleHtml.appendChild( TitleText );
      HolderBox.appendChild( TitleHtml );
   }

   var DateHtml = document.createElement( "description" );
   var startDate = new Date( calendarEvent.start.getTime() );
   var DateText = document.createTextNode( "Start: "+gCalendarWindow.dateFormater.getFormatedDate( startDate )+" "+gCalendarWindow.dateFormater.getFormatedTime( startDate ) );
   DateHtml.appendChild( DateText );
   HolderBox.appendChild( DateHtml );

   DateHtml = document.createElement( "description" );
   var endDate = new Date( calendarEvent.end.getTime() );
   DateText = document.createTextNode( "End: "+gCalendarWindow.dateFormater.getFormatedDate( endDate )+" "+gCalendarWindow.dateFormater.getFormatedTime( endDate ) );
   DateHtml.appendChild( DateText );
   HolderBox.appendChild( DateHtml );

   if (calendarEvent.description)
   {
      var DescriptionHtml = document.createElement( "description" );
      var DescriptionText = document.createTextNode( "Description: "+calendarEvent.description );
      DescriptionHtml.appendChild( DescriptionText );
      HolderBox.appendChild( DescriptionHtml );
   }

   return ( HolderBox );
}

