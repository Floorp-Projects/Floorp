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

#ifndef __JavaConsoleWindow_H
#define __JavaConsoleWindow_H
// javaio.h : header file
//

#include "prfile.h"


/////////////////////////////////////////////////////////////////////////////
// CNSPRStream abstract class

class CNSPRStream
{
public:
    CNSPRStream(void);

    void MapFileDescriptor( PROSFD fd );

protected:
    virtual int32 Init  (PRFileDesc *fd)                                = 0;
    virtual int32 Close (PRFileDesc *fd)                                = 0;
    virtual int32 Read  (PRFileDesc *fd, void *buf, int32 amount)       = 0;
    virtual int32 Write (PRFileDesc *fd, const void *buf, int32 amount) = 0;

    static int32 InitMethod  (PRFileDesc * fd);
    static int32 CloseMethod (PRFileDesc * fd);
    static int32 ReadMethod  (PRFileDesc * fd, void *buf, int32 amount);
    static int32 WriteMethod (PRFileDesc * fd, const void *buf, int32 amount);

// Member data
protected:
    static PRIOMethods IOMethods;
};


/////////////////////////////////////////////////////////////////////////////
// CJavaStdError stream

class CJavaStdError : public CNSPRStream
{
public:
    CJavaStdError(void);

// Overrides
protected:
    virtual int32 Init (PRFileDesc *fd);
    virtual int32 Close(PRFileDesc *fd);
    virtual int32 Read (PRFileDesc *fd, void *buf, int32 amount);
    virtual int32 Write(PRFileDesc *fd, const void *buf, int32 amount);
};

class CJavaOutputWnd;
/////////////////////////////////////////////////////////////////////////////
// CJavaStdOutput
class CJavaStdOutput : public CWinThread, public CNSPRStream
{
public:
    CJavaStdOutput(void);
    virtual ~CJavaStdOutput();

    BOOL ToggleVisible(void);
    BOOL IsVisible(void);

// Overrides
    BOOL PreTranslateMessage(MSG* pMsg);
    BOOL InitInstance(void);
    int ExitInstance(void);

// Overrides
protected:
    virtual int32 Init (PRFileDesc *fd);
    virtual int32 Close (PRFileDesc *fd);
    virtual int32 Read  (PRFileDesc *fd, void *buf, int32 amount);
    virtual int32 Write (PRFileDesc *fd, const void *buf, int32 amount);

// Member Data
    CJavaOutputWnd *m_pWnd;
    DWORD           m_ThreadId;
    PRThread       *m_pOwnThread;
    PRMonitor      *m_pOutputMon;

    UINT            m_msgWrite;
};


/////////////////////////////////////////////////////////////////////////////
// CJavaStdOutput window

class CJavaOutputWnd : public CFrameWnd
{
    DECLARE_DYNCREATE(CJavaOutputWnd)

public:
    CJavaOutputWnd(void);
    virtual ~CJavaOutputWnd();

    BOOL Initialize(PRMonitor *pOutMon);
    void Display( const char *buf, int32 amount);

// Overrides
protected:
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnSysCommand(UINT nId, LPARAM lParam);
    afx_msg void OnKeyUp(UINT nChar, UINT nRep, UINT nFlags);

// Member Data
    CGenericEdit     *m_pEdit;
    PRMonitor *m_pOutputMon;

protected:
    DECLARE_MESSAGE_MAP()
};



#endif /* __JavaConsoleWindow_H */


