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

#include "stdafx.h"
#include "fehist.h"
#include "mhst_ent.h"
CHistory::CHistory(HISTORY *pUseThis, BOOL bClone)
{
    m_pHistory = NULL;

    if(pUseThis)    {
        if(!bClone) {
            m_pHistory = pUseThis;
        }
        else    {
            m_pHistory = Clone(pUseThis);
        }
    }
    else    {
        JMCException *pEX = NULL;
        m_pHistory = HISTORYFactory_Create(&pEX);
        if(pEX) {
            CJMCException e(pEX);
            e.ReportError();

            m_pHistory = NULL;
        }
    }
}

CHistory::~CHistory()
{
    JMCException *pEX = NULL;
    if(m_pHistory)  {
        HISTORY_release(m_pHistory, &pEX);
        if(pEX) {
            CJMCException e(pEX);
            e.ReportError();
        }
        m_pHistory = NULL;
    }
}

void CHistory::SetHISTORY(HISTORY *pUsurp, BOOL bClone)
{
    JMCException *pEX = NULL;
    if(m_pHistory)  {
        HISTORY_release(m_pHistory, &pEX);
        if(pEX) {
            CJMCException e(pEX);
            e.ReportError();
        }
        m_pHistory = NULL;
    }

    if(!bClone) {
        m_pHistory = pUsurp;
    }
    else    {
        m_pHistory = Clone(pUsurp);
    }
}

HISTORY *CHistory::GetHISTORY()
{
    return(m_pHistory);
}

HISTORY *CHistory::Clone(HISTORY *pTwin)
{
    JMCException *j = NULL;
    HISTORY *pRetval = NULL;

    if(pTwin)  {
        pRetval = (HISTORY *)HISTORY_clone(pTwin, &j);
        if(j)   {
            CJMCException e(j);
            e.ReportError();
            pRetval = NULL;
        }
    }

    return(pRetval);
}

HST_ENT *CHistory::Clone(HST_ENT *pTwin)
{
    JMCException *j = NULL;
    HST_ENT *pRetval = NULL;

    if(pTwin)  {
        pRetval = (HST_ENT *)HST_ENT_clone(pTwin, &j);
        if(j)   {
            CJMCException e(j);
            e.ReportError();
            pRetval = NULL;
        }
    }

    return(pRetval);
}

BOOL CHistory::Compare(HISTORY *p1, HISTORY *p2)
{
    BOOL bRetval = FALSE;
    JMCException *j = NULL;

    if(p1 && p2)    {
        bRetval = HISTORY_equals(p1, (void *)p2, &j);
        if(j)   {
            CJMCException e(j);
            e.ReportError();

            bRetval = FALSE;
        }
    }
    else if(!p1 && !p2) {
        //  NULLs are true.
        bRetval = TRUE;
    }

    return(bRetval);
}

BOOL CHistory::Compare(HST_ENT *p1, HST_ENT *p2)
{
    BOOL bRetval = FALSE;
    JMCException *j = NULL;

    if(p1 && p2)    {
        bRetval = HST_ENT_equals(p1, (void *)p2, &j);
        if(j)   {
            CJMCException e(j);
            e.ReportError();

            bRetval = FALSE;
        }
    }
    else if(!p1 && !p2) {
        //  NULLs are true.
        bRetval = TRUE;
    }

    return(bRetval);
}


void CHistory::Add(HST_ENT *pAdd)
{
    if(m_pHistory && pAdd)  {
        HISTORY_addHistoryEntry(m_pHistory, pAdd, TRUE);
    }
}

HST_ENT *CHistory::Add(URL *pUrl)
{
    HST_ENT *pRetval = NULL;
    JMCException *j;

    if(pUrl)    {
        pRetval = HST_ENTFactory_Create(&j, pUrl, NULL);
        if(j)   {
            CJMCException e(j);
            e.ReportError();

            pRetval = NULL;
        }
    }

    return(pRetval);
}

void CHistory::SetCurrent(HST_ENT *pCurrent)
{
    if(m_pHistory && pCurrent)  {
        HISTORY_setCurrentHistoryEntry(m_pHistory, pCurrent);
    }
}

void CHistory::SetCurrentIndex(int iIndex)
{
    SetCurrent(GetIndex(iIndex));
}

HST_ENT *CHistory::GetCurrent()
{
    HST_ENT *pRetval = NULL;

    if(m_pHistory)    {
        pRetval = HISTORY_getCurrentHistoryEntry(m_pHistory);
    }

    return(pRetval);
}

BOOL CHistory::Remove(HST_ENT *pRemove)
{
    BOOL bRetval = FALSE;

    if(m_pHistory)  {
        bRetval = HISTORY_removeHistoryEntry(m_pHistory, pRemove);
    }

    return(bRetval);
}

HST_ENT *CHistory::GetNext()
{
    HST_ENT *pRetval = NULL;

    if(m_pHistory)    {
        pRetval = HISTORY_getNextHistoryEntry(m_pHistory, GetCurrent());
    }

    return(pRetval);
}

HST_ENT *CHistory::GetPrev()
{
    HST_ENT *pRetval = NULL;

    if(m_pHistory)    {
        pRetval = HISTORY_getPreviousHistoryEntry(m_pHistory, GetCurrent());
    }

    return(pRetval);
}

HST_ENT *CHistory::GetIndex(int iIndex)
{
    HST_ENT *pRetval = NULL;

    if(m_pHistory)    {
        pRetval = HISTORY_getIndexedHistoryEntry(m_pHistory, iIndex);
    }

    return(pRetval);
}

int CHistory::GetIndex(HST_ENT *pEntry)
{
    int iRetval = 0;

    if(m_pHistory && pEntry)    {
        iRetval = HISTORY_getHistoryEntryIndex(m_pHistory, pEntry);
    }

    return(iRetval);
}

void CHistory::SetUrl(HST_ENT *pHist, URL *pUrl)
{
    if(pHist && pUrl)   {
        HST_ENT_setURL(pHist, pUrl);
    }
}

URL *CHistory::GetUrl(HST_ENT *pHist)
{
    URL *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getURL(pHist);
    }

    return(pRetval);
}

int CHistory::SetTitle(HST_ENT *pHist, const char *pTitle)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL title....
    if(pHist) {
        iRetval = HST_ENT_setTitle(pHist, pTitle);
    }

    return(iRetval);
}

const char *CHistory::GetTitle(HST_ENT *pHist)
{
    const char *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getTitle(pHist);
    }

    return(pRetval);
}

int CHistory::SetFormData(HST_ENT *pHist, void *pData)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL data....
    if(pHist)  {
        iRetval = HST_ENT_setFormData(pHist, pData);
    }

    return(iRetval);
}

void *CHistory::GetFormData(HST_ENT *pHist)
{
    void *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getFormData(pHist);
    }

    return(pRetval);
}

int CHistory::SetEmbedData(HST_ENT *pHist, void *pData)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL data....
    if(pHist)  {
        iRetval = HST_ENT_setEmbedData(pHist, pData);
    }

    return(iRetval);
}

void *CHistory::GetEmbedData(HST_ENT *pHist)
{
    void *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getEmbedData(pHist);
    }

    return(pRetval);
}

int CHistory::SetGridData(HST_ENT *pHist, void *pData)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL data....
    if(pHist)  {
        iRetval = HST_ENT_setGridData(pHist, pData);
    }

    return(iRetval);
}

void *CHistory::GetGridData(HST_ENT *pHist)
{
    void *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getGridData(pHist);
    }

    return(pRetval);
}

int CHistory::SetWindowData(HST_ENT *pHist, void *pData)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL data....
    if(pHist)  {
        iRetval = HST_ENT_setWindowData(pHist, pData);
    }

    return(iRetval);
}

void *CHistory::GetWindowData(HST_ENT *pHist)
{
    void *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getWindowData(pHist);
    }

    return(pRetval);
}

int CHistory::SetAppletData(HST_ENT *pHist, void *pData)
{
    int iRetval = 0;    //  What's the default?

    //  Allow NULL data....
    if(pHist)  {
        iRetval = HST_ENT_setAppletData(pHist, pData);
    }

    return(iRetval);
}

void *CHistory::GetAppletData(HST_ENT *pHist)
{
    void *pRetval = NULL;

    if(pHist)   {
        pRetval = HST_ENT_getAppletData(pHist);
    }

    return(pRetval);
}

void CHistory::SetPosition(HST_ENT *pHist, long lEleID)
{
    if(pHist)   {
        HST_ENT_setDocumentPosition(pHist, lEleID);
    }
}

long CHistory::GetPosition(HST_ENT *pHist)
{
    long lRetval = 0;   //  What's a good default?

    if(pHist)   {
        lRetval = HST_ENT_getDocumentPosition(pHist);
    }

    return(lRetval);
}

HST_ENT *CHistory::GetNext(HST_ENT *pEntry)
{
    HST_ENT *pRetval = NULL;
    if(pEntry)  {
        pRetval = HST_ENT_getNext(pEntry);
    }

    return(pRetval);
}

void CHistory::SetNext(HST_ENT *pThis, HST_ENT *pNext)
{
    //  Allow NULL next....
    if(pThis)   {
        HST_ENT_setNext(pThis, pNext);
    }
}
