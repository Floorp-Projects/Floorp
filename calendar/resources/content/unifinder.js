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

function calendarUnifinderInit( )
{
   var unifinderEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         /* 
         This no longer works since we moved to a tree 
         */
         
         var SearchTree = document.getElementById( "unifinder-search-results-listbox" );
         
         SearchTree.setAttribute( "suppressonselect", "true" );

         SearchTree.treeBoxObject.selection.select( -1 );
         
         if( EventSelectionArray.length > 0 )
         {
            for( i = 0; i < EventSelectionArray.length; i++ )
            {
               var SearchTreeItem = document.getElementById( "search-unifinder-treeitem-"+EventSelectionArray[i].id );
               
               if( SearchTreeItem )
               {
                  var Index = SearchTree.contentView.getIndexOfItem( SearchTreeItem );
                  
                  SearchTree.treeBoxObject.ensureRowIsVisible( Index );
      
                  SearchTree.treeBoxObject.selection.select( Index );
               }
            }
         }
         /*SearchTree.clearSelection();
         
         if( EventSelectionArray.length > 0 )
         {
            for( i = 0; i < EventSelectionArray.length; i++ )
            {
               var SearchTreeItem = document.getElementById( "search-unifinder-treeitem-"+EventSelectionArray[i].id );
               
               //you need this for when an event is added. It doesn't yet exist.
               if( SearchTreeItem )
                  SearchTree.addItemToSelection( SearchTreeItem );
            }
         }
         dump( "\nAllow on select now!" );
         SearchTree.removeAttribute( "suppressonselect" );
         */
      }
   }
      
   gCalendarWindow.EventSelection.addObserver( unifinderEventSelectionObserver );
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
   //gEventSource.onlyFutureEvents = (document.getElementById( 'unifinder-future-events' ).getAttribute( "checked" ) == "true" );
   eventTable = gEventSource.getCurrentEvents();

   gEventSource.onlyFutureEvents = false;
   
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
      var treeitem = tree.treeBoxObject.view.getItemAtIndex( row.value );
      var eventId = treeitem.getAttribute("eventId");
      return gICalLib.fetchEvent( eventId );
   }
}

/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderClickEvent( event )
{
   // only change checkbox on left mouse-button click
   if( event.button != 0)
      return;
      
   var tree = document.getElementById( UnifinderTreeName );
   var ThisEvent = getCalendarEventFromEvent( event );
   var ArrayOfEvents = new Array( ThisEvent );
   
   gCalendarWindow.EventSelection.setArrayToSelection( ArrayOfEvents );

   if( ArrayOfEvents.length == 1 )
   {
      /*start date is either the next or last occurence, or the start date of the event */
      var eventStartDate = getNextOrPreviousRecurrence( ThisEvent );
      
      /* you need this in case the current day is not visible. */
      gCalendarWindow.currentView.goToDay( eventStartDate, true);
   }
}

/**
*  This is called from the unifinder's edit command
*/

function unifinderEditCommand()
{
   if( gCalendarWindow.EventSelection.selectedEvents.length == 1 )
   {
      var calendarEvent = gCalendarWindow.EventSelection.selectedEvents[0];

      if( calendarEvent != null )
      {
         editEvent( calendarEvent );
      }
   }
}


/**
*  This is called from the unifinder's delete command
*/

function unifinderDeleteCommand( DoNotConfirm )
{
   if( unifinderToDoHasFocus() )
   {
      unifinderDeleteToDoCommand( DoNotConfirm );
      return;
   }
   
   var SelectedItems = gCalendarWindow.EventSelection.selectedEvents;
   
   if( SelectedItems.length == 1 )
   {
      var calendarEvent = SelectedItems[0];

      if ( calendarEvent.title != "" ) {
         if( !DoNotConfirm ) {        
            if ( confirm( confirmDeleteEvent+" "+calendarEvent.title+"?" ) ) {
               gICalLib.deleteEvent( calendarEvent.id );
            }
         }
         else
            gICalLib.deleteEvent( calendarEvent.id );
      }
      else
      {
         if( !DoNotConfirm ) {        
            if ( confirm( confirmDeleteUntitledEvent ) ) {
               gICalLib.deleteEvent( calendarEvent.id );
            }
         }
         else
            gICalLib.deleteEvent( calendarEvent.id );
      }
   }
   else if( SelectedItems.length > 1 )
   {
      if( !DoNotConfirm )
      {
         if( confirm( "Are you sure you want to delete everything?" ) )
         {
            while( SelectedItems.length )
            {
               var ThisItem = SelectedItems.pop();
               
               gICalLib.deleteEvent( ThisItem.id );
            }
         }
      }
      else
      {
         while( SelectedItems.length )
         {
            var ThisItem = SelectedItems.pop();
            
            gICalLib.deleteEvent( ThisItem.id );
         }
      }
   }
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
      eventTable = gEventSource.getCurrentEvents();
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


function unifinderShowEventsWithAlarmsOnly()
{
   var eventTable = gEventSource.getEventsWithAlarms();

   refreshEventTree( eventTable );
}


function unifinderShowFutureEventsOnly( event )
{
   var Today = new Date();
   //do this to allow all day events to show up all day long
   var StartDate = new Date( Today.getFullYear(), Today.getMonth(), Today.getDate(), 0, 0, 0 );
   
   switch( event.currentTarget.parentNode.selectedItem.value )
   {
      case "all":
         gEventSource.onlyFutureEvents = false;
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
      default: 
         alert( "there's no case for ".event.currentTarget.selectedItem.value );
         break;
   }

   refreshEventTree( eventTable );

   /* The following isn't exactly right. It should actually reload after the next event happens. */

   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   setTimeout( "refreshEventTree( eventTable )", milliSecsTillTomorrow );
}

/**
*  Attach the calendarToDo event to the treeitem
*/

function setUnifinderEventTreeItem( treeItem, calendarEvent )
   {
      treeItem.calendarEvent = calendarEvent;
      treeItem.setAttribute( "eventId", calendarEvent.id );
      treeItem.setAttribute( "onmouseover", "changeToolTipTextForEvent( event )" );
      treeItem.setAttribute( "ondblclick" , "unifinderDoubleClickEvent(event)" );
      treeItem.setAttribute( "onclick" , "unifinderClickEvent(event)" );
      treeItem.setAttribute( "calendarevent", "true" );
      treeItem.setAttribute( "id", "search-unifinder-treeitem-"+calendarEvent.id );

      var treeRow = document.createElement( "treerow" );
      var treeCellTitle     = document.createElement( "treecell" );
      var treeCellStartdate = document.createElement( "treecell" );
      var treeCellEnddate   = document.createElement( "treecell" );
      var treeCellCategories = document.createElement( "treecell" );

      var now = new Date();
      
      var thisMorning = new Date( now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0 );

      if(treeItem.getElementsByTagName( "treerow" )[0])
        treeItem.removeChild( treeItem.getElementsByTagName( "treerow" )[0] );

      if( calendarEvent.title == "" )
         var titleText = "Untitled";
      else  
         var titleText = calendarEvent.title;

      treeCellTitle.setAttribute( "label", titleText );

      var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
      var eventEndDate = new Date( calendarEvent.end.getTime() );
      var startDate = formatUnifinderEventDate( eventStartDate );
      var startTime = formatUnifinderEventTime( eventStartDate );
      var endTime  = formatUnifinderEventTime( eventEndDate );
      
      if( calendarEvent.allDay )
      {
         startText = "All day " + startDate;
         endText = "All day " + startDate;
      }
      else
      {
         startText = startDate + " " + startTime;
         endText = startDate + " " + endTime;
      }
      
      treeCellStartdate.setAttribute( "label", startText );
      treeCellEnddate.setAttribute( "label", endText );
      treeCellCategories.setAttribute( "label", calendarEvent.categories );

      treeRow.appendChild( treeCellTitle );
      treeRow.appendChild( treeCellStartdate );
      treeRow.appendChild( treeCellEnddate );
      treeRow.appendChild( treeCellCategories );
      treeItem.appendChild( treeRow );
}


/**
*  Redraw the categories unifinder tree
*/

function refreshEventTree( eventArray )
{
   // get the old tree children item and remove it
   
   var tree = document.getElementById( UnifinderTreeName );

   var elementsToRemove = document.getElementsByAttribute( "calendarevent", "true" );
   
   for( var i = 0; i < elementsToRemove.length; i++ )
   {
      elementsToRemove[i].parentNode.removeChild( elementsToRemove[i] );
   }

   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarEvent = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "treeitem" );
      
      setUnifinderEventTreeItem( treeItem, calendarEvent );

      tree.getElementsByTagName( "treechildren" )[0]. appendChild( treeItem );
   }  
}

function unifinderClearSearchCommand()
{
   document.getElementById( "unifinder-search-field" ).value = "";

   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );
}

function focusFirstItemIfNoSelection()
{
   if( gCalendarWindow.EventSelection.selectedEvents.length == 0 )
   {
      //select the first event in the list.
      var ListBox = document.getElementById( "unifinder-search-results-listbox" );

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
