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

#ifndef __FEHistory_H
#define __FEHistory_H

//  Self contained history operations.
class CHistory    {
private:
    HISTORY *m_pHistory;
public:
    CHistory(HISTORY *pUseThis = NULL, BOOL bClone = FALSE);
    ~CHistory();
    void SetHISTORY(HISTORY *pUsurp, BOOL bClone = FALSE);

public:
    HISTORY *GetHISTORY();

    //  Duping.
public:
    static HISTORY *Clone(HISTORY *pTwin);
    static HST_ENT *Clone(HST_ENT *pTwin);

    //  Comparison.
public:
    static BOOL Compare(HISTORY *p1, HISTORY *p2);
    static BOOL Compare(HST_ENT *p1, HST_ENT *p2);
    
    //  Management
public:
    void Add(HST_ENT *pAdd);
    BOOL Remove(HST_ENT *pRemove);
    HST_ENT *Add(URL *pUrl);
    void AddCurrent(URL *pUrl)  {
        SetCurrent(Add(pUrl));
    }

    //  State
public:
    void SetCurrent(HST_ENT *pCurrent);
    void SetCurrentIndex(int iIndex);

    HST_ENT *GetCurrent();
    HST_ENT *GetNext();
    HST_ENT *GetPrev();
    HST_ENT *GetIndex(int iIndex);

    //  Command UI helpers.
public:
    BOOL CanGoBack()    {
        if(GetPrev())   {
            return(TRUE);
        }
        return(FALSE);
    }
    BOOL CanGoForward()    {
        if(GetNext())   {
            return(TRUE);
        }
        return(FALSE);
    }

    //  Indexing
public:
    int GetIndex(HST_ENT *pEntry);
    int GetCurrentIndex()   {
        return(GetIndex(GetCurrent()));
    }
    int GetNextIndex()   {
        return(GetIndex(GetNext()));
    }
    int GetPrevIndex()   {
        return(GetIndex(GetPrev()));
    }

    //  URLs.
public:
    static void SetUrl(HST_ENT *pHist, URL *pUrl);
    static URL *GetUrl(HST_ENT *pHist);
    URL *GetCurrentUrl()    {
        return(GetUrl(GetCurrent()));
    }
    URL *GetNextUrl()    {
        return(GetUrl(GetNext()));
    }
    URL *GetPrevUrl()    {
        return(GetUrl(GetPrev()));
    }
    URL *GetIndexUrl(int iIndex)    {
        return(GetUrl(GetIndex(iIndex)));
    }

    //  Title.
public:
    static int SetTitle(HST_ENT *pHist, const char *pTitle);
    int SetCurrentTitle(const char *pTitle)   {
        return(SetTitle(GetCurrent(), pTitle));
    }
    int SetNextTitle(const char *pTitle)   {
        return(SetTitle(GetNext(), pTitle));
    }
    int SetPrevTitle(const char *pTitle)   {
        return(SetTitle(GetPrev(), pTitle));
    }
    int SetIndexTitle(const char *pTitle, int iIndex)   {
        return(SetTitle(GetIndex(iIndex), pTitle));
    }
    static const char *GetTitle(HST_ENT *pHist);
    const char *GetCurrentTitle()   {
        return(GetTitle(GetCurrent()));
    }
    const char *GetNextTitle()   {
        return(GetTitle(GetNext()));
    }
    const char *GetPrevTitle()   {
        return(GetTitle(GetPrev()));
    }
    const char *GetIndexTitle(int iIndex)   {
        return(GetTitle(GetIndex(iIndex)));
    }

    //  Form data.
public:
    static int SetFormData(HST_ENT *pHist, void *pData);
    static void *GetFormData(HST_ENT *pHist);
    void *GetCurrentFormData()  {
        return(GetFormData(GetCurrent()));
    }
    void *GetNextFormData()  {
        return(GetFormData(GetNext()));
    }
    void *GetPrevFormData()  {
        return(GetFormData(GetPrev()));
    }
    void *GetIndexFormData(int iIndex)  {
        return(GetFormData(GetIndex(iIndex)));
    }

    //  Embed data.
public:
    static int SetEmbedData(HST_ENT *pHist, void *pData);
    static void *GetEmbedData(HST_ENT *pHist);
    void *GetCurrentEmbedData()  {
        return(GetEmbedData(GetCurrent()));
    }
    void *GetNextEmbedData()  {
        return(GetEmbedData(GetNext()));
    }
    void *GetPrevEmbedData()  {
        return(GetEmbedData(GetPrev()));
    }
    void *GetIndexEmbedData(int iIndex)  {
        return(GetEmbedData(GetIndex(iIndex)));
    }

    //  Grid data.
public:
    static int SetGridData(HST_ENT *pHist, void *pData);
    static void *GetGridData(HST_ENT *pHist);
    void *GetCurrentGridData()  {
        return(GetGridData(GetCurrent()));
    }
    void *GetNextGridData()  {
        return(GetGridData(GetNext()));
    }
    void *GetPrevGridData()  {
        return(GetGridData(GetPrev()));
    }
    void *GetIndexGridData(int iIndex)  {
        return(GetGridData(GetIndex(iIndex)));
    }

    //  Window data.
public:
    static int SetWindowData(HST_ENT *pHist, void *pData);
    static void *GetWindowData(HST_ENT *pHist);
    void *GetCurrentWindowData()  {
        return(GetWindowData(GetCurrent()));
    }
    void *GetNextWindowData()  {
        return(GetWindowData(GetNext()));
    }
    void *GetPrevWindowData()  {
        return(GetWindowData(GetPrev()));
    }
    void *GetIndexWindowData(int iIndex)  {
        return(GetWindowData(GetIndex(iIndex)));
    }

    //  Applet data.
public:
    static int SetAppletData(HST_ENT *pHist, void *pData);
    static void *GetAppletData(HST_ENT *pHist);
    void *GetCurrentAppletData()  {
        return(GetAppletData(GetCurrent()));
    }
    void *GetNextAppletData()  {
        return(GetAppletData(GetNext()));
    }
    void *GetPrevAppletData()  {
        return(GetAppletData(GetPrev()));
    }
    void *GetIndexAppletData(int iIndex)  {
        return(GetAppletData(GetIndex(iIndex)));
    }

    //  Document position.
public:
    static void SetPosition(HST_ENT *pHist, long lEleID);
    void SetCurrentPosition(long lEleID)    {
        SetPosition(GetCurrent(), lEleID);
    }
    void SetNextPosition(long lEleID)    {
        SetPosition(GetNext(), lEleID);
    }
    void SetPrevPosition(long lEleID)    {
        SetPosition(GetPrev(), lEleID);
    }
    void SetIndexPosition(long lEleID, int iIndex)    {
        SetPosition(GetIndex(iIndex), lEleID);
    }
    static long GetPosition(HST_ENT *pHist);
    long GetCurrentPosition()   {
        return(GetPosition(GetCurrent()));
    }
    long GetNextPosition()   {
        return(GetPosition(GetNext()));
    }
    long GetPrevPosition()   {
        return(GetPosition(GetPrev()));
    }
    long GetIndexPosition(int iIndex)   {
        return(GetPosition(GetIndex(iIndex)));
    }

    //  History entry list manipulation.
public:
    static HST_ENT *GetNext(HST_ENT *pEntry);
    static void SetNext(HST_ENT *pThis, HST_ENT *pNext);
};

#endif // __FEHistory_H
