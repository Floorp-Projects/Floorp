/* 
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
 * The Original Code is OEone Corporation.
 *
 * The Initial Developer of the Original Code is
 * OEone Corporation.
 * Portions created by OEone Corporation are Copyright (C) 2001
 * OEone Corporation. All Rights Reserved.
 *
 * Contributor(s): Garth Smedley (garths@oeone.com) , Mike Potter (mikep@oeone.com)
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
 * 
*/

/**
*  This is called from the unifinder when they need a new category
*/

function unifinderNewCategoryCommand( )
{
   var args = new Object();

   args.mode = "new";

   args.Category = gCategoryManager.getNewCategory();

   args.onOk = respondNewCategoryCommand;

   calendar.openDialog( "caNewFolder", "chrome://calendar/content/calendarFolderDialog.xul", true, args );
   
}


/**
*  This is called from the unifinder when they need a new category
*/

function respondNewCategoryCommand( Category )
{
   gCategoryManager.addCategory( Category.name );
}




/*
** launches the add catgory dialog.
*/ 

function launchEditCategoryDialog( Category )
{
   var args = new Object();

   args.mode = "edit";

   args.Category = Category;

   args.onOk = editCategoryDialogResponse;

   calendar.openDialog( "caNewFolder", "chrome://calendar/content/calendarFolderDialog.xul", true, args );
   
}

/*
** Function that actually adds the category.
*/

function editCategoryDialogResponse( Category )
{
   gCategoryManager.modifyCategory( Category );
}




/**
*    Called when the calendar is loaded.
*    Sets up the category observers
*/
function prepareCalendarCategories()
{
   gCategoryManager.addObserver( calendarCategoryObserver );
}

function finishCalendarCategories()
{
   gCategoryManager.removeObserver( calendarCategoryObserver );
}  


var calendarCategoryObserver =
{
   onAddItem : function( calendarCategory )
   {
      refreshEventTree( gEventSource.getAllEvents(), "unifinder-categories-tree-children" ); 
   },

   onModifyItem : function( calendarCategory )
   {
      refreshEventTree( gEventSource.getAllEvents(), "unifinder-categories-tree-children" );
   },

   onDeleteItem : function( calendarCategory )
   {
      refreshEventTree( gEventSource.getAllEvents(), "unifinder-categories-tree-children" );
   }
}
