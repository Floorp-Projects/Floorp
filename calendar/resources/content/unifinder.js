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
         var SearchTree = document.getElementById( "unifinder-search-results-listbox" );
         
         SearchTree.setAttribute( "suppressonselect", "true" );

         SearchTree.clearSelection();
         
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
            unifinderRefesh();
        }
   },
   
   onStartBatch   : function()
   {
   },
    
   onEndBatch   : function()
   {
        unifinderRefesh();
   },
    
   onAddItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
            if( calendarEvent )
            {
                unifinderRefesh();
            }
        }
   },

   onModifyItem : function( calendarEvent, originalEvent )
   {
        if( !gICalLib.batchMode )
        {
            unifinderRefesh();
        }
   },

   onDeleteItem : function( calendarEvent )
   {
        if( !gICalLib.batchMode )
        {
            unifinderRefesh();
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

function unifinderRefesh()
{
   gEventSource.onlyFutureEvents = (document.getElementById( 'unifinder-future-events' ).getAttribute( "checked" ) == "true" );
   
   unifinderSearchKeyPress( document.getElementById( 'unifinder-search-field' ), null );
}


/**
*  This is attached to the ondblclik attribute of the events shown in the unifinder
*/

function unifinderDoubleClickEvent( id )
{
   // find event by id
   
   var calendarEvent = gICalLib.fetchEvent( id );
   
   if( calendarEvent != null )
   {
      // go to day view, of the day of the event, select the event
      
      editEvent( calendarEvent );
   }
}


/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderClickEvent( CallingListBox )
{
   var ArrayOfEvents = new Array();
   
   for( i = 0; i < CallingListBox.selectedItems.length; i++ )
   {
      var calendarEvent = CallingListBox.selectedItems[i].event;
            
      ArrayOfEvents[ ArrayOfEvents.length ] = calendarEvent;
   }
   
   gCalendarWindow.EventSelection.setArrayToSelection( ArrayOfEvents );

   if( CallingListBox.selectedItems.length == 1 )
   {
      /*start date is either the next or last occurence, or the start date of the event */
      var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
      
      /* you need this in case the current day is not visible. */
      gCalendarWindow.currentView.goToDay( eventStartDate, true);
   }
   
}

/**
*  This is called from the unifinder's edit command
*/

function unifinderEditCommand()
{
   var SelectedItem = document.getElementById( "unifinder-search-results-listbox" ).selectedItems[0];

   if( SelectedItem )
   {
      if( SelectedItem.getAttribute( "container" ) == "true" )
      {
          launchEditCategoryDialog( SelectedItem.categoryobject );
      }
      else
      {
         var calendarEvent = gCalendarWindow.EventSelection.selectedEvents[0];

         if( calendarEvent != null )
         {
            editEvent( calendarEvent );
         }  
      }
   }
}


/**
*  This is called from the unifinder's delete command
*/

function unifinderDeleteCommand( DoNotConfirm )
{
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
      searchTextItem.value = '';
   }
   else
   {
      var FieldsToSearch = new Array( "title", "description", "location" );
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
   gEventSource.onlyFutureEvents = (event.target.getAttribute( "checked" ) == "true" );
   
   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );

   /* The following isn't exactly right. It should reload after the next event happens. */

   // get the current time
   var now = new Date();
   
   var tomorrow = new Date( now.getFullYear(), now.getMonth(), ( now.getDate() + 1 ) );
   
   var milliSecsTillTomorrow = tomorrow.getTime() - now.getTime();
   
   setTimeout( "refreshEventTree( eventTable )", milliSecsTillTomorrow );
}

/**
*  Redraw the categories unifinder tree
*/

function refreshEventTree( eventArray )
{
   // get the old tree children item and remove it
   
   var oldTreeChildren = document.getElementById( UnifinderTreeName );
   
   while( oldTreeChildren.hasChildNodes() )
      oldTreeChildren.removeChild( oldTreeChildren.lastChild );

   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarEvent = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "listitem" );
      
      treeItem.setAttribute( "id", "search-unifinder-treeitem-"+calendarEvent.id );

      treeItem.event = calendarEvent;

      treeItem.setAttribute( "onmouseover", "changeToolTipTextForEvent( event )" );
      treeItem.setAttribute( "ondblclick" , "unifinderDoubleClickEvent(" + calendarEvent.id + ")" );
      //treeItem.setAttribute( "onclick" , "unifinderClickEvent(" + calendarEvent.id + ")" );
      
      var treeCell = document.createElement( "listcell" );
      treeCell.setAttribute( "flex" , "1" );
      treeCell.setAttribute( "crop", "right" );
      
      var image = document.createElement( "image" );
      image.setAttribute( "class", "unifinder-calendar-event-icon-class" );
      
      if ( calendarEvent.alarm ) 
      {
         image.setAttribute( "alarm", "true" );
      }
      else if( calendarEvent.recur == true )
      {
         image.setAttribute( "recur", "true" );
      }

      var treeCellHBox = document.createElement( "hbox" );
      
      treeCellHBox.setAttribute( "flex" , "1" );
      treeCellHBox.setAttribute( "class", "unifinder-treecell-box-class" );
      treeCellHBox.setAttribute( "crop", "right" );
      treeCellHBox.setAttribute( "align", "center" );

      var treeCellVBox = document.createElement( "vbox" );
      treeCellVBox.setAttribute( "crop", "right" );
      treeCellVBox.setAttribute( "flex", "1" );

      var text1 = document.createElement( "label" );
      text1.setAttribute( "crop", "right" );
      
      var text2 = document.createElement( "label" );
      text2.setAttribute( "crop", "right" );

      // set up the display and behaviour of the tree items
      // set the text of the two text items
      
      text1.setAttribute( "value" , calendarEvent.title );
      
      var eventStartDate = getNextOrPreviousRecurrence( calendarEvent );
      var eventEndDate = new Date( calendarEvent.end.getTime() );
      var startDate = formatUnifinderEventDate( eventStartDate );
      var startTime = formatUnifinderEventTime( eventStartDate );
      var endTime  = formatUnifinderEventTime( eventEndDate );
      
      if( calendarEvent.allDay )
      {
         text2.setAttribute( "value" , "All day on " + startDate + "" );
      }
      else
      {
         text2.setAttribute( "value" , startDate + " " + startTime + " - " +  endTime );
      }
      
      
      // add the items
      if ( calendarEvent.title ) 
      {
         treeCellVBox.appendChild( text1 );
      }
      treeCellVBox.appendChild( text2 );
      
      treeCellHBox.appendChild( image );
      treeCellHBox.appendChild( treeCellVBox );
      
      treeCell.appendChild( treeCellHBox );

      treeItem.appendChild( treeCell );

      oldTreeChildren.appendChild( treeItem );

      //you need this for when an event is added.
      if( gCalendarWindow.EventSelection.isSelectedEvent( calendarEvent ) )
         oldTreeChildren.addItemToSelection( treeItem );
   }  
}


function unifinderClearSearchCommand()
{
   document.getElementById( "unifinder-search-field" ).value = "";

   var eventTable = gEventSource.getCurrentEvents();

   refreshEventTree( eventTable );
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
