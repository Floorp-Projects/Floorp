/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


/* functions for disabling front end elements when the appropriate
   back-end preference is locked. */


var nsPrefBranch = null;

// Prefs in mailnews require dynamic portions to indicate 
// which of multiple servers or identies.  This function
// takes a string and a xul element.
//  The string is a prefstring with a token %tokenname%.
//  The xul element has an attribute of name |tokenname|
//  whose value is substituted into the string and returned
//  by the function.
//  Any tokens which do not have associated attribute value
//  are not substituted, and left in the string as-is.
function substPrefTokens(aStr, element)
{
  var tokenpat = /%(\w+)%/;
  var token;
  var newprefstr = "";

  var prefPartsArray = aStr.split(".");
  /* here's a little loop that goes through
     each part of the string separated by a dot, and
     if any parts are of the form %string%, it will replace
     them with the value of the attribute of that name from
     the xul object */
  for (var i=0; i< prefPartsArray.length; i++) {
    token = prefPartsArray[i].match(tokenpat);
    if (token) { /* we've got a %% match */
      if (token[1]) {
        if (element[token[1]]) {
          newprefstr += element[token[1]] + "."; // here's where we get the info
        } else { /* all we got was this stinkin % */
          newprefstr += prefPartsArray[i] + ".";
        }
      }
    } else /* if (token) */ {
      newprefstr += prefPartsArray[i] + ".";
    }
  }
  newprefstr = newprefstr.slice(0,-1); // remove the last char, a dot
  if (newprefstr.length <=0 )
    newprefstr = null;

  return newprefstr;
}

// A simple function which given a xul element with
// the pref related attributes (pref, preftype, prefstring)
// return if the prefstring specified in that element is
// locked (true/false).
// If it does not have a valid prefstring, a false is returned.
function getAccountValueIsLocked(element)
{
  var prefstr = "";
  var preftype;
  var prefval;
  var prefstring;

  if (!nsPrefBranch) {
    var prefService = Components.classes["@mozilla.org/preferences-service;1"];
    prefService = prefService.getService();
    prefService = prefService.QueryInterface(Components.interfaces.nsIPrefService);

    nsPrefBranch = prefService.getBranch(null);
  }

  prefstring = element.getAttribute("prefstring");
  if (prefstring) {
    preftype    = element.getAttribute("preftype");
    prefstr = substPrefTokens(prefstring, element);
    // see if the prefstring is locked
    if (prefstr) {
      var bLocked=nsPrefBranch.prefIsLocked(prefstr);
      return bLocked;
    }
  }
  return false;
}

