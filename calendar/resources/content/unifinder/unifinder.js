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

function unifinderInit( CalendarWindow )
{
   var unifinderEventSelectionObserver = 
   {
      onSelectionChanged : function( EventSelectionArray )
      {
         var CategoriesTree = document.getElementById( "unifinder-categories-tree" );

         var SearchTree = document.getElementById( "unifinder-search-results-tree" );

         CategoriesTree.clearSelection();

         SearchTree.clearSelection();
         
         if( EventSelectionArray.length > 0 )
         {
            for( i = 0; i < EventSelectionArray.length; i++ )
            {
               CategoriesTree.addItemToSelection( document.getElementById( "unifinder-treeitem-"+EventSelectionArray[i].id ) );
   
               SearchTree.addItemToSelection( document.getElementById( "search-unifinder-treeitem-"+EventSelectionArray[i].id ) );
            }
         }
      }
   }
      
   gCalendarWindow.EventSelection.addObserver( unifinderEventSelectionObserver );


}


function unifinderSelectTab( tabid )
{
    var unifinderList = document.getElementsByTagName("unifinder");
    
    if( unifinderList.length > 0 )
    {
        var unifinder = unifinderList[0];
        
        var newTab = document.getElementById( tabid );
        
        if( newTab )
        {
            unifinder.selectedTab = newTab;
        }
    }
}


var gSearchEventTable = new Array();

var gDateFormater = new DateFormater();

/**
*   Observer for the calendar event data source. This keeps the unifinder
*   display up to date when the calendar event data is changed
*/

var unifinderEventDataSourceObserver =
{
   onLoad   : function()
   {
      unifinderRefesh();
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
      if( calendarEvent )
      {
         unifinderRefesh();
      }
   },

   onModifyItem : function( calendarEvent, originalEvent )
   {
      unifinderRefesh();
   },

   onDeleteItem : function( calendarEvent )
   {
      unifinderRefesh();
   },

   onAlarm : function( calendarEvent )
   {
      
   }

};


/**
*   Called when the calendar is loaded
*/

function prepareCalendarUnifinder( eventSource )
{
   // tell the unifinder to get ready
   unifinderInit();
 
   // set up our calendar event observer
   
   gICalLib.addObserver( unifinderEventDataSourceObserver );
}


/**
*   Called when the calendar is unloaded
*/

function finishCalendarUnifinder( eventSource )
{
//   eventSource.removeObserver( unifinderEventDataSourceObserver  );
}


/**
*   Helper function to display event dates in the unifinder
*/

function formatUnifinderEventDate( date )
{
   var monthDayString = gDateFormater.getFormatedDate( date );
   
   return  monthDayString + ", " + date.getFullYear();
}


/**
*   Helper function to display event times in the unifinder
*/

function formatUnifinderEventTime( time )
{
   var timeString = gDateFormater.getFormatedTime( time );
   return timeString;
   
}

/**
*   Called by event observers to update the display
*/

function unifinderRefesh()
{
   var eventTable = gEventSource.getAllEvents();
   
   refreshCategoriesTree( eventTable );
   
   refreshSearchTree( eventTable );

   unifinderSearchKeyPress( document.getElementById( 'unifinder-search-field' ), null );
}


/**
*   Redraw the event tree
*/

function refreshCategoriesTree( eventTable )
{
   refreshEventTree( eventTable, "unifinder-categories-tree" );
}


/**
*  Redraw the search tree
*/

function refreshSearchTree( SearchEventTable )
{
   gSearchEventTable = SearchEventTable;

   refreshEventTree( gSearchEventTable, "unifinder-search-results-tree", false );
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
      
      //gCalendarWindow.dayView.clickEventBox( calendarEvent );

      editEvent( calendarEvent );

   }
}


/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderClickEvent( id, event )
{
   var calendarEvent = gICalLib.fetchEvent( id );
   
   gCalendarWindow.EventSelection.replaceSelection( calendarEvent );

   var eventBox = gCalendarWindow.currentView.getVisibleEvent( calendarEvent );
   if ( !eventBox )
   {
      var eventStartDate = new Date( calendarEvent.start.getTime() );
      
      gCalendarWindow.currentView.goToDay( eventStartDate, true);
      
   }
}

/**
*  This is called from the unifinder's modify command
*/

function unifinderModifyCommand()
{
   var SelectedItem = document.getElementById( "unifinder-categories-tree" ).selectedItems[0];

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
*  This is called from the unifinder's remove command
*/

function unifinderRemoveCommand( DoNotConfirm )
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
      if( confirm( "Are you sure you want to delete everything?" ) )
      {
         while( SelectedItems.length )
         {
            var ThisItem = SelectedItems.pop();

            gICalLib.deleteEvent( gICalLib.fetchEvent( ThisItem.calendarEvent.id ) );
         }
      }
   }
}


/**
*  This is called from the unifinder when a key is pressed in the search field
*/

function unifinderSearchKeyPress( searchTextItem, event )
{
   var searchText = searchTextItem.value;
   
   if ( searchTextItem.value == '' ) 
   {
      gSearchEventTable = gEventSource.getAllEvents();
   }
   else if ( searchTextItem.value == " " ) 
   {
      searchTextItem.value = '';
   }
   else
   {
      var FieldsToSearch = new Array( "title", "description", "location" );
      gSearchEventTable = gEventSource.search( searchText, FieldsToSearch );
   }
    
   refreshSearchTree( gSearchEventTable );
}

/**
*  Redraw the categories unifinder tree
*/

function refreshEventTree( eventArray, childrenName, Categories )
{
   // get the old tree children item and remove it
   
   var oldTreeChildren = document.getElementById( childrenName );
   while( oldTreeChildren.hasChildNodes() )
      oldTreeChildren.removeChild( oldTreeChildren.lastChild );

   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarEvent = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "listitem" );
      
      if( Categories != false )
         treeItem.setAttribute( "id", "unifinder-treeitem-"+calendarEvent.id );
      else
         treeItem.setAttribute( "id", "search-unifinder-treeitem-"+calendarEvent.id );

      treeItem.setAttribute( "ondblclick" , "unifinderDoubleClickEvent(" + calendarEvent.id + ")" );
      treeItem.setAttribute( "onclick" , "unifinderClickEvent(" + calendarEvent.id + ")" );
      
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
      
      var eventStartDate = new Date( calendarEvent.start.getTime() );
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
   }  
}