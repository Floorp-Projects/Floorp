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
  EditHdrDialog.cpp -- add or remove arbitrary mail headers.
  Created: Akkana Peck <akkana@netscape.com>, 19-Nov-97.
 */



#include "EditHdrDialog.h"

#include "xp_str.h"
#include "prefapi.h"
#include "xpgetstr.h"

#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/RowColumn.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleBG.h>
#include <Xm/PushBG.h>

// we need this in the post() method.
extern "C" void fe_EventLoop();

extern "C" char *
XFE_PromptWithCaption(MWContext *context, const char *caption,
		      const char *message, const char *deflt);

extern int XFE_CUSTOM_HEADER;   // for XP_GetString

XFE_EditHdrDialog::XFE_EditHdrDialog(Widget parent, char *name,
                                     MWContext* context)
	: XFE_Dialog(parent, 
                 name, 
                 TRUE, // ok
                 TRUE, // cancel
                 FALSE, // help
                 FALSE, // apply
                 TRUE, // separator
                 TRUE // modal
        )
{
    m_contextData = context;

    Widget form = XtCreateManagedWidget("dialogForm",
                                        xmFormWidgetClass,
                                        m_chrome,
                                        NULL, 0);
    Widget labl = XtVaCreateManagedWidget("editHdrLabel",
                                          xmLabelGadgetClass, form,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          0);
    Widget rc = XtVaCreateManagedWidget("buttonRC",
                                        xmRowColumnWidgetClass, form,
                                        XmNorientation, XmVERTICAL,
                                        XmNtopAttachment, XmATTACH_WIDGET,
                                        XmNtopWidget, labl,
                                        XmNrightAttachment, XmATTACH_FORM,
                                        XmNbottomAttachment, XmATTACH_FORM,
                                        0);

    Arg av[8];
	int ac = 0;
//	XtSetArg(av[ac], XmNvisibleItemCount, 6); ac++;
	XtSetArg(av[ac], XmNselectionPolicy, XmEXTENDED_SELECT); ac++;
	XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
	XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNrightWidget, rc); ac++;
	XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
	XtSetArg(av[ac], XmNtopWidget, labl); ac++;
	XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
	m_list = XmCreateScrolledList(form, "hdrList", av, ac);
	XtManageChild(m_list);
    XtAddCallback(m_list, XmNdefaultActionCallback, butn_cb, this);
    XtAddCallback(m_list, XmNextendedSelectionCallback, select_cb, this);

    Widget butn = XtCreateManagedWidget("new",
                                        xmPushButtonGadgetClass, rc,
                                        NULL, 0);
    XtAddCallback(butn, XmNactivateCallback, butn_cb, this);
    m_editButton = XtCreateManagedWidget("edit",
                                         xmPushButtonGadgetClass, rc,
                                         NULL, 0);
    XtAddCallback(m_editButton, XmNactivateCallback, butn_cb, this);
    m_deleteButton = XtCreateManagedWidget("delete",
                                           xmPushButtonGadgetClass, rc,
                                           NULL, 0);
    XtAddCallback(m_deleteButton, XmNactivateCallback, butn_cb, this);

    // Edit and delete buttons aren't turned on until something is selected:
    XtSetSensitive(m_editButton, FALSE);
    XtSetSensitive(m_deleteButton, FALSE);

    XtAddCallback(m_chrome, XmNokCallback, ok_cb, this);
    XtAddCallback(m_chrome, XmNcancelCallback, cancel_cb, this);
}

XFE_EditHdrDialog::~XFE_EditHdrDialog()
{
    // nothing needed (that I'm aware of) yet.
}

//
// It's not completely clear whether the string returned here should be
// freed or not.  Currently we're not freeing it.
// The code eventually ends up in dialogs.c and it's not clear
// where the string is coming from.
// The only other place I can find where fe_prompt is used
// is in MNView.cpp, which also does not free the result.
//
char *
XFE_EditHdrDialog::promptHeader(char* oldhdr)
{
	char* newhdr = XFE_PromptWithCaption(m_contextData,
                                         "",
                                         XP_GetString(XFE_CUSTOM_HEADER),
                                         oldhdr);
	

    return newhdr;
}

char*
XFE_EditHdrDialog::post()
{
    m_doneWithLoop = False;
  
    XtVaSetValues(m_chrome,
                  XmNdeleteResponse, XmUNMAP,
                  NULL);

    // Get the current list of custom headers and show them in the list:
    XmListDeleteAllItems(m_list);
    int lengthOfHdrPref = 0;
    PREF_GetCharPref("mailnews.customHeaders", 0, &lengthOfHdrPref);
    if (lengthOfHdrPref > 0)
    {
        char* buf = new char [lengthOfHdrPref];
        PREF_GetCharPref("mailnews.customHeaders", buf, &lengthOfHdrPref);
        if (lengthOfHdrPref > 0)
        {
            char* sep = " ";
            char* placeholder = 0;
            char* next;
            char* strtokbuf = buf;
            while (next = XP_STRTOK_R(strtokbuf, sep, &placeholder))
            {
                strtokbuf = 0;		// only want it for first XP_STRTOK_R call
                while (isspace(*next))
                    ++next;
                XmString item = XmStringCreateSimple(next);
                XmListAddItem(m_list, item, 0);
                XmStringFree(item);
            }
        }
    }

    show();
    while(!m_doneWithLoop)
        fe_EventLoop();

    if (!m_retVal)
        return 0;

    // User hit OK, so return the currently selected header:
    int* posList = 0;
    int posCount = 0;
    XmListGetSelectedPos(m_list, &posList, &posCount);
    if (!posCount)
        return 0;

    int itemCount;
    XmString* items;
    char* hdr;
    XtVaGetValues(m_list,
                  XmNitemCount, &itemCount,
                  XmNitems, &items,
                  0);

    if (posList[0]-1 >= itemCount)
        return 0;

    XmStringGetLtoR(items[posList[0]-1], XmFONTLIST_DEFAULT_TAG, &hdr);

    if (posList)
        XtFree((char*)posList);

    return hdr;     // yes, this return value should be freed by the caller
}

void
XFE_EditHdrDialog::selection(XmListCallbackStruct* cbs)
{
    if (cbs->selected_item_count > 0)
        XtSetSensitive(m_deleteButton, TRUE);
    else
        XtSetSensitive(m_deleteButton, FALSE);
    if (cbs->selected_item_count == 1)
        XtSetSensitive(m_editButton, TRUE);
    else
        XtSetSensitive(m_editButton, FALSE);
}

//
// Handle new/edit/delete buttons and default action callback.
// w is the widget (button or list) which caused the callback.
//
void
XFE_EditHdrDialog::button(Widget w)
{
    if (*(XtName(w)) == 'n')         // New button
    {
        // bring up an empty new-header dialog
        char* newhdr = promptHeader(0);
        if (newhdr)
        {
            XmListDeselectAllItems(m_list);
            XmString xmhdr = XmStringCreateSimple(newhdr);
            XmListAddItem(m_list, xmhdr, 0);
            XmListSelectPos(m_list, 0, FALSE);
            XmStringFree(xmhdr);

            // This should leave the new header selected, so turn on buttons:
            XtSetSensitive(m_editButton, TRUE);
            XtSetSensitive(m_deleteButton, TRUE);
        }

        return;
    }

    int* posList = 0;
    int posCount = 0;
    XmListGetSelectedPos(m_list, &posList, &posCount);
    if (posCount <= 0 || posList == 0)
    {
        if (posList)
            XtFree((char*)posList);
        return;
    }

    if (w == m_deleteButton)
    {
        XmListDeletePositions(m_list, posList, posCount);

        // Now nothing is selected, so turn off edit/delete buttons:
        XtSetSensitive(m_editButton, FALSE);
        XtSetSensitive(m_deleteButton, FALSE);
    }
    else if (w == m_list || w == m_editButton)
    {
        int whichitem = posList[0] - 1;
        int itemCount;
        XmString* items;
        char* hdr;
        XtVaGetValues(m_list,
                      XmNitemCount, &itemCount,
                      XmNitems, &items,
                      0);
        XmStringGetLtoR(items[whichitem], XmFONTLIST_DEFAULT_TAG, &hdr);
        if (hdr)
        {
            char* newhdr = promptHeader(hdr);
            if (newhdr)
            {
                XmString xmhdr = XmStringCreateSimple(newhdr);
                XmListReplaceItemsPos(m_list, &xmhdr, 1, posList[0]);
                XmStringFree(xmhdr);
            }
            XtFree(hdr);
        }
    }

    XtFree((char*)posList);
}

void
XFE_EditHdrDialog::ok()
{
    m_doneWithLoop = True;
    m_retVal = True;

    // Save the current set of headers in the pref "mailnews.customHeaders":
    XmString* items;
    int itemCount;
    XtVaGetValues(m_list,
                  XmNitemCount, &itemCount,
                  XmNitems, &items,
                  0);

    // first, figure out how many chars we need to allocate:
    int totalLength = 0;
    int i;
    for (i=0; i < itemCount; ++i)
    {
        char* next;
        XmStringGetLtoR(items[i], XmFONTLIST_DEFAULT_TAG, &next);
        if (next)
        {
            totalLength += XP_STRLEN(next) + 1;
            XtFree(next);
        }
    }

    char* buf = new char [totalLength];
    buf[0] = '\0';
    for (i=0; i < itemCount; ++i)
    {
        char* next;
        XmStringGetLtoR(items[i], XmFONTLIST_DEFAULT_TAG, &next);
        if (next)
        {
            if (i > 0)
                XP_STRCAT(buf, " ");
            XP_STRCAT(buf, next);
            XtFree(next);
        }
    }

    PREF_SetCharPref("mailnews.customHeaders", buf);
			
    hide();
}

void
XFE_EditHdrDialog::cancel()
{
    m_doneWithLoop = True;
    m_retVal = False;

    hide();
}

void 
XFE_EditHdrDialog::ok_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_EditHdrDialog *obj = (XFE_EditHdrDialog*)clientData;

	obj->ok();
}

void
XFE_EditHdrDialog::cancel_cb(Widget, XtPointer clientData, XtPointer)
{
	XFE_EditHdrDialog *obj = (XFE_EditHdrDialog*)clientData;

	obj->cancel();
}

void
XFE_EditHdrDialog::butn_cb(Widget w, XtPointer clientData, XtPointer)
{
	XFE_EditHdrDialog *obj = (XFE_EditHdrDialog*)clientData;

	obj->button(w);
}

void
XFE_EditHdrDialog::select_cb(Widget, XtPointer clientData, XtPointer callData)
{
	XFE_EditHdrDialog *obj = (XFE_EditHdrDialog*)clientData;

	obj->selection((XmListCallbackStruct *)callData);
}

