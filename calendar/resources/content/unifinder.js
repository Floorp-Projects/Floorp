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


var gSearchEventTable = new Array();

var gDateFormater = new DateFormater();

var gUnifinderSelection = null;


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
   
   onAddItem : function( calendarEvent )
   {
      if( calendarEvent )
      {
         unifinderRefesh();
      }
   },

   onModifyItem : function( calendarEvent )
   {
      if( calendarEvent )
      {
         unifinderRefesh();
      }
   },

   onDeleteItem : function( calendarEvent )
   {
      unifinderRefesh();
   },

};


/**
*   Called when the calendar is loaded
*/

function prepareCalendarUnifinder( eventSource )
{
   // tell the unifinder to get ready
   unifinderInit();
 
   // set up our calendar event observer
   
   eventSource.addObserver( unifinderEventDataSourceObserver  );
}


/**
*   Called when the calendar is unloaded
*/

function finishCalendarUnifinder( eventSource )
{
   eventSource.removeObserver( unifinderEventDataSourceObserver  );
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
   refreshEventTree( eventTable, "unifinder-categories-tree-children" );
}


/**
*  Redraw the search tree
*/

function refreshSearchTree( SearchEventTable )
{
   gSearchEventTable = SearchEventTable;

   refreshSearchEventTree( gSearchEventTable, "unifinder-search-results-tree-children" );
}


/**
*  This is attached to the ondblclik attribute of the events shown in the unifinder
*/

function unifinderDoubleClickEvent( id )
{
   // find event by id
   
   var calendarEvent = gEventSource.getCalendarEventById( id );
   
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
   gUnifinderSelection = id;
   
   var calendarEvent = gEventSource.getCalendarEventById( id );
   
   var eventBox = gCalendarWindow.currentView.getVisibleEvent( calendarEvent );
   
   if ( eventBox )
   {
      //the event box exists, so the event is visible.
      gCalendarWindow.currentView.clickEventBox( eventBox, event );
   }
   else //else we go to the day
   {
      gCalendarWindow.currentView.clearSelectedDate( );

      gCalendarWindow.setSelectedEvent( calendarEvent );

      gCalendarWindow.currentView.goToDay( calendarEvent.start, true);
      
      var eventBox = gCalendarWindow.currentView.getVisibleEvent( calendarEvent );
   
      gCalendarWindow.currentView.clickEventBox( eventBox, event );

      
   }
   

}

/**
*  This is called from the unifinder's modify command
*/

function unifinderModifyCommand()
{
   var SelectedItem = document.getElementById( "unifinder-categories-tree" ).selectedItems.item(0);

   if( SelectedItem )
   {
      if( SelectedItem.getAttribute( "container" ) == "true" )
      {
          launchEditCategoryDialog( SelectedItem.categoryobject );
      }
      else
      {
         if( gUnifinderSelection != null )
         {
            var calendarEvent = gEventSource.getCalendarEventById( gUnifinderSelection );
      
            if( calendarEvent != null )
            {
               editEvent( calendarEvent );
            }  
         }
      }
   }
}


/**
*  This is called from the unifinder's remove command
*/

function unifinderRemoveCommand()
{
   var SelectedItem = document.getElementById( "unifinder-categories-tree" ).selectedItems[0];

   if( SelectedItem )
   {
      if ( SelectedItem.getAttribute( "container" ) == "true" ) 
      {
         if ( confirm( "Are you sure you want to delete this category?" ) ) 
         {
            gCategoryManager.deleteCategory( SelectedItem.categoryobject );
         }
      }
      else
      {
         if( gUnifinderSelection != null )
         {
            var calendarEvent = gEventSource.getCalendarEventById( gUnifinderSelection );
         
            if( calendarEvent != null )
            {
               if ( confirm( confirmDeleteEvent+" "+calendarEvent.title+"?" ) ) {
                  gEventSource.deleteEvent( calendarEvent );
               }
            }
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
*  Redraw the unifinder tree, either search or categories
*/

function refreshSearchEventTree( eventArray, childrenName )
{
   // get the old tree children item and remove it
   
   var oldTreeChildren = document.getElementById( childrenName );
   var parent = oldTreeChildren.parentNode;
   
   oldTreeChildren.parentNode.removeChild( oldTreeChildren );
   
   // make a new treechildren item
   
   var treeChildren = document.createElement( "treechildren" );
   treeChildren.setAttribute( "id" , childrenName );
   treeChildren.setAttribute( "flex" , "1" );
   
   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarEvent = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "treeitem" );
      treeItem.setAttribute( "id", "search-unifinder-treeitem-"+calendarEvent.id );
      treeItem.setAttribute( "ondblclick" , "unifinderDoubleClickEvent(" + calendarEvent.id + ")" );
      treeItem.setAttribute( "onclick" , "unifinderClickEvent(" + calendarEvent.id + ")" );
      treeItem.setAttribute( "flex", "1" );

      var treeRow = document.createElement( "treerow" );
      treeRow.setAttribute( "flex", "1" );

      var treeCell = document.createElement( "treecell" );
      treeCell.setAttribute( "flex" , "1" );
      treeCell.setAttribute( "crop", "right" );

      var treeCellBox = document.createElement( "vbox" );
      treeCellBox.setAttribute( "class" , "unifinder-treecell-box-class" );
      treeCellBox.setAttribute( "flex" , "1" );
      treeCellBox.setAttribute( "id", "search-unifinder-treecell-box" );
      treeCellBox.setAttribute( "crop", "right" );

      var text1 = document.createElement( "label" );
      text1.setAttribute( "class", "calendar-unifinder-event-text" );
      text1.setAttribute( "crop", "right" );
      var text2 = document.createElement( "label" );
      text1.setAttribute( "class", "calendar-unifinder-event-text" );
      text1.setAttribute( "crop", "right" );

      // set up the display and behaviour of the tree items
      // set the text of the two text items
      
      text1.setAttribute( "value" , calendarEvent.title );
      
      var startDate = formatUnifinderEventDate( calendarEvent.start );
      var startTime = formatUnifinderEventTime( calendarEvent.start );
      var endTime  = formatUnifinderEventTime( calendarEvent.end );
      
      if( calendarEvent.allDay )
      {
         text2.setAttribute( "value" , "All day on " + startDate + "" );
      }
      else
      {
         text2.setAttribute( "value" , startDate + " " + startTime + " - " +  endTime );
      }
      
      // add the items
      
      treeChildren.appendChild( treeItem );
      treeItem.appendChild( treeRow );
      treeRow.appendChild( treeCell );
      treeCell.appendChild( treeCellBox );
      if ( calendarEvent.title ) 
      {
         treeCellBox.appendChild( text1 );
      }
      treeCellBox.appendChild( text2 );
   }
   
   // add the treeChildren item to the tree
   
   parent.appendChild( treeChildren );
}



/**
*  Redraw the categories unifinder tree
*/

function refreshEventTree( eventArray, childrenName )
{
   // get the old tree children item and remove it
   
   var oldTreeChildren = document.getElementById( childrenName );
   var parent = oldTreeChildren.parentNode;
   
   oldTreeChildren.parentNode.removeChild( oldTreeChildren );
   
   //add the categories to the tree first.
   // make a new treechildren item

   var catTreeChildren = document.createElement( "treechildren" );
   catTreeChildren.setAttribute( "id", childrenName );
   catTreeChildren.setAttribute( "flex", "1" );
   /* WE HAVE NO CATEGORIES RIGHT NOW
   Categories = gCategoryManager.getAllCategories();

   for ( i in Categories ) 
   {
      var ThisCategory = Categories[i];

      var catTreeItem = document.createElement( "treeitem" );
      catTreeItem.setAttribute( "container", "true" );
      catTreeItem.categoryobject = ThisCategory;
      //catTreeItem.setAttribute( "id", "catitem"+Categories[i].id );
      
      var catTreeRow = document.createElement( "treerow" );
      
      var catTreeCell = document.createElement( "treecell" );
      catTreeCell.setAttribute( "class", "treecell-indent" );
      catTreeCell.setAttribute( "label", ThisCategory.name );
      
      thisCatChildren = document.createElement( "treechildren" );
      thisCatChildren.setAttribute( "id", "catchild"+ThisCategory.id );
      
      catTreeRow.appendChild( catTreeCell );
      catTreeItem.appendChild( catTreeRow );
      catTreeItem.appendChild( thisCatChildren );
      catTreeChildren.appendChild( catTreeItem );
   }
   
   */
   parent.appendChild( catTreeChildren );

   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarEvent = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "treeitem" );
      treeItem.setAttribute( "id", "unifinder-treeitem-"+calendarEvent.id );
      treeItem.setAttribute( "ondblclick" , "unifinderDoubleClickEvent(" + calendarEvent.id + ")" );
      treeItem.setAttribute( "onclick" , "unifinderClickEvent(" + calendarEvent.id + ")" );
      
      var treeRow = document.createElement( "treerow" );
      treeRow.setAttribute( "flex", "1" );
      
      var treeCell = document.createElement( "treecell" );
      treeCell.setAttribute( "flex" , "1" );
      treeCell.setAttribute( "crop", "right" );
      
      var image = document.createElement( "image" );
      
      if ( calendarEvent.alarm ) 
      {
         var imageSrc = "chrome://calendar/skin/event_alarm.png";
      }
      else
      {
         var imageSrc = "chrome://calendar/skin/event.png";
      }

      image.setAttribute( "src", imageSrc );
      
      var treeCellHBox = document.createElement( "hbox" );
      
      /* 
      ** HACK! 
      ** There is a mysterious child of the HBox, with a flex of one, so set the flex on the HBox really high to hide the child. 
      */
      treeCellHBox.setAttribute( "flex" , "1000" );
      treeCellHBox.setAttribute( "id", "unifinder-treecell-box" );
      treeCellHBox.setAttribute( "crop", "right" );


      var treeCellVBox = document.createElement( "vbox" );
      treeCellVBox.setAttribute( "crop", "right" );
      treeCellVBox.setAttribute( "flex", "1" );

      var text1 = document.createElement( "label" );
      //text1.setAttribute( "class", "calendar-unifinder-event-text" );
      text1.setAttribute( "crop", "right" );
      
      var text2 = document.createElement( "label" );
      //text2.setAttribute( "class", "calendar-unifinder-event-text" );
      text2.setAttribute( "crop", "right" );

      // set up the display and behaviour of the tree items
      // set the text of the two text items
      
      text1.setAttribute( "value" , calendarEvent.title );
      
      var startDate = formatUnifinderEventDate( calendarEvent.start );
      var startTime = formatUnifinderEventTime( calendarEvent.start );
      var endTime  = formatUnifinderEventTime( calendarEvent.end );
      
      if( calendarEvent.allDay )
      {
         text2.setAttribute( "value" , "All day on " + startDate + "" );
      }
      else
      {
         text2.setAttribute( "value" , startDate + " " + startTime + " - " +  endTime );
      }
      
      
      //find the parent category treeitem.
      var treeChild = document.getElementById( "catchild"+calendarEvent.category );

      if ( treeChild ) 
      {
         treeChild.appendChild( treeItem );
         //treeCell.setAttribute( "class", "calendar-unifinder-event-indent" );
      }
      //otherwise append it to the root node.
      else 
      {
         catTreeChildren.appendChild( treeItem );
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
      
      treeRow.appendChild( treeCell );
      
      treeItem.appendChild( treeRow );
   }  
}