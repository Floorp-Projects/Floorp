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
 *                 Colin Phillips <colinp@oeone.com> 
 *                 Chris Charabaruk <ccharabaruk@meldstar.com>
 *                 ArentJan Banck <ajbanck@planet.nl>
 *                 Chris Allen
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
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



/***** calendar/printDialog.js
* PRIMARY AUTHOR
*   Chris Allen
* REQUIRED INCLUDES 
*   <script type="application/x-javascript" src="chrome://calendar/content/dateUtils.js"/>
*   
* NOTES
*   Code for the calendar's print dialog.
*
*  Invoke this dialog to print a Calendar as follows:
*  args = new Object();
*  args.eventSource = youreventsource;
*  args.selectedEvents=currently selected events
*  args.selectedDate=currently selected date
*  args.weeksInView=multiweek how many weeks to show
*  args.prevWeeksInView=previous weeks to show in view
*  args.startOfWeek=zero based day to start the week
*  calendar.openDialog("chrome://calendar/content/eventDialog.xul", "printdialog", "chrome,modal", args );
*
* IMPLEMENTATION NOTES
**********
*/


/*-----------------------------------------------------------------
*   W I N D O W      V A R I A B L E S
*/


var eventSource; // event source sent by opener
var selectedEvents; // selected events send by opener
var selectedDate; // current selected date sent by opener

var gCategoryManager; // for future

var gStartDate = new Date( );
var gEndDate = new Date( );

this.dateStringBundle = srGetStrBundle("chrome://calendar/locale/dateFormat.properties");

var monthNames=new Array(12);
monthNames[0]=this.dateStringBundle.GetStringFromName("month.1.name" );
monthNames[1]=this.dateStringBundle.GetStringFromName("month.2.name" );
monthNames[2]=this.dateStringBundle.GetStringFromName("month.3.name" );
monthNames[3]=this.dateStringBundle.GetStringFromName("month.4.name" );
monthNames[4]=this.dateStringBundle.GetStringFromName("month.5.name" );
monthNames[5]=this.dateStringBundle.GetStringFromName("month.6.name" );
monthNames[6]=this.dateStringBundle.GetStringFromName("month.7.name" );
monthNames[7]=this.dateStringBundle.GetStringFromName("month.8.name" );
monthNames[8]=this.dateStringBundle.GetStringFromName("month.9.name" );
monthNames[9]=this.dateStringBundle.GetStringFromName("month.10.name" );
monthNames[10]=this.dateStringBundle.GetStringFromName("month.11.name" );
monthNames[11]=this.dateStringBundle.GetStringFromName("month.12.name" );


var weeksInView;
var prevWeeksInView;
var startOfWeek;

/*-----------------------------------------------------------------
*   W I N D O W      F U N C T I O N S
*/

/**
*   Called when the dialog is loaded.
*/


function loadCalendarPrintDialog()
{

   // load up the sent arguments.

   args=window.arguments[0];
   eventSource=args.eventSource;
   selectedEvents=args.selectedEvents;
   selectedDate=args.selectedDate;
   gStartDate=selectedDate;
   weeksInView=args.weeksInView;
   prevWeeksInView=args.prevWeeksInView;
   startOfWeek=args.startOfWeek;


   // set the date to the currently selected date
   document.getElementById( "start-date-picker" ).value = selectedDate;

   /* Categories stuff */
   // Load categories


/****
   var categoriesString = opener.GetUnicharPref(opener.gCalendarWindow.calendarPreferences.calendarPref, "categories.names", getDefaultCategories() );
   var categoriesList = categoriesString.split( "," );


   // categoriesList.sort();

   var oldMenulist = document.getElementById( "categories-menulist-menupopup" );
   while( oldMenulist.hasChildNodes() )
      oldMenulist.removeChild( oldMenulist.lastChild );

   document.getElementById( "categories-field" ).appendItem("All", "All");

   for (i = 0; i < categoriesList.length ; i++)
   {
      document.getElementById( "categories-field" ).appendItem(categoriesList[i], categoriesList[i]);
   }

   document.getElementById( "categories-field" ).selectedIndex = 0;
**/

   // start focus on title
   var firstFocus = document.getElementById( "title-field" );
   firstFocus.focus();

   opener.setCursor( "default" );
}


function printCalendar() {
  var caltype=document.getElementById("view-field");
  if (caltype.value == '')
     caltype.value='month';
  if (caltype.value == 'month')
     printMonthView(gStartDate);
  else
     if (caltype.value == 'list')
        printEventArray( selectedEvents);
     else
        if (caltype.value == 'day')
           printDayView(gStartDate);
        else
           if (caltype.value == 'week')
              printWeekView(gStartDate);
           else
              if (caltype.value == 'multiweek')
                 printMultiWeekView(gStartDate);
              else
                 alert("That view is not implemented yet");
  return true;
}

function returnTime(timeval) {
   retval= opener.gCalendarWindow.dateFormater.getFormatedTime( timeval );
   if (retval.indexOf("AM")>-1)
      retval=retval.substring(0,retval.indexOf("AM")-1)+'a';
   if (retval.indexOf("PM")>-1)
      retval=retval.substring(0,retval.indexOf("PM")-1)+'p';
   if (retval=='12:00p')
      retval='Noon';
   return retval;
}

function printMultiWeekView(currentDate) {
var dayStart=currentDate.getDate();
var dowStart = (startOfWeek <= currentDate.getDay()) ?  currentDate.getDay()-startOfWeek : 7-startOfWeek;
var weekStart=new Date(currentDate.getFullYear(), currentDate.getMonth(), dayStart - dowStart - (prevWeeksInView*7));
var printwindow=window.open("","printwindow");
printwindow.document.open();
printwindow.document.write("<html><head><title>"+windowTitle+"</title></head><body style='font-size:11px;'>");
var mytitle=document.getElementById("title-field").value;
if (mytitle.length > 0)
  {
    printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr><td valign=bottom align=center>");
    printwindow.document.write(mytitle);
    printwindow.document.write("</td></tr></table>");
  }

var weekNumber = DateUtils.getWeekNumber(currentDate) ;

printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>Week "+weekNumber+"</td></tr></table>");
printwindow.document.write("<table style='border:1px solid black;' width=100%>")
printwindow.document.write("<tr>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
printwindow.document.write("</tr>");
// content here
dayToStart=weekStart.getDate();
monthToStart=weekStart.getMonth();
yearToStart=weekStart.getFullYear();
var showprivate=document.getElementById("private-checkbox");

for (var w=0; w<weeksInView; w++)
{
printwindow.document.write("<tr>");
for (var i=0; i<7; i++)
{
   var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart+i+(w*7));
   printwindow.document.write("<td style='border:1px solid black;' valign=top width=14%>");
   printwindow.document.write("<table valign=top height=100 width=100% border=0>");
   printwindow.document.write("<tr valign=top><td colspan=2 align=center valign=top >");
   printwindow.document.write(monthNames[thisDaysDate.getMonth()].substring(0,3)+" "+thisDaysDate.getDate());
   printwindow.document.write("</td></tr>");
   printwindow.document.write("<tr valign=top><td valign=top width=20%></td><td valign=top width=80%></td></tr>");
   var calendarEventDisplay
   // add each calendarEvent
   dayEventList = eventSource.getEventsForDay( thisDaysDate );

   for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
         {
              calendarEventDisplay = dayEventList[ eventIndex ];
              var listpriv=true;
              if (calendarEventDisplay.event.privateEvent)
                if (! showprivate.checked)
                  listpriv=false;
              if (listpriv)
              {
                     eventTitle=calendarEventDisplay.event.title;
                     var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
                     var formattedStartTime=returnTime(eventStartTime);
                     var eventEndTime = new Date( calendarEventDisplay.event.end.getTime() ) ;
                     var formattedEndTime=returnTime(eventEndTime);
                     var formattedTime=formattedStartTime+"-"+formattedEndTime;
                     if (calendarEventDisplay.event.allDay)
                        formattedTime=''; // all day event
                     if (calendarEventDisplay.event.allDay)
                        printwindow.document.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>");
                     else
                        printwindow.document.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>"+formattedTime+"</td></tr><tr><td></td><td valign=top style='font-size:11px;'>");
                     printwindow.document.write(eventTitle);
                     if (calendarEventDisplay.event.location)
                         printwindow.document.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+locationTag+": "+calendarEventDisplay.event.location);
                     if (calendarEventDisplay.event.url)
                         printwindow.document.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+uriTag+": "+calendarEventDisplay.event.url);
                     printwindow.document.write("</td></tr>");
              }
       }
    printwindow.document.write("</table>");

}
printwindow.document.write("</tr>");
} // end of all weeks
printwindow.document.write("</table>")
printwindow.document.write("</body></html>");
printwindow.document.close();



}


function printWeekView(currentDate) {
var dayStart=currentDate.getDate();
var dowStart = (startOfWeek <= currentDate.getDay()) ?  currentDate.getDay()-startOfWeek : 7-startOfWeek;
var weekStart=new Date(currentDate.getFullYear(), currentDate.getMonth(), dayStart - dowStart);
var printwindow=window.open("","printwindow");
printwindow.document.open();
printwindow.document.write("<html><head><title>"+windowTitle+"</title></head><body>");
var mytitle=document.getElementById("title-field").value;
if (mytitle.length > 0)
  {
    printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr><td valign=bottom align=center>");
    printwindow.document.write(mytitle);
    printwindow.document.write("</td></tr></table>");
  }

var weekNumber = DateUtils.getWeekNumber(currentDate) ;

printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>Week "+weekNumber+"</td></tr></table>");
printwindow.document.write("<table style='border:1px solid black;' width=100%>")
printwindow.document.write("<tr>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
printwindow.document.write("</tr>");
// content here
dayToStart=weekStart.getDate();
monthToStart=weekStart.getMonth();
yearToStart=weekStart.getFullYear();
var showprivate=document.getElementById("private-checkbox");

printwindow.document.write("<tr>");
for (var i=0; i<7; i++)
{
   var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart+i);
   printwindow.document.write("<td style='border:1px solid black;' valign=top width=14% height=500>");
   printwindow.document.write("<table valign=top width=100 border=0>"); // to force uniform width
   printwindow.document.write("<tr valign=top><td valign=top colspan=2 align=center>");
   printwindow.document.write(monthNames[thisDaysDate.getMonth()].substring(0,3)+" "+thisDaysDate.getDate());
   printwindow.document.write("</td></tr>");
   printwindow.document.write("<tr><td width=20%></td><td width=80%></td></tr>");
   var calendarEventDisplay
   // add each calendarEvent
   dayEventList = eventSource.getEventsForDay( thisDaysDate );

   for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
         {
              calendarEventDisplay = dayEventList[ eventIndex ];
              var listpriv=true;
              if (calendarEventDisplay.event.privateEvent)
                if (! showprivate.checked)
                  listpriv=false;
              if (listpriv)
              {
                     eventTitle=calendarEventDisplay.event.title;
                     var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
                     var formattedStartTime=returnTime(eventStartTime);
                     var eventEndTime = new Date( calendarEventDisplay.event.end.getTime() ) ;
                     var formattedEndTime=returnTime(eventEndTime);
                     var formattedTime=formattedStartTime+"-"+formattedEndTime;
                     if (calendarEventDisplay.event.allDay)
                        printwindow.document.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>");
                     else
                        printwindow.document.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>"+formattedTime+"</td></tr><tr><td></td><td valign=top style='font-size:11px;'>");
                     printwindow.document.write(eventTitle);
                     if (calendarEventDisplay.event.location)
                         printwindow.document.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+locationTag+": "+calendarEventDisplay.event.location);
                     if (calendarEventDisplay.event.url)
                         printwindow.document.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+uriTag+": "+calendarEventDisplay.event.url);
                     printwindow.document.write("</td></tr>");
              }
       }
    printwindow.document.write("</table>");

}
printwindow.document.write("</tr>");

printwindow.document.write("</table>")
printwindow.document.write("</body></html>");
printwindow.document.close();

}

function printDayView(currentDate) {
var dayStart = currentDate.getDate();
printwindow = window.open( "", "CalendarPrintWindow");
printwindow.document.open();
printwindow.document.write("<html><head><title>"+windowTitle+"</title></head><body style='font-size:12px;'>");
printwindow.document.write("<table width=100% style='border:1px solid black;'>");
var mytitle=document.getElementById("title-field").value;
if (mytitle.length > 0)
  {
    printwindow.document.write("<tr><td colspan=2 align=center style='font-size:26px;font-weight:bold;'>");
    printwindow.document.write(mytitle);
    printwindow.document.write("</td></tr>");
  }
var mydateshow="";
mydateshow+=ArrayOfDayNames[currentDate.getDay()];
mydateshow+=", ";
mydateshow+=monthNames[currentDate.getMonth()]+" "+currentDate.getDate()+" "+currentDate.getFullYear();
printwindow.document.write("<tr ><td colspan=2 align=center style='font-size:26px;font-weight:bold;border-bottom:1px solid black;'>");
printwindow.document.write(mydateshow);
printwindow.document.write("</td></tr>");
printwindow.document.write("<tr><td width=20% style='border-bottom:1px solid black;'>Time</td><td width=80% style='border-bottom:1px solid black;'>Event</td></tr>");
var showprivate=document.getElementById("private-checkbox");
printwindow.document.write("<tr style='height=20px;'><td colspan=2 style='border-bottom:1px solid black;'> </td></tr>"); // for entering a new appt
var calendarEventDisplay
   // add each calendarEvent
dayEventList = eventSource.getEventsForDay( currentDate );

for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
     {
         calendarEventDisplay = dayEventList[ eventIndex ];

         var listpriv=true;
         if (calendarEventDisplay.event.privateEvent)
            if (! showprivate.checked)
              listpriv=false;
         if (listpriv)
           {
             printwindow.document.write("<tr style='height=20px;'><td valign=top style='border-bottom:1px solid black;'>");
             var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
             var formattedStartTime=returnTime(eventStartTime);
             var eventEndTime = new Date( calendarEventDisplay.event.end.getTime() ) ;
             var formattedEndTime=returnTime(eventEndTime);
             var formattedTime=formattedStartTime+"-"+formattedEndTime;
             if (calendarEventDisplay.event.allDay)
                formattedTime=''; // all day event
             printwindow.document.write(formattedTime);
             printwindow.document.write("</td><td valign=top style='border-bottom:1px solid black;'>"+calendarEventDisplay.event.title);
             if (calendarEventDisplay.event.description)
               printwindow.document.write("<br>"+descriptionTag+": "+calendarEventDisplay.event.description);
             if (calendarEventDisplay.event.location)
               printwindow.document.write("<br>"+locationTag+": "+calendarEventDisplay.event.location);
             if (calendarEventDisplay.event.url)
               printwindow.document.write("<br>"+uriTag+": "+calendarEventDisplay.event.url);
             var mystat='Cancelled';
             if (calendarEventDisplay.event.status == 10029)
               mystat='Tentative';
             if (calendarEventDisplay.event.status == 10030)
               mystat='Confirmed';
             printwindow.document.write("<br>Status: "+mystat);
             printwindow.document.write("</td></tr>");
             printwindow.document.write("<tr style='height=20px;'><td colspan=2  style='border-bottom:1px solid black;'> </td></tr>"); // for entering a new appt
         }
    }

printwindow.document.write("</table>");
printwindow.document.write("</body></html>");
printwindow.document.close();

}

function printEventArray( calendarEventArray)
{
   printwindow = window.open( "", "CalendarPrintWindow");
   printwindow.document.open();
   printwindow.document.write("<html><head><title>"+windowTitle+"</title></head><body style='font-size:12px;'>");
   printwindow.document.write("<table width=100%>");
   var mytitle=document.getElementById("title-field").value;
   if (mytitle.length > 0)
     {
       printwindow.document.write("<tr><td colspan=3 align=center style='font-size:26px;font-weight:bold;'>>");
       printwindow.document.write(mytitle);
       printwindow.document.write("</td></tr>");
     }
   printwindow.document.write("<tr><td width=20%>Starts</td><td width=20%>Ends</td><td width=60%>Event</td></tr>");
   var showprivate=document.getElementById("private-checkbox");
   for (i in calendarEventArray)
    {
      var calEvent=calendarEventArray[i];
      var useit=true;

      if (calEvent.privateEvent)
         if (! showprivate.checked)
             useit=false;
      if (useit)
      {
        printwindow.document.write("<tr><td valign=top>");
        if (calEvent.allDay)
         {
          printwindow.document.write("All Day");
          printwindow.document.write("</td><td>");
         }
        else
        {
         printwindow.document.write(calEvent.start);
         printwindow.document.write("</td><td valign=top>");
         printwindow.document.write(calEvent.end);
        }
        printwindow.document.write("</td><td valign=top>");
        printwindow.document.write(calEvent.title);
        if (calEvent.description)
           printwindow.document.write("<br>"+descriptionTag+": "+calEvent.description);
        if (calEvent.location)
          printwindow.document.write("<br>"+locationTag+": "+calEvent.location);
        if (calEvent.url)
          printwindow.document.write("<br>"+uriTag+": "+calEvent.url);
        var mystat='Cancelled';
        if (calEvent.status == 10029)
          mystat='Tentative';
        if (calEvent.status == 10030)
          mystat='Confirmed';

        printwindow.document.write("<br>Status: "+mystat);
        printwindow.document.write("</td></tr>");
      }
   }
   printwindow.document.write("</table>");
   printwindow.document.write("</body></html>");
   printwindow.document.close();
}



function printMonthView(currentDate) {
// ok first let's get the array of events for this month.
var calDate = new Date(currentDate.getFullYear(), currentDate.getMonth(), 1);
var dayFirst = calDate.getDay();
var dowStart = (startOfWeek <= dayFirst) ?  dayFirst-startOfWeek : 7-startOfWeek;
var weekStart=new Date(calDate.getFullYear(),calDate.getMonth(),1-dowStart);
var startOfMonthDate = new Date(currentDate.getFullYear(), currentDate.getMonth(), 1);
var endOfMonthDate = new Date(currentDate.getFullYear(), currentDate.getMonth()+1, 0);
var daysInMonth =  endOfMonthDate.getDate();
var printwindow=window.open('','printwindow');
printwindow.document.open();
printwindow.document.write("<html><head><title>"+windowTitle+"</title></head><body>");
var mytitle=document.getElementById("title-field").value;
if (mytitle.length > 0)
  {
    printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr><td valign=bottom align=center>");
    printwindow.document.write(mytitle);
    printwindow.document.write("</td></tr></table>");
  }
printwindow.document.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>"+monthNames[currentDate.getMonth()]+" "+currentDate.getFullYear()+"</td></tr></table>");
printwindow.document.write("<table style='border:1px solid black;' width=100%>")
printwindow.document.write("<tr>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
printwindow.document.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
printwindow.document.write("</tr>");


var showprivate=document.getElementById("private-checkbox");

dayToStart=weekStart.getDate();
monthToStart=weekStart.getMonth();
yearToStart=weekStart.getFullYear();
var showprivate=document.getElementById("private-checkbox");
var inMonth=true;
var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart);

for (var w=0; w<6; w++)
{
  if (inMonth)
  {
     printwindow.document.write("<tr>");
     for (var i=0; i<7; i++)
     {
      printwindow.document.write("<td align=left valign=top style='border:1px solid black;vertical-alignment:top;' >");
      printwindow.document.write("<table valign=top height=100 width=100 style='font-size:10px;'><tr valign=top><td valign=top width=20%>");
      if (thisDaysDate.getMonth()==currentDate.getMonth())
         printwindow.document.write(thisDaysDate.getDate());
      printwindow.document.write("</td><td width=80% valign=top></td></tr>");
      if (thisDaysDate.getMonth()==currentDate.getMonth())
       {
         dayEventList = eventSource.getEventsForDay( thisDaysDate );
         var calendarEventDisplay
         // add each calendarEvent
         for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
           {
              calendarEventDisplay = dayEventList[ eventIndex ];
              var listpriv=true;
              if (calendarEventDisplay.event.privateEvent)
                if (! showprivate.checked)
                  listpriv=false;
              if (listpriv)
                  {
                     eventTitle=calendarEventDisplay.event.title;
                     var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
                     var formattedStartTime=returnTime(eventStartTime);
                     if (calendarEventDisplay.event.allDay)
                        printwindow.document.write("<tr><td valign=top colspan=2 style='font-size:11px;'>");
                     else
                        printwindow.document.write("<tr><td valign=top align=right style='font-size:11px;'>"+formattedStartTime+"</td><td valign=top style='font-size:11px;'>");
                     printwindow.document.write(eventTitle);
                     printwindow.document.write("</td></tr>");
                   }
           } //end of events
         } // if it was in the month
        printwindow.document.write("</table>");
        printwindow.document.write("</td>")
	  //advance to the next day
	thisDaysDate.setDate(thisDaysDate.getDate()+1);
      } //end of each day
      printwindow.document.write("</tr>");
  } // ok it was in the month
  if((thisDaysDate.getMonth() > currentDate.getMonth())||
     (thisDaysDate.getFullYear() > currentDate.getFullYear()) )
    inMonth=false;
} // end of each week

printwindow.document.write("</table>")
printwindow.document.write("</body></html>");
printwindow.document.close();


}



/**
*   Called when a datepicker is finished, and a date was picked.
*/

function onDatePick( datepicker )
{
   var ThisDate = new Date( datepicker.value);

   if( datepicker.id == "start-date-picker" )
   {
      gStartDate.setMonth( ThisDate.getMonth() );
      gStartDate.setDate( ThisDate.getDate() );
      gStartDate.setFullYear( ThisDate.getFullYear() );
   }

}




function getWeekNumberOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-date-picker" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth )
   {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   return( weekNumber );
}

function isLastDayOfWeekOfMonth()
{
   //get the day number for today.
   var startTime = document.getElementById( "start-date-picker" ).value;
   
   var oldStartTime = startTime;

   var thisMonth = startTime.getMonth();
   
   var monthToCompare = thisMonth;

   var weekNumber = 0;

   while( monthToCompare == thisMonth )
   {
      startTime = new Date( startTime.getTime() - ( 1000 * 60 * 60 * 24 * 7 ) );

      monthToCompare = startTime.getMonth();
      
      weekNumber++;
   }
   
   if( weekNumber > 3 )
   {
      var nextWeek = new Date( oldStartTime.getTime() + ( 1000 * 60 * 60 * 24 * 7 ) );

      if( nextWeek.getMonth() != thisMonth )
      {
         //its the last week of the month
         return( true );
      }
   }

   return( false );
}


function getWeekNumberText( weekNumber )
{
   switch( weekNumber )
   {
   case 1:
      return( "First" );
   case 2:
      return( "Second" );
   case 3:
      return( "Third" );
   case 4:
      return( "Fourth" );
   case 5:
      return( "Last" );
   default:
      return( false );
   }

}





/**
*   Helper function for filling the form, set the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to set 
*      newValue      - value to set property to ( if undefined no change is made )
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*/

function setFieldValue( elementId, newValue, propertyName  )
{
   var undefined;
   
   if( newValue !== undefined )
   {
      var field = document.getElementById( elementId );
      
      if( newValue === false )
      {
         field.removeAttribute( propertyName );
      }
      else
      {
         if( propertyName )
         {
            field.setAttribute( propertyName, newValue );
         }
         else
         {
            field.value = newValue;
         }
      }
   }
}


/**
*   Helper function for getting data from the form, 
*   Get the value of a property of a XUL element
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
*      propertyName  - OPTIONAL name of property to set, default is "value", use "checked" for 
*                               radios & checkboxes, "data" for drop-downs
*   RETURN
*      newValue      - value of property
*/

function getFieldValue( elementId, propertyName )
{
   var field = document.getElementById( elementId );
   
   if( propertyName )
   {
      return field[ propertyName ];
   }
   else
   {
      return field.value;
   }
}

/**
*   Helper function for getting a date/time from the form.
*   The element must have been set up with  setDateFieldValue or setTimeFieldValue.
*
* PARAMETERS
*      elementId     - ID of XUL element to get from 
* RETURN
*      newValue      - Date value of element
*/


function getDateTimeFieldValue( elementId )
{
   var field = document.getElementById( elementId );
   return field.editDate;
}



/**
*   Helper function for filling the form, set the value of a date field
*
* PARAMETERS
*      elementId     - ID of time textbox to set 
*      newDate       - Date Object to use
*/

function setDateFieldValue( elementId, newDate  )
{
   // set the value to a formatted date string 
   
   var field = document.getElementById( elementId );
   field.value = formatDate( newDate );
   
   // add an editDate property to the item to hold the Date object 
   // used in onDatePick to update the date from the date picker.
   // used in getDateTimeFieldValue to get the Date back out.
   
   // we clone the date object so changes made in place do not propagte 
   
   field.editDate = new Date( newDate );
}



/**
*   Take a Date object and return a displayable date string i.e.: May 5, 1959
*  :TODO: This should be moved into DateFormater and made to use some kind of
*         locale or user date format preference.
*/

function formatDate( date )
{
   return( opener.gCalendarWindow.dateFormater.getFormatedDate( date ) );
}


function debug( Text )
{
   dump( "\nprintDialog.js:"+ Text + "\n");

}