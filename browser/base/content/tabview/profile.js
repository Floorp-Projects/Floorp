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
 * The Original Code is profile.js.
 *
 * The Initial Developer of the Original Code is
 * Ian Gilman <ian@iangilman.com>
 * Portions created by the Initial Developer are Copyright (C) 2010
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

// **********
// Title: profile.js

(function(){
// ##########
// Class: Profile
// A simple profiling helper. 
// TODO: remove before shipping.
window.Profile = {
  // Variable: silent
  // If true, disables logging of results. 
  silent: true,
  
  // Variable: cutoff
  // How many ms a wrapped function needs to take before it gets logged.
  cutoff: 4, 
  
  // Variable: _time
  // Private. The time of the last checkpoint.
  _time: Date.now(),
    
  // ----------
  // Function: wrap
  // Wraps the given object with profiling for each method. 
  wrap: function(obj, name) {
    let self = this;
    [i for (i in Iterator(obj))].forEach(function([key, val]) { 
      if (typeof val != "function") 
        return;
  
      obj[key] = function() {
        let start = Date.now(); 
        try { 
          return val.apply(obj, arguments);
        } finally { 
          let diff = Date.now() - start;
          if(diff >= self.cutoff && !self.silent)
            Utils.log("profile: " + name + "." + key + " = " + diff + "ms"); 
        }
      };
    });
  },
  
  // ----------
  // Function: checkpoint
  // Reset the clock. If label is provided, print the time in milliseconds since the last reset.
  checkpoint: function(label) {
    var now = Date.now();
    
    if(label && !this.silent)
      Utils.log("profile checkpoint: " + label + " = " + (now - this._time) + "ms");
      
    this._time = now;
  }
};

})();
