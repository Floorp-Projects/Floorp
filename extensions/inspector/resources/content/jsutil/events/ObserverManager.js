/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

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
