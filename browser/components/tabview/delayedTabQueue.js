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
 * The Original Code is delayedTabQueue.js.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Tim Taubert <ttaubert@mozilla.com>
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
// Title: delayedTabQueue.js

// ##########
// Class: DelayedTabQueue
// A queue that delays calls to a given callback specific for tabs. Tabs are
// sorted by priority.
//
// Parameters:
//   callback - the callback that is called with the tab to process as the first
//              argument
//   options - various options for this tab queue (see below)
//
// Possible options:
//   interval - interval between the heart beats in msecs
//   cap - maximum time in msecs to be used by one heart beat
function DelayedTabQueue(callback, options) {
  this._callback = callback;
  this._heartbeatInterval = (options && options.interval) || 500;
  this._heartbeatCap = (options && options.cap) || this._heartbeatInterval / 2;

  this._entries = [];
  this._tabPriorities = new WeakMap();
}

DelayedTabQueue.prototype = {
  _callback: null,
  _entries: null,
  _tabPriorities: null,
  _isPaused: false,
  _heartbeat: null,
  _heartbeatCap: 0,
  _heartbeatInterval: 0,
  _lastExecutionTime: 0,

  // ----------
  // Function: pause
  // Pauses the heartbeat.
  pause: function DQ_pause() {
    this._isPaused = true;
    this._stopHeartbeat();
  },

  // ----------
  // Function: resume
  // Resumes the heartbeat.
  resume: function DQ_resume() {
    this._isPaused = false;
    this._startHeartbeat();
  },

  // ----------
  // Function: push
  // Pushes a new tab onto the queue.
  //
  // Parameters:
  //   tab - the tab to be added to the queue
  push: function DQ_push(tab) {
    let prio = this._getTabPriority(tab);

    if (this._tabPriorities.has(tab)) {
      let oldPrio = this._tabPriorities.get(tab);

      // re-sort entries if the tab's priority has changed
      if (prio != oldPrio) {
        this._tabPriorities.set(tab, prio);
        this._sortEntries();
      }

      return;
    }

    let shouldDefer = this._isPaused || this._entries.length ||
                      Date.now() - this._lastExecutionTime < this._heartbeatInterval;

    if (shouldDefer) {
      this._tabPriorities.set(tab, prio);

      // create the new entry
      this._entries.push(tab);
      this._sortEntries();
      this._startHeartbeat();
    } else {
      // execute immediately if there's no reason to defer
      this._executeCallback(tab);
    }
  },

  // ----------
  // Function: _sortEntries
  // Sorts all entries in the queue by their priorities.
  _sortEntries: function DQ__sortEntries() {
    let self = this;

    this._entries.sort(function (left, right) {
      let leftPrio = self._tabPriorities.get(left);
      let rightPrio = self._tabPriorities.get(right);

      return leftPrio - rightPrio;
    });
  },

  // ----------
  // Function: _getTabPriority
  // Determines the priority for a given tab.
  //
  // Parameters:
  //   tab - the tab for which we want to get the priority
  _getTabPriority: function DQ__getTabPriority(tab) {
    if (this.parent && (this.parent.isStacked() &&
        !this.parent.isTopOfStack(this) &&
        !this.parent.expanded))
      return 0;

    return 1;
  },

  // ----------
  // Function: _startHeartbeat
  // Starts the heartbeat.
  _startHeartbeat: function DQ__startHeartbeat() {
    if (!this._heartbeat) {
      this._heartbeat = setTimeout(this._checkHeartbeat.bind(this),
                                   this._heartbeatInterval);
    }
  },

  // ----------
  // Function: _checkHeartbeat
  // Checks the hearbeat and processes as many items from queue as possible.
  _checkHeartbeat: function DQ__checkHeartbeat() {
    this._heartbeat = null;

    // return if processing is paused or there are no entries
    if (this._isPaused || !this._entries.length)
      return;

    // process entries only if the UI is idle
    if (UI.isIdle()) {
      let startTime = Date.now();
      let timeElapsed = 0;

      do {
        // remove the tab from the list of entries and execute the callback
        let tab = this._entries.shift();
        this._tabPriorities.delete(tab);
        this._executeCallback(tab);

        // track for how long we've been processing entries and make sure we
        // dont't do it longer than {_heartbeatCap} msecs
        timeElapsed = this._lastExecutionTime - startTime;
      } while (this._entries.length && timeElapsed < this._heartbeatCap);
    }

    // keep the heartbeat active until all entries have been processed
    if (this._entries.length)
      this._startHeartbeat();
  },

  _executeCallback: function DQ__executeCallback(tab) {
    this._lastExecutionTime = Date.now();
    this._callback(tab);
  },

  // ----------
  // Function: _stopHeartbeat
  // Stops the heartbeat.
  _stopHeartbeat: function DQ__stopHeartbeat() {
    if (this._heartbeat)
      this._heartbeat = clearTimeout(this._heartbeat);
  },

  // ----------
  // Function: remove
  // Removes a given tab from the queue.
  //
  // Parameters:
  //   tab - the tab to remove
  remove: function DQ_remove(tab) {
    if (!this._tabPriorities.has(tab))
      return;

    this._tabPriorities.delete(tab);

    let index = this._entries.indexOf(tab);
    if (index > -1)
      this._entries.splice(index, 1);
  },

  // ----------
  // Function: clear
  // Removes all entries from the queue.
  clear: function DQ_clear() {
    let tab;

    while (tab = this._entries.shift())
      this._tabPriorities.delete(tab);
  }
};

