/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape 
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* 
 * txnobjfy.cpp
 * John Sun
 * 4/13/98 11:23:24 AM
 */

#include "jdefines.h"
#include "julnstr.h"
#include "txnobjfy.h"
#include "sttxnobj.h"
#include "getxnobj.h"
#include "detxnobj.h"
#include "nscal.h"

//---------------------------------------------------------------------

TransactionObjectFactory::TransactionObjectFactory()
{
    PR_ASSERT(FALSE);
}

//---------------------------------------------------------------------

TransactionObject *
TransactionObjectFactory::Make(NSCalendar & cal, 
                               JulianPtrArray & components, 
                               User & user, 
                               JulianPtrArray & recipients,
                               UnicodeString & subject, 
                               JulianPtrArray & modifiers, 
                               JulianForm * jf,
                               MWContext * context,
                               UnicodeString & attendeeName,
                               TransactionObject::EFetchType fetchType)
{
    TransactionObject * outTxnObj = 0;

    NSCalendar::METHOD method = cal.getMethod(); 

    // For fetch type not set to NONE, make GetTransactionType
    // ignore method arg
    if (fetchType != TransactionObject::EFetchType_NONE)
    {
        outTxnObj = new GetTransactionObject(cal, components, user,
            recipients, subject, modifiers, jf, context, attendeeName, fetchType);
    }
    else
    {
        switch (method)
        {
        case NSCalendar::METHOD_PUBLISH:
            // store
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_REQUEST:
            // store
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_REPLY:
            // store
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_CANCEL:
            // delete
            outTxnObj = new DeleteTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_REFRESH:
            // ??? store ???
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_COUNTER:
            // no CAPI, use IRIP, IMIP
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_DECLINECOUNTER:
            // no CAPI, use IRIP, IMIP
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        case NSCalendar::METHOD_ADD:
            // store
            outTxnObj = new StoreTransactionObject(cal, components, user,
                recipients, subject, modifiers, jf, context, attendeeName);
            break;
        default:
            break;
        }
    }

    return outTxnObj;
}
