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

const FIRST_DELAY_STARTUP = 100;
const FIRST_DELAY_RESCHEDULE = 100;
const FIRST_DELAY_REGISTER = 10000;
const FIRST_DELAY_UNREGISTER = 0;
const REPEAT_DELAY = 180000;

var sideBarOperationListener = {
    onOperationComplete: function(aCalendar, aStatus, aOperationType, aId, aDetail) {
        if (!Components.isSuccessCode(aStatus)) {
            var invitationsBox = document.getElementById("invitations");
            invitationsBox.setAttribute("hidden", "true");
        }
    },
    onGetResult: function(aCalendar, aStatus, aItemType, aDetail, aCount, aItems) {
        if (!Components.isSuccessCode(aStatus)) {
            return;
        }
        var invitationsBox = document.getElementById("invitations");
        var value = invitationsLabel + " (" + aCount + ")";
        invitationsBox.setAttribute("value", value);
        invitationsBox.removeAttribute("hidden");
    }
};

var calendarManagerObserver = {
    mSideBar: this,
    onCalendarRegistered: function(aCalendar) {
        this.mSideBar.rescheduleInvitationsUpdate(FIRST_DELAY_REGISTER, REPEAT_DELAY);
    },
    onCalendarUnregistering: function(aCalendar) {
        this.mSideBar.rescheduleInvitationsUpdate(FIRST_DELAY_UNREGISTER, REPEAT_DELAY);
    },
    onCalendarDeleting: function(aCalendar) {
    },
    onCalendarPrefSet: function(aCalendar, aName, aValue) {
    },
    onCalendarPrefDeleting: function(aCalendar, aName) {
    }
};

function onLoad() {
    scheduleInvitationsUpdate(FIRST_DELAY_STARTUP, REPEAT_DELAY);
    getCalendarManager().addObserver(calendarManagerObserver);
    document.addEventListener("unload", onUnload, true);
}

function onUnload() {
    document.removeEventListener("unload", onUnload, true);
    getCalendarManager().removeObserver(calendarManagerObserver);
}

function scheduleInvitationsUpdate(firstDelay, repeatDelay) {
    getInvitationsManager().scheduleInvitationsUpdate(firstDelay, repeatDelay, sideBarOperationListener);
}

function rescheduleInvitationsUpdate(firstDelay, repeatDelay) {
    getInvitationsManager().cancelInvitationsUpdate();
    scheduleInvitationsUpdate(firstDelay, repeatDelay);
}

function openInvitationsDialog() {
    getInvitationsManager().cancelInvitationsUpdate();
    getInvitationsManager().openInvitationsDialog(
        sideBarOperationListener,
        function() { scheduleInvitationsUpdate(FIRST_DELAY_RESCHEDULE, REPEAT_DELAY); });
}

onLoad();
