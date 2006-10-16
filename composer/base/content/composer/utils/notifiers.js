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
 * The Original Code is Composer.
 *
 * The Initial Developer of the Original Code is
 * Disruptive Innovations SARL.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Glazman (glazman@disruptive-innovations.com), Original author
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

var NotifierUtils = {

  mKeywords: {},
  mContexts: {},

  // PRIVATE

  _error: function _error(aErrorString, aHelperString)
  {
    var caller = this._error.caller;
    var s = "";
    for (i in this)
      if (this[i].toString() == caller)
      {
        s = i + "()";
        break;
      }
    var errorString = aErrorString +
                      (aHelperString ? " '" + aHelperString + "'" : "");
    dump("### NotifierUtils: " + s + " " + errorString + "\n");
  },

  // PUBLIC

  cleanNotifier: function cleanNotifier(aKeyword)
  {
    if (aKeyword in this.mKeywords)
    {
      this.mKeywords[aKeyword] = [];
      this.mContexts[aKeyword] = [];
    }
    else
      this._error("called with unrecognized notifier id", aKeyword);
  },

  addNotifier: function addNotifier(aKeyword)
  {
    if (aKeyword)
    {
      this.mKeywords[aKeyword] = [];
      this.mContexts[aKeyword] = [];
    }
  },

  addNotifierCallback: function addNotifierCallback(aKeyword, aFn, aContext)
  {
    if (!aKeyword || !aFn || (typeof aFn != "function"))
    {
      this._error("invalid call\n");
      return;
    }

    if (aKeyword in this.mKeywords)
    {
      this.mKeywords[aKeyword].push( aFn );
      this.mContexts[aKeyword].push( aContext );
    }
    else
      this._error("called with unrecognized notifier id", aKeyword);
  },

  removeNotifierCallback: function removeNotifierCallback(aKeyword, aFn)
  {
    if (!aKeyword || !aFn || (typeof aFn != "function"))
    {
      this._error("invalid call\n");
      return;
    }

    if (aKeyword in this.mKeywords)
    {
      var index = this.mKeywords[aKeyword].indexOf(aFn);
      if (index != -1)
      {
        this.mKeywords[aKeyword].splice(index, 1);
        this.mContexts[aKeyword].splice(index, 1);
      }
      else
        this._error("no such callback for notifier id", aKeyword);
    }
    else
      this._error("called with unrecognized notifier id", aKeyword);
  },

  notify: function notify(aKeyword)
  {
    if (aKeyword in this.mKeywords)
    {
      var processes = this.mKeywords[aKeyword];
      var contexts  = this.mContexts[aKeyword];

      for (var i = 0; i < processes.length; i++)
        try {
          processes[i].apply(contexts[i], arguments);
        }
        catch (e) {
          this._error("callback raised an exception for notifier id", aKeyword);
        }
    }
    else
      this._error("called with unrecognized notifier id", aKeyword);
  }

};
