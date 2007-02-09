/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Sun Microsystems code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Thomas Benisch <thomas.benisch@sun.com>
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

function onLoad() {
    var operationListener = {
        onOperationComplete:
        function(aCalendar, aStatus, aOperationType, aId, aDetail) {
            var updatingBox = document.getElementById("updating-box");
            updatingBox.setAttribute("hidden", "true");
            var richListBox = document.getElementById("invitations-listbox");
            if (richListBox.getRowCount() > 0) {
                richListBox.selectedIndex = 0;
            } else {
                var noInvitationsBox =
                    document.getElementById("noinvitations-box");
                noInvitationsBox.removeAttribute("hidden");
            }
        },
        onGetResult:
        function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
            if (!Components.isSuccessCode(aStatus))
                return;
            document.title = invitationsText + " (" + aCount + ")";
            var updatingBox = document.getElementById("updating-box");
            updatingBox.setAttribute("hidden", "true");
            var richListBox = document.getElementById("invitations-listbox");
            for each (var item in aItems) {
                richListBox.addCalendarItem(item);
            }
        }
    };

    var updatingBox = document.getElementById("updating-box");
    updatingBox.removeAttribute("hidden");

    var args = window.arguments[0];
    args.invitationsManager.getInvitations(
        false,
        operationListener,
        args.onLoadOperationListener);

    opener.setCursor("auto");
}

function onUnload() {
    var args = window.arguments[0];
    args.requestManager.cancelPendingRequests();
}

function onAccept() {
    var args = window.arguments[0];
    fillJobQueue(args.queue);
    args.invitationsManager.processJobQueue(args.queue, args.finishedCallBack);
    return true;
}

function onCancel() {
    var args = window.arguments[0];
    if (args.finishedCallBack) {
        args.finishedCallBack();
    }
}

function fillJobQueue(queue) {
    var richListBox = document.getElementById("invitations-listbox");
    var rowCount = richListBox.getRowCount();
    for (var i = 0; i < rowCount; i++) {
        var richListItem = richListBox.getItemAtIndex(i);
        var newStatus = richListItem.participationStatus;
        var oldStatus = richListItem.initialParticipationStatus;
        if (newStatus != oldStatus) {
            var actionString = "modify";
            var oldCalendarItem = richListItem.calendarItem;
            var newCalendarItem = (oldCalendarItem.isMutable)
                ? oldCalendarItem : oldCalendarItem.clone();
            richListItem.setCalendarItemParticipationStatus(newCalendarItem,
                newStatus);
            var job = {
                action: actionString,
                oldItem: oldCalendarItem,
                newItem: newCalendarItem
            };
            queue.push(job);
        }
    }
}
