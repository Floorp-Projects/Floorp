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
   PrefsProxiesView.cpp -- The view dialog for proxies preference
   Created: Linda Wei <lwei@netscape.com>, 24-Oct-96.
 */



#include "felocale.h"
#include "structs.h"
#include "fonts.h"
#include "xpassert.h"
#include "xfe.h"
#include "prprf.h"
extern "C" {
#include "prnetdb.h"
}
#include "PrefsDialog.h"
#include "PrefsProxiesView.h"
#include "prefapi.h"

#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/PushB.h>
#include <Xm/PushBG.h>
#include <Xm/Form.h>
#include <Xm/TextF.h> 
#include <Xm/ToggleBG.h> 
#include <Xfe/Xfe.h>

extern int XFE_WARNING;
extern int XFE_HOST_IS_UNKNOWN;
extern int XFE_FTP_PROXY_HOST;
extern int XFE_NO_PORT_NUMBER;
extern int XFE_GOPHER_PROXY_HOST;
extern int XFE_HTTP_PROXY_HOST;
extern int XFE_HTTPS_PROXY_HOST;
extern int XFE_WAIS_PROXY_HOST;
extern int XFE_SOCKS_HOST;
extern int XFE_ERROR_SAVING_OPTIONS;

extern "C"
{
	char     *XP_GetString(int i);
	void      fe_installProxiesView();
}

// ==================== Public Member Functions ====================

// Member:       XFE_PrefsProxiesViewDialog
// Description:  Constructor
// Inputs:
// Side effects: Creates the View dialog for Proxies Preferences

XFE_PrefsProxiesViewDialog::XFE_PrefsProxiesViewDialog(XFE_PrefsDialog *prefsDialog, // prefs dialog
													   Widget     parent,      // dialog parent
													   char      *name,        // dialog name
													   Boolean    modal)       // modal dialog?
	: XFE_Dialog(parent, 
				 name,
				 TRUE,     // ok
				 TRUE,     // cancel
				 FALSE,    // help
				 FALSE,    // apply
				 FALSE,    // separator
				 modal     // modal
				 ),
	  m_prefsDialog(prefsDialog),
	  m_prefsDataProxiesView(0)
{
	PrefsDataProxiesView *fep = NULL;

	fep = new PrefsDataProxiesView;
	memset(fep, 0, sizeof(PrefsDataProxiesView));
	m_prefsDataProxiesView = fep;
	
	Widget     form;
	Widget     kids[100];
	Arg        av[50];
	int        ac;
	int        i;

	form = XtVaCreateWidget("form", xmFormWidgetClass, m_chrome,
							XmNmarginWidth, 8,
							XmNtopAttachment, XmATTACH_FORM,
							XmNleftAttachment, XmATTACH_FORM,
							XmNrightAttachment, XmATTACH_FORM,
							XmNbottomAttachment, XmATTACH_FORM,
							NULL);
	XtManageChild (form);

	Widget     ftp_proxy_label;
	Widget     ftp_proxy_text;
	Widget     ftp_port_label;
	Widget     ftp_port_text;
	Widget     gopher_proxy_label;
	Widget     gopher_proxy_text;
	Widget     gopher_port_label;
	Widget     gopher_port_text;
	Widget     http_proxy_label;
	Widget     http_proxy_text;
	Widget     http_port_label;
	Widget     http_port_text;
	Widget     https_proxy_label;
	Widget     https_proxy_text;
	Widget     https_port_label;
	Widget     https_port_text;
	Widget     wais_proxy_label;
	Widget     wais_proxy_text;
	Widget     wais_port_label;
	Widget     wais_port_text;
	Widget     no_proxy_label;
	Widget     no_proxy_text;
	Widget     socks_host_label;
	Widget     socks_host_text;
	Widget     socks_port_label;
	Widget     socks_port_text;
	Widget     label1;
	Widget     label2;

	ac = 0;
	i = 0;

	kids[i++] = label1 = 
		XmCreateLabelGadget(form, "proxyViewLabel1", av, ac);
	kids[i++] = label2 = 
		XmCreateLabelGadget(form, "proxyViewLabel2", av, ac);

	kids[i++] = ftp_proxy_label = 
		XmCreateLabelGadget(form, "ftpProxyLabel", av, ac);
	kids[i++] = gopher_proxy_label = 
		XmCreateLabelGadget(form, "gopherProxyLabel", av, ac);
	kids[i++] = http_proxy_label = 
		XmCreateLabelGadget(form, "httpProxyLabel", av, ac);
	kids[i++] = https_proxy_label = 
		XmCreateLabelGadget(form, "httpsProxyLabel", av, ac);
	kids[i++] = wais_proxy_label = 
		XmCreateLabelGadget(form, "waisProxyLabel", av, ac);
	kids[i++] = no_proxy_label = 
		XmCreateLabelGadget(form, "noProxyLabel", av, ac);
	kids[i++] = socks_host_label = 
		XmCreateLabelGadget(form, "socksHostLabel", av, ac);

	kids[i++] = ftp_port_label = 
		XmCreateLabelGadget(form, "ftpPortLabel", av, ac);
	kids[i++] = gopher_port_label = 
		XmCreateLabelGadget(form, "gopherPortLabel", av, ac);
	kids[i++] = http_port_label = 
		XmCreateLabelGadget(form, "httpPortLabel", av, ac);
	kids[i++] = https_port_label = 
		XmCreateLabelGadget(form, "httpsPortLabel", av, ac);
	kids[i++] = wais_port_label = 
		XmCreateLabelGadget(form, "waisPortLabel", av, ac);
	kids[i++] = socks_port_label = 
		XmCreateLabelGadget(form, "socksPortLabel", av, ac);

	kids[i++] = ftp_proxy_text = 
		fe_CreateTextField(form, "ftpProxyText", av, ac);
	kids[i++] = gopher_proxy_text = 
		fe_CreateTextField(form, "gopherProxyText", av, ac);
	kids[i++] = http_proxy_text = 
		fe_CreateTextField(form, "httpProxyText", av, ac);
	kids[i++] = https_proxy_text = 
		fe_CreateTextField(form, "httpsProxyText", av, ac);
	kids[i++] = wais_proxy_text = 
		fe_CreateTextField(form, "waisProxyText", av, ac);
	kids[i++] = no_proxy_text = 
		fe_CreateTextField(form, "noProxyText", av, ac);
	kids[i++] = socks_host_text = 
		fe_CreateTextField(form, "socksHostText", av, ac);

	kids[i++] = ftp_port_text = 
		fe_CreateTextField(form, "ftpPortText", av, ac);
	kids[i++] = gopher_port_text = 
		fe_CreateTextField(form, "gopherPortText", av, ac);
	kids[i++] = http_port_text = 
		fe_CreateTextField(form, "httpPortText", av, ac);
	kids[i++] = https_port_text = 
		fe_CreateTextField(form, "httpsPortText", av, ac);
	kids[i++] = wais_port_text = 
		fe_CreateTextField(form, "waisPortText", av, ac);
	kids[i++] = socks_port_text = 
		fe_CreateTextField(form, "socksPortText", av, ac);

	
	int labels_width;
	labels_width = XfeVaGetWidestWidget(ftp_proxy_label,
										gopher_proxy_label,
										http_proxy_label,
										https_proxy_label,
										wais_proxy_label,
										no_proxy_label,
										socks_host_label,
										NULL);
	Dimension margin_width;
	XtVaGetValues(form, 
				  XmNmarginWidth, &margin_width,
				  NULL);
	labels_width+= margin_width;

	int labels_height;
	labels_height = XfeVaGetTallestWidget(ftp_proxy_label,
										  ftp_proxy_text,
										  NULL);

	XtVaSetValues(label1,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(ftp_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(ftp_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, label1,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(gopher_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(gopher_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, ftp_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(http_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(http_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, gopher_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(https_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(https_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, http_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(wais_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(wais_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, https_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(label2,
				  XmNalignment, XmALIGNMENT_BEGINNING,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, wais_proxy_label,
				  XmNtopOffset, 8,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNbottomAttachment, XmATTACH_NONE,
				  XmNrightAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(no_proxy_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(no_proxy_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, label2,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(socks_host_label,
				  XmNheight, labels_height,
				  RIGHT_JUSTIFY_VA_ARGS(socks_host_label,labels_width),
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, no_proxy_label,
				  XmNtopOffset, 8,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(ftp_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, ftp_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(gopher_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, gopher_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(http_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, http_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(https_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, https_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(wais_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wais_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(no_proxy_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, no_proxy_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(socks_host_text,
				  XmNcolumns, 20,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNleftOffset, labels_width,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, socks_host_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(ftp_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, ftp_proxy_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, ftp_proxy_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(gopher_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, gopher_proxy_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, gopher_proxy_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(http_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, http_proxy_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, http_proxy_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(https_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, https_proxy_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, https_proxy_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(wais_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wais_proxy_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wais_proxy_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(socks_port_label,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, socks_host_text,
				  XmNleftOffset, 10,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, socks_host_text,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(ftp_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, ftp_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, ftp_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(gopher_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, gopher_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, gopher_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(http_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, http_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, http_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(https_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, https_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, https_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(wais_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, wais_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, wais_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);
	XtVaSetValues(socks_port_text,
				  XmNcolumns, 5,
				  XmNheight, labels_height,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, socks_port_label,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, socks_port_label,
				  XmNbottomAttachment, XmATTACH_NONE,
				  NULL);

	if (fe_globalData.nonterminal_text_translations) {
		XtOverrideTranslations (ftp_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (ftp_port_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (gopher_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (gopher_port_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (http_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (http_port_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (https_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (https_port_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (wais_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (wais_port_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (no_proxy_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (socks_host_text,
								fe_globalData.nonterminal_text_translations);
		XtOverrideTranslations (socks_port_text,
								fe_globalData.nonterminal_text_translations);
    }

	fep->ftp_proxy_text = ftp_proxy_text;
	fep->ftp_port_text = ftp_port_text;
	fep->gopher_proxy_text = gopher_proxy_text;
	fep->gopher_port_text = gopher_port_text;
	fep->http_proxy_text = http_proxy_text;
	fep->http_port_text = http_port_text;
	fep->https_proxy_text = https_proxy_text;
	fep->https_port_text = https_port_text;
	fep->wais_proxy_text = wais_proxy_text;
	fep->wais_port_text = wais_port_text;
	fep->no_proxy_text = no_proxy_text;
	fep->socks_host_text = socks_host_text;
	fep->socks_port_text = socks_port_text;

	// Add callbacks

	XtAddCallback(m_chrome, XmNokCallback, prefsProxiesViewCb_ok, this);
	XtAddCallback(m_chrome, XmNcancelCallback, prefsProxiesViewCb_cancel, this);

	XtManageChildren(kids, i);
}

// Member:       ~XFE_PrefsProxiesViewDialog
// Description:  Destructor
// Inputs:
// Side effects: 

XFE_PrefsProxiesViewDialog::~XFE_PrefsProxiesViewDialog()
{
	// Clean up

	delete m_prefsDataProxiesView;
}

// Member:       show
// Description:  Pop up dialog
// Inputs:
// Side effects: 

void XFE_PrefsProxiesViewDialog::show()
{
	// TODO: Initialize the dialog

	// Manage the top level

	XFE_Dialog::show();

	// Set focus to the OK button

	XmProcessTraversal(m_okButton, XmTRAVERSE_CURRENT);
}

// Member:       initPage
// Description:  Initializes page for GeneralProxiesView
// Inputs:
// Side effects: 

void XFE_PrefsProxiesViewDialog::initPage()
{
	XP_ASSERT(m_prefsDataProxiesView);

	PrefsDataProxiesView  *fep = m_prefsDataProxiesView;
	XFE_GlobalPrefs       *prefs = &fe_globalPrefs;
    char buf[1024];
    char* host;
    int port;

#define FROB(SLOT,SUFFIX,DEF)                               \
    host = prefs->SLOT##_##SUFFIX;                          \
    port = prefs->SLOT##_##SUFFIX##_port;                   \
    if ( port == 0 ) {                                      \
        strcpy(buf, DEF);                                   \
    } else {                                                \
	    sprintf(buf, "%d", port);                           \
    }                                                       \
	fe_SetTextField(fep->SLOT##_##SUFFIX##_text, host); \
	fe_SetTextField(fep->SLOT##_port_text, buf)

	FROB(ftp,      proxy, "");
    FROB(gopher,   proxy, "");
    FROB(http,     proxy, "");
#ifndef NO_SECURITY
    FROB(https,    proxy, "");
#endif
	FROB(wais,     proxy, "");
	FROB(socks,    host,  "1080");
#undef FROB
	
	XtSetSensitive(fep->ftp_proxy_text, !PREF_PrefIsLocked("network.proxy.ftp"));
	XtSetSensitive(fep->ftp_port_text, !PREF_PrefIsLocked("network.proxy.ftp_port"));
	XtSetSensitive(fep->gopher_proxy_text, !PREF_PrefIsLocked("network.proxy.gopher"));
	XtSetSensitive(fep->gopher_port_text, !PREF_PrefIsLocked("network.proxy.gopher_port"));
	XtSetSensitive(fep->http_proxy_text, !PREF_PrefIsLocked("network.proxy.http"));
	XtSetSensitive(fep->http_port_text, !PREF_PrefIsLocked("network.proxy.http_port"));
	XtSetSensitive(fep->https_proxy_text, !PREF_PrefIsLocked("network.proxy.ssl"));
	XtSetSensitive(fep->https_port_text, !PREF_PrefIsLocked("network.proxy.ssl_port"));
	XtSetSensitive(fep->wais_proxy_text, !PREF_PrefIsLocked("network.proxy.wais"));
	XtSetSensitive(fep->wais_port_text, !PREF_PrefIsLocked("network.proxy.wais_port"));
	XtSetSensitive(fep->no_proxy_text, !PREF_PrefIsLocked("network.proxy.no_proxies_on"));
	XtSetSensitive(fep->socks_host_text, !PREF_PrefIsLocked("network.hosts.socks_server"));
	XtSetSensitive(fep->socks_port_text, !PREF_PrefIsLocked("network.hosts.socks_serverport"));

	fe_SetTextField(fep->no_proxy_text, prefs->no_proxy);
}

// Member:       verifyPage
// Description:  verify page for GeneralProxiesView
// Inputs:
// Side effects: 

Boolean XFE_PrefsProxiesViewDialog::verifyPage()
{
	char         buf[10000];
	char        *buf2;
	char        *warning;
	int          size;

	buf2 = buf;
	strcpy (buf, XP_GetString(XFE_WARNING));
	buf2 = buf + strlen (buf);
	warning = buf2;
	size = buf + sizeof (buf) - warning;

	XP_ASSERT(m_prefsDataProxiesView);
	PrefsDataProxiesView  *fep = m_prefsDataProxiesView;

	PREFS_CHECK_PROXY (fep->ftp_proxy_text,   fep->ftp_port_text,
					   XP_GetString(XFE_FTP_PROXY_HOST),  
					   True, warning, size);
	PREFS_CHECK_PROXY (fep->gopher_proxy_text,fep->gopher_port_text,
					   XP_GetString(XFE_GOPHER_PROXY_HOST),
					   True, warning, size);
	PREFS_CHECK_PROXY (fep->http_proxy_text,  fep->http_port_text,
					   XP_GetString(XFE_HTTP_PROXY_HOST),  
					   True, warning, size);
	PREFS_CHECK_PROXY (fep->https_proxy_text, fep->https_port_text,
					   XP_GetString(XFE_HTTPS_PROXY_HOST), 
					   True, warning, size);
	PREFS_CHECK_PROXY (fep->wais_proxy_text,   fep->wais_port_text,
					   XP_GetString(XFE_WAIS_PROXY_HOST),  
					   True, warning, size);
	PREFS_CHECK_PROXY (fep->socks_host_text,  fep->socks_port_text,
					   XP_GetString(XFE_SOCKS_HOST),     
					   False, warning, size);

	if (*buf2) {
		FE_Alert (m_prefsDialog->getContext(), fe_StringTrim (buf));
		return FALSE;
	}
	else {
		return TRUE;
	}
}

// Member:       install changes
// Description:  install changes for GeneralProxiesView
// Inputs:
// Side effects: 

void XFE_PrefsProxiesViewDialog::installChanges()
{
	fe_installProxiesView();
}

// Member:       getContext
// Description:  returns context
// Inputs:
// Side effects: 

MWContext *XFE_PrefsProxiesViewDialog::getContext()
{
	return (m_prefsDialog->getContext());
}

// ==================== Friend Functions ====================

// Friend:       prefsProxiesViewCb_ok
// Description:  
// Inputs:
// Side effects: 

void prefsProxiesViewCb_ok(Widget    w,
						   XtPointer closure,
						   XtPointer callData)
{
	XFE_PrefsProxiesViewDialog *theDialog = (XFE_PrefsProxiesViewDialog *)closure;
	PrefsDataProxiesView       *fep = theDialog->m_prefsDataProxiesView;

	XP_ASSERT(fep);
	if (! theDialog->verifyPage()) return;

	char *s1;
	char *s2;

	PREFS_SET_GLOBALPREF_TEXT(no_proxy, no_proxy_text);

#define SNARFP(NAME,SUFFIX,empty_port_ok) \
{ \
    s1 = fe_GetTextField(fep->NAME##_##SUFFIX##_text); \
	s2 = fe_GetTextField(fep->NAME##_port_text); \
    if (*s1 && (*s2 || empty_port_ok)) { \
        fe_globalPrefs.NAME##_##SUFFIX = strdup(s1); \
        fe_globalPrefs.NAME##_##SUFFIX##_port = *s2 ? atoi(s2) : 0; \
    } else { \
        fe_globalPrefs.NAME##_##SUFFIX = strdup(""); \
        fe_globalPrefs.NAME##_##SUFFIX##_port = 0; \
    } \
	if (s1) XtFree(s1); \
	if (s2) XtFree(s2); \
}

    SNARFP (ftp,    proxy, False);
	SNARFP (gopher, proxy, False);
	SNARFP (http,   proxy, False);
#ifndef NO_SECURITY
	SNARFP (https,  proxy, False);
#endif
	SNARFP (wais,   proxy, False);
	SNARFP (socks,  host,  True);
#undef SNARFP

	// Install changes

	theDialog->installChanges();

	// Simulate a cancel

	prefsProxiesViewCb_cancel(w, closure, callData);

	// Save the preferences at the end, so that if we've found some setting that
	// crashes, it won't get saved...
	
	if (!fe_CheckVersionAndSavePrefs((char *) fe_globalData.user_prefs_file, &fe_globalPrefs))
		fe_perror (theDialog->getContext(), XP_GetString( XFE_ERROR_SAVING_OPTIONS));

}

// Friend:       prefsProxiesViewCb_cancel
// Description:  
// Inputs:
// Side effects: 

void prefsProxiesViewCb_cancel(Widget    /* w */,
							   XtPointer closure,
							   XtPointer /* callData */)
{
	XFE_PrefsProxiesViewDialog *theDialog = (XFE_PrefsProxiesViewDialog *)closure;

	// Delete the dialog

	delete theDialog;
}

