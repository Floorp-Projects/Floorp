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


/***** oe-datepicker
* AUTHOR
*   Garth Smedley
*
* NOTES
*   Date picker popup. 
*   Closes when a day is clicked, and passes the picked date into your oncommand function.
*
* To use the datepicker include the overlay in your XUL, this .js file is included from there

      <?xul-overlay href="chrome://penglobal/content/datepicker-overlay.xul"?>
*
* Make a popupset in your XUL that includes the picker popup from the overlay
*
     <popupset>
       <popup  id="oe-date-picker-popup"  oncommand="yourCommandFunction( this )" >
       </popup>
     </popupset>
* 
* Attach the date picker to an item using the popup attribute, an image for example:
* 
      <image class="four-state-image-button" popup="oe-date-picker-popup" onmousedown="yourPrepareDatePickerFunction()"  />
 
* 
* In your .js, initialize the picker with the inital date when the user clicks, set to now in this example:
* 
      function yourPrepareDatePickerFunction( )
      {
         var datePickerPopup = document.getElementById( "oe-date-picker-popup" );
         
         datePickerPopup.setAttribute( "value", new Date() );
      }
* 
* In your .js, respond to the user picking a date, this function will not be called if they just click 
* out of the picker
* 
      function yourCommandFunction( datepopup )
      {
         var newDate =  datepopup.value;
         
         // datepopup.value is a Date object with 
         // the year, month, day set to the user selection
      }
 
* IMPLEMENTATION NOTES 
*
*  In order to prevent name space pollution, all of the date picker methods
*  are static members of the oeDatePicker class.
*
**********
*/

 
 
/**
*   Date picker class, has static members only.
*/

 function oeDatePicker()
 {
 
 }
 
/**
*   Static variables
*/


/** The popup window containing the picker */
 oeDatePicker.gPopup  = null;
 
 
/** The original starting date and currently selected date */
 oeDatePicker.gOriginalDate = null; 
 oeDatePicker.gSelectedDate = null; 
 
 /* selected items */
 oeDatePicker.gSelectedMonthItem = null;
 oeDatePicker.gSelectedDayItem = null;
 
 
 
/**
*   Set up the picker, called when the popup pops
*/

oeDatePicker.onpopupshowing = function( popup )
{
   // remember the popup item so we can close it when we are done
   
   oeDatePicker.gPopup = popup;
   
   // get the start date from the popup value attribute and select it
   
   var startDate = popup.getAttribute( "value" );
   
   oeDatePicker.gOriginalDate = new Date( startDate );
   oeDatePicker.gSelectedDate = new Date( startDate );
   
   // Avoid problems when changing months if the date is at the end of the month
   // i.e. if date is 31 march and you do a setmonth to april, the month would 
   // actually be set to may, beacause april only has 30 days.
   // This is why we keep the original date around.
   
   oeDatePicker.gSelectedDate.setDate( 1 );   
    
   // draw the year based on the selected date
   
   oeDatePicker.redrawYear();
   
   // draw the month based on the selected date
   
   var month = oeDatePicker.gSelectedDate.getMonth() + 1;
   var selectedMonthBoxItem = document.getElementById( "oe-date-picker-year-month-" + month + "-box"  );
   
   oeDatePicker.selectMonthItem( selectedMonthBoxItem );
   
   // draw in the days for the selected date
   
   oeDatePicker.redrawDays();
}
 
 
/**
*   Called when a day is clicked, close the picker and call the client's oncommand
*/
 
 
oeDatePicker.clickDay = function( newDayItemNumber )
{
   // get the clicked day
     
   var dayNumberItem = document.getElementById( "oe-date-picker-month-day-text-" + newDayItemNumber );
   
   var dayNumber = dayNumberItem.getAttribute( "value" );
   
   // they may have clicked an unfilled day, if so ignore it and leave the picker up
   
   if( dayNumber != "" )
   {
      // set the selected date to what they cliked on
      
      oeDatePicker.gSelectedDate.setDate( dayNumber );
      
      oeDatePicker.selectDate();

      oeDatePicker.gPopup.closePopup();
   }
}


oeDatePicker.selectDate = function()
{
   // We copy the picked date to avoid problems with changing the Date object in place
      
   var pickedDate = new Date( oeDatePicker.gSelectedDate );
   
   // put the selected date in the popup item's value property
   
   oeDatePicker.gPopup.value = pickedDate;
   
   // get the client oncommand function, call it if there is one
   
   var commandEventMethod = oeDatePicker.gPopup.getAttribute( "oncommand" );
   
   if( commandEventMethod != null )
   {
      // set up a variable date, that will be avaialable from within the 
      // client method
   
      var date = pickedDate;
   
      // Make the function a member of the popup before calling it so that 
      // 'this' will be the popup
      
      oeDatePicker.gPopup.oeDatePickerFunction =  function() { eval( commandEventMethod ); };
      
      oeDatePicker.gPopup.oeDatePickerFunction();
   }
   
   // close the popup
   
   //oeDatePicker.gPopup.closePopup ();

}


/**
* Called when a month box is clicked 
*/

oeDatePicker.clickMonth = function( newMonthItem, newMonthNumber )
{
   // already selected, return
   
   if( oeDatePicker.gSelectedMonthItem  == newMonthItem )
   {
      return;
   }
   
   // update the selected date
   
   oeDatePicker.gSelectedDate.setMonth( newMonthNumber - 1 );
   
   // select Month
   
   oeDatePicker.selectMonthItem( newMonthItem );
 
   // redraw days
   
   oeDatePicker.redrawDays();

   oeDatePicker.selectDate();
}
 

/**
* Called when previous Year button is clicked 
*/

oeDatePicker.previousYearCommand = function()
{
   // update the selected date
   
   var oldYear = oeDatePicker.gSelectedDate.getFullYear(); 
   oeDatePicker.gSelectedDate.setFullYear( oldYear - 1 ); 
   
   // redraw the year and the days
   
   oeDatePicker.redrawYear();
   oeDatePicker.redrawDays();

   oeDatePicker.selectDate();
}


/**
* Called when next Year button is clicked 
*/

oeDatePicker.nextYearCommand = function()
{
   // update the selected date
   
   var oldYear = oeDatePicker.gSelectedDate.getFullYear(); 
   oeDatePicker.gSelectedDate.setFullYear( oldYear + 1 ); 
   
   // redraw the year and the days
   
   oeDatePicker.redrawYear();
   oeDatePicker.redrawDays();

   oeDatePicker.selectDate();
}
 

/**
* Draw the year based in the selected date 
*/

oeDatePicker.redrawYear = function()
{
   var yearTitleItem = document.getElementById( "oe-date-picker-year-title-text" );
   yearTitleItem.setAttribute( "value", oeDatePicker.gSelectedDate.getFullYear() );
}


/**
* Select a month box 
*/

oeDatePicker.selectMonthItem = function( newMonthItem )
{
   // clear old selection, if there is one
   
   if( oeDatePicker.gSelectedMonthItem != null )
   {
      oeDatePicker.gSelectedMonthItem.setAttribute( "selected" , false );
   }

   // Set the selected attribute, used to give it a different style
   
   newMonthItem.setAttribute( "selected" , true );
      
   // Remember new selection
  
  oeDatePicker.gSelectedMonthItem = newMonthItem;

}


/**
* Select a day box 
*/

oeDatePicker.selectDayItem = function( newDayItem )
{
   // clear old selection, if there is one
   
   if( oeDatePicker.gSelectedDayItem != null )
   {
      oeDatePicker.gSelectedDayItem.setAttribute( "selected" , false );
   }

   if( newDayItem != null )
   {
      // Set the selected attribute, used to give it a different style
      
      newDayItem.setAttribute( "selected" , true );
   }
         
   // Remember new selection
      
   oeDatePicker.gSelectedDayItem = newDayItem;

}


/**
* Redraw day numbers based on the selected date
*/

oeDatePicker.redrawDays = function( )
{
  // Write in all the day numbers
   
   var firstDate = new Date( oeDatePicker.gSelectedDate.getFullYear(), oeDatePicker.gSelectedDate.getMonth(), 1 );
   var firstDayOfWeek = firstDate.getDay();
   
   var lastDayOfMonth = DateUtils.getLastDayOfMonth( oeDatePicker.gSelectedDate.getFullYear(), oeDatePicker.gSelectedDate.getMonth() )
   
   
   // clear the selected day item
   
   oeDatePicker.selectDayItem( null );
   
   // redraw each day bax in the 7 x 6 grid
   
   var dayNumber = 1;
   
   for( var dayIndex = 0; dayIndex < 42; ++dayIndex )
   {
      // get the day text box
      
      var dayNumberItem = document.getElementById( "oe-date-picker-month-day-text-" + (dayIndex + 1) );
      
      // if it is an unfilled day ( before first or after last ), just set its value to "",
      // and don't increment the day number.
      
      if( dayIndex < firstDayOfWeek || dayNumber > lastDayOfMonth )
      {
         dayNumberItem.setAttribute( "value" , "" );  
      }
      else
      {
         // set the value to the day number
         
         dayNumberItem.setAttribute( "value" , dayNumber );
         
         // draw the day as selected, if the Original Day is visible
         
         if( dayNumber == oeDatePicker.gOriginalDate.getDate() &&
             oeDatePicker.gSelectedDate.getYear() == oeDatePicker.gOriginalDate.getYear() && 
             oeDatePicker.gSelectedDate.getMonth() == oeDatePicker.gOriginalDate.getMonth() ) 
         {
            var dayNumberBoxItem = document.getElementById( "oe-date-picker-month-day-" + (dayIndex + 1) + "-box"  );
            oeDatePicker.selectDayItem( dayNumberBoxItem );
         }
   
         // advance the day number
         
         ++dayNumber;  
      }
   }
   
 }
