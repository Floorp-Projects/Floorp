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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

var gLightningPane = {
    init: function() {
        dump("init called\n");
        var tzListBox = document.getElementById("timezone-listbox");
        var icsService = Components.classes["@mozilla.org/calendar/ics-service;1"].createInstance(Components.interfaces.calIICSService);
        var timezones = icsService.timezoneIds;

        prefValue = document.getElementById("calendar.timezone.local").value;
        while (timezones.hasMore()) {
            var tzid = timezones.getNext();
            var listItem = tzListBox.appendItem(tzid, tzid);
            if (tzid == prefValue) {
                tzListBox.selectItem(listItem);
                tzListBox.scrollToIndex(tzListBox.getIndexOfItem(listItem));
            }
        }
    },

    getTimezoneResult: function() {
        dump("looking for timezone\n");
        var tzListBox = document.getElementById("timezone-listbox");
        if (tzListBox.selectedItems.length > 0) {
            var value = tzListBox.selectedItems[0].getAttribute("label");
            dump("value: " + value + "\n");
            return value;
        }
        return undefined;
    },

    setTimezone: function() {
        var prefValue = document.getElementById("calendar.timezone.local")
            .value;

        var tzListBox = document.getElementById("timezone-listbox");
        var children = tzListBox.childNodes;
        for (var i = 0; i < children.length; i++) {
            var label = children[i];
            if (label.getAttribute("value") == prefValue) {
                dump("selecting: " + label.getAttribute("value") + "\n");
                tzListBox.selectItem(label);
                return;
            }
        }
    }
}
