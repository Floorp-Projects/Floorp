/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/***************************************************************
* ObserverManager -----------------------------------------------
*  Manages observer and event dispatching for an object.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
****************************************************************/

////////////////////////////////////////////////////////////////////////////
//// class ObserverManager

function ObserverManager(aTarget)
{
  this.mTarget = aTarget;
  this.mObservers = {};
}

ObserverManager.prototype = 
{
  addObserver: function(aEventName, aObserver)
  {
    var list;
    if (!(aEventName in this.mObservers)) {
      list = [];
      this.mObservers[aEventName] = list;
    } else
      list = this.mObservers[aEventName];
      
    
    list.push(aObserver);
  },

  removeObserver: function(aEventName, aObserver)
  {
    if (aEventName in this.mObservers) {
      var list = this.mObservers[aEventName];
      for (var i = 0; i < list.length; ++i) {
        if (list[i] == aObserver) {
          list.splice(i, 1);
          return;
        }
      }
    }
  },

  dispatchEvent: function(aEventName, aEventData)
  {
    if (aEventName in this.mObservers) {
      if (!("target" in aEventData))
        aEventData.target = this.mTarget;
      aEventData.type = aEventName;
      
      var list = this.mObservers[aEventName];
      for (var i = 0; i < list.length; ++i)
        list[i].onEvent(aEventData);
    }
  }

};
