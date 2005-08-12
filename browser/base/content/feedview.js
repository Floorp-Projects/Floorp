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
 * The Original Code is Feedview for Firefox.
 *
 * The Initial Developer of the Original Code is
 * Tom Germeau <tom.germeau@epigoon.com>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Goodger <ben@mozilla.org>
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

var FeedView = {
  /**
   * Attempt to get a JavaScript Date object from a string
   * @param   str
   *          A string that may contain a formatted date
   * @returns A JavaScript Date object representing the date
   */
  _xmlDate: function(str) {
    str = str.replace("Z", "+00:00");
    var d = str.replace(/^(\d{4})-(\d\d)-(\d\d)T([0-9:]*)([.0-9]*)(.)(\d\d):(\d\d)$/, '$1/$2/$3 $4 $6$7$8');
    d = Date.parse(d);
    d += 1000 * RegExp.$5;
    return new Date(d);
  },
  
  /**
   * Normalizes date strings embedded in the feed into a common format. 
   */
  _initializeDates: function() {
    // Normalize date formatting for all feed entries
    var divs = document.getElementsByTagName("div");
    for (var i = 0; i < divs.length; i++) {
      var d = divs[i].getAttribute("date");
      dump("*** D = " + d + "\n");
      if (d) {
        // If it is an RFC... date -> first parse it...
        // otherwise try the Date() constructor.
        d = d.indexOf("T") ? xmlDate(d) : new Date(d);

        // If the date could be parsed...
        if (d instanceof Date) {
          // XXX It would be nicer to say day = "Today" or "Yesterday".
          var day = d.toGMTString();
          day = day.substring(0, 11);
          
          function padZeros(num) {
            return num < 10 ? "0" + num : num;
          }
          divs[i].getElementsByTagName("span")[0].textContent =
            day + " @ " + padZeros(d.getHours()) +  ":" + padZeros(d.getMinutes());
        }
      }
    }
  },
  
  /**
   * Returns the value of a boolean preference
   * @param   name
   *          The name of the preference
   * @returns The value of the preference
   */
  _getBooleanPref: function(name) {
    return document.getElementById("data").getAttribute(name) == "true";
  },
  
  /**
   * Returns the value of an integer preference
   * @param   name
   *          The name of the preference
   * @returns The value of the preference
   */
  _getIntegerPref: function(name) {
    return parseInt(document.getElementById("data").getAttribute(name));
  },
  
  /**
   * Reload the page after an interval
   */
  reload: function() {
    location.reload();
  },
  
  /**
   * Initialize the feed view
   */
  init: function() {
    // Hide the menu if the user chose to have it closed
    if (!this._getBooleanPref("showMenu")) 
      document.getElementById("menubox").style.display = "none";

    // Normalize the date formats
    this._initializeDates();

    // Set up the auto-reload timer    
    setTimeout("RSSPrettyPrint.refresh()", 
               this._getIntegerPref("reloadInterval") * 1000);
  },
};


