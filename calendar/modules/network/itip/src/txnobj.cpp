/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- 
 * 
 * The contents of this file are subject to the Netscape Public License 
 * Version 1.0 (the "NPL"); you may not use this file except in 
 * compliance with the NPL.  You may obtain a copy of the NPL at 
 * http://www.mozilla.org/NPL/ 
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL 
 * for the specific language governing rights and limitations under the 
 * NPL. 
 * 
 * The Initial Developer of this code under the NPL is Netscape 
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights 
 * Reserved. 
 */

// txnobj.cpp
// John Sun
// 11:36 AM March 10, 1998

#include "jdefines.h"
#include "julnstr.h"
#include "txnobj.h"
#include "datetime.h"

#if CAPI_READY
#include <capi.h>
#endif /* #if CAPI_READY */

#include "vevent.h"
#include "jlog.h"
#include "icalsrdr.h"
#include "keyword.h"

//---------------------------------------------------------------------

TransactionObject::TransactionObject()
: m_Calendar(0), m_ICalComponentVctr(0), m_Modifiers(0), m_Recipients(0)
{
}

//---------------------------------------------------------------------

TransactionObject::TransactionObject(NSCalendar & cal,
                                     JulianPtrArray & components,
                                     User & user,
                                     JulianPtrArray & recipients,
                                     UnicodeString & subject,
                                     JulianPtrArray & modifiers,
                                     /*JulianForm * jf, MWContext * context, */
                                     UnicodeString & attendeeName,
                                     EFetchType fetchType)
: m_Modifiers(0), m_Recipients(0)
{
    m_Calendar = &cal;

    // assumption is that components are processed.
    m_ICalComponentVctr = &components;
    m_User = &user;
    m_Recipients = &recipients;
    m_Subject = subject;

    m_Modifiers = &modifiers;

    m_AttendeeName = attendeeName;
    m_FetchType = fetchType;

    // for sending IMIP messages
    /*
    m_JulianForm = jf;
    m_Context = context;
    */
}
//---------------------------------------------------------------------
TransactionObject::~TransactionObject()
{

}
//---------------------------------------------------------------------

TransactionObject::ETxnErrorCode
TransactionObject::executeHelper(ETransportType * transports,
                                 t_int32 transportsSize,
                                 JulianPtrArray * out)
{
    t_int32 i = 0;
    t_bool * status = 0;
    t_int32 statusSize = 0;
    PR_ASSERT(transports != 0);
    if (transports == 0)
    {

        return TR_OK;
    }
    //PR_ASSERT(m_Recipients != 0 && m_Recipients->GetSize() > 0);
    if (m_Recipients == 0 || m_Recipients->GetSize() == 0)
    {
        // TODO: return the correct error
        return TR_OK;
    }
    // clone recipients
    // 
    JulianPtrArray * tempRecipients = 0;
    tempRecipients = new JulianPtrArray();
    if (tempRecipients == 0)
    {
        // TODO: return the correct error
        return TR_OK;
    }
    User::cloneUserVector(m_Recipients, tempRecipients);
    for (i = 0; i < transportsSize; i++)
    {
        statusSize = tempRecipients->GetSize();
        status = new t_bool[statusSize];
        setStatusToFalse(status, statusSize);
        if (status != 0)
        {
            switch (transports[i])
            {
            case ETransportType_CAPI:
#if CAPI_READY
                executeCAPI(out, status, statusSize);
#else
                executeFCAPI(out, status, statusSize);
#endif
                break;
            case ETransportType_IRIP:
                executeIRIP(out, status, statusSize);
                break;
            case ETransportType_IMIP:
                executeIMIP(out, status, statusSize);
                break;
            case ETransportType_FCAPI:
                executeFCAPI(out, status, statusSize);
                break;
            }
            removeSuccessfulUsers(tempRecipients, status, statusSize);
            delete [] status; status = 0;
        }
        else
        {
            // TODO: return an error
            return TR_IMIP_FAILED;
        }
    }
            
    User::deleteUserVector(tempRecipients);            
    delete tempRecipients; tempRecipients = 0;
    return TR_OK;
}
//---------------------------------------------------------------------

void
TransactionObject::execute(JulianPtrArray * out, 
                           ETxnErrorCode & status)
{
#if 0
    PR_ASSERT(m_User != 0 && m_Recipients != 0);
    outStatus = TR_OK;
    
    t_int32 size = 0;
    
    if (m_Recipients != 0)
    {
        size = m_Recipients->GetSize();
    }
    t_bool * recipientStatus = new t_bool[size];
    
#if CAPI_READY
    executeCAPI(out, ouStatus);
#endif
    executeIMIP(out, ouStatus);
    delete [] recipientStatus; recipientStatus = 0;

#else /* #if 0 */

    ETxnErrorCode outStatus = TR_OK;
    ETransportType * transports = 0;
    t_int32 transportSize = 0;    
    PR_ASSERT(m_User != 0 && m_Recipients != 0);

#if CAPI_READY
    transportSize = 3;
#else
    transportSize = 1;
#endif /* #if CAPI_READY */

    transports = new ETransportType[transportSize];
    if (transports != 0)
    {
#if CAPI_READY
        // for now make the order of precedence capi, irip, imip
        transports[0] = ETransportType_CAPI;
        transports[1] = ETransportType_IRIP;
        transports[2] = ETransportType_IMIP;
#else
        transports[0] = ETransportType_IMIP;
#endif /* #if CAPI_READY */
        status = executeHelper(transports, transportSize, out);
        delete [] transports; transports = 0;
    }
#endif /* #if 0 #else */
}

//---------------------------------------------------------------------

void
TransactionObject::removeSuccessfulUsers(JulianPtrArray * recipients,
                                         t_bool * status,
                                         t_int32 statusSize)
{
    t_int32 i;
    User * aUser = 0;
    if (recipients == 0)
        return;
    PR_ASSERT(statusSize == recipients->GetSize());
    for (i = statusSize - 1; i >= 0; i--)
    {
        if (status[i])
        {
            aUser = (User *) recipients->GetAt(i);
            delete aUser;
            recipients->RemoveAt(i);
        }
    }
}
//---------------------------------------------------------------------
void
TransactionObject::setStatusToFalse(t_bool * status,
                                    t_int32 statusSize)
{
    t_int32 i;
    for (i = 0; i < statusSize; i++)
        status[i] = FALSE;
}
//---------------------------------------------------------------------
UnicodeString & 
TransactionObject::createContentTypeHeader(UnicodeString & sMethod,
                                           UnicodeString & sCharset, 
                                           UnicodeString & sComponentType,
                                           UnicodeString & sContentTypeHeader)
{
    sContentTypeHeader = "Content-Type: text/calendar; method=";
    sContentTypeHeader += sMethod;
    sContentTypeHeader += "; charset=";
    sContentTypeHeader += sCharset;
    sContentTypeHeader += "; component=";
    sContentTypeHeader += sComponentType;

    return sContentTypeHeader;
}

//---------------------------------------------------------------------


TransactionObject::ETxnErrorCode
TransactionObject::executeIRIP(JulianPtrArray * out, 
                               t_bool * outStatus, 
                               t_int32 outStatusSize)

{
    //PR_ASSERT(FALSE);
    ETxnErrorCode status;
    // NOTE: remove later to avoid compiler warnings
    if (out != 0 && outStatus != 0);
    /* NOT YET DEFINED */
    return status;
}

//---------------------------------------------------------------------

TransactionObject::ETxnErrorCode
TransactionObject::executeFCAPI(JulianPtrArray * out, 
                                t_bool * outStatus, 
                                t_int32 outStatusSize)
{
    //PR_ASSERT(FALSE);
    ETxnErrorCode status;
    // NOTE: remove later to avoid compiler warnings
    if (out != 0 && outStatus != 0);
    /* NOT YET DEFINED */
    return status;
}

//---------------------------------------------------------------------

TransactionObject::ETxnErrorCode 
TransactionObject::executeIMIP(JulianPtrArray * out, 
                               t_bool * outStatus, 
                               t_int32 outStatusSize)    
{
    ETxnErrorCode status = TR_OK;
    UnicodeString itipMessage;
    UnicodeString sContentTypeHeader;
    // prints FROM: User and TO: users
    UnicodeString sCharSet = "UTF-8";
    UnicodeString sMethod;
    UnicodeString sComponentType;

    t_int32 i;
    //User * u;
    PR_ASSERT(m_User != 0 && m_Recipients != 0);
    if (m_User == 0 || m_Recipients == 0)
    {
        return TR_IMIP_FAILED;
    }
    
    // Create the body of the message
    itipMessage = MakeITIPMessage(itipMessage);

    // Create the content header    
    PR_ASSERT(m_Calendar != 0);
    if (m_Calendar != 0)
    {
        sMethod = NSCalendar::methodToString(m_Calendar->getMethod(), sMethod);
    }
    PR_ASSERT(m_ICalComponentVctr != 0 && m_ICalComponentVctr->GetSize() > 0);
    if (m_ICalComponentVctr != 0 && m_ICalComponentVctr->GetSize() > 0)
    {
        // Use first component in m_ICalComponentVctr to get type
        sComponentType = 
            ICalComponent::componentToString(((ICalComponent *)m_ICalComponentVctr->GetAt(0))->GetType());
    }
    createContentTypeHeader(sMethod, sCharSet, sComponentType, sContentTypeHeader);

    // Use preference "calendar.imip.add_content_disp" to decide whether to add 
    // "Content-Disposition: ... "
    // Default doesn't write it.
    XP_Bool do_add_content_disp = FALSE;

    if ((m_JulianForm) && (m_JulianForm->getCallbacks()) && (m_JulianForm->getCallbacks()->BoolPref))
		  (*m_JulianForm->getCallbacks()->BoolPref)("calendar.imip.add_content_disp", &do_add_content_disp);
  
    if (do_add_content_disp)
    {
      if (((ICalComponent *)m_ICalComponentVctr->GetAt(0))->GetType() == ICalComponent::ICAL_COMPONENT_VEVENT)
      {
        sContentTypeHeader += "\r\nContent-Dispostion: inline; filename=\"event.ics\"";
      }
      else if (((ICalComponent *)m_ICalComponentVctr->GetAt(0))->GetType() == ICalComponent::ICAL_COMPONENT_VFREEBUSY)
      { 
        sContentTypeHeader += "\r\nContent-Dispostion: inline; filename=\"freebusy.ifb\"";
      }
    }
    
//#ifdef DEBUG_ITIP
    m_DebugITIPMessage = itipMessage;
//#endif /* DEBUG_ITIP */
    
    // TODO: send it via mail API here
    if (/*m_JulianForm != 0 && m_Context != 0 &&*/ m_Recipients != 0)
    {
        int iOut;
        User * userTo;
        UnicodeString uTo;
        UnicodeString u;
        char * to = 0;
        char * from = m_User->getIMIPAddress().toCString("");
        char * subject = m_Subject.toCString("");
        char * body = itipMessage.toCString("");
        char * otherheaders = sContentTypeHeader.toCString("");
        PR_ASSERT(from != 0 && subject != 0 && body != 0 && otherheaders != 0);

        // append the recipients IMIP address to the to string.
        // also, if the imip address is null (in this case empty string)
        // set outStatus[i] to FALSE.
        for (i = 0; i < m_Recipients->GetSize(); i++)
        {
            userTo = (User *) m_Recipients->GetAt(i);
            
            if (userTo->getIMIPAddress().size() > 0)
            {
                outStatus[i] = TRUE;
                uTo += userTo->getIMIPAddress();
                if (i < m_Recipients->GetSize() - 1)
                    uTo += ",";
            }
            else
                outStatus[i] = FALSE;
    
        }
        if (uTo.size() > 0)
            to = uTo.toCString("");
        if (to != 0)
        {
          /*
            if (m_JulianForm->getCallbacks()->SendMessageUnattended)
                iOut = (*m_JulianForm->getCallbacks()->SendMessageUnattended)(m_Context, to, subject, otherheaders, body);
            else
                iOut = 0;
                */
            delete [] to;
        }        
        if (from != 0)
        {
            delete [] from; from = 0;
        }
        if (subject != 0)
        {
            delete [] subject; subject = 0;
        }
        if (body != 0)
        {
            delete [] body; body = 0;
        }
        if (otherheaders != 0)
        {
            delete [] otherheaders; otherheaders = 0;
        }

    }
    return status;
}

//---------------------------------------------------------------------

UnicodeString &
TransactionObject::MakeITIPMessage(UnicodeString & out)
{
    // first print header, then print components
    UnicodeString s, sName, sMethod;
    t_bool isRecurring = FALSE;

    PR_ASSERT(m_Calendar != 0);
    if (m_Calendar != 0)
    {
        out = "BEGIN:VCALENDAR\r\n";
        out += m_Calendar->createCalendarHeader(s);
        sMethod = NSCalendar::methodToString(m_Calendar->getMethod(), sMethod);

        sName = m_AttendeeName;

        out += printComponents(m_ICalComponentVctr, sMethod, 
            sName, isRecurring, s);
        out += "END:VCALENDAR\r\n";
    }
    return out;
}
//---------------------------------------------------------------------

UnicodeString &
TransactionObject::printComponents(JulianPtrArray * components,
                                   UnicodeString & sMethod,
                                   UnicodeString & sName,
                                   t_bool isRecurring, 
                                   UnicodeString & out)
{
    out = "";
    PR_ASSERT(components != 0 && components->GetSize() > 0);
    if (components != 0 && components->GetSize() > 0)
    {
        t_int32 i;
        ICalComponent * ic;
        DateTime dtStamp; 

        for (i = 0; i < components->GetSize(); i++)
        {
            ic = (ICalComponent *) components->GetAt(i);
            out += ic->toICALString(sMethod, sName, isRecurring);
        }
    }
    return out;
}

//---------------------------------------------------------------------
#if CAPI_READY
//---------------------------------------------------------------------
TransactionObject::ETxnErrorCode 
TransactionObject::FetchEventsByRange(User * loggedInUser,              
                                      JulianPtrArray * usersToFetchOn,  
                                      DateTime dtStart,                 
                                      DateTime dtEnd,                   
                                      JulianPtrArray * eventsToFillIn)
{
    TransactionObject::ETxnErrorCode status = TR_OK;
    if (loggedInUser == 0 || usersToFetchOn == 0)
        return TR_CAPI_FETCH_BAD_ARGS;
    if (!dtStart.isValid() || !dtEnd.isValid() || dtEnd.beforeDateTime(dtStart))
        return TR_CAPI_FETCH_BAD_ARGS;
    if (eventsToFillIn == 0)
        return TR_CAPI_FETCH_BAD_ARGS;
    
    if (loggedInUser->getCAPISession() == 0)
        return TR_CAPI_FETCH_BAD_ARGS;

    return status;
}

//---------------------------------------------------------------------

#if 0
void 
TransactionObject::setCAPIInfo(char * userxstring,
                               char * password,
                               char * hostname,
                               char * node,
                               char * recipientxstring)
{
    m_userxstring = userxstring;
    m_password = password;
    m_hostname = hostname;
    m_node = node;
    m_recipientxstring = recipientxstring;
}
#endif
//---------------------------------------------------------------------

//void
//TransactionObject::executeCAPI(JulianPtrArray * outCalendars, 
//                               ETxnErrorCode & status)
TransactionObject::ETxnErrorCode
TransactionObject::executeCAPI(JulianPtrArray * outCalendars, 
                               t_bool * outStatus, t_int32 outStatusSize)
{
    CAPISession pS = NULL;
    CAPIHandle pH;
    char ** strings;
    char * eventFetchedByUID;
    t_int32 numstrings;
    t_int32 i;

    ETxnErrorCode status = TR_OK;

    // Do the following:
    // If GetType() is a store or a delete, always try to add m_User to handles.
    // Try logging in with m_User information

    // Don't need recipients.
    if (m_User != 0)
    //if (m_User != 0 && m_Recipients != 0 && m_Recipients->GetSize() > 0)
    {

        User * aUser;
        char * xstring = 0;
        // Execute specific CAPI call (fetch, store, delete)
        EFetchType outType;
        char sDebugOut[100];
        t_int32 handleCount = 0;
        t_int32 recipientCount = 0;
        t_int32 extraUserHandle = 0;
        CAPIStatus cStat = CAPI_ERR_OK;
        

        // On a store or delete set handleCount to 1 (always try to use m_User)
        if (GetType() == ETxnType_STORE ||
            GetType() == ETxnType_DELETE)
        {
            handleCount = 1;
            extraUserHandle = 1;
        }

        // If recipients, add size of recipients to handleCount
        if (m_Recipients != 0)
        {
            handleCount += m_Recipients->GetSize();
            recipientCount = m_Recipients->GetSize();
        }

        //PR_ASSERT(outCalendars != 0);       

        // TODO: horrible hack, restricting number of handles to 20
        CAPIHandle handles[20]; // wish I could do handles[handleCount]
        if (handleCount > 20) 
            handleCount = 20;

        for (i = 0; i < handleCount; i++)
        {
            handles[i] = 0;
        }

        // Capabilities       
        //if (FALSE) TRACE("CAPICapabilities:%s\r\n", CAPI_Capabilities((long) 0));
        //sprintf(sDebugOut, "CAPICapabilities:%s\r\n", CAPI_Capabilities((long) 0));
        
        // Logon

        // TODO: eventually use the CAPAddress
        //char * userxstring = m_User->getXString().toCString("");        
        char * userlogonstring = m_User->getLogonString().toCString("");        
        char * password = m_User->getPassword().toCString("");
        char * hostname = m_User->getHostname().toCString("");
        char * node = m_User->getNode().toCString("");
        PR_ASSERT(userlogonstring != 0 && password != 0 && hostname != 0 && node != 0);

#ifdef TESTING_ITIPRIG
        if (TRUE) TRACE("logon user: %s,\r\n password: %s,\r\n hostname: %s,\r\n node: %s,\r\n", 
            userlogonstring, password, hostname, node);
#endif        
        
        cStat = CAPI_Logon(userlogonstring, password, hostname, 0, &pS);
        //PR_ASSERT(FALSE);
        
        //if (FALSE) TRACE("CAPI_Logon:%lx\r\n", foo);
        //sprintf(sDebugOut, "CAPI_Logon:%lx\r\n", foo);

        if (cStat != CAPI_ERR_OK && cStat != CAPI_ERR_EXPIRED)
        {
            status = TR_CAPI_LOGIN_FAILED;
        }
        else
        {

            // On Store or Delete, Handle 0 will always be current user.
            // Get Handle for each recipient and user, first m_User in handle[0]
            
            if (GetType() == ETxnType_STORE ||
                GetType() == ETxnType_DELETE)
            {
                xstring = m_User->getXString().toCString("");
                PR_ASSERT(xstring != 0);
                CAPI_GetHandle(pS, xstring, 0, &handles[0]);
                delete [] xstring; xstring = 0;
            }

            if (m_Recipients != 0 && m_Recipients->GetSize() > 0)
            {
                for (i = 0; i < recipientCount; i++)
                {
                    aUser = (User *) m_Recipients->GetAt(i);
                    xstring = aUser->getXString().toCString("");
                    PR_ASSERT(xstring != 0);
                    CAPI_GetHandle(pS, xstring, 0, &handles[i + extraUserHandle]);
                    delete [] xstring; xstring = 0;
                }
            }
            //if (FALSE) TRACE("CAPI_GetHandle:%lx\r\n", foo);
            //sprintf(sDebugOut, "CAPI_GetHandle:%lx\r\n", foo);

            // HandleCAPI call
            // OLD WAY
            //foo = handleCAPI(pS, pH, m_Modifiers, &strings, 
            //    &eventFetchedByUID, numstrings, outType);
            //
           cStat = handleCAPI(pS, &handles[0], handleCount, 0, m_ICalComponentVctr, m_Calendar,
                m_Modifiers, outCalendars, outType);

            if (cStat != CAPI_ERR_OK)
            {
                if (status == TR_OK)
                {
                    status = TR_CAPI_HANDLE_CAPI_FAILED;
                }
            }

            // Logoff
            cStat = CAPI_Logoff(&pS, (long) 0);
            if (cStat != CAPI_ERR_OK)
            {
                if (status == TR_OK)
                {
                    status = TR_CAPI_LOGOFF_FAILED;
                }
            }
            //if (FALSE) TRACE("CAPI_GetLogoff:%lx\r\n", foo);
            //sprintf(sDebugOut, "CAPI_GetLogoff:%lx\r\n", status);

            // Destroy Handle
            //foo = CAPI_DestroyHandle(&pH);
            CAPI_DestroyHandles(pS, &handles[0], handleCount, 0);

            if (userlogonstring != 0)
            {
                delete [] userlogonstring; userlogonstring = 0;
            }
            if (password != 0)
            {
                delete [] password; password = 0;
            }
            if (hostname != 0)
            {
                delete [] hostname; hostname = 0;
            }
            if (node != 0)
            {
                delete [] node; node = 0;
            }            
        }
    }
    return status;
}
#endif /* #if CAPI_READY */
//---------------------------------------------------------------------
