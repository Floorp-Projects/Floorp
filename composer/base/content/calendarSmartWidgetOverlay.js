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
 * The Original Code is Mozilla.org.
 *
 * The Initial Developer of the Original Code is Daniel Glazman.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

// this function contains all the script to be copied into the document
function innerCalendarCode()
{
  function montharr(m0, m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11)
  {
     this[0] = m0;
     this[1] = m1;
     this[2] = m2;
     this[3] = m3;
     this[4] = m4;
     this[5] = m5;
     this[6] = m6;
     this[7] = m7;
     this[8] = m8;
     this[9] = m9;
     this[10] = m10;
     this[11] = m11;
  }

  function calendar()
  {
     var monthNames = "JanFebMarAprMayJunJulAugSepOctNovDec";
     var today = new Date();
     var thisDay;
     var monthDays = new montharr(31, 28, 31, 30, 31, 30, 31, 31, 30,
        31, 30, 31);
     
     year = today.getYear();
     if (year <= 200)
         year += 1900;
     thisDay = today.getDate();
     

     if (((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0))
        monthDays[1] = 29;


     nDays = monthDays[today.getMonth()];


     firstDay = today;
     firstDay.setDate(1); // works fine for most systems
     testMe = firstDay.getDate();
     if (testMe == 2)
          firstDay.setDate(0);    

     startDay = firstDay.getDay();
       
     document.writeln("<CENTER>");
     document.write("<TABLE BORDER='1' BGCOLOR=White>");
     document.write("<TR><TH COLSPAN=7>");
     document.write(monthNames.substring(today.getMonth() * 3,
        (today.getMonth() + 1) * 3));
     document.write(". ");
     document.write(year);
    
     document.write("<TR><TH>Sun<TH>Mon<TH>Tue<TH>Wed<TH>Thu<TH>Fri<TH>Sat");


     document.write("<TR>");
     column = 0;
     for (i=0; i<startDay; i++)
     {
        document.write("<TD>");
        column++;
     }

     for (i=1; i<=nDays; i++)
     {
        document.write("<TD>");
        if (i == thisDay)
           document.write("<FONT COLOR=\"#FF0000\">")
        document.write(i);
        if (i == thisDay)
          document.write("</FONT>")
        column++;
        if (column == 7)
        {
           document.write("<TR>"); 
           column = 0;
        }
     }
     document.write("</TABLE>");
     document.writeln("</CENTER>");
  }

  calendar();
}


function addCalendarSmartWidget()
{
  var bodyElt = GetBodyElement();
  var doc = GetCurrentEditor().document;

  var enclosingDiv     = doc.createElement("div");

  var innerScript      = doc.createElement("script");
  var innerScriptCdata = doc.createTextNode(innerCalendarCode.toString() +
                                            "\n\n  innerCalendarCode();");
  innerScript.appendChild(innerScriptCdata);

  var noScript            = doc.createElement("noscript");
  var innerNoScriptImage  = doc.createElement("img");
  innerNoScriptImage.setAttribute("src", "chrome://editor/content/images/calendarSW.gif");
  noScript.appendChild(innerNoScriptImage);

  enclosingDiv.appendChild(noScript);
  enclosingDiv.appendChild(innerScript);

  bodyElt.appendChild(enclosingDiv);
  enclosingDiv.setAttribute("style",
                            "position: absolute;" +
                            "top: " + (window.scrollX + 50) + "px;" +
                            "left: " + (window.scrollY + 50) + "px;");
}
