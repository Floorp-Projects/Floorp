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
#ifndef __CX_ABSTRACT_H
//	Avoid include redundancy
//
#define __CX_ABSTRACT_H

#ifndef _APICX_H
#include "apicx.h"
#endif
#include "ni_pixmp.h"

//	Purpose:    Provide the abstract context implementation.
//	Comments:   Pure virtual; must be derived from.
//              Through wrapping functions, the C XP libs will call into the context, and
//                  virtual resolution will resolve the correct context.
//	Revision History:
//      04-26-95    created GAB
//

//	Required Includes
//

//	Constants
//

//	Structures
//
//  Mechanism for run time information.
//  Must be updated with the addition of every new context type.
//  For optimizations for specific output devices in common code.
//  All classes meant to be abstract will have values of 0x0.
//      This allows us to ASSERT if the context is of a valid type.

typedef enum ContextType    {
    Abstract = 0x0000,
	DeviceContext = 0x0000,
    Stubs = 0x0000,
	Network,
    Save,
	Print,
	Pane,
	Window,
    Bookmarks,
	MetaFile,
	MailCX,
	NewsCX,
	MailThreadCX,
	NewsThreadCX,
	SearchCX,
	AddressCX,
    HtmlHelp,
    HistoryCX,    
	IconCX,
	RDFSlave,
    MaxContextTypes   //  Leave as last entry please
} ContextType;

//  The abstract windows context
class CAbstractCX: public IMWContext {
private:
    //  The actual XP context.
    MWContext *m_pXPCX;

protected:
    //  Set this in the constructor of each derived class for run time type information.
    ContextType m_cxType;
	IL_GroupContext* m_pImageGroupContext;

public:
    CAbstractCX();
    virtual ~CAbstractCX();
	//	Function to load a URL with this context, with the custom exit routing handler.
	//	Use this to NET_GetURL instead or XL_TranslateText instead.
	virtual int GetUrl(URL_Struct *pUrl, FO_Present_Types iFormatOut, BOOL bReallyLoad = TRUE, BOOL bForceNew = FALSE);
    virtual XL_TextTranslation TranslateText(URL_Struct *pUrl, const char *pFileName, const char *pPrefix = NULL, int iWidth = 75);

    virtual void UpdateStopState(MWContext *pContext) = 0;

	//	Generic brain dead interface to load a URL.
	//	This used to be known as OnNormalLoad.
	int NormalGetUrl(const char *pUrl, const char *pReferrer = NULL, const char *pTarget = NULL, BOOL bForceNew = FALSE);

	//	Atomic load operations regarding history.
	virtual void Reload(NET_ReloadMethod iReloadType = NET_NORMAL_RELOAD);
	virtual void NiceReload(int usePassInType = 0, NET_ReloadMethod iReloadType = NET_NORMAL_RELOAD);
	virtual void Back();
	virtual void Forward();
	virtual void ImageComplete(NI_Pixmap* image) {;}
	virtual BITMAPINFO*	NewPixmap(NI_Pixmap* pImage, BOOL mask = FALSE) { return NULL;}


	//	Function for easy creation of a URL from the current history entry.
	virtual URL_Struct *CreateUrlFromHist(BOOL bClearStateData = FALSE, SHIST_SavedData *pSavedData = NULL, BOOL bWysiwyg = FALSE);
	virtual BOOL CanCreateUrlFromHist();
//  Construction/destruction (creates/destroys XP context)
	virtual void DestroyContext();
	void NiceDestroyContext();
private:
	BOOL m_bDestroyed;
public:
    char * m_pLastStatus;
	BOOL IsDestroyed() const	{
		//	This function is very important.
		//	It allows you to write code which can detect when a context
		//		is actually in the process of being destroyed.
		//	In many cases, you will receive callbacks from the XP libraries
		//		while the context is being destroyed.  If you GPF, then use
		//		this function to better detect your course of action.
		return(m_bDestroyed);
	}

public:
    //  Access to type information, for optimizations in common code.
    ContextType GetContextType() const  {
        ASSERT(m_cxType);
        return(m_cxType);
    }

	int16 GetWinCsid();

    BOOL IsDCContext() const    {
        ASSERT(m_cxType);
        switch(m_cxType)    {
		case Print:
		case Window:
		case MetaFile:
		case Pane:
			return(TRUE);

        default:
            return(FALSE);
        }
    }
	BOOL IsPureDCContext() const	{
		//	Return TRUE if all we depend upon is
		//		a DC for display (non windowed).
		return(!IsWindowContext() && IsDCContext() == TRUE);
	}
    BOOL IsPrintContext() const {
        ASSERT(m_cxType);
        switch(m_cxType)    {
		case Print:
			return(TRUE);

        default:
            return(FALSE);
        }
    }
	BOOL IsWindowContext() const	{
		ASSERT(m_cxType);
		switch(m_cxType)	{
		case Window:
		case Pane:
			return(TRUE);

		default:
			return(FALSE);
		}
	}

	BOOL IsFrameContext() const {
		ASSERT(m_cxType);
		switch (m_cxType) {
		case Window:
			return TRUE;

		default:
			return FALSE;
		}
	}

public:
	//	Owner of any dialogs that we'll bring up.
	virtual CWnd *GetDialogOwner() const;

public:
    //  For the foolhardy, who desire access to the XP context in our hour of need.
    MWContext *GetContext() const   {
        return(m_pXPCX);
    }
    MWContext *GetParentContext() const   {
		//	Only valid if IsGridCell is true.
        return(m_pXPCX->grid_parent);
    }
	// Override to allow XP Context->Frame matching
	virtual CFrameGlue *GetFrame() const { return NULL; }

	//	Named contexts and grid stuff.
	void SetContextName(const char *pName);
	void SetParentContext(MWContext *pParentContext);
	BOOL IsGridCell() const;
	BOOL IsGridParent() const;


    //  layout modularization effort.
public:
    MWContext *GetDocumentContext() const   {
        return(m_pXPCX);
    }

public:
	//	A function to interrupt any loads in this context.
	//	This happens immediatly or idly.
	//	Also, if the call is not nested, it may destroy this object
	//		if DestroyContext has been called previously.
	virtual void Interrupt();
public:
	BOOL m_bIdleInterrupt;
	BOOL m_bIdleDestroy;
private:
	int m_iInterruptNest;

private:
	BOOL m_bImagesLoading; // True if any images in this context are loading.
	BOOL m_bImagesLooping; // True if any images in this context are looping.
	BOOL m_bImagesDelayed; // True if any images in this context are delayed. 
	BOOL m_bMochaImagesLoading;
	BOOL m_bMochaImagesLooping;
	BOOL m_bMochaImagesDelayed;
	BOOL IsContextStoppableRecurse();

public:
    BOOL m_bNetHelpWnd;

    BOOL IsNetHelpWnd() { return m_bNetHelpWnd; }
    
public:
	// A wrapper around XP_IsContextStoppable.  This is necessary
	// because a context is stoppable if its image context is
	// stoppable, and the image context is owned by the Front End.
	BOOL IsContextStoppable();

	// Returns TRUE if this context or its children have any looping images.
	BOOL IsContextLooping();

	void SetImagesLoading(BOOL val) {m_bImagesLoading = val;}
	void SetImagesLooping(BOOL val) {m_bImagesLooping = val;}
    void SetImagesDelayed(BOOL val) {m_bImagesDelayed = val;}
	void SetMochaImagesLoading(BOOL val) {m_bMochaImagesLoading = val;}
	void SetMochaImagesLooping(BOOL val) {m_bMochaImagesLooping = val;}
    void SetMochaImagesDelayed(BOOL val) {m_bMochaImagesDelayed = val;}

	//  Client pull timer information.
public:
    void *m_pClientPullData;
    void *m_pClientPullTimeout;

private:
	//	Information to time a load.
	time_t m_ttStopwatch;
	time_t m_ttOldwatch;
public:
	BOOL ProgressReady(time_t ttCurTime)	{
		BOOL bRetval = ttCurTime != m_ttOldwatch;
		m_ttOldwatch = ttCurTime;
		return(bRetval);
	}
	time_t GetElapsedSeconds(time_t ttCurTime)	{
		return(ttCurTime - m_ttStopwatch);
	}

	//moved to .cpp due to dependency problems
	void ResetStopwatch();

public:
	//	Context Identification.
	DWORD GetContextID()	{
		return((DWORD)(m_pXPCX->context_id));
	}
	static CAbstractCX *FindContextByID(DWORD dwID);

public:
	//	NCAPI context pointer.
	CNcapiUrlData *m_pNcapiUrlData;

//	Progress Helpers
	virtual int32 QueryProgressPercent() { return 0; }
	virtual void StartAnimation() {}
	virtual void StopAnimation() {}

//	MFC Helpers;  abstraction away
public:
	virtual void MailDocument();
	virtual BOOL CanMailDocument();
	virtual void NewWindow();
	virtual BOOL CanNewWindow();
	virtual void OpenUrl();
	virtual BOOL CanOpenUrl();
	virtual void AllBack();
	virtual BOOL CanAllBack();
	virtual void AllForward();
	virtual BOOL CanAllForward();
	virtual void AllInterrupt();
	virtual BOOL CanAllInterrupt();
	virtual void AllReload(NET_ReloadMethod iReloadType = NET_NORMAL_RELOAD);
	virtual BOOL CanAllReload();
	virtual void CopySelection();
	virtual BOOL CanCopySelection();
	virtual void AddToBookmarks();
	virtual BOOL CanAddToBookmarks();

#ifdef LAYERS
       virtual BOOL HandleLayerEvent(CL_Layer * pLayer, CL_Event * pEvent) 
          { return FALSE; }
       virtual BOOL HandleEmbedEvent(LO_EmbedStruct *embed, CL_Event * pEvent) 
          { return FALSE; }
#endif
	virtual void GoHome(){}
	virtual void AllFind(MWContext *pSearchContext = NULL){}
	virtual BOOL DoFind(CWnd * pWnd, const char * pFindString, 
			    BOOL bMatchCase, BOOL bSearchDown, BOOL bAlertOnNotFound)
	    { return FALSE; }
	virtual void PrintContext(){}
	virtual void GetWindowOffset(int32 *x, int32 *y){}
	
       void    MochaDestructionComplete();
};
//	Global variables
//

//	Macros
//

//	Function declarations
//
//  The common front end, called in every context, that in turn uses a virtual call into
//      the correct context, except in specific optimized situations.
#define MAKE_FE_FUNCS_PREFIX(funkage)   CFE_##funkage
#define MAKE_FE_FUNCS_EXTERN
#include "mk_cx_fn.h"

//  The exit routines.
void CFE_GetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);
void CFE_SimpleGetUrlExitRoutine(URL_Struct *pUrl, int iStatus, MWContext *pContext);
void CFE_TextTranslationExitRoutine(PrintSetup *pTextFE);

#endif // __CX_ABSTRACT_H
