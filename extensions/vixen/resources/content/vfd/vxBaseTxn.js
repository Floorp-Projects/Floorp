/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Ben Goodger <ben@netscape.com> (Original Author)
 */

function vxBaseTxn ()
{
}  

vxBaseTxn.prototype = {
  mID: null,

  mListeners: [],

  notifyListeners: function (aEvent, aTransaction, aIRQ) 
  {
    for (var i = 0; i < this.mListeners.length; i++) {
      if (aEvent in this.mListeners[i])
        this.mListeners[i][aEvent](undefined, aTransaction, aIRQ);
    }
  },
  
  addListener: function (aListener)
  {
    this.mListeners = this.mListeners.concat(aListener);
  },
  
  removeListener: function (aListener)
  {
    var listeners = [].concat(aListener);
    for (var i = 0; i < this.mListeners.length; i++) {
      for (var k = 0; k < listeners.length; k++) {
        if (this.mListeners[i] == listeners[k]) {
          this.mListeners.splice(i,1);
          break;
        }
      }
    }
  },
  
  init: function ()
  {
    this.mID += this.generateID();
  },
  
  /** 
   * Generate a unique identifier for a transaction
   */
  generateID: function ()
  {
    var val = ((new Date()).getUTCMilliseconds())*Math.random()*100000;
    return Math.floor(val);
  }
};

