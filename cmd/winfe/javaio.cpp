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

#ifdef JAVA_WIN32
// javaio.cpp : implementation file
//

#include "mainfrm.h"
#include "javaio.h"
#include "io.h"
#include "prthread.h"
#include "prmon.h"
#include "prfile.h"
#include "prdump.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


// Maximun number of characters in the edit box before truncation occurs...
#define MAX_JAVA_OUTPUT_SIZE        10000
#define JAVA_IO_THREAD_STACK_SIZE   16768

/////////////////////////////////////////////////////////////////////////////
// CNSPRStream

PRIOMethods CNSPRStream::IOMethods = { CNSPRStream::InitMethod,
                                       CNSPRStream::CloseMethod, 
                                       CNSPRStream::ReadMethod, 
                                       CNSPRStream::WriteMethod };

CNSPRStream::CNSPRStream(void)
{
}


void CNSPRStream::MapFileDescriptor( PROSFD fd )
{
    PR_MapFileHandle( fd, (CNSPRStream *)this, &IOMethods );
}


int32 CNSPRStream::InitMethod(PRFileDesc * fd)
{
    int32 ret = -1; 

    if( fd->instance ) {
        ret = ((CNSPRStream *)(fd->instance))->Init(fd);
    } 
    return( ret );
}

int32 CNSPRStream::CloseMethod(PRFileDesc * fd)
{
    int32 ret = -1; 

    if( fd->instance ) {
        ret = ((CNSPRStream *)(fd->instance))->Close(fd);
    } 
    return( ret );
}


int32 CNSPRStream::ReadMethod (PRFileDesc * fd, void *buf, int32 amount)
{
    int32 ret = -1; 

    if( fd->instance ) {
        ret = ((CNSPRStream *)(fd->instance))->Read(fd, buf, amount);
    } 
    return( ret );
}


int32 CNSPRStream::WriteMethod(PRFileDesc * fd, const void *buf, int32 amount)
{
    int32 ret = -1; 

    if( fd->instance ) {
        ret = ((CNSPRStream *)(fd->instance))->Write(fd, buf, amount);
    } 
    return( ret );
}



/////////////////////////////////////////////////////////////////////////////
// CJavaStdError

CJavaStdError::CJavaStdError(void)
{
}


int32 CJavaStdError::Init(PRFileDesc *fd)
{
    return( fd->osfd );
}


int32 CJavaStdError::Close(PRFileDesc *fd)
{
    /* Never close stderr */
    return( -1 );
}


int32 CJavaStdError::Read(PRFileDesc *fd, void *buf, int32 amount)
{
    /* Cannot read from stderr */
    return( -1 );
}


int32 CJavaStdError::Write(PRFileDesc *fd, const void *buf, int32 amount)
{
    char buffer[1024];
    int32 size;
    int len;

    size = amount;
    while (size) {
        len = size > 1023 ? 1023 : size;
        strncpy( buffer, (const char *)buf, len );
        buffer[len] = '\0';
        size -= len;

	//
	// Calling MFC's MessageBox causes a deadlock if the calling thread is NOT the
	// main UI thread of the navigator.  In practice, this is ALWAYS the case :-(
	//
	//AfxGetApp()->GetMainWnd()->MessageBox( buffer, "Java Error" );
	(void)::MessageBox(NULL, buffer, "Java Error", MB_SETFOREGROUND | MB_OK);
    }
    return(amount);
}

/////////////////////////////////////////////////////////////////////////////
// CJavaStdOutput

#ifdef __BORLANDC__
	static int64 SleepForever = 0x7fffffffffffffff;
#else
	static int64 SleepForever = LL_MAXINT;
#endif

CJavaStdOutput::CJavaStdOutput(void)
{
    m_bAutoDelete = TRUE;

    m_pOwnThread  = 0;
    m_ThreadId    = 0;
    m_pWnd        = 0;
    m_pOutputMon  = PR_NewNamedMonitor("output-monitor");

    m_msgWrite    = ::RegisterWindowMessage("NSPR_ConsoleWrite");
}


CJavaStdOutput::~CJavaStdOutput()
{
    if( m_pOutputMon ) {
        PR_DestroyMonitor( m_pOutputMon );
        m_pOutputMon = NULL;
    }
}




BOOL CJavaStdOutput::PreTranslateMessage(MSG* pMsg)
{
    BOOL bResult;

    if( (pMsg->hwnd == 0) && (pMsg->message == m_msgWrite) ) {
        m_pWnd->Display( (const char *)pMsg->lParam, (int32)pMsg->wParam );
        bResult = TRUE;
    } else {
        bResult = CWinThread::PreTranslateMessage(pMsg);
    }
    return bResult;
}


BOOL CJavaStdOutput::InitInstance(void)
{
    PRThreadStack *ts;
    BOOL bRet = FALSE;
    CWinThread::InitInstance();


    /* Set up the thread stack information */
    ts = (PRThreadStack*) calloc(1, sizeof(PRThreadStack));
    if( ts ) {
        ts->stackSize = JAVA_IO_THREAD_STACK_SIZE;

        m_pOwnThread  = PR_AttachThread("Java IO", 15, ts);
        m_pOwnThread->flags |= _PR_NO_SUSPEND;
        m_ThreadId    = ::GetCurrentThreadId();

        if( m_pWnd = new CJavaOutputWnd() ) {
            bRet = m_pWnd->Initialize( m_pOutputMon );
        }

    }

    return(bRet);
}
    
int CJavaStdOutput::ExitInstance(void)
{
    if( m_pWnd ) {
        delete m_pWnd;
        m_pWnd = NULL;
    }
    if( m_pOwnThread ) {
        PR_DetachThread( m_pOwnThread );
        m_pOwnThread = NULL;
    }
    return CWinThread::ExitInstance();
}


BOOL CJavaStdOutput::ToggleVisible(void)
{
    BOOL bRet = m_pWnd->IsWindowVisible();

    m_pWnd->ShowWindow( bRet ? SW_HIDE : SW_SHOW );
    return( ! bRet );
}

BOOL CJavaStdOutput::IsVisible(void)
{
    return m_pWnd->IsWindowVisible();
}
    


int32 CJavaStdOutput::Init(PRFileDesc *fd)
{
    int32 ret = fd->osfd;

    if(! CreateThread(0, JAVA_IO_THREAD_STACK_SIZE, 0)) {
        ret = -1;
    }
    return ret;
}


int32 CJavaStdOutput::Close(PRFileDesc *fd)
{
    ::PostThreadMessage( m_ThreadId, WM_QUIT, 0, 0 );
    return 0;
}


int32 CJavaStdOutput::Read(PRFileDesc *fd, void *buf, int32 amount)
{
    /* Cannot read from stdout */
    return( -1 );
}


int32 CJavaStdOutput::Write(PRFileDesc *fd, const void *buf, int32 amount)
{
    if( PR_CurrentThread() == m_pOwnThread ) {
        m_pWnd->Display((const char *)buf, amount);
    } else {
        PR_EnterMonitor( m_pOutputMon );
        ::PostThreadMessage( m_ThreadId, m_msgWrite, (WPARAM)amount, (LPARAM)buf );
        PR_Wait( m_pOutputMon, SleepForever );
        PR_ExitMonitor( m_pOutputMon);
    }    
    return( amount );
}




/////////////////////////////////////////////////////////////////////////////
// CJavaOutputWnd
CJavaOutputWnd::CJavaOutputWnd()
{
    m_pEdit      = 0;
    m_pOutputMon = 0;
}


CJavaOutputWnd::~CJavaOutputWnd()
{
    if( m_pEdit ) {
        delete m_pEdit;
        m_pEdit = NULL;
    }
}


BOOL CJavaOutputWnd::Initialize(PRMonitor *pMon)
{
    BOOL bRet;
    RECT rect;

    m_pOutputMon = pMon;

    // Create the Frame Window
    bRet = CFrameWnd::Create( NULL, "Java Output", 
                   WS_OVERLAPPEDWINDOW | WS_VISIBLE );
    if( bRet ) {
        GetClientRect( &rect );

        // Create the Child Edit control used to display the text
        m_pEdit = new CGenericEdit();
        bRet = m_pEdit->Create( WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                rect, this, 100 );

        // Initialize the font for the edit control...
///        pContext = CMainFrame::GetLastActiveFrame()->GetMainContext()->GetContext();
///        if (INTL_CharSetType(pContext) == SINGLEBYTE) {
            m_pEdit->SetFont(CFont::FromHandle((HFONT)GetStockObject(ANSI_FIXED_FONT)));
///        } else {
            // For multibyte, we can not use ANSI font
///            m_pEdit->SetFont(CFont::FromHandle((HFONT)GetStockObject(SYSTEM_FIXED_FONT)));
///        }
    }
    return(bRet);
    
}


void CJavaOutputWnd::Display( const char *buf, int32 amount )
{
    char buffer[90];
    const char *pbuf;
    int32 size;
    int i, j, len;

    PR_EnterMonitor( m_pOutputMon );

    size = amount;
    pbuf = (const char *)buf;
    while (size) {
        len = size > 79 ? 79 : size;
        i = j = 0;
        // Translate \n to \r\n for the edit box
        while( j<len ) {
            if( pbuf[i] == '\n' ) {
                buffer[j++] = '\r';
            } 
            buffer[j++] = pbuf[i++];
        }
        buffer[j] = '\0';
        size -= i;
        pbuf += i;

        // Truncate the edit box text if it is reaching its limit
        len = m_pEdit->GetWindowTextLength();
        if( len > MAX_JAVA_OUTPUT_SIZE ) {
         len -= (MAX_JAVA_OUTPUT_SIZE/2);
            i = j = 0;
            while ( len > 0) {
                int k = m_pEdit->LineLength(j++);

                len -= k;
                i   += k;
            }
            m_pEdit->SetSel( 0, i, TRUE );
            m_pEdit->ReplaceSel( "" );
        }

        // Append the new text into the edit box
        m_pEdit->SetSel( m_pEdit->GetWindowTextLength(), -1, TRUE );
        m_pEdit->ReplaceSel( buffer );
    }

    PR_Notify( m_pOutputMon );
    PR_ExitMonitor( m_pOutputMon );
}


afx_msg void CJavaOutputWnd::OnSize(UINT type, int cx, int cy)
{
    if( m_pEdit ) {
        m_pEdit->MoveWindow(0, 0, cx, cy, TRUE);
    }
}

afx_msg void CJavaOutputWnd::OnSysCommand(UINT nId, LPARAM lParam)
{
    if( nId == SC_CLOSE ) {
        ShowWindow(SW_HIDE);
    } else {
        CFrameWnd::OnSysCommand(nId, lParam);
    }
}

afx_msg void CJavaOutputWnd::OnKeyUp(UINT nChar, UINT nRep, UINT nFlags)
{
#ifdef DEBUG
    if( nChar == VK_F1 ) {
        MessageBeep(0);
        PR_DumpThreads();
        PR_DumpMonitors();
    }
#endif
}

#ifndef _AFXDLL
#undef new
#endif
IMPLEMENT_DYNCREATE(CJavaOutputWnd, CFrameWnd)
#ifndef _AFXDLL
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CJavaOutputWnd, CFrameWnd)
    ON_WM_SIZE ( )
    ON_WM_SYSCOMMAND ( )
    ON_WM_KEYUP ( )
END_MESSAGE_MAP()


#endif /* JAVA_WIN32 */
