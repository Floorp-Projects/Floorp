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

#include "calAttributeHelpers.h"

NS_IMPL_ISUPPORTS1(calItemBase, calIItemBase)

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

/*
 * calIItemBase attributes
 */


// General attrs

CAL_ISUPPORTS_ATTR(calItemBase, calIDateTime, CreationDate)
CAL_VALUETYPE_ATTR_GETTER(calItemBase, PRTime, LastModifiedTime)

CAL_ISUPPORTS_ATTR(calItemBase, calICalendar, Parent)

CAL_STRINGTYPE_ATTR(calItemBase, nsACString, Id)

CAL_STRINGTYPE_ATTR(calItemBase, nsACString, Title)
CAL_VALUETYPE_ATTR(calItemBase, PRInt16, Priority)
CAL_VALUETYPE_ATTR(calItemBase, PRBool, IsPrivate)

CAL_VALUETYPE_ATTR(calItemBase, PRInt32, Method)
CAL_VALUETYPE_ATTR(calItemBase, PRInt32, Status)

// Alarm attrs

CAL_VALUETYPE_ATTR(calItemBase, PRBool, HasAlarm)
CAL_ISUPPORTS_ATTR(calItemBase, calIDateTime, AlarmTime)

// Recurrence attrs

CAL_VALUETYPE_ATTR(calItemBase, PRInt32, RecurType)
CAL_ISUPPORTS_ATTR(calItemBase, calIDateTime, RecurEnd)
CAL_ISUPPORTS_ATTR_GETTER(calItemBase, nsIMutableArray, RecurrenceExceptions)

// Attachments

CAL_ISUPPORTS_ATTR_GETTER(calItemBase, nsIMutableArray, Attachments)

// Contacts

CAL_ISUPPORTS_ATTR_GETTER(calItemBase, nsIMutableArray, Contacts)

// Properties

CAL_ISUPPORTS_ATTR_GETTER(calItemBase, nsIWritablePropertyBag, Properties)

/*
 * calIItemBase methods
 */

NS_IMETHODIMP
calItemBase::Clone (calIItemBase **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
calItemBase::SnoozeAlarm (calIDateTime *aSnoozeFor)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
calItemBase::GetNextOccurrence (calIDateTime *aStartTime, calIItemOccurrence **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
calItemBase::GetPreviousOccurrence (calIDateTime *aStartTime, calIItemOccurrence **_retval)
{
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
calItemBase::GetAllOccurrences (calIDateTime *aStartTime, calIDateTime *aEndTime, nsIMutableArray **_retval)
{
    return NS_ERROR_FAILURE;
}


/***
 *** calItemOccurrence impl
 ***/

NS_IMPL_ISUPPORTS1(calItemOccurrence, calIItemOccurrence)

calItemOccurrence::calItemOccurrence()
{
}

calItemOccurrence::calItemOccurrence(calIItemBase *aBaseItem,
                                     calIDateTime *aStart,
                                     calIDateTime *aEnd)
    : mItem(aBaseItem), mOccurrenceStartDate(aStart), mOccurrenceEndDate(aEnd)
{
}

CAL_ISUPPORTS_ATTR_GETTER(calItemOccurrence, calIItemBase, Item)
CAL_ISUPPORTS_ATTR_GETTER(calItemOccurrence, calIDateTime, OccurrenceStartDate)
CAL_ISUPPORTS_ATTR_GETTER(calItemOccurrence, calIDateTime, OccurrenceEndDate)
