/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsHashPropertyBag.h"

#include "calItemBase.h"

NS_IMPL_ISUPPORTS1(calItemBase, calIItemBase);

/***
 *** calItemBase impl
 ***/

calItemBase::calItemBase()
{
    // do we need a ::Init method?
    NS_NewHashPropertyBag(getter_AddRefs(mProperties));
}

/**
 ** calIItemBase interface
 **/

// helpers for value types
#define VALUETYPE_ATTR_GETTER(mtype,name) \
NS_IMETHODIMP \
calItemBase::Get##name (mtype *_retval) { \
    *_retval = m##name; \
    return NS_OK; \
}

#define VALUETYPE_ATTR_SETTER(mtype,name) \
NS_IMETHODIMP \
calItemBase::Set##name (mtype aValue) { \
    if (m##name != aValue) { \
        m##name = aValue; \
        mLastModified = PR_Now(); \
    } \
    return NS_OK;
}

#define VALUETYPE_ATTR(mtype,name) \
    VALUETYPE_ATTR_GETTER(mtype,name) \
    VALUETYPE_ATTR_SETTER(mtype,name)

// helpers for interface types
#define ISUPPORTS_ATTR_GETTER(mtype,name)
NS_IMETHODIMP
calItemBase::Get##name (mtype **_retval) { \
    NS_IF_ADDREF (*_retval = m##name); \
    return NS_OK; \
}

#define ISUPPORTS_ATTR_SETTER(mtype,name) \
NS_IMETHODIMP \
calItemBase::Set##name (mtype *aValue) { \
    if (m##name != aValue) { \
        m##name = aValue; \
        mLastModified = PR_Now(); \
    } \
    return NS_OK;
}

#define ISUPPORTS_ATTR(mtype,name) \
    ISUPPORTS_ATTR_GETTER(mtype,name) \
    ISUPPORTS_ATTR_SETTER(mtype,name)

/*
 * calIItemBase attributes
 */


// General attrs

ISUPPORTS_ATTR(calIDateTime, CreationDate);
VALUETYPE_ATTR_GETTER(PRTime, LastModifiedTime);

ISUPPORTS_ATTR(calICalendar, Parent);

VALUETYPE_ATTR(nsACString&, Id);

VALUETYPE_ATTR(nsACString&, Title);
VALUETYPE_ATTR(PRInt16, Priority);
VALUETYPE_ATTR(PRBool, IsPrivate);

VALUETYPE_ATTR(PRInt32, Method);
VALUETYPE_ATTR(PRInt32, Status);

// Alarm attrs

VALUETYPE_ATTR(PRBool, HasAlarm);
ISUPPORTS_ATTR(calIDateTime, AlarmTime);

// Recurrence attrs

VALUETYPE_ATTR(PRInt32, RecurType);
ISUPPORTS_ATTR(calIDateTime, RecurEnd);
ISUPPORTS_ATTR_GETTER(nsISupportsArray, RecurrenceExceptions);

// Attachments

ISUPPORTS_ATTR_GETTER(nsISupportsArray, Attachments);

// Contacts

ISUPPORTS_ATTR_GETTER(nsISupportsArray, Contacts);

// Properties

ISUPPORTS_ATTR_GETTER(nsIWritablePropertyBag, Properties);

/*
 * calIItemBase methods
 */

NS_IMETHODIMP
calItemBase::Clone (calIItemBase **_retval)
{
}

NS_IMETHODIMP
calItemBase::SnoozeAlarm (calIDateTime *aSnoozeFor)
{
}

NS_IMETHODIMP
calItemBase::GetNextOccurrence (calIDateTime *aStartTime)
{
}

NS_IMETHODIMP
calItemBase::GetPreviousOccurrence (calIDateTime *aStartTime)
{
}

NS_IMETHODIMP
calItemBase::GetAllOccurrences (calIDateTime *aStartTime)
{
}


/***
 *** calItemOccurrence impl
 ***/

NS_IMPL_ISUPPORTS1(calItemOccurrence, calIItemOccurrence);

calItemOccurrence::calItemOccurrence()
{
}

calItemOccurrence::calItemOccurrence(calIItemBase *aBaseItem,
                                     calIDateTime *aStart,
                                     calIDateTime *aEnd)
    : mItem(aBaseItem), mOccurrenceStartDate(aStart), mOccurrenceEndDate(aEnd)
{
}

ISUPPORTS_ATTR_GETTER(calIItemBase, Item);
ISUPPORTS_ATTR_GETTER(calIDateTime, OccurrenceStartDate);
ISUPPORTS_ATTR_GETTER(calIDateTime, OccurrenceEndDate);
