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
/* 
   NewsPromptDialog.cpp -- dialog for letting the user specify how many articles to download
   Created: Chris Toshok <toshok@netscape.com>, 2-Mar-97.
 */



#include "NewsPromptDialog.h"
#include "prefapi.h"

#include "xp_mem.h"
#include "prprf.h"

#include <Xm/Form.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>

#include "xpgetstr.h"
#include "felocale.h"
extern int XFE_THERE_ARE_N_ARTICLES;

// needed for the post() method.
extern "C" void fe_EventLoop();

XFE_NewsPromptDialog::XFE_NewsPromptDialog(Widget parent,
										   int numMessages)
	: XFE_Dialog(parent, 
				 "NewsDownload",
				 TRUE, // ok
				 TRUE, // cancel
				 FALSE, // help
				 FALSE, // apply
				 TRUE, // separator
				 TRUE // modal
				 )
{
	Widget form;
	XmString str;
	char *buf;
	int32 current_max;

	m_downloadall = False;

	form = XmCreateForm(m_chrome, "downloadForm", NULL, 0);
	
	buf = PR_smprintf(XP_GetString(XFE_THERE_ARE_N_ARTICLES), numMessages);

	str = XmStringCreate(buf, XmFONTLIST_DEFAULT_TAG);

	XP_FREE(buf);

	m_label = XtVaCreateManagedWidget("downloadCaption",
									  xmLabelGadgetClass,
									  form,
									  XmNlabelString, str,
									  XmNleftAttachment, XmATTACH_FORM,
									  XmNrightAttachment, XmATTACH_FORM,
									  XmNtopAttachment, XmATTACH_FORM,
									  XmNbottomAttachment, XmATTACH_NONE,
									  NULL);

	XmStringFree(str);

	m_alltoggle = XtVaCreateManagedWidget("allToggle",
										  xmToggleButtonGadgetClass,
										  form,
										  XmNindicatorType, XmONE_OF_MANY,
										  XmNleftAttachment, XmATTACH_FORM,
										  XmNrightAttachment, XmATTACH_NONE,
										  XmNtopAttachment, XmATTACH_WIDGET,
										  XmNtopWidget, m_label,
										  XmNbottomAttachment, XmATTACH_NONE,
										  NULL);
									  
	m_nummessages_toggle = XtVaCreateManagedWidget("numMessagesToggle",
												   xmToggleButtonGadgetClass,
												   form,
												   XmNindicatorType, XmONE_OF_MANY,
												   XmNleftAttachment, XmATTACH_FORM,
												   XmNrightAttachment, XmATTACH_NONE,
												   XmNtopAttachment, XmATTACH_WIDGET,
												   XmNtopWidget, m_alltoggle,
												   XmNbottomAttachment, XmATTACH_NONE,
												   NULL);

	m_nummessages_text = XtVaCreateManagedWidget("numMessagesText",
												 xmTextFieldWidgetClass,
												 form,
												 XmNleftAttachment, XmATTACH_WIDGET,
												 XmNleftWidget, m_nummessages_toggle,
												 XmNrightAttachment, XmATTACH_NONE,
												 XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
												 XmNtopWidget, m_nummessages_toggle,
												 XmNbottomAttachment, XmATTACH_NONE,
												 NULL);

	m_nummessages_caption = XtVaCreateManagedWidget("numMessagesCaption",
													xmLabelGadgetClass,
													form,
													XmNleftAttachment, XmATTACH_WIDGET,
													XmNleftWidget, m_nummessages_text,
													XmNrightAttachment, XmATTACH_NONE,
													XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
													XmNtopWidget, m_nummessages_text,
													XmNbottomAttachment, XmATTACH_NONE,
													NULL);

	PREF_GetIntPref("news.max_articles", &current_max);

	buf = PR_smprintf("%d", current_max);

	fe_SetTextFieldAndCallBack(m_nummessages_text, buf);

	XP_FREE(buf);

	{
		XP_Bool markothersread;

		PREF_GetBoolPref("news.mark_old_read", &markothersread);

		m_markothersread_toggle = XtVaCreateManagedWidget("markOthersRead",
														  xmToggleButtonGadgetClass,
														  form,
														  XmNset, markothersread,
														  XmNleftAttachment, XmATTACH_FORM,
														  XmNleftOffset, 20,
														  XmNrightAttachment, XmATTACH_NONE,
														  XmNtopAttachment, XmATTACH_WIDGET,
														  XmNtopWidget, m_nummessages_text,
														  XmNbottomAttachment, XmATTACH_NONE,
														  NULL);
	}

	XtAddCallback(m_alltoggle, XmNvalueChangedCallback, toggle_cb, this);
	XtAddCallback(m_nummessages_toggle, XmNvalueChangedCallback, toggle_cb, this);

	XtAddCallback(m_markothersread_toggle, XmNvalueChangedCallback, toggle_markread_cb, this);

	XmToggleButtonGadgetSetState(m_alltoggle, True, True);

	XtManageChild(form);

	XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
	XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
}

XFE_NewsPromptDialog::~XFE_NewsPromptDialog()
{
	// nothing needed (that I'm aware of) yet.
}

XP_Bool
XFE_NewsPromptDialog::post()
{
	m_doneWithLoop = False;
  
	XtVaSetValues(m_chrome,
				  XmNdeleteResponse, XmUNMAP,
				  NULL);

	show();

	while(!m_doneWithLoop)
		{
			fe_EventLoop();
		}

	return m_retVal;
}

XP_Bool
XFE_NewsPromptDialog::getDownloadAll()
{
	return m_downloadall;
}

void
XFE_NewsPromptDialog::ok()
{
	char *num_messages_str;
	int num_messages;

	num_messages_str = fe_GetTextField(m_nummessages_text);

	num_messages = atoi(num_messages_str);

	//	XtFree(num_messages_str);  ??

	if (num_messages)
		{
			PREF_SetIntPref("news.max_articles", num_messages);
		}

	m_doneWithLoop = True;
	m_retVal = True;

	hide();
}

void
XFE_NewsPromptDialog::cancel()
{
	m_doneWithLoop = True;
	m_retVal = False;

	hide();
}

void
XFE_NewsPromptDialog::toggle(Widget w, XtPointer cd)
{
	XmToggleButtonCallbackStruct *cb = (XmToggleButtonCallbackStruct*)cd;

	if (!cb->set)
		{
			XtVaSetValues(w, XmNset, True, 0);
		}
	else if (w == m_alltoggle)
		{
			m_downloadall = True;
			
			XtVaSetValues(m_nummessages_toggle, XmNset, False, 0);
			XtSetSensitive(m_nummessages_text, False);
			XtSetSensitive(m_markothersread_toggle, False);
		}
	else
		{
			m_downloadall = False;

			XtVaSetValues(m_alltoggle, XmNset, False, 0);
			XtSetSensitive(m_nummessages_text, True);
			XtSetSensitive(m_markothersread_toggle, True);
		}
}

void
XFE_NewsPromptDialog::toggle_markread()
{
	PREF_SetBoolPref("news.mark_old_read", XmToggleButtonGadgetGetState(m_markothersread_toggle));
}

void
XFE_NewsPromptDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsPromptDialog *obj = (XFE_NewsPromptDialog*)clientData;

	obj->ok();
}

void
XFE_NewsPromptDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsPromptDialog *obj = (XFE_NewsPromptDialog*)clientData;

	obj->cancel();
}

void
XFE_NewsPromptDialog::toggle_cb(Widget w, XtPointer clientData, XtPointer cd)
{
	XFE_NewsPromptDialog *obj = (XFE_NewsPromptDialog*)clientData;

	obj->toggle(w, cd);
}

void
XFE_NewsPromptDialog::toggle_markread_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_NewsPromptDialog *obj = (XFE_NewsPromptDialog*)clientData;

	obj->toggle_markread();
}

