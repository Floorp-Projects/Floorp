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
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
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

//
// calTodo.js
//

//
// constructor
//
function calTodo() {
    this.wrappedJSObject = this;
    this.initItemBase();
    this.initTodo();

    this.todoPromotedProps = {
        "DTSTART": true,
        "DTEND": true,
        "DTSTAMP": true,
        "DUE": true,
        "COMPLETED": true,
        "PERCENT-COMPLETE": true,
        __proto__: this.itemBasePromotedProps
    };
}

// var trickery to suppress lib-as-component errors from loader
var calItemBase;

var calTodoClassInfo = {
    getInterfaces: function (count) {
        var ifaces = [
            Components.interfaces.nsISupports,
            Components.interfaces.calIItemBase,
            Components.interfaces.calITodo,
            Components.interfaces.calIInternalShallowCopy,
            Components.interfaces.nsIClassInfo
        ];
        count.value = ifaces.length;
        return ifaces;
    },

    getHelperForLanguage: function (language) {
        return null;
    },

    contractID: "@mozilla.org/calendar/todo;1",
    classDescription: "Calendar Todo",
    classID: Components.ID("{7af51168-6abe-4a31-984d-6f8a3989212d}"),
    implementationLanguage: Components.interfaces.nsIProgrammingLanguage.JAVASCRIPT,
    flags: 0
};

calTodo.prototype = {
    __proto__: calItemBase ? (new calItemBase()) : {},

    QueryInterface: function (aIID) {
        if (aIID.equals(Components.interfaces.calITodo) ||
            aIID.equals(Components.interfaces.calIInternalShallowCopy))
            return this;

        if (aIID.equals(Components.interfaces.nsIClassInfo))
            return calTodoClassInfo;

        return this.__proto__.__proto__.QueryInterface.apply(this, aIID);
    },

    initTodo: function () {
        this.mEntryDate = new CalDateTime();
        this.mDueDate = new CalDateTime();
        this.mCompletedDate = new CalDateTime();
        this.mPercentComplete = 0;
    },

    cloneShallow: function (aNewParent) {
        var m = new calTodo();
        this.cloneItemBaseInto(m, aNewParent);
        m.mEntryDate = this.mEntryDate.clone();
        m.mDueDate = this.mDueDate.clone();
        m.mCompletedDate = this.mCompletedDate.clone();
        m.mPercentComplete = this.mPercentComplete;
        return m;
    },

    clone: function () {
        var m;

        if (this.parentItem != this) {
            var clonedParent = this.mParentItem.clone();
            m = clonedParent.recurrenceInfo.getExceptionFor (this.recurrenceId, true);
        } else {
            m = this.cloneShallow(null);
        }

        return m;
    },

    createProxy: function () {
        var m = new calTodo();
        m.initializeProxy(this);

        return m;
    },

    makeImmutable: function () {
        this.mEntryDate.makeImmutable();
        this.mDueDate.makeImmutable();
        this.mCompletedDate.makeImmutable();

        this.makeItemBaseImmutable();
    },

    get recurrenceStartDate() {
        return this.mEntryDate;
    },

    icsEventPropMap: [
    { cal: "mEntryDate", ics: "startTime" },
    { cal: "mDueDate", ics: "dueTime" },
    { cal: "mCompletedDate", ics: "completedTime" }],

    set icalString(value) {
        this.icalComponent = icalFromString(value);
    },

    get icalString() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var calcomp = icssvc.createIcalComponent("VCALENDAR");
        calcomp.prodid = "-//Mozilla Calendar//NONSGML Sunbird//EN";
        calcomp.version = "2.0";
        calcomp.addSubcomponent(this.icalComponent);
        return calcomp.serializeToICS();
    },

    get icalComponent() {
        const icssvc = Components.
          classes["@mozilla.org/calendar/ics-service;1"].
          getService(Components.interfaces.calIICSService);
        var icalcomp = icssvc.createIcalComponent("VTODO");
        this.fillIcalComponentFromBase(icalcomp);
        this.mapPropsToICS(icalcomp, this.icsEventPropMap);

        var percentprop = icssvc.createIcalProperty("PERCENT-COMPLETE");
        percentprop.stringValue = this.mPercentComplete;
        icalcomp.addProperty(percentprop);

        var bagenum = this.mProperties.enumerator;
        while (bagenum.hasMoreElements()) {
            var iprop = bagenum.getNext().
                QueryInterface(Components.interfaces.nsIProperty);
            try {
                var icalprop = icssvc.createIcalProperty(iprop.name);
                icalprop.stringValue = iprop.value;
                icalcomp.addProperty(icalprop);
            } catch (e) {
                // dump("failed to set " + iprop.name + " to " + iprop.value +
                // ": " + e + "\n");
            }
        }
        return icalcomp;
    },

    todoPromotedProps: null,

    set icalComponent(todo) {
        this.modify();
        if (todo.componentType != "VTODO") {
            todo = todo.getFirstSubcomponent("VTODO");
            if (!todo)
                throw Components.results.NS_ERROR_INVALID_ARG;
        }

        this.setItemBaseFromICS(todo);
        this.mapPropsFromICS(todo, this.icsEventPropMap);
        this.mIsAllDay = this.mStartDate && this.mStartDate.isDate;

        this.importUnpromotedProperties(todo, todoPromotedProps);
        // Importing didn't really change anything
        this.mDirty = false;
    },

    isPropertyPromoted: function (name) {
        return (this.todoPromotedProps[name]);
    },
};
        
// var decl to prevent spurious error messages when loaded as component

var makeMemberAttr;
if (makeMemberAttr) {
    makeMemberAttr(calTodo, "mEntryDate", null, "entryDate");
    makeMemberAttr(calTodo, "mDueDate", null, "dueDate");
    makeMemberAttr(calTodo, "mCompletedDate", null, "completedDate");
    makeMemberAttr(calTodo, "mPercentComplete", 0, "percentComplete");
}
