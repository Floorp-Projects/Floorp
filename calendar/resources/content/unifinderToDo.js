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

const ToDoUnifinderTreeName = "unifinder-todo-listbox";

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
      toDoUnifinderRefesh();
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
   var eventTable = gEventSource.getAllToDos();
   
   refreshToDoTree( eventTable );
}


/**
*  This is attached to the onclik attribute of the events shown in the unifinder
*/

function unifinderClickToDo( event )
{
}

/**
*  This is attached to the onclik attribute of the to do list shown in the unifinder
*/

function unifinderDoubleClickToDo( event )
{
   //open the edit to do dialog box

   var ThisToDo = event.currentTarget.toDo;
   
   editToDo( ThisToDo );

}

function checkboxClick( event )
{
   var ThisToDo = event.currentTarget.parentNode.parentNode.toDo;
   
   if( event.currentTarget.checked == true )
   {
      var completedTime = new Date();
      
      ThisToDo.completed.setTime( completedTime );
   }
   else
      ThisToDo.completed.clear();

   gICalLib.modifyTodo( ThisToDo );

   toDoUnifinderRefesh();
}


/**
*  This is called from the unifinder's edit command
*/

function unifinderEditCommand()
{
}


/**
*  This is called from the unifinder's delete command
*/

function unifinderDeleteCommand( DoNotConfirm )
{
   
}


/**
*  Redraw the categories unifinder tree
*/

function refreshToDoTree( eventArray )
{
   // get the old tree children item and remove it
   
   var oldTreeChildren = document.getElementById( ToDoUnifinderTreeName );

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
      var oldTreeItems = document.getElementsByAttribute( "name", "sample-todo-listitem" );

      var oldTreeItem = oldTreeItems[0];
      
      var treeItem = oldTreeItem.cloneNode( true );
      
      treeItem.toDo = calendarToDo;

      var completed = calendarToDo.completed.getTime();

      if( completed > 0 )
         treeItem.getElementsByTagName( "checkbox" )[0].checked = true;

      treeItem.getElementsByAttribute( "name", "title-listcell" )[0].setAttribute( "label", calendarToDo.title );

      var dueDate = new Date( calendarToDo.due.getTime() );

      var FormattedDueDate = formatUnifinderEventDate( dueDate );

      treeItem.getElementsByAttribute( "name", "due-date-listcell" )[0].setAttribute( "label", FormattedDueDate );

      treeItem.removeAttribute( "collapsed" );

      treeItem.setAttribute( "taskitem", "true" );

      oldTreeChildren.appendChild( treeItem );
   }  
}
