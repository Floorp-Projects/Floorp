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
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is
 * Jussi Kukkonen (jussi.kukkonen@welho.com).
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/**
*   Window variables
*/

var args;
  
/**
*   Called when the dialog is loaded (which is done by Outlook-CSV parser when
*   Outlook CSV data.is not in english or it is otherwise impossible to 
*   decipher without user input.
*/
function loadOutlookImportDialog()
{  
  args = window.arguments[0];
  
  var menuList;
  var k;

  //fill all menupopups
  menuList = document.getElementById( "title-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("titleIndex" in args)
    menuList.selectedIndex = args.titleIndex;
  menuList.focus();
  
  menuList = document.getElementById( "startdate-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("startDateIndex" in args)
    menuList.selectedIndex = args.startDateIndex;
  
  menuList = document.getElementById( "starttime-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("startTimeIndex" in args)
    menuList.selectedIndex = args.startTimeIndex;

  menuList = document.getElementById( "enddate-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("endDateIndex" in args)
    menuList.selectedIndex = args.endDateIndex;

  menuList = document.getElementById( "endtime-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("endTimeIndex" in args)
    menuList.selectedIndex = args.endTimeIndex;

  menuList = document.getElementById( "location-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("locationIndex" in args)
    menuList.selectedIndex = args.locationIndex;
  
  menuList = document.getElementById( "description-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("descriptionIndex" in args)
    menuList.selectedIndex = args.descriptionIndex;

  menuList = document.getElementById( "allday-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("allDayIndex" in args)
    menuList.selectedIndex = args.allDayIndex;

  menuList = document.getElementById( "private-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("privateIndex" in args)
    menuList.selectedIndex = args.privateIndex;

  menuList = document.getElementById( "alarm-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("alarmIndex" in args)
    menuList.selectedIndex = args.alarmIndex;

  menuList = document.getElementById( "alarmdate-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("alarmDateIndex" in args)
    menuList.selectedIndex = args.alarmDateIndex;

  menuList = document.getElementById( "alarmtime-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("alarmTimeIndex" in args)
    menuList.selectedIndex = args.alarmTimeIndex;

  menuList = document.getElementById( "categories-list" );
  for( k = 0; k < args.fieldList.length; k++ )
    menuList.appendItem( args.fieldList[k] );
  if ("categoriesIndex" in args)
    menuList.selectedIndex = args.categoriesIndex;

  opener.setCursor( "auto" );
}

/**
*   Called when the OK button is clicked.
*/
function onOKCommand()
{
  // update indexes with user selections
  args.titleIndex       = document.getElementById( "title-list" ).selectedIndex;
  args.startDateIndex   = document.getElementById( "startdate-list" ).selectedIndex;
  args.startTimeIndex   = document.getElementById( "starttime-list" ).selectedIndex;
  args.endDateIndex     = document.getElementById( "enddate-list" ).selectedIndex;
  args.endTimeIndex     = document.getElementById( "endtime-list" ).selectedIndex;
  args.allDayIndex      = document.getElementById( "allday-list" ).selectedIndex;
  args.privateIndex     = document.getElementById( "private-list" ).selectedIndex;
  args.alarmIndex       = document.getElementById( "alarm-list" ).selectedIndex;
  args.alarmDateIndex   = document.getElementById( "alarmdate-list" ).selectedIndex;
  args.alarmTimeIndex   = document.getElementById( "alarmtime-list" ).selectedIndex;
  args.categoriesIndex  = document.getElementById( "categories-list" ).selectedIndex;
  args.descriptionIndex = document.getElementById( "description-list" ).selectedIndex;
  args.locationIndex    = document.getElementById( "location-list" ).selectedIndex;
  args.privateIndex     = document.getElementById( "private-list" ).selectedIndex;
  args.cancelled        = false;
  return true;
}

/**
*   Called when the Cancel button is clicked.
*/
function onCancelCommand()
{
  args.cancelled = true;
  return true;
}
