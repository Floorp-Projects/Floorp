//////////////////////////////////////////////////////////////////////
// ProgressFrame.cpp
// By Daniel Malmer
//
// Implementation of the XP progress dialog.
// Basically a wrapper around a DownloadFrame.
//////////////////////////////////////////////////////////////////////

#include "ProgressFrame.h"
#include "pw_public.h"
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
 * XFE_ProgressFrame
 */
XFE_ProgressFrame::XFE_ProgressFrame(Widget parent) : XFE_DownloadFrame(parent, NULL)
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
 * ~XFE_ProgressFrame
 */
XFE_ProgressFrame::~XFE_ProgressFrame()
{
}


/*
 * XFE_ProgressFrame::Create
 */
pw_ptr
XFE_ProgressFrame::Create(MWContext* /*parent*/, PW_WindowType /*type*/)
{
    return (pw_ptr) this;
}
    

/*
 * XFE_ProgressFrame::Destroy
 */
void
XFE_ProgressFrame::Destroy(void)
{
#if 1
	/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
	 * leads to core dump
	 */
	delete_response();
#endif
}
    

/*
 * XFE_ProgressFrame::SetCancelCallback
 */
void
XFE_ProgressFrame::SetCancelCallback(PW_CancelCallback cancelcb, void* cancelClosure)
{
	m_cancel_cb = cancelcb;
	m_cancel_closure = cancelClosure;
}


/*
 * XFE_ProgressFrame::isCommandEnabled
 */
XP_Bool
XFE_ProgressFrame::isCommandEnabled(CommandType cmd,
								void */*calldata*/, XFE_CommandInfo*)
{
	return ( cmd == xfeCmdStopLoading );
}


/*
 * XFE_ProgressFrame::handlesCommand
 */
XP_Bool
XFE_ProgressFrame::handlesCommand(CommandType cmd,
							void */*calldata*/, XFE_CommandInfo*)
{
	return ( cmd == xfeCmdStopLoading );
}


/*
 * XFE_ProgressFrame::doCommand
 */
void
XFE_ProgressFrame::doCommand(CommandType cmd,
						void */*calldata*/, XFE_CommandInfo*)
{
	if ( cmd == xfeCmdStopLoading && m_cancel_cb ) {
		m_cancel_cb(m_cancel_closure);
	}

	delete_response();
}


/*
 * XFE_ProgressFrame::SetWindowTitle
 */
void
XFE_ProgressFrame::SetWindowTitle(const char *title)
{
	XP_ASSERT(title);

	XFE_SetDocTitle(fe_WidgetToMWContext(getBaseWidget()), (char*) title);
}


/*
 * XFE_ProgressFrame::SetLine1
 */
void
XFE_ProgressFrame::SetLine1(const char* text)
{
	setAddress((char*) text);
}


/*
 * XFE_ProgressFrame::SetLine2
 */
void
XFE_ProgressFrame::SetLine2(const char* text)
{
	setDestination((char*) text);
}


/*
 * XFE_ProgressFrame::SetProgressText
 */
void
XFE_ProgressFrame::SetProgressText(const char* text)
{
	m_dashboard->setStatusText(text);
}


/*
 * XFE_ProgressFrame::SetProgressRange
 */
void
XFE_ProgressFrame::SetProgressRange(int32 minimum, int32 maximum)
{
	m_progress_bar_min = minimum;
	m_progress_bar_max = maximum;
}


/*
 * XFE_ProgressFrame::SetProgressValue
 */
void
XFE_ProgressFrame::SetProgressValue(int32 value)
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
XFE_ProgressFrame::AssociateWindowWithContext(MWContext *context)
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
    XFE_ProgressFrame* pw;

	if ( context == NULL ) return NULL;

	pw = new XFE_ProgressFrame(CONTEXT_WIDGET(context));

    return pw->Create(context, type);
}


/*
 * pw_SetCancelCallback
 */
void
pw_SetCancelCallback(pw_ptr pw, PW_CancelCallback cancelcb, void* cancelClosure)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetCancelCallback(cancelcb, cancelClosure);
}


/*
 * pw_Show
 */
void
pw_Show(pw_ptr pw)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->show();
}


/*
 * pw_Hide
 */
void
pw_Hide(pw_ptr pw)
{
    if (pw) {
        ((XFE_ProgressFrame *)pw)->hide();
#if 1
		/* 116773: Reopen ThreadFrame after "OK" IMAP Upgrade dialog 
		 * leads to core dump
		 */
		((XFE_ProgressFrame *)pw)->notifyInterested(XFE_Component::logoStopAnimation, NULL);
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
        ((XFE_ProgressFrame *)pw)->Destroy();
}


/*
 * pw_SetWindowTitle
 */
void
pw_SetWindowTitle(pw_ptr pw, const char *title)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetWindowTitle(title);
}


/*
 * pw_SetLine1
 */
void
pw_SetLine1(pw_ptr pw, const char *text)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetLine1(text);
}


/*
 * pw_SetLine2
 */
void
pw_SetLine2(pw_ptr pw, const char *text)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetLine2(text);
}


/*
 * pw_SetProgressText
 */
void
pw_SetProgressText(pw_ptr pw, const char * text)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetProgressText(text);
}


/*
 * pw_SetProgressRange
 */
void
pw_SetProgressRange(pw_ptr pw, int32 minimum, int32 maximum)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetProgressRange(minimum, maximum);
}


/*
 * pw_SetProgressValue
 */
void
pw_SetProgressValue(pw_ptr pw, int32 value)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->SetProgressValue(value);
}

void
fe_pw_AssociateWindowWithContext(MWContext *context,
                                 pw_ptr pw)
{
    if (pw)
        ((XFE_ProgressFrame *)pw)->AssociateWindowWithContext(context);
}

