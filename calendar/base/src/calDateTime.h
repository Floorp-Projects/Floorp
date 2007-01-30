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

#ifndef CALDATETIME_H_
#define CALDATETIME_H_

#include "nsString.h"

#include "jsapi.h"
#include "nsIXPCScriptable.h"

#include "calIDateTime.h"

struct icaltimetype;
struct _icaltimezone;

/* This class subclasses nsCString so we can check tzids for "freshness". */
class calTzId : public nsCString
{
public:
    /* Used by AssignLiteral when we set the tzid to "utc" or "floating". */
    void Assign(char* c);
    /* Used by mTimezone.Assign as our hook to check the tzid's "freshness". */
    void Assign(const nsACString_internal& aStr);
};

class calDateTime : public calIDateTime,
                    public nsIXPCScriptable
{
public:
    calDateTime ();
    calDateTime (struct icaltimetype *timeptr);
    calDateTime (const calDateTime& cdt);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // calIDateTime interface
    NS_DECL_CALIDATETIME

    // nsIXPCScriptable interface
    NS_DECL_NSIXPCSCRIPTABLE
protected:
    PRBool mImmutable;

    PRBool mIsValid;

    PRTime mNativeTime;

    PRInt16 mYear;
    PRInt16 mMonth;
    PRInt16 mDay;
    PRInt16 mHour;
    PRInt16 mMinute;
    PRInt16 mSecond;

    PRBool mIsDate;
    calTzId mTimezone;

    PRInt16 mWeekday;
    PRInt16 mYearday;

    void FromIcalTime(icaltimetype *icalt);
    nsresult GetIcalTZ(const nsACString& tzid, struct _icaltimezone **tzp);

    PRTime IcaltimeToPRTime(const icaltimetype *const icalt, struct _icaltimezone *const tz);
    void PRTimeToIcaltime(const PRTime time, const PRBool isdate, struct _icaltimezone *const tz, icaltimetype *icalt);

    PRTime mLastModified;
};

#endif /* CALDATETIME_H_ */
