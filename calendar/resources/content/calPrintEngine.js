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


/*-----------------------------------------------------------------
 *   W I N D O W      V A R I A B L E S
 */



this.dateStringBundle = document.getElementById("bundle_date");

var monthNames=new Array(12);
monthNames[0] = this.dateStringBundle.getString( "month.1.name" );
monthNames[1] = this.dateStringBundle.getString( "month.2.name" );
monthNames[2] = this.dateStringBundle.getString( "month.3.name" );
monthNames[3] = this.dateStringBundle.getString( "month.4.name" );
monthNames[4] = this.dateStringBundle.getString( "month.5.name" );
monthNames[5] = this.dateStringBundle.getString( "month.6.name" );
monthNames[6] = this.dateStringBundle.getString( "month.7.name" );
monthNames[7] = this.dateStringBundle.getString( "month.8.name" );
monthNames[8] = this.dateStringBundle.getString( "month.9.name" );
monthNames[9] = this.dateStringBundle.getString( "month.10.name" );
monthNames[10]= this.dateStringBundle.getString( "month.11.name" );
monthNames[11]= this.dateStringBundle.getString( "month.12.name" );


var eventSource; // event source sent by opener
var weeksInView;
var prevWeeksInView;
var startOfWeek;
var gMyTitle;
var gShowprivate ;
var gCalendarWindow ;
var gHtmlDocument ;

var gPrintSettingsAreGlobal = true;
var gSavePrintSettings = true;
var gPrintSettings = null;
var gWebProgress = null;

/*-----------------------------------------------------------------
 *   W I N D O W      F U N C T I O N S
 */

function getWebNavigation()
{
  try {
    return gContent.webNavigation;
  } catch (e) {
    return null;
  }
}

var gPrintPreviewObs = {
    observe: function(aSubject, aTopic, aData)
    {
      setTimeout(FinishPrintPreview, 0);
    },

    QueryInterface : function(iid)
    {
     if (iid.equals(Components.interfaces.nsIObserver) || iid.equals(Components.interfaces.nsISupportsWeakReference))
      return this;
     
     throw Components.results.NS_NOINTERFACE;
    }
};

function BrowserPrintPreview()
{
  var ifreq;
  var webBrowserPrint;  
  try {
    ifreq = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    gPrintSettings = GetPrintSettings();

  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
  gWebProgress = new Object();

  var printPreviewParams    = new Object();
  var notifyOnOpen          = new Object();
  var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                  .getService(Components.interfaces.nsIPrintingPromptService);
  if (printingPromptService) {
    // just in case we are already printing,
    // an error code could be returned if the Prgress Dialog is already displayed
    try {
      printingPromptService.showProgress(this, webBrowserPrint, gPrintSettings, gPrintPreviewObs, false, gWebProgress,
                                         printPreviewParams, notifyOnOpen);
      if (printPreviewParams.value) {
        var webNav = getWebNavigation();
        printPreviewParams.value.docTitle = webNav.document.title;
        printPreviewParams.value.docURL   = webNav.currentURI.spec;
      }

      // this tells us whether we should continue on with PP or
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || gWebProgress.value == null) {
        FinishPrintPreview();
      }
    } catch (e) {
      FinishPrintPreview();
    }
  }
}

function FinishPrintPreview()
{
  try {
    var ifreq = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
    var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings();
      webBrowserPrint.printPreview(gPrintSettings, null, gWebProgress.value);
    }
    showPrintPreviewToolbar();

    content.focus();
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}

/*-----------------------------------------------------------------
 *   Called when the dialog is loaded.
 */

function OnLoadPrintEngine(){
  gContent = document.getElementById("content") ;

  if ( window.arguments && window.arguments[0] != null ) {
    HTMLViewFunction =  window.arguments[0] ;
    HTMLFunctionArgs =  window.arguments[1] ;
    gMyTitle = window.arguments[2] ;
    gShowprivate = window.arguments[3] ;
    gArgs = window.arguments[4] ;
    gCalendarWindow = window.arguments[5] ;
    eventSource=gArgs.eventSource;
    weeksInView=gArgs.weeksInView;
    prevWeeksInView=gArgs.prevWeeksInView;
    startOfWeek=gArgs.startOfWeek;

    gHtmlDocument = window.content.document ;
    initHTMLView();
    eval(HTMLViewFunction)(HTMLFunctionArgs);
    finishHTMLView() ;
  }
  BrowserPrintPreview();
}

function OnUnloadPrintEngine()
{
}

function showPrintPreviewToolbar()
{
  const kXULNS = 
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

  var printPreviewTB = document.createElementNS(kXULNS, "toolbar");
  printPreviewTB.setAttribute("printpreview", true);
  printPreviewTB.setAttribute("id", "print-preview-toolbar");

  gContent.parentNode.insertBefore(printPreviewTB, gContent);
}

function BrowserExitPrintPreview()
{
  window.close();
}

function initHTMLView()
{
  //printwindow.document.open();
  gHtmlDocument.write("<html><head><title>"+windowTitle+"</title></head><body style='font-size:11px;'>");
  if (gMyTitle.length > 0)
  {
//  gHtmlDocument.write("<tr><td colspan=3 align=center style='font-size:26px;font-weight:bold;'>>");
//  gHtmlDocument.write("<tr><td colspan=2 align=center style='font-size:26px;font-weight:bold;'>");
//  gHtmlDocument.write(mytitle);
//  gHtmlDocument.write("</td></tr>");

    gHtmlDocument.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr><td valign=bottom align=center>");
    gHtmlDocument.write(gMyTitle);
    gHtmlDocument.write("</td></tr></table>");
  }
  return gHtmlDocument;
}

function finishHTMLView() 
{
  gHtmlDocument.write("</body></html>");
  gHtmlDocument.close();
}

function printMultiWeekView(currentDate)
{
  var dayStart=currentDate.getDate();
  var dowStart = (startOfWeek <= currentDate.getDay()) ?  currentDate.getDay()-startOfWeek : 7-startOfWeek;
  var weekStart=new Date(currentDate.getFullYear(), currentDate.getMonth(), dayStart - dowStart - (prevWeeksInView*7));

  var weekNumber = DateUtils.getWeekNumber(currentDate) ;

  gHtmlDocument.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>Week "+weekNumber+"</td></tr></table>");
  gHtmlDocument.write("<table style='border:1px solid black;' width=100%>")
  gHtmlDocument.write("<tr>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
  gHtmlDocument.write("</tr>");

  // content here
  dayToStart=weekStart.getDate();
  monthToStart=weekStart.getMonth();
  yearToStart=weekStart.getFullYear();

  for (var w=0; w<weeksInView; w++)
  {
    gHtmlDocument.write("<tr>");
    for (var i=0; i<7; i++)
    {
      var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart+i+(w*7));
      gHtmlDocument.write("<td style='border:1px solid black;' valign=top width=14%>");
      gHtmlDocument.write("<table valign=top height=100 width=100% border=0>");
      gHtmlDocument.write("<tr valign=top><td colspan=2 align=center valign=top >");
      gHtmlDocument.write(monthNames[thisDaysDate.getMonth()].substring(0,3)+" "+thisDaysDate.getDate());
      gHtmlDocument.write("</td></tr>");
      gHtmlDocument.write("<tr valign=top><td valign=top width=20%></td><td valign=top width=80%></td></tr>");
      var calendarEventDisplay
      // add each calendarEvent
      dayEventList = eventSource.getEventsForDay( thisDaysDate );

      for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
      {
        calendarEventDisplay = dayEventList[ eventIndex ];
        var listpriv=true;
        if (calendarEventDisplay.event.privateEvent)
          if (! gShowprivate.checked)
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
            gHtmlDocument.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>");
          else
            gHtmlDocument.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>"+formattedTime+"</td></tr><tr><td></td><td valign=top style='font-size:11px;'>");
          gHtmlDocument.write(eventTitle);
          if (calendarEventDisplay.event.location)
            gHtmlDocument.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+locationTag+": "+calendarEventDisplay.event.location);
          if (calendarEventDisplay.event.url)
            gHtmlDocument.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+uriTag+": "+calendarEventDisplay.event.url);
          gHtmlDocument.write("</td></tr>");
        }
      }
      gHtmlDocument.write("</table>");
    }
    gHtmlDocument.write("</tr>");
  } // end of all weeks
  gHtmlDocument.write("</table>")
}

function printWeekView(currentDate)
{
  var dayStart=currentDate.getDate();
  var dowStart = (startOfWeek <= currentDate.getDay()) ?  currentDate.getDay()-startOfWeek : 7-startOfWeek;
  var weekStart=new Date(currentDate.getFullYear(), currentDate.getMonth(), dayStart - dowStart);
  var weekNumber = DateUtils.getWeekNumber(currentDate) ;

  gHtmlDocument.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>Week "+weekNumber+"</td></tr></table>");
  gHtmlDocument.write("<table style='border:1px solid black;' width=100%>")
  gHtmlDocument.write("<tr>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
  gHtmlDocument.write("</tr>");
  // content here
  dayToStart=weekStart.getDate();
  monthToStart=weekStart.getMonth();
  yearToStart=weekStart.getFullYear();

  gHtmlDocument.write("<tr>");
  for (var i=0; i<7; i++)
  {
    var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart+i);
    gHtmlDocument.write("<td style='border:1px solid black;' valign=top width=14% height=500>");
    gHtmlDocument.write("<table valign=top width=100 border=0>"); // to force uniform width
    gHtmlDocument.write("<tr valign=top><td valign=top colspan=2 align=center>");
    gHtmlDocument.write(monthNames[thisDaysDate.getMonth()].substring(0,3)+" "+thisDaysDate.getDate());
    gHtmlDocument.write("</td></tr>");
    gHtmlDocument.write("<tr><td width=20%></td><td width=80%></td></tr>");
    var calendarEventDisplay
    // add each calendarEvent
    dayEventList = eventSource.getEventsForDay( thisDaysDate );

    for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
    {
      calendarEventDisplay = dayEventList[ eventIndex ];
      var listpriv=true;
      if (calendarEventDisplay.event.privateEvent)
        if (! gShowprivate.checked)
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
          gHtmlDocument.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>");
        else
          gHtmlDocument.write("<tr valign=top><td valign=top colspan=2 style='font-size:11px;'>"+formattedTime+"</td></tr><tr><td></td><td valign=top style='font-size:11px;'>");
        gHtmlDocument.write(eventTitle);
        if (calendarEventDisplay.event.location)
          gHtmlDocument.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+locationTag+": "+calendarEventDisplay.event.location);
        if (calendarEventDisplay.event.url)
          gHtmlDocument.write("</td></tr><tr valign=top><td></td><td valign=top style='font-size:11px;'>"+uriTag+": "+calendarEventDisplay.event.url);
        gHtmlDocument.write("</td></tr>");
      }
    }
    gHtmlDocument.write("</table>");

  }
  gHtmlDocument.write("</tr>");

  gHtmlDocument.write("</table>")
}

function printDayView(currentDate) {
  var dayStart = currentDate.getDate();

  var mydateshow="";
  mydateshow+=ArrayOfDayNames[currentDate.getDay()];
  mydateshow+=", ";
  mydateshow+=monthNames[currentDate.getMonth()]+" "+currentDate.getDate()+" "+currentDate.getFullYear();
  gHtmlDocument.write("<table style='border:1px solid black;' width=100%>");
  gHtmlDocument.write("<tr ><td colspan=2 align=center style='font-size:26px;font-weight:bold;border-bottom:1px solid black;'>");
  gHtmlDocument.write(mydateshow);
  gHtmlDocument.write("</td></tr>");
  gHtmlDocument.write("<tr><td width=20% style='border-bottom:1px solid black;'>Time</td><td width=80% style='border-bottom:1px solid black;'>Event</td></tr>");
  gHtmlDocument.write("<tr style='height=20px;'><td colspan=2 style='border-bottom:1px solid black;'> </td></tr>"); // for entering a new appt
  var calendarEventDisplay
  // add each calendarEvent
  dayEventList = eventSource.getEventsForDay( currentDate );

  for( var eventIndex = 0; eventIndex < dayEventList.length; eventIndex++ )
  {
    calendarEventDisplay = dayEventList[ eventIndex ];

    var listpriv=true;
    if (calendarEventDisplay.event.privateEvent)
      if (! gShowprivate.checked)
        listpriv=false;
    if (listpriv)
    {
      gHtmlDocument.write("<tr style='height=20px;'><td valign=top style='border-bottom:1px solid black;'>");
      var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
      var formattedStartTime=returnTime(eventStartTime);
      var eventEndTime = new Date( calendarEventDisplay.event.end.getTime() ) ;
      var formattedEndTime=returnTime(eventEndTime);
      var formattedTime=formattedStartTime+"-"+formattedEndTime;
      if (calendarEventDisplay.event.allDay)
        formattedTime='All Day'; // all day event
      gHtmlDocument.write(formattedTime);
      gHtmlDocument.write("</td><td valign=top style='border-bottom:1px solid black;'>"+calendarEventDisplay.event.title);
      if (calendarEventDisplay.event.description)
        gHtmlDocument.write("<br><Strong>"+descriptionTag+"</strong>: "+calendarEventDisplay.event.description);
      if (calendarEventDisplay.event.location)
        gHtmlDocument.write("<br><strong>"+locationTag+"</strong>: "+calendarEventDisplay.event.location);
      if (calendarEventDisplay.event.url)
        gHtmlDocument.write("<br><strong>"+uriTag+"</strong>: "+calendarEventDisplay.event.url);
      var mystat='Cancelled';
      if (calendarEventDisplay.event.status == 10029)
        mystat='Tentative';
      if (calendarEventDisplay.event.status == 10030)
        mystat='Confirmed';
      gHtmlDocument.write("<br><strong>Status</strong>: "+mystat);
      gHtmlDocument.write("</td></tr>");
      gHtmlDocument.write("<tr style='height=20px;'><td colspan=2  style='border-bottom:1px solid black;'> </td></tr>"); // for entering a new appt
    }
  }

  gHtmlDocument.write("</table>");
}

function printEventArray( calendarEventArray)
{
  gHtmlDocument.write("<table width=100%>");
  gHtmlDocument.write("<tr><td width=20%>Starts</td><td width=20%>Ends</td><td width=60%>Event</td></tr>");
  for (i in calendarEventArray)
  {
    var calEvent=calendarEventArray[i];
    var useit=true;

    if (calEvent.privateEvent)
      if (! gShowprivate.checked)
        useit=false;
    if (useit)
    {
      gHtmlDocument.write("<tr><td valign=top>");
      if (calEvent.allDay)
      {
        gHtmlDocument.write("All Day");
        gHtmlDocument.write("</td><td>");
      } else {
        gHtmlDocument.write(calEvent.start);
        gHtmlDocument.write("</td><td valign=top>");
        gHtmlDocument.write(calEvent.end);
      }
      gHtmlDocument.write("</td><td valign=top>");
      gHtmlDocument.write(calEvent.title);
      if (calEvent.description)
        gHtmlDocument.write("<br>"+descriptionTag+": "+calEvent.description);
      if (calEvent.location)
        gHtmlDocument.write("<br>"+locationTag+": "+calEvent.location);
      if (calEvent.url)
        gHtmlDocument.write("<br>"+uriTag+": "+calEvent.url);
      var mystat='Cancelled';
      if (calEvent.status == 10029)
        mystat='Tentative';
      if (calEvent.status == 10030)
        mystat='Confirmed';

      gHtmlDocument.write("<br>Status: "+mystat);
      gHtmlDocument.write("</td></tr>");
    }
  }
  gHtmlDocument.write("</table>");
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

  gHtmlDocument.write("<table border=0 width=100% style='font-size:26px;font-weight:bold;'><tr ><td align=center valign=bottom>"+monthNames[currentDate.getMonth()]+" "+currentDate.getFullYear()+"</td></tr></table>");
  gHtmlDocument.write("<table style='border:1px solid black;' width=100%>")
  gHtmlDocument.write("<tr>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[weekStart.getDay()]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+1 >6) ? weekStart.getDay()+1-7:weekStart.getDay()+1]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+2 >6) ? weekStart.getDay()+2-7:weekStart.getDay()+2]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+3 >6) ? weekStart.getDay()+3-7:weekStart.getDay()+3]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+4 >6) ? weekStart.getDay()+4-7:weekStart.getDay()+4]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+5 >6) ? weekStart.getDay()+5-7:weekStart.getDay()+5]+"</td>");
  gHtmlDocument.write("<td align=center style='border:1px solid black;background-color:#e0e0e0;FONT-SIZE:12px;FONT-WEIGHT: bold'>"+ArrayOfDayNames[(weekStart.getDay()+6 >6) ? weekStart.getDay()+6-7:weekStart.getDay()+6]+"</td>");
  gHtmlDocument.write("</tr>");

  dayToStart=weekStart.getDate();
  monthToStart=weekStart.getMonth();
  yearToStart=weekStart.getFullYear();
  var inMonth=true;
  var thisDaysDate=new Date(yearToStart, monthToStart, dayToStart);

  for (var w=0; w<6; w++)
  {
    if (inMonth)
    {
      gHtmlDocument.write("<tr>");
      for (var i=0; i<7; i++)
      {
        gHtmlDocument.write("<td align=left valign=top style='border:1px solid black;vertical-alignment:top;' >");
        gHtmlDocument.write("<table valign=top height=100 width=100 style='font-size:10px;'><tr valign=top><td valign=top width=20%>");
        if (thisDaysDate.getMonth()==currentDate.getMonth())
          gHtmlDocument.write(thisDaysDate.getDate());
        gHtmlDocument.write("</td><td width=80% valign=top></td></tr>");
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
              if (! gShowprivate.checked)
                listpriv=false;
            if (listpriv)
            {
              eventTitle=calendarEventDisplay.event.title;
              var eventStartTime = new Date( calendarEventDisplay.event.start.getTime() ) ;
              var formattedStartTime=returnTime(eventStartTime);
              if (calendarEventDisplay.event.allDay)
                gHtmlDocument.write("<tr><td valign=top colspan=2 style='font-size:11px;'>");
              else
                gHtmlDocument.write("<tr><td valign=top align=right style='font-size:11px;'>"+formattedStartTime+"</td><td valign=top style='font-size:11px;'>");
              gHtmlDocument.write(eventTitle);
              gHtmlDocument.write("</td></tr>");
            }
          } //end of events
        } // if it was in the month
        gHtmlDocument.write("</table>");
        gHtmlDocument.write("</td>")
	//advance to the next day
	thisDaysDate.setDate(thisDaysDate.getDate()+1);
      } //end of each day
      gHtmlDocument.write("</tr>");
    } // ok it was in the month
    if ( ( thisDaysDate.getMonth() > currentDate.getMonth() ) ||
        ( thisDaysDate.getFullYear() > currentDate.getFullYear() ) )
      inMonth=false;
  } // end of each week

  gHtmlDocument.write("</table>")
}

function returnTime(timeval) {
  retval= gCalendarWindow.dateFormater.getFormatedTime( timeval );
  if (retval.indexOf("AM") > -1)
    retval=retval.substring(0,retval.indexOf("AM")-1)+'a';
  if (retval.indexOf("PM") > -1)
    retval=retval.substring(0,retval.indexOf("PM")-1)+'p';
  if (retval=='12:00p')
    retval='Noon';
  return retval;
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

/*-----------------------------------------------------------------
 *   Take a Date object and return a displayable date string i.e.: May 5, 1959
 *  :TODO: This should be moved into DateFormater and made to use some kind of
 *         locale or user date format preference.
 */

function formatDate( date )
{
  return( gCalendarWindow.dateFormater.getFormatedDate( date ) );
}

//Is this function Needed in this file?
//No calls to it in this file.
function debug( Text )
{
  dump( "\ncalPrintEngine.js :"+ Text + "\n");
}
