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
 *                 Curtis Jewell <csjewell@mail.freeshell.org>
 *                 Eric Belhaire <eric.belhaire@ief.u-psud.fr>
 *			          Mark Swaffer <swaff@fudo.org>
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

var gTaskArray = new Array();

/**
*   Observer for the calendar event data source. This keeps the unifinder
*   display up to date when the calendar event data is changed
*/

var unifinderToDoDataSourceObserver =
{
   onLoad   : function()
   {
      toDoUnifinderRefresh();
   },
   
   onStartBatch   : function()
   {
   },
    
   onEndBatch   : function()
   {
      toDoUnifinderRefresh();
   },
    
   onAddItem : function( calendarToDo )
   {
      toDoUnifinderRefresh();
   },

   onModifyItem : function( calendarToDo, originalToDo )
   {
      toDoUnifinderRefresh( );
   },

   onDeleteItem : function( calendarToDo )
   {
      if( !gICalLib.batchMode )
      {
         toDoUnifinderRefresh();
      }
   },

   onAlarm : function( calendarToDo )
   {
      
   },

   onError : function()
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
*   Helper function to display todo datetimes in the unifinder
*/

function formatUnifinderToDoDateTime( datetime )
{
  return gCalendarWindow.dateFormater.formatDateTime( datetime, true );
}

/**
*   Called by event observers to update the display
*/

function toDoUnifinderRefresh()
{
   var Checked = document.getElementById( "only-completed-checkbox" ).checked;
   
   gICalLib.resetFilter();
      
   if( Checked === true )
      gICalLib.filter.completed.setTime( new Date() );
   
   var taskTable = gEventSource.getAllToDos();
   
   refreshToDoTree( taskTable );
}

function getToDoFromEvent( event )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );

   var row = tree.treeBoxObject.getRowAt( event.clientX, event.clientY );

   if( row != -1 && row < tree.view.rowCount )
   { 
      return( tree.taskView.getCalendarTaskAtRow( row ) );
   }
   return false;
}

function getSelectedToDo()
{
   var tree = document.getElementById( ToDoUnifinderTreeName );
   // .toDo object sometimes isn't available?
   return( tree.taskView.getCalendarTaskAtRow(tree.currentIndex) );
   
}

/**
*  This is attached to the onclik attribute of the to do list shown in the unifinder
*/

function modifyToDoCommand( event )
{
   // we only care about button 0 (left click) events
   if (event.button != 0) return;

   //open the edit todo dialog box
   var ThisToDo = getToDoFromEvent( event );
   
   if( ThisToDo )
      editToDo( ThisToDo );
   else
     newToDoCommand();
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
      document.getElementById( "delete_todo_command" ).removeAttribute( "disabled" );
      document.getElementById( "print_command" ).setAttribute( "disabled", "true" );
   } else
   {
      if(event.button == 2)
          treechildren.setAttribute("context", "context-menu");
      tree.view.selection.clearSelection();

      // TODO HACK notifiers should be rewritten to integrate events and todos
      document.getElementById( "delete_todo_command" ).setAttribute( "disabled", "true" );
      //  printing tasks not supported
      document.getElementById( "print_command" ).setAttribute( "disabled", "true" );
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


/*
This function return the progress state of a ToDo task :
completed, overdue, duetoday, inprogress, future
 */
function ToDoProgressAtom( calendarToDo )
{
      var now = new Date();
   
      var thisMorning = new Date( now.getFullYear(), now.getMonth(), now.getDate(), 0, 0, 0 );
      
      var completed = calendarToDo.completed.getTime();
      
      if( completed > 0 )
      {
         return("completed");
      }
      
      var startDate     = new Date( calendarToDo.start.getTime() );
      var dueDate       = new Date( calendarToDo.due.getTime() );
      var tonightMidnight = new Date( now.getFullYear(), now.getMonth(), now.getDate(), 23, 59, 00 );
      
      if( thisMorning.getTime() > dueDate.getTime() )
      {
         return("overdue");
      } else 
	if ( dueDate.getFullYear() == now.getFullYear() &&
                  dueDate.getMonth() == now.getMonth() &&
                  dueDate.getDate() == now.getDate() )
      {
	    return("duetoday");
      } else
	if( tonightMidnight.getTime() < startDate.getTime() )
	{
	    return("future");
      }

      else
      {
	    return("inprogress");
      }
}

var toDoTreeView =
{
   rowCount : gEventArray.length,
   selectedColumn : null,
   sortDirection : null,
   sortStartedTime : new Date().getTime(), // updated just before sort

   isContainer : function(){return false;},
   // By getCellProperties, the properties defined with 
   // treechildren:-moz-tree-cell-text in CSS are used.
   // It is use here to color a particular row with a color
   // given by the progress state of the ToDo task.
   getCellProperties : function( row,column, props )
   { 
      calendarToDo = gTaskArray[row];
      var aserv=Components.classes["@mozilla.org/atom-service;1"].createInstance(Components.interfaces.nsIAtomService);

      // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
      if( (typeof(column)=="object" ? column.id : column) == "unifinder-todo-tree-col-priority" )
      {
      if(calendarToDo.priority > 0 && calendarToDo.priority < 5)
         props.AppendElement(aserv.getAtom("highpriority"));
      if(calendarToDo.priority > 5 && calendarToDo.priority < 10)
         props.AppendElement(aserv.getAtom("lowpriority"));                                                  
      }
      props.AppendElement(aserv.getAtom(ToDoProgressAtom(calendarToDo)));
   },
   getColumnProperties : function(){return false;},
   // By getRowProperties, the properties defined with 
   // treechildren:-moz-tree-row in CSS are used.
   // It is used here to color the background of a selected
   // ToDo task with a color
   // given by  the progress state of the ToDo task.
   getRowProperties : function( row,props ){
      calendarToDo = gTaskArray[row];
      
      var aserv=Components.classes["@mozilla.org/atom-service;1"].createInstance(Components.interfaces.nsIAtomService);
      props.AppendElement(aserv.getAtom(ToDoProgressAtom( calendarToDo )));
   },
   isSorted : function(){return false;},
   isEditable : function(){return true;},
   isSeparator : function(){return false;},
   // Return the empty string in order 
   // to use moz-tree-image pseudoelement : 
   // it is mandatory to return "" and not false :-(
   getImageSrc : function(){return("");},
   cycleCell : function(row,col)
   {
    calendarToDo = gTaskArray[row];
    if( !calendarToDo ) return;

    // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
    if( (typeof(col)=="object" ? col.id : col) == "unifinder-todo-tree-col-completed")  {
	  var completed = calendarToDo.completed.getTime();
	  
	  if( completed > 0 )
	  checkboxClick( calendarToDo, false ) ;
	  else 
	  checkboxClick( calendarToDo, true ) ;
	}
   },
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
      this.sortStartedTime = new Date().getTime(); // for null dates during sort
      gTaskArray.sort( compareTasks );

      document.getElementById( ToDoUnifinderTreeName ).view = this;
   },
   setTree : function( tree ){this.tree = tree;},
   getCellText : function(row,column)
   {
      calendarToDo = gTaskArray[row];
      if( !calendarToDo )
         return false;

      // Moz1.8 trees require column.id, moz1.7 and earlier trees use column.
      switch( typeof(column)=="object" ? column.id : column )
      {
         case "unifinder-todo-tree-col-completed":
            return( "" );
         
         case "unifinder-todo-tree-col-priority":
            return( "" );

         case "unifinder-todo-tree-col-title":
            var titleText;
            if( calendarToDo.title == "" )
               titleText = gCalendarBundle.getString( "eventUntitled" );
            else  
               titleText = calendarToDo.title;
            return( titleText );
         case "unifinder-todo-tree-col-startdate":
            return( formatUnifinderToDoDateTime( new Date( calendarToDo.start.getTime() ) ) );
         case "unifinder-todo-tree-col-duedate":
            return( formatUnifinderToDoDateTime( new Date( calendarToDo.due.getTime() ) ) );
         case "unifinder-todo-tree-col-completeddate":
            return( formatUnifinderToDoDateTime( new Date( calendarToDo.completed.getTime() ) ) );
         case "unifinder-todo-tree-col-percentcomplete":
            return( calendarToDo.percent+"%" );
         case "unifinder-todo-tree-col-categories":
            return( calendarToDo.categories );
         default:
            return false;
      }
   }
}

function compareTasks( taskA, taskB )
{
   var modifier = 1;
   if (toDoTreeView.sortDirection == "descending")
	{
		modifier = -1;
	}

   switch(toDoTreeView.selectedColumn)
   {
      case "unifinder-todo-tree-col-priority":  // 0-9
         return compareNumber(taskA.priority, taskB.priority) * modifier;
   
      case "unifinder-todo-tree-col-title":
         return compareString(taskA.title, taskB.title) * modifier;
   
      case "unifinder-todo-tree-col-startdate":
         return compareDate(taskA.start, taskB.start) * modifier;
   
      case "unifinder-todo-tree-col-duedate":
         return compareDate(taskA.due, taskB.due) * modifier;
   
      case "unifinder-todo-tree-col-completed": // checkbox if date exists
      case "unifinder-todo-tree-col-completeddate":
         return compareDate(taskA.completed, taskB.completed) * modifier;
      
      case "unifinder-todo-tree-col-percentcomplete":
         return compareNumber(taskA.percent, taskB.percent) * modifier;
   
      case "unifinder-todo-tree-col-categories":
         return compareString(taskA.categories, taskB.categories) * modifier;

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

function compareNumber(a, b) {
  // Number converts a date to msecs since 1970, and converts a null to 0.
  a = Number(a); 
  b = Number(b);
  return ((a < b) ? -1 :      // avoid underflow problems of subtraction
          (a > b) ?  1 : 0); 
}
function compareDate(a, b) {
  a = dateToMilliseconds(a);
  b = dateToMilliseconds(b);
  return ((a < b) ? -1 :      // avoid underflow problems of subtraction
          (a > b) ?  1 : 0); 
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


function calendarTaskView( taskArray )
{
   this.taskArray = taskArray;   
}

calendarTaskView.prototype.getCalendarTaskAtRow = function( i )
{
   return( gTaskArray[ i ] );
}

calendarTaskView.prototype.getRowOfCalendarTask = function( Task )
{
   for( var i = 0; i < this.gTaskArray.length; i++ )
   {
      if( this.gTaskArray[i].id == Event.id )
         return( i );
   }
   return( "null" );
}

/**
*  Redraw the categories unifinder tree
*/

function refreshToDoTree( taskArray )
{
   if( taskArray === false )
      taskArray = getTaskTable();

   gTaskArray = taskArray;

   toDoTreeView.rowCount = gTaskArray.length;
   
   var ArrayOfTreeCols = document.getElementById( ToDoUnifinderTreeName ).getElementsByTagName( "treecol" );
   
   for( var i = 0; i < ArrayOfTreeCols.length; i++ )
   {
      if( ArrayOfTreeCols[i].getAttribute( "sortActive" ) == "true" )
      {
         toDoTreeView.selectedColumn = ArrayOfTreeCols[i].getAttribute( "id" );
         toDoTreeView.sortDirection = ArrayOfTreeCols[i].getAttribute("sortDirection");
         toDoTreeView.sortStartedTime = new Date().getTime(); //for null/0 dates
         gTaskArray.sort(compareTasks);
         break;
      }
   }

   document.getElementById( ToDoUnifinderTreeName ).view = toDoTreeView;

   document.getElementById( ToDoUnifinderTreeName ).taskView = new calendarTaskView( gTaskArray );
}


function getTaskTable( )
{
   var taskTable;

   gICalLib.resetFilter();
      
   if( document.getElementById( "only-completed-checkbox" ).getAttribute( "checked" ) == "true" )
   {
      var now = new Date();

      gICalLib.filter.completed.setTime( now );
   }

   taskTable = gEventSource.getAllToDos();
   
   return( taskTable );
}


function contextChangeProgress( event, Progress )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );

   if (tree.view.selection.count > 0)
   {
      var toDoItem = tree.taskView.getCalendarTaskAtRow( tree.currentIndex );
      if(toDoItem)
      {
         toDoItem.percent = Progress;
         gICalLib.modifyTodo( toDoItem );
      }
   }
}


function contextChangePriority( event, Priority )
{
   var tree = document.getElementById( ToDoUnifinderTreeName );

   if (tree.view.selection.count > 0)
   {
      var toDoItem = tree.taskView.getCalendarTaskAtRow( tree.currentIndex );
      if(toDoItem)
      {
         toDoItem.priority = Priority;
         gICalLib.modifyTodo( toDoItem );
      }
   }
}

function changeToolTipTextForToDo( event )
{
   var toDoItem = getToDoFromEvent( event );

   var Html = document.getElementById( "taskTooltip" );

   while( Html.hasChildNodes() )
   {
      Html.removeChild( Html.firstChild ); 
   }
   var HolderBox = getPreviewTextForTask( toDoItem ) ;
   if( HolderBox ) Html.appendChild( HolderBox ) ;
}

function changeContextMenuForToDo( event )
{
   var toDoItem = getToDoFromEvent( event );
   
   if( toDoItem )
   {
      var liveList = document.getElementById( "taskitem-context-menu" ).getElementsByAttribute( "checked", "true" );
      // Delete in reverse order.  Moz1.8+ getElementsByAttribute list is
      // 'live', so when attribute is deleted the indexes of later elements
      // change, but Moz1.7- is not.  Reversed order works with both.
      for (var i = liveList.length - 1; i >= 0; i-- )
      {
         liveList.item(i).removeAttribute( "checked" );
      }

      if( document.getElementById( "percent-"+toDoItem.percent+"-menuitem" ) )
      {
         document.getElementById( "percent-"+toDoItem.percent+"-menuitem" ).setAttribute( "checked", "true" );
      }
   
      if( document.getElementById( "priority-"+toDoItem.priority+"-menuitem" ) )
      {
         document.getElementById( "priority-"+toDoItem.priority+"-menuitem" ).setAttribute( "checked", "true" );
      }
   }
}

