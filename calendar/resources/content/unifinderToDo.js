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

const ToDoUnifinderTreeName = "unifinder-todo-tree";

/**
*   Observer for the calendar event data source. This keeps the unifinder
*   display up to date when the calendar event data is changed
*/

var unifinderToDoDataSourceObserver =
{
   onLoad   : function()
   {
      toDoUnifinderRefesh();
   },
   
   onStartBatch   : function()
   {
   },
    
   onEndBatch   : function()
   {
      toDoUnifinderRefesh();
   },
    
   onAddItem : function( calendarToDo )
   {
      toDoUnifinderRefesh();
   },

   onModifyItem : function( calendarToDo, originalToDo )
   {
      toDoUnifinderItemUpdate( calendarToDo );
   },

   onDeleteItem : function( calendarToDo )
   {
      toDoUnifinderRefesh();
   },

   onAlarm : function( calendarToDo )
   {
      
   }

};


/**
*   Called when the calendar is loaded
*/

function prepareCalendarToDoUnifinder( )
{
   // set up our calendar event observer
   gICalLib.addTodoObserver( unifinderToDoDataSourceObserver );
}

/**
*   Called when the calendar is unloaded
*/

function finishCalendarToDoUnifinder( )
{
   gICalLib.removeTodoObserver( unifinderToDoDataSourceObserver  );
}


/**
*   Called by event observers to update the display
*/

function toDoUnifinderRefesh()
{
   var Checked = document.getElementById( "only-completed-checkbox" ).checked;
   
   if( Checked === true )
   {
      var now = new Date();

      gICalLib.filter.completed.setTime( now );
   }
   else
   {
      gICalLib.resetFilter()
   }

   var eventTable = gEventSource.getAllToDos();
   
   refreshToDoTree( eventTable );
}

function unifinderToDoHasFocus()
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   return( tree.treeBoxObject.focused );
}


function getToDoFromEvent( event )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   var row = new Object();

   tree.treeBoxObject.getCellAt( event.clientX, event.clientY, row, {}, {} );

   if( row.value != -1 && row.value < tree.view.rowCount )
   { 
      var treeitem = tree.treeBoxObject.view.getItemAtIndex( row.value );
      var todoId = treeitem.getAttribute("toDoID");
      return gICalLib.fetchTodo( todoId );
   }
}

function getSelectedToDo()
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   // .toDo object sometimes isn't available?
   var todoId = tree.contentView.getItemAtIndex(tree.currentIndex).getAttribute("toDoID");
   return gICalLib.fetchTodo( todoId );
}


/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderClickToDo( event )
{
   // only change checkbox on left mouse-button click
   if( event.button != 0)
      return;
      
   var tree = document.getElementById( ToDoUnifinderTreeName );
   var ThisToDo = getToDoFromEvent( event );
   var row = new Object();
   var childElt = { };
   var colID = { };
   tree.treeBoxObject.getCellAt( event.clientX, event.clientY, row, colID, childElt );

   if( colID.value == "unifinder-todo-tree-col-completed" && childElt.value == "image" )
   {      
      var treeitem = tree.treeBoxObject.view.getItemAtIndex( row.value );
      var isChecked = treeitem.getAttribute( "checked" );
      if( isChecked )
         treeitem.removeAttribute( "checked" )
      else
         treeitem.setAttribute(" checked", true );
      checkboxClick( ThisToDo, !isChecked )
   }
}

/**
*  This is attached to the onclik attribute of the to do list shown in the unifinder
*/

function unifinderDoubleClickToDo( event )
{
   //open the edit todo dialog box
   var ThisToDo = getToDoFromEvent( event );
   if( ThisToDo )
      editToDo( ThisToDo );
}

/**
*  Set the context menu on mousedown to change it before it is opened
*/

function unifinderMouseDownToDo( event )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   var treechildren = tree.getElementsByTagName( "treechildren" )[0];

   var ThisToDo = getToDoFromEvent( event );
   if( ThisToDo ) 
   {
      if(event.button == 2)
         treechildren.setAttribute("context", "taskitem-context-menu")

      // TODO HACK notifiers should be rewritten to integrate events and todos
      document.getElementById( "delete_command" ).removeAttribute( "disabled" );
      document.getElementById( "delete_command_no_confirm" ).removeAttribute( "disabled" );
      document.getElementById( "print_command" ).setAttribute( "disabled", "true" );
   } else
   {
      if(event.button == 2)
          treechildren.setAttribute("context", "context-menu");
      tree.treeBoxObject.selection.clearSelection();

      // TODO HACK notifiers should be rewritten to integrate events and todos
      document.getElementById( "delete_command" ).setAttribute( "disabled", "true" );
      document.getElementById( "delete_command_no_confirm" ).setAttribute( "disabled", "true" );
      //  printing tasks not supported
      document.getElementById( "print_command" ).setAttribute( "disabled", "true" );
   }
}

/**
*  Delete the current selected item with focus from the ToDo unifinder list
*/

function unifinderDeleteToDoCommand( DoNotConfirm )
{
// TODO Implement Confirm
   var tree = document.getElementById( ToDoUnifinderTreeName );
   if (tree.treeBoxObject.selection.count > 0)
   {
      var treeitem = tree.treeBoxObject.view.getItemAtIndex( tree.currentIndex );
      if(treeitem)
      {
         var todoId = treeitem.getAttribute("toDoID");
         gICalLib.deleteTodo( todoId );
      }
   }
}

function checkboxClick( ThisToDo, completed )
{
   // var ThisToDo = event.currentTarget.parentNode.parentNode.toDo;
   if( completed )
   {
      var completedTime = new Date();
      
      ThisToDo.completed.setTime( completedTime );
      
      ThisToDo.status = ThisToDo.ICAL_STATUS_COMPLETED;
      
   }
   else
   {
      ThisToDo.completed.clear();

      if( ThisToDo.percent == 0 )
         ThisToDo.status = ThisToDo.ICAL_STATUS_NEEDSACTION;
      else
         ThisToDo.status = ThisToDo.ICAL_STATUS_INPROCESS;
   }
   gICalLib.modifyTodo( ThisToDo );
}


/**
*  Attach the calendarToDo event to the treeitem
*/

function setUnifinderToDoTreeItem( treeItem, calendarToDo )
   {
      treeItem.toDo = calendarToDo;
      treeItem.setAttribute( "toDoID", calendarToDo.id );

      var treeRow = document.createElement( "treerow" );
      var treeCellCompleted = document.createElement( "treecell" );
      var treeCellPriority  = document.createElement( "treecell" );
      var treeCellTitle     = document.createElement( "treecell" );
      var treeCellStartdate = document.createElement( "treecell" );
      var treeCellDuedate   = document.createElement( "treecell" );
      var treeCellPercentcomplete = document.createElement( "treecell" );
      var treeCellCompleteddate = document.createElement( "treecell" );
      var treeCellCategories = document.createElement( "treecell" );

      var now = new Date();
      
      var thisMorning = new Date( now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0 );

      if(treeItem.getElementsByTagName( "treerow" )[0])
        treeItem.removeChild( treeItem.getElementsByTagName( "treerow" )[0] );

      var textProperties = "";
      if( calendarToDo.start.getTime() <= thisMorning.getTime() )
      {
         //this task should be started
         textProperties = textProperties + " started";
      }

      var completed = calendarToDo.completed.getTime();

      if( completed > 0 )
      {
         /* for setting some css */
         var completedDate = new Date( calendarToDo.completed.getTime() );
         var FormattedCompletedDate = formatUnifinderEventDate( completedDate );

         treeCellCompleteddate.setAttribute( "label", FormattedCompletedDate );
         textProperties = textProperties + " completed";
         treeItem.setAttribute( "checked", "true" );
      }
      else
         treeCellCompleteddate.setAttribute( "label", "" );
         

      treeCellTitle.setAttribute( "label", calendarToDo.title );

      var startDate     = new Date( calendarToDo.start.getTime() );
      var dueDate = new Date( calendarToDo.due.getTime() );

      var tonightMidnight = new Date( now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 59 );

      if( now.getTime() > tonightMidnight.getTime() )
      {
         /* for setting some css */
         textProperties = textProperties + " overdue";
      } else
      {
         textProperties = textProperties + " inprogress";
      }
      if(calendarToDo.priority > 0 && calendarToDo.priority < 5)
         textProperties = textProperties + " highpriority";
      if(calendarToDo.priority > 5 && calendarToDo.priority < 10)
         textProperties = textProperties + " lowpriority";


      var FormattedStartDate = formatUnifinderEventDate( startDate );
      var FormattedDueDate = formatUnifinderEventDate( dueDate );

      treeCellStartdate.setAttribute( "label", FormattedStartDate );
      treeCellDuedate.setAttribute( "label", FormattedDueDate );
      treeCellPercentcomplete.setAttribute( "label", calendarToDo.percent + "%" );
      treeCellCategories.setAttribute( "label", calendarToDo.categories );

      treeItem.setAttribute( "taskitem", "true" );
      
      treeRow.setAttribute("properties", textProperties);
      treeCellCompleted.setAttribute("properties", textProperties);
      treeCellPriority.setAttribute("properties", textProperties);
      treeCellTitle.setAttribute("properties", textProperties);
      treeCellStartdate.setAttribute("properties", textProperties);
      treeCellDuedate.setAttribute("properties", textProperties);
      treeCellCompleteddate.setAttribute("properties", textProperties);
      treeCellPercentcomplete.setAttribute("properties", textProperties);
      treeCellCategories.setAttribute("properties", textProperties);
      
      treeRow.appendChild( treeCellCompleted );
      treeRow.appendChild( treeCellPriority );
      treeRow.appendChild( treeCellTitle );
      treeRow.appendChild( treeCellStartdate );
      treeRow.appendChild( treeCellDuedate );
      treeRow.appendChild( treeCellCompleteddate );
      treeRow.appendChild( treeCellPercentcomplete );
      treeRow.appendChild( treeCellCategories );
      treeItem.appendChild( treeRow );
}


/**
*  Redraw the categories unifinder tree
*/

function refreshToDoTree( eventArray )
{
   // get the old tree children item and remove it
   
   var tree = document.getElementById( ToDoUnifinderTreeName );

   var elementsToRemove = document.getElementsByAttribute( "taskitem", "true" );
   
   
   for( var i = 0; i < elementsToRemove.length; i++ )
   {
      elementsToRemove[i].parentNode.removeChild( elementsToRemove[i] );
   }

   // add: tree item, row, cell, box and text items for every event
   for( var index = 0; index < eventArray.length; ++index )
   {
      var calendarToDo = eventArray[ index ];
      
      // make the items
      
      var treeItem = document.createElement( "treeitem" );
      
      setUnifinderToDoTreeItem( treeItem, calendarToDo );

      tree.getElementsByTagName( "treechildren" )[0]. appendChild( treeItem );
   }  
}


/**
*  Redraw a single item of the ToDo unifinder tree
*/

function toDoUnifinderItemUpdate( calendarToDo )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );

   var elementsToRemove = document.getElementsByAttribute( "taskitem", "true" );

   for( var i = 0; i < elementsToRemove.length; i++ )
   {
      if(elementsToRemove[i].getAttribute("toDoID") == calendarToDo.id)
      {
         setUnifinderToDoTreeItem( elementsToRemove[i], calendarToDo );
      }
   }
}
