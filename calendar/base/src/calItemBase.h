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

#ifndef CALITEMBASE_H_
#define CALITEMBASE_H_

#include "nsString.h"

#include "nsCOMPtr.h"
#include "nsIPropertyBag.h"

#include "nsIArray.h"

#include "calIDateTime.h"
#include "calICalendar.h"
#include "calIItemBase.h"

class calItemBase : public calIItemBase, public nsIWritablePropertyBag
{
public:
    calItemBase();

    // nsISupports
    NS_DECL_ISUPPORTS

    // calIItemBase
    NS_DECL_CALIITEMBASE

    // nsIWritablePropertyBag interface
    NS_DECL_NSIPROPERTYBAG
    NS_DECL_NSIWRITABLEPROPERTYBAG

protected:
    nsCOMPtr<calICalendar> mParent;

    nsCOMPtr<calIDateTime> mCreationDate;
    PRTime mLastModifiedTime;

    nsCString mId;
    nsCString mTitle;

    PRInt16 mPriority;
    PRBool mIsPrivate;

    PRInt32 mMethod;
    PRInt32 mStatus;

    // alarm
    PRBool mHasAlarm;
    nsCOMPtr<calIDateTime> mAlarmTime;

    // recurrence
    PRInt32 mRecurType;
    nsCOMPtr<calIDateTime> mRecurEnd;
    //nsCOMArray<calIDateTime> mRecurrenceExceptions;
    nsCOMPtr<nsIMutableArray> mRecurrenceExceptions;

    // attachments
    //nsCOMArray<nsIMsgAttachment> mAttachments;
    nsCOMPtr<nsIMutableArray> mAttachments;

    // contacts
    //nsCOMArray<nsIAbCard> mContacts;
    nsCOMPtr<nsIMutableArray> mContacts;

    // properties
    nsCOMPtr<nsIWritablePropertyBag> mProperties;
};


class calItemOccurrence : public calIItemOccurrence
{
public:
    calItemOccurrence();
    calItemOccurrence(calIItemBase *aBaseItem,
                      calIDateTime *aStart,
                      calIDateTime *aEnd);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // calIItemOccurrence interface
    NS_DECL_CALIITEMOCCURRENCE

protected:
    nsCOMPtr<calIItemBase> mItem;
    nsCOMPtr<calIDateTime> mOccurrenceStartDate;
    nsCOMPtr<calIDateTime> mOccurrenceEndDate;
};

#endif /* CALITEMBASE_H_ */
