/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var XPCU = 
{ 
  getService: function(aURL, aInterface)
  {
    try {
      return Components.classes[aURL].getService(Components.interfaces[aInterface]);
    } catch (ex) {
      dump("Error getting service: " + aURL + ", " + aInterface + "\n" + ex);
      return null;
    }
  },

  createInstance: function (aURL, aInterface)
  {
    try {
      return Components.classes[aURL].createInstance(Components.interfaces[aInterface]);
    } catch (ex) {
      dump("Error creating instance: " + aURL + ", " + aInterface + "\n" + ex);
      return null;
    }
  },

  QI: function(aEl, aIName)
  {
    try {
      return aEl.QueryInterface(Components.interfaces[aIName]);
    } catch (ex) {
      dump("Error in QI: " + aIName + "\n" + ex);
      return null;
    }
  }

};