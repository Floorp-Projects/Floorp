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
#ifndef _MOZPROBE
#define _MOZPROBE

/* mozprobe.h
 *
 * This file describes a the WinFE interface for third-party
 * applications to access the layout probe API.  This file is intended
 * to be used by mozilla.exe (specifically, hiddenfr.cpp) to permit it
 * to use the code in mozprobe.dll to service layout probe requests from
 * such applications.  It will also be used by third-party applications
 * to access mozprobe.dll in order to utilize the layout probe APIs.
 *
 * See mozprobe.cpp for more details.
 * 
 * The intent is for hiddenfr.cpp to do something like:
 *    #include "mozprobe.h"
 *    hmodHook = LoadLibrary( mozProbeDLLName );
 *    PROBESERVERPROC serverProc = GetProcAddress( hmodHook, mozProbeServerProc );
 *    if ( hook ) {
 *       PROBEAPITABLE fns = { LO_QA_CreateProbe, ... };
 *       serverProc( wparam, lparam, &fns );
 *    }
 *
 * This way, nothing happens unless mozprobe.dll is present on the user's system
 * and the code is much more loosely coupled.
 *
 * Client C++ applications will do something like:
 *    #include "mozprobe.h"
 *    MozillaLayoutProbe probe;
 *
 *    BOOL bResult = probe.GotoFirstElement();
 *
 *    if ( bResult ) {
 *       printf( "First element is of type %d\n", probe.GetElementType() );
 *    }
 *
 * Client C applications will do something like:
 *   #include "mozprobe.h"
 *
 *   HINSTANCE hmodProbe = LoadLibrary( mozProbeDLLName );
 *   long (*LO_QA_CreateProbe)( MWContext* ) = (long(*)(MWContext*))GetProcAddress( hmodProbe, "LO_QA_CreateProbe" );
 *   BOOL (*LO_QA_GotoFirstElement)( long ) = (BOOL(*)(long))GetProcAddress( hmodProbe, "LO_QA_GotoFirstElement" );
 *   BOOL (*LO_QA_GetElementType)( long, int* ) = (int(*)(long))GetProcAddress( hmodProbe, "LO_QA_GetElementType" );
 *   
 *   long probeID = LO_QA_CreateProbe((MWContext*)1);
 *   long type;
 *   LO_QA_GotoFirstElement( probeID );
 *   LO_QA_GetElementType( probeID, &type );
 *   printf( "First element is of type %d\n", type );
 *
 * mozprobe.dll will export all the LO_QA_* entry points for use by C (or other
 * flavor) applications.  All these entry points have exactly the same interface and
 * semantics as the corresponding functions declared in mozilla/lib/layout/layprobe.h,
 * with one exception: LO_QA_CreateProbe takes as argument a "MWContext*" but interprets
 * this as an int context ordinal (i.e., 1->first browser window, 2->the second, etc.).
 * The reason is that a separate application can't have a real MWContext*, anyway, so
 * we just use these int values as "fake" ones.  The corresponding MWContext* will be
 * obtained on the server side (see ProcessMozillaLayoutProbeHook).
 */

/*
 * This enum replicates ColorType in layprobe.h.
 */
typedef enum {
    Probe_Background,
    Probe_Foreground,
    Probe_Border
} PROBECOLORTYPE;

/*
 * This enum defines the same constants as the LO_* #defines in layprobe.h.
 */
typedef enum {
    Probe_Text   = 1,
    Probe_HRule  = 3,
    Probe_Image  = 4,
    Probe_Bullet = 5,
    Probe_Form   = 6,
    Probe_Table  = 8,
    Probe_Cell   = 9,
    Probe_Embed  = 10,
    Probe_Java   = 12,
    Probe_Object = 14
} PROBEELEMTYPE;

/*
 * Layout probe IPC request IDs.  These are passed in the dwData field of
 * the COPYDATASTRUCT.
 */
typedef enum {
    NSCP_Probe_StartRequestID = 900,
    NSCP_Probe_Create,
    NSCP_Probe_Destroy,
    NSCP_Probe_GotoFirstElement,
    NSCP_Probe_GotoNextElement,
    NSCP_Probe_GotoChildElement,
    NSCP_Probe_GotoParentElement,
    NSCP_Probe_GetElementType,
    NSCP_Probe_GetElementXPosition,
    NSCP_Probe_GetElementYPosition,
    NSCP_Probe_GetElementWidth,
    NSCP_Probe_GetElementHeight,
    NSCP_Probe_ElementHasURL,
    NSCP_Probe_ElementHasText,
    NSCP_Probe_ElementHasColor,
    NSCP_Probe_ElementHasChild,
    NSCP_Probe_ElementHasParent,
    NSCP_Probe_GetElementText,
    NSCP_Probe_GetElementTextLength,
    NSCP_Probe_GetElementColor,
    NSCP_Probe_EndRequestID
} PROBE_IPC_REQUEST;

/*
 * This structure is used by hiddenfr.cpp to pass pointers to the real
 * layout probe APIs to the server proc.
 */
struct MWContext_;
typedef struct {
    long (*LO_QA_CreateProbe)( struct MWContext_ * );
    void (*LO_QA_DestroyProbe)( long );
    BOOL (*LO_QA_GotoFirstElement)( long );
    BOOL (*LO_QA_GotoNextElement)( long );
    BOOL (*LO_QA_GotoChildElement)( long );
    BOOL (*LO_QA_GotoParentElement)( long );
    BOOL (*LO_QA_GetElementType)( long, int * );
    BOOL (*LO_QA_GetElementXPosition)( long, long * );
    BOOL (*LO_QA_GetElementYPosition)( long probeID, long * );
    BOOL (*LO_QA_GetElementWidth)( long, long * );
    BOOL (*LO_QA_GetElementHeight)( long, long * );
    BOOL (*LO_QA_HasURL)( long, BOOL * );
    BOOL (*LO_QA_HasText)( long, BOOL * );
    BOOL (*LO_QA_HasColor)( long, BOOL * );
    BOOL (*LO_QA_HasChild)( long, BOOL * );
    BOOL (*LO_QA_HasParent)( long, BOOL * );
    BOOL (*LO_QA_GetText)( long, char *, long );
    BOOL (*LO_QA_GetTextLength)( long, long * );
    BOOL (*LO_QA_GetColor)( long, long *, PROBECOLORTYPE );
    struct MWContext_* (*Ordinal2Context)( long );
} PROBEAPITABLE;

static const char * const mozProbeDLLName        = "mozprobe";
static const char * const mozProbeServerProcName = "mozProbeServerProc";

/*
 * Typedef for the "server" proc exported by mozprobe.dll.
 */
typedef LONG (*PROBESERVERPROC)(WPARAM,LPARAM,PROBEAPITABLE*);

#ifdef __cplusplus
/* 
 * Client C++ interface (for use in external modules).
 */
struct MozillaLayoutProbe {
    // pseudo-Constructor.  You must use this to create an instance (and
    // then delete it!).  This is to minimize the entry points exported from
    // mozprobe.dll.
    static MozillaLayoutProbe *MakeProbe( long context );

    // Dtor.  This will destroy the probe.
    virtual ~MozillaLayoutProbe();

    // Positioning.
    virtual BOOL GotoFirstElement();
    virtual BOOL GotoNextElement();
    virtual BOOL GotoChildElement();
    virtual BOOL GotoParentElement();

    // Element attributes.
    virtual int  GetElementType() const;
    virtual long GetElementXPosition() const;
    virtual long GetElementYPosition() const;
    virtual long GetElementWidth() const;
    virtual long GetElementHeight() const;
    virtual long GetElementColor( PROBECOLORTYPE attr ) const;
    virtual long GetElementTextLength() const;
    virtual long GetElementText( char *buffer, long bufLen ) const;

    // Element queries.
    virtual BOOL ElementHasURL() const;
    virtual BOOL ElementHasText() const;
    virtual BOOL ElementHasColor() const;
    virtual BOOL ElementHasChild() const;
    virtual BOOL ElementHasParent() const;

    // Status (indicates whether most recent request succeeded).
    virtual BOOL IsOK() const;

    // Internals.
    private:
        // Contructor.  This will create the probe.  The "context"
        // specifies the browser window to probe (1st, 2nd, etc.).
        // This is private!  Use MakeProbe() to create objects.
        MozillaLayoutProbe( long context = 1 );
    
        // Utilities to consolidate code.
        BOOL  position( PROBE_IPC_REQUEST ) const;
        long  getLongAttribute( PROBE_IPC_REQUEST, void* = 0 ) const;
        BOOL  getBOOLAttribute( PROBE_IPC_REQUEST ) const;
        long  sendRequest( PROBE_IPC_REQUEST, unsigned long = 0, void* = 0 ) const;
        void  setOK( BOOL ) const;

        // Shared memory handling.
        BOOL allocSharedMem( unsigned long ) const;

        // Data members.
        long          m_lProbeID;
        BOOL          m_bOK;
        HANDLE        m_hSharedMem;
        unsigned long m_ulSharedMemSize;
        void*         m_pSharedMem;
};
#endif /* __cpluscplus */

#endif /* _MOZPROBE */
