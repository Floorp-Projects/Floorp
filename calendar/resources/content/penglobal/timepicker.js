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


/***** oe-timepicker
* AUTHOR
*   Garth Smedley
*
* NOTES
*   Time picker popup. 
*   Closes when a minute is clicked, and passes the picked time into your oncommand function.
*
* To use the timepicker include the overlay in your XUL, this .js file is included from there

      <?xul-overlay href="chrome://penglobal/content/timepicker-overlay.xul"?>
*
* Make a popupset in your XUL that includes the picker popup from the overlay
*
     <popupset>
       <popup  id="oe-time-picker-popup"  oncommand="yourCommandFunction( this )" >
       </popup>
     </popupset>
* 
* Attach the time picker to an item using the popup attribute, an image for example:
* 
      <image class="four-state-image-button" popup="oe-time-picker-popup" onmousedown="yourPrepareTimePickerFunction()"  />
 
* 
* In your .js, initialize the picker with the inital time when the user clicks, set to now in this example:
* 
      function yourPrepareTimePickerFunction( )
      {
         var timePickerPopup = document.getElementById( "oe-time-picker-popup" );
         
         timePickerPopup.setAttribute( "value", new Date() );
      }
* 
* In your .js, respond to the user picking a time, this function will not be called if they just click 
* out of the picker
* 
      function yourCommandFunction( timepopup )
      {
         var newTime =  timepopup.value;
         
         // timepopup.value is a Date object with 
         // the hours, minutes and seconds set to the user selection
      }
 
* IMPLEMENTATION NOTES 
*
*  In order to prevent name space pollution, all of the time picker methods
*  are static members of the oeTimePicker class.
*
**********
*/

 
 
/**
*   Time picker class, has static members only.
*/

function oeTimePicker()
{

}


/**
*   Static variables
*/


/** The popup window containing the picker */
oeTimePicker.gPopup  = null;

/** The currently selected time */
oeTimePicker.gSelectedTime = new Date(); 

/** The selected Am/pm, selected hour and selected minute items */
oeTimePicker.gSelectedAmPmItem = null;
oeTimePicker.gSelectedHourItem = null;
oeTimePicker.gSelectedMinuteItem = null;

/** constants use to specify one anf five minue view */
oeTimePicker.kMinuteView_Five = 5;
oeTimePicker.kMinuteView_One = 1;

/**
*   Set up the picker, called when the popup pops
*/

oeTimePicker.onpopupshowing = function( popup )
{
   // remember the popup
    
   oeTimePicker.gPopup = popup;
   
   // if there is a Date object in the popup item's value attribute, use it,
   // otherwise use Now.
   
   var inputTime = oeTimePicker.gPopup.getAttribute( "value" );
   
   if( inputTime )
   {
      oeTimePicker.gSelectedTime = new Date( inputTime );
   }
   else
   {
      oeTimePicker.gSelectedTime = new Date();
   }
      
   // Select the AM or PM item based on whether the hour is 0-11 or 12-23 
   
   var hours24 = oeTimePicker.gSelectedTime.getHours();
   
   if( oeTimePicker.isTimeAm( oeTimePicker.gSelectedTime ) )
   {
      var amPmItem = document.getElementById( "oe-time-picker-am-box" );
      var hours12 = hours24;
   }
   else
   {
      var amPmItem = document.getElementById( "oe-time-picker-pm-box" );
      var hours12 = hours24 - 12;
   }
   
   oeTimePicker.selectAmPmItem( amPmItem );
   
   // select the hour item
   
   var hourItem = document.getElementById( "oe-time-picker-hour-box-" + hours12 );
   oeTimePicker.selectHourItem( hourItem );
 
   // Show the five minute view if we are an even five minutes, one minute 
   // otherwise
  
   var minutesByFive = oeTimePicker.calcNearestFiveMinutes( oeTimePicker.gSelectedTime );
   
   if( minutesByFive == oeTimePicker.gSelectedTime.getMinutes() )
   {
      oeTimePicker.clickLess();
   }
   else
   {
      oeTimePicker.clickMore();
   }
}


/**
*   Called when the AM box is clicked
*/

oeTimePicker.clickAm = function( theItem )
{
   // if not already AM, set the selected time to AM ( by subtracting 12 from what must be a PM time )
   // and select the AM item
   
   if( !oeTimePicker.isTimeAm( oeTimePicker.gSelectedTime ) )
   {
      var hours = oeTimePicker.gSelectedTime.getHours();
      oeTimePicker.gSelectedTime.setHours( hours - 12 ); 
      
      oeTimePicker.selectAmPmItem( theItem );
   }
}


/**
*   Called when the PM box is clicked
*/

oeTimePicker.clickPm = function( theItem )
{
   // if not already PM, set the selected time to PM ( by adding 12 to what must be an AM time )
   // and select the PM item
   
   if( oeTimePicker.isTimeAm( oeTimePicker.gSelectedTime ) )
   {
      var hours = oeTimePicker.gSelectedTime.getHours();
      oeTimePicker.gSelectedTime.setHours( hours + 12 ); 
      
      oeTimePicker.selectAmPmItem( theItem );
   }
}



/**
*   Called when one of the hour boxes is clicked
*/

oeTimePicker.clickHour = function( hourItem, hourNumber )
{
   // select the item
   
   oeTimePicker.selectHourItem( hourItem );
   
   // Change the hour in the selected time, add 12 if PM. 
   
   if( oeTimePicker.isTimeAm( oeTimePicker.gSelectedTime ) )
   {
      var hour24 = hourNumber;
   }
   else
   {
      var hour24 = hourNumber + 12;
   }
   
   oeTimePicker.gSelectedTime.setHours( hour24 );

   this.selectTime();
   
}


/**
*   Called when the more tab is clicked, and possibly at startup 
*/
 
oeTimePicker.clickMore = function()
{
   // switch to one minute view
   
   oeTimePicker.switchMinuteView( oeTimePicker.kMinuteView_One );
   
   // select minute box corresponding to the time
   
   var minutes = oeTimePicker.gSelectedTime.getMinutes();
   
   var oneMinuteItem = document.getElementById( "oe-time-picker-one-minute-box-" + minutes );
   oeTimePicker.selectMinuteItem( oneMinuteItem );
}


/**
*   Called when the less tab is clicked, and possibly at startup 
*/
 
oeTimePicker.clickLess = function()
{
   // switch to five minute view
   
   oeTimePicker.switchMinuteView( oeTimePicker.kMinuteView_Five );
   
   // select closest five minute box, 
   
   // BUT leave the selected time at what may NOT be an even five minutes
   // So that If they click more again the proper non-even-five minute box will be selected
   
   var minutesByFive = oeTimePicker.calcNearestFiveMinutes( oeTimePicker.gSelectedTime );
   
   var fiveMinuteItem = document.getElementById( "oe-time-picker-five-minute-box-" + minutesByFive );
   oeTimePicker.selectMinuteItem( fiveMinuteItem );
}
 
 


/**
*   Called when one of the minute boxes is clicked, 
*   Calls the client's oncommand and Closes the popup
*/

oeTimePicker.clickMinute = function( minuteItem, minuteNumber )
{
   // set the minutes in the selected time
   
   oeTimePicker.gSelectedTime.setMinutes( minuteNumber );

   oeTimePicker.selectMinuteItem( minuteItem );
   
   oeTimePicker.selectTime();

   oeTimePicker.gPopup.closePopup ();

}

/**
*   Called when one of the minute boxes is clicked, 
*   Calls the client's oncommand and Closes the popup
*/

oeTimePicker.selectTime = function()
{
   // We copy the picked time to avoid problems with changing the Date object in place
 
   var pickedTime = new Date( oeTimePicker.gSelectedTime )
   
   // put the picked time in the value property of the popup item.
   
   oeTimePicker.gPopup.value = pickedTime;
   
   // get and call the client's oncommand function
   
   var commandEventMethod = oeTimePicker.gPopup.getAttribute( "oncommand" );
   
   if( commandEventMethod  )
   {
      // set up a variable date, that will be avaialable from within the 
      // client method
      
      var date = pickedTime;
      
      // Make the function a member of the popup before calling it so that 
      // 'this' will be the popup
      
      oeTimePicker.gPopup.oeTimePickerFunction =  function() { eval( commandEventMethod ); };
      
      oeTimePicker.gPopup.oeTimePickerFunction();
   }
   
   // close the popup
   
   //oeTimePicker.gPopup.closePopup ();

}



/**
*   Helper function to switch between "one" and "five" minute views 
*/


oeTimePicker.switchMinuteView = function( view )   
{
   var fiveMinuteBox = document.getElementById( "oe-time-picker-five-minute-grid-box" );
   var oneMinuteBox = document.getElementById( "oe-time-picker-one-minute-grid-box" );

   if( view == oeTimePicker.kMinuteView_One )
   {
      fiveMinuteBox.setAttribute( "collapsed", true );
      oneMinuteBox.setAttribute( "collapsed", false );
   }
   else
   {
      fiveMinuteBox.setAttribute( "collapsed", false );
      oneMinuteBox.setAttribute( "collapsed", true );
   }
}

/**
*   Helper function to select the AM or PM box 
*/

oeTimePicker.selectAmPmItem = function( amPmItem )
{
   // clear old selection, if there is one
   
   if( oeTimePicker.gSelectedAmPmItem != null )
   {
      oeTimePicker.gSelectedAmPmItem.setAttribute( "selected" , "false" );
   }
   
   // set selected attribute, to cause the selected style to apply
   
   amPmItem.setAttribute( "selected" , "true" );
   
   // remember the selected item so we can deselect it
   
   oeTimePicker.gSelectedAmPmItem = amPmItem;

   this.selectTime();
}

/**
*   Helper function to select an hour item 
*/

oeTimePicker.selectHourItem = function( hourItem )
{
   // clear old selection, if there is one
   
   if( oeTimePicker.gSelectedHourItem != null )
   {
      oeTimePicker.gSelectedHourItem.setAttribute( "selected" , "false" );
   }
   
   // set selected attribute, to cause the selected style to apply
   
   hourItem.setAttribute( "selected" , "true" );
   
   // remember the selected item so we can deselect it
   
   oeTimePicker.gSelectedHourItem = hourItem;
}


/**
*   Helper function to select an minute item 
*/

oeTimePicker.selectMinuteItem = function( minuteItem )
{
   // clear old selection, if there is one
   
   if( oeTimePicker.gSelectedMinuteItem != null )
   {
      oeTimePicker.gSelectedMinuteItem.setAttribute( "selected" , "false" );
   }
   
   // set selected attribute, to cause the selected style to apply
   
   minuteItem.setAttribute( "selected" , "true" );
   
   // remember the selected item so we can deselect it
   
   oeTimePicker.gSelectedMinuteItem = minuteItem;
}


/**
*   Helper function to calulate the nearset even five minutes 
*/

oeTimePicker.calcNearestFiveMinutes = function( time )
{
   var minutes = time.getMinutes();
   var minutesByFive = Math.round( minutes / 5 ) * 5;
   
   if( minutesByFive > 59 )
   {
      minutesByFive = 55;
   }
   
   return minutesByFive;
}

/**
*   Helper function to work out if the 24-hour time is AM or PM
*/

oeTimePicker.isTimeAm = function( time )
{
   var currentHour = time.getHours( );
   
   if( currentHour >= 12 )
   {
      return false;
   }
   else
   {
      return true;
   }
}


