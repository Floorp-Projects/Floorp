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

#ifndef __Context_Stubs_H
//	Avoid include redundancy
//
#define __Context_Stubs_H

//	Purpose:    Create a stub context class that will allow deriving contexts
//                  from the abstract context easier.
//              Only derive from the stub context iff you context has a very limited
//                  implementation (such as no or little UI, no CDC, etc....).

//	Required Includes
//
#include "cxabstra.h"

class CStubsCX : public CAbstractCX {
	ULONG m_ulRefCount;
	LPUNKNOWN m_pOuterUnk;

public:
	// IUnknown Interface
	STDMETHODIMP		 QueryInterface(REFIID,LPVOID *);
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);

	// IContext Interface
	
    //  All this context does is define empty member functions
    //      from the abstract pure virtual class.
#define FE_DEFINE(funkage, returns, args) virtual returns funkage args;
#include "mk_cx_fn.h"

public:
    //  Other overridables we must define as stubs.
    virtual void GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);
    virtual void TextTranslationExitRoutine(PrintSetup *pTextFE);

#ifdef LAYERS
       virtual BOOL HandleLayerEvent(CL_Layer * pLayer, CL_Event * pEvent) 
          { return FALSE; }
       virtual BOOL HandleEmbedEvent(LO_EmbedStruct *embed, CL_Event * pEvent) 
          { return FALSE; }
#endif

       virtual void UpdateStopState(MWContext *pContext);

public:
    //  Construction/destruction (sets context type).
    CStubsCX();
    CStubsCX(ContextType ctMyType, MWContextType XPType);
    CStubsCX(LPUNKNOWN pOuterUnk);
    CStubsCX(LPUNKNOWN pOuterUnk, ContextType ctMyType, MWContextType XPType);
    ~CStubsCX();
};

class CFrameCX: public CStubsCX {
private:
	CFrameGlue *m_pFrame;

public:
	CFrameCX(ContextType ctMyType, CFrameGlue *pFrame);
	~CFrameCX();

//	Progress helpers.
protected:
	int32 m_lOldPercent;
public:
	int32 QueryProgressPercent();
	void SetProgressBarPercent(MWContext *pContext, int32 lPercent);

	void StartAnimation();
	void StopAnimation();

	void Progress(MWContext *pContext, const char *pMessage);
	void AllConnectionsComplete(MWContext *pContext);
    
	CWnd *GetDialogOwner() const;

	CFrameGlue *GetFrame() const { return m_pFrame; }
};

#endif // __Context_Stubs_H
