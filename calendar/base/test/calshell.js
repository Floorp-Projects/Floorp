/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org calendar code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Dan Mosedale <dan.mosedale@oracle.com>
 *                 Mike Shaver <mike.x.shaver@oracle.com>
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

const C = Components;
const CI = C.interfaces;

// I wonder how many copies of this are floating around
function findErr(result)
{
    for (var i in C.results) {
        if (C.results[i] == result) {
            return i;
        }
    }
    dump("No result code found for " + result + "\n");
}

function getService(contract, iface)
{
    return C.classes[contract].getService(CI[iface]);
}

function createInstance(contract, iface)
{
    return C.classes[contract].createInstance(CI[iface]);
}



function calOpListener() {
}

calOpListener.prototype = 
{

  onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, 
                                aDetail) {
    dump("onOperationComplete:\n\t");
    dump(aCalendar + "\n\t" + findErr(aStatus) + "\n\t" + aOperationType + 
	 "\n\t" + aId + "\n\t" + aDetail + "\n");
    return;
  },

  onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, 
                        aItems) {
    dump("onGetResult: \n\t");
    dump(aCalendar + "\n\t" + findErr(aStatus) + "\n\t" + aItemType + "\n\t" +
	 aDetail + "\n\t" + aCount + "\n\t" + aItems + "\n");
  }
}

function GETITEM(aId) {
  
}
const ioSvc = getService("@mozilla.org/network/io-service;1",
                         "nsIIOService");
 
function URLFromSpec(spec)
{
  return ioSvc.newURI(spec, null, null);
}

const evQSvc = getService("@mozilla.org/event-queue-service;1",
                          "nsIEventQueueService");
const evQ = evQSvc.getSpecialEventQueue(CI.nsIEventQueueService.CURRENT_THREAD_EVENT_QUEUE);

function runEventPump()
{
    pumpRunning = true;
    while (pumpRunning) {
        evQ.processPendingEvents();
    }
}

function stopEventPump()
{
    pumpRunning = false;
}

