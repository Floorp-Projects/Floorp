/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

//////////////////////////////////////////////////////////////////////
// Progress.cpp
// By Daniel Malmer
//
// Implementation of the XP progress dialog.
// Basically a wrapper around a DownloadFrame.
//////////////////////////////////////////////////////////////////////

#include "Progress.h"
#include "progress.h"
#include "Xfe/ProgressBar.h"
#include "Dashboard.h"
#include "xpassert.h"

#if 0
#include "ViewGlue.h"
#endif /* 

//////////////////////////////////////////////////////////////////////
// C++ implementation of C API
//////////////////////////////////////////////////////////////////////

/*
 * XFE_Progress
 */
XFE_Progress::XFE_Progress(Widget parent) : XFE_DownloadFrame(parent, NULL)
{
	Widget label;

	label = XtNameToWidget(getBaseWidget(), "*downloadURLLabel");
	if ( label ) fe_SetString(label, XmNlabelString, "");

	label = XtNameToWidget(getBaseWidget(), "*downloadFileLabel");
	if ( label ) fe_SetString(label, XmNlabelString, "");
#if 1
	/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
	 * leads to core dump
	 */
	notifyInterested(XFE_Component::logoStartAnimation, NULL);
#endif
}


/*
 * ~XFE_Progress
 */
XFE_Progress::~XFE_Progress()
{
}


/*
 * XFE_Progress::Create
 */
pw_ptr
XFE_Progress::Create(MWContext* /*parent*/, PW_WindowType /*type*/)
{
    return (pw_ptr) this;
}
    

/*
 * XFE_Progress::Destroy
 */
void
XFE_Progress::Destroy(void)
{
#if 1
	/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
	 * leads to core dump
	 */
	delete_response();
#endif
}
    

/*
 * XFE_Progress::SetCancelCallback
 */
void
XFE_Progress::SetCancelCallback(PW_CancelCallback cancelcb, void* cancelClosure)
{
	m_cancel_cb = cancelcb;
	m_cancel_closure = cancelClosure;
}


/*
 * XFE_Progress::isCommandEnabled
 */
XP_Bool
XFE_Progress::isCommandEnabled(CommandType cmd,
								void */*calldata*/, XFE_CommandInfo*)
{
	return ( cmd == xfeCmdStopLoading );
}


/*
 * XFE_Progress::handlesCommand
 */
XP_Bool
XFE_Progress::handlesCommand(CommandType cmd,
							void */*calldata*/, XFE_CommandInfo*)
{
	return ( cmd == xfeCmdStopLoading );
}


/*
 * XFE_Progress::doCommand
 */
void
XFE_Progress::doCommand(CommandType cmd,
						void */*calldata*/, XFE_CommandInfo*)
{
	if ( cmd == xfeCmdStopLoading && m_cancel_cb ) {
		m_cancel_cb(m_cancel_closure);
	}
#if 0
	/* 130116: core in canceling AB import progress dlg
	 */
	delete_response();
#endif
}


/*
 * XFE_Progress::SetWindowTitle
 */
void
XFE_Progress::SetWindowTitle(const char *title)
{
	XP_ASSERT(title);

	XFE_SetDocTitle(fe_WidgetToMWContext(getBaseWidget()), (char*) title);
}


/*
 * XFE_Progress::SetLine1
 */
void
XFE_Progress::SetLine1(const char* text)
{
	setAddress((char*) text);
}


/*
 * XFE_Progress::SetLine2
 */
void
XFE_Progress::SetLine2(const char* text)
{
	setDestination((char*) text);
}


/*
 * XFE_Progress::SetProgressText
 */
void
XFE_Progress::SetProgressText(const char* text)
{
	m_dashboard->setStatusText(text);
}


/*
 * XFE_Progress::SetProgressRange
 */
void
XFE_Progress::SetProgressRange(int32 minimum, int32 maximum)
{
	m_progress_bar_min = minimum;
	m_progress_bar_max = maximum;
}


/*
 * XFE_Progress::SetProgressValue
 */
void
XFE_Progress::SetProgressValue(int32 value)
{
	int32 pct;

	XP_ASSERT(value >= m_progress_bar_min);
	XP_ASSERT(value <= m_progress_bar_max);

	pct = (int32) (100 * 
		(double) value / (double) ( m_progress_bar_max - m_progress_bar_min ));

	XFE_SetProgressBarPercent(getContext(), pct);
}

/*
 * this is a slight hack so that the CONTEXT_DATA macro
 * still works even with this type of context
 */
void
XFE_Progress::AssociateWindowWithContext(MWContext *context)
{
    XP_ASSERT(context);
    if (!context) return;
    
    XP_ASSERT(context->type==MWContextProgressModule);
    if (context->type!=MWContextProgressModule) return;
    

#if 1
	/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
	 * leads to core dump
	 */
	fe_ContextData *fec = XP_NEW_ZAP (fe_ContextData);
	XP_ASSERT(fec);
	CONTEXT_DATA(context) = fec;
    CONTEXT_WIDGET(context) = getBaseWidget();
	// ViewGlue_addMapping(this, (void *)context);
#else
	fe_ContextData *fec;
    fec = (fe_ContextData *)XP_CALLOC(sizeof(fe_ContextData), 1);
    
    CONTEXT_DATA(context) = fec;
    CONTEXT_WIDGET(context) = getBaseWidget();
#endif
}

/**********************************************************************
 these are the C-visible functions from progress.h
**********************************************************************/

/*
 * pw_Create
 */
pw_ptr
pw_Create(MWContext* context, PW_WindowType type)
{
    XFE_Progress* pw;

	pw = new XFE_Progress(context ? CONTEXT_WIDGET(context) : FE_GetToplevelWidget());

    return pw->Create(context, type);
}


/*
 * pw_SetCancelCallback
 */
void
pw_SetCancelCallback(pw_ptr pw, PW_CancelCallback cancelcb, void* cancelClosure)
{
    if (pw)
        ((XFE_Progress *)pw)->SetCancelCallback(cancelcb, cancelClosure);
}


/*
 * pw_Show
 */
void
pw_Show(pw_ptr pw)
{
    if (pw)
        ((XFE_Progress *)pw)->show();
}


/*
 * pw_Hide
 */
void
pw_Hide(pw_ptr pw)
{
    if (pw) {
        ((XFE_Progress *)pw)->hide();
#if 1
		/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
		 * leads to core dump
		 */
		((XFE_Progress *)pw)->notifyInterested(XFE_Component::logoStopAnimation, NULL);
#endif
	}
}


/*
 * pw_Destroy
 */
void
pw_Destroy(pw_ptr pw)
{
    if (pw)
        ((XFE_Progress *)pw)->Destroy();
}


/*
 * pw_SetWindowTitle
 */
void
pw_SetWindowTitle(pw_ptr pw, const char *title)
{
    if (pw)
        ((XFE_Progress *)pw)->SetWindowTitle(title);
}


/*
 * pw_SetLine1
 */
void
pw_SetLine1(pw_ptr pw, const char *text)
{
    if (pw)
        ((XFE_Progress *)pw)->SetLine1(text);
}


/*
 * pw_SetLine2
 */
void
pw_SetLine2(pw_ptr pw, const char *text)
{
    if (pw)
        ((XFE_Progress *)pw)->SetLine2(text);
}


/*
 * pw_SetProgressText
 */
void
pw_SetProgressText(pw_ptr pw, const char * text)
{
    if (pw)
        ((XFE_Progress *)pw)->SetProgressText(text);
}


/*
 * pw_SetProgressRange
 */
void
pw_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum)
{
    if (pw)
        ((XFE_Progress *)pw)->SetProgressRange(minimum, maximum);
}


/*
 * pw_SetProgressValue
 */
void
pw_SetProgressValue(pw_ptr pw, int32 value)
{
    if (pw)
        ((XFE_Progress *)pw)->SetProgressValue(value);
}

void
fe_pw_AssociateWindowWithContext(MWContext *context,
                                 pw_ptr pw)
{
    if (pw)
        ((XFE_Progress *)pw)->AssociateWindowWithContext(context);
}

