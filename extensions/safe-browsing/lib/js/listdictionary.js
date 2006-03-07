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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// This file implements a Dictionary data structure using a list
// (array). We could instead use an object, but using a list enables
// us to have ordering guarantees and iterators. The interface it exposes 
// is:
// 
// addMember(item)
// removeMember(item)
// isMember(item)
// forEach(func)
//
// TODO: this class isn't really a Dictionary, it's more like a 
//       membership set (i.e., a set without union and whatnot). We
//       should probably change the name to avoid confusion.

/**
 * Create a new Dictionary data structure.
 *
 * @constructor
 * @param name A string used to name the dictionary
 */
function ListDictionary(name) {
  this.name_ = name;
  this.members_ = [];
}

/**
 * Look an item up.
 *
 * @param item An item to look up in the dictionary
 * @returns Boolean indicating if the parameter is a member of the dictionary
 */
ListDictionary.prototype.isMember = function(item) {
  for (var i=0; i < this.members_.length; i++)
    if (this.members_[i] == item)
      return true;
  return false;
}

/**
 * Add an item
 *
 * @param item An item to add (does not check for dups)
 */
ListDictionary.prototype.addMember = function(item) {
  this.members_.push(item);
}

/**
 * Remove an item
 *
 * @param item The item to remove (doesn't check for dups)
 * @returns Boolean indicating if the item was removed
 */
ListDictionary.prototype.removeMember = function(item) {
  for (var i=0; i < this.members_.length; i++) {
    if (this.members_[i] == item) {
      for (var j=i; j < this.members_.length; j++)
        this.members_[j] = this.members_[j+1];

      this.members_.length--;
      return true;
    }
  }
  return false;
}

/**
 * Apply a function to each of the members. Does NOT replace the members
 * in the dictionary with results -- it just calls the function on each one.
 *
 * @param func Function to apply to the dictionary's members
 */
ListDictionary.prototype.forEach = function(func) {
  if (typeof func != "function")
    throw new Error("argument to forEach is not a function, it's a(n) " + 
                    typeof func);

  for (var i=0; i < this.members_.length; i++)
    func(this.members_[i]);
}
