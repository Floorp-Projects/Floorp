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
 * Corporation.  Portions created by Netscape are Copyright (C) 1996
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*
   PrefsPageMessages.cpp - Messages preference pane
   Created: Alec Flett <alecf@netscape.com>
 */

#include "felocale.h"
#include "prefapi.h"
#include "PrefsPageMessages.h"
#include "xpgetstr.h"
#include <Xfe/Xfe.h>
#include <DtWidgets/ComboBox.h>

#define SPELLCHECK "mail.SpellCheckBeforeSend"
#define FORWARD "mail.forward_message_mode"

#define AUTOQUOTE "mail.auto_quote"
#define AUTOQUOTE_STYLE "mailnews.reply_on_top"

#define WRAP_LENGTH "mailnews.wraplength"
#define WRAP_LONG_LINES "mail.wrap_long_lines"

#define EIGHTBIT_MIME "mail.strictly_mime"


// string resources
extern int XFE_AUTOQUOTE_STYLE_ABOVE;
extern int XFE_AUTOQUOTE_STYLE_BELOW;
extern int XFE_AUTOQUOTE_STYLE_SELECT;

extern int XFE_FORWARD_INLINE;
extern int XFE_FORWARD_QUOTED;
extern int XFE_FORWARD_ATTACH;

XFE_PrefsPageMessages::XFE_PrefsPageMessages(XFE_PrefsDialog *dialog)
    : XFE_PrefsPage(dialog)
{
    m_autoquote_strings[AUTOQUOTE_PREF_ABOVE] =
        XmStringCreateLocalized(XP_GetString(XFE_AUTOQUOTE_STYLE_ABOVE));
    m_autoquote_strings[AUTOQUOTE_PREF_BELOW] =
        XmStringCreateLocalized(XP_GetString(XFE_AUTOQUOTE_STYLE_BELOW));
    m_autoquote_strings[AUTOQUOTE_PREF_SELECT] =
        XmStringCreateLocalized(XP_GetString(XFE_AUTOQUOTE_STYLE_SELECT));
    m_autoquote_strings[AUTOQUOTE_ITEMS] = NULL;

    m_forward_strings[FORWARD_PREF_INLINE] =
        XmStringCreateLocalized(XP_GetString(XFE_FORWARD_INLINE));
    m_forward_strings[FORWARD_PREF_QUOTED] =
        XmStringCreateLocalized(XP_GetString(XFE_FORWARD_QUOTED));
    m_forward_strings[FORWARD_PREF_ATTACH] =
        XmStringCreateLocalized(XP_GetString(XFE_FORWARD_ATTACH));
    m_forward_strings[FORWARD_ITEMS] = NULL;
    
}

XFE_PrefsPageMessages::~XFE_PrefsPageMessages()
{
    XmStringFree(m_autoquote_strings[AUTOQUOTE_PREF_ABOVE]);
    XmStringFree(m_autoquote_strings[AUTOQUOTE_PREF_BELOW]);
    XmStringFree(m_autoquote_strings[AUTOQUOTE_PREF_SELECT]);

    XmStringFree(m_forward_strings[FORWARD_PREF_QUOTED]);
    XmStringFree(m_forward_strings[FORWARD_PREF_INLINE]);
    XmStringFree(m_forward_strings[FORWARD_PREF_ATTACH]);
}


void XFE_PrefsPageMessages::create()
{
    Arg av[5];
    int ac;
    
    Widget form;

    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;

    m_wPage = form =
        XmCreateForm(m_wPageForm, "mailnewsMessages", av,ac);
    XtManageChild(form);

    Widget reply_frame=createReplyFrame(form, NULL);
    Widget spell_frame=createSpellFrame(form, reply_frame);
    Widget wrap_frame =createWrapFrame (form, spell_frame);
    Widget eightbit_frame=createEightBitFrame(form, wrap_frame);

    setCreated(TRUE);
}

Widget XFE_PrefsPageMessages::createReplyFrame (Widget parent,
                                                Widget attachTo)
{
    Widget kids[7];
    int i=0;
    Arg av[10];
    int ac;
    Widget frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    frame =
        XmCreateFrame(parent,"replyFrame",av,ac);
  
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form =
        XmCreateForm(frame,"replyForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "replyLabel",av,ac);

    // create the widgets
    i=0;

    // we need this crap because XmLists are stupid
    Visual *v=0;
    Colormap cmap=0;
    Cardinal depth=0;
    XtVaGetValues(getPrefsDialog()->getPrefsParent(),
                  XtNvisual, &v,
                  XtNcolormap, &cmap,
                  XtNdepth, &depth,
                  0);
    ac=0;
    XtSetArg(av[ac], XtNvisual, v); ac++;
    XtSetArg(av[ac], XtNcolormap, cmap); ac++;
    XtSetArg(av[ac], XtNdepth, depth); ac++;
    XtSetArg(av[ac], XmNarrowType, XmMOTIF); ac++;

    Widget combo_label;
    Widget forward_label;

    kids[i++] = forward_label =
        XmCreateLabelGadget(form, "forwardLabel", NULL,0);
    kids[i++] = m_forward_combo =
        DtCreateComboBox(form, "forwardCombo", av, ac);

    
    kids[i++] = m_autoquote_toggle =
        XmCreateToggleButtonGadget(form, "autoquoteToggle", NULL, 0);
    
    kids[i++] = combo_label =
        XmCreateLabelGadget(form, "autoquoteMyReplyLabel", NULL, 0);
    
    // av holds the visual/colormap/depth stuff
    kids[i++] = m_autoquote_style_combo =
        DtCreateComboBox(form,"autoquoteStyleCombo",av,ac);

    
    // lay them out
    int max_height1 = XfeVaGetTallestWidget(forward_label,
                                            m_forward_combo,
                                            NULL);
    
    int max_height3 = XfeVaGetTallestWidget(combo_label,
                                            m_autoquote_style_combo,
                                            NULL);
    // 1st row
    XtVaSetValues(forward_label,
                  XmNheight, max_height1,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  NULL);

    XtVaSetValues(m_forward_combo,
                  XmNheight, max_height1,
                  XmNitems, m_forward_strings,
                  XmNitemCount, FORWARD_ITEMS,
                  XmNvisibleItemCount, FORWARD_ITEMS,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, forward_label,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, forward_label,
                  NULL);
    
    // 2nd row
    XtVaSetValues(m_autoquote_toggle,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, forward_label,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    // 3rd row
    XtVaSetValues(combo_label,
                  XmNheight, max_height3,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_autoquote_toggle,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    XtVaSetValues(m_autoquote_style_combo,
                  XmNheight, max_height3,
                  XmNitems, m_autoquote_strings,
                  XmNitemCount, AUTOQUOTE_ITEMS,
                  XmNvisibleItemCount, AUTOQUOTE_ITEMS,
                  XmNheight, max_height3,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, combo_label,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, combo_label,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    
    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);

    return frame;
}

Widget XFE_PrefsPageMessages::createSpellFrame(Widget parent,
                                               Widget attachTo)
{
    Widget kids[2];
    int i=0;
    Arg av[10];
    int ac;

    Widget frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    frame =
        XmCreateFrame(parent,"spellFrame",av,ac);
  
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form =
        XmCreateForm(frame,"spellForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "spellLabel",av,ac);

    kids[i++] = m_spellcheck_toggle =
        XmCreateToggleButtonGadget(form, "spellToggle", NULL, 0);

    XtVaSetValues(m_spellcheck_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);

    return frame;
}

Widget XFE_PrefsPageMessages::createWrapFrame (Widget parent,
                                               Widget attachTo)
{
    Widget kids[10];
    int i=0;
    Arg av[10];
    int ac;

    Widget frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    frame =
        XmCreateFrame(parent,"wrapFrame",av,ac);
  
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form =
        XmCreateForm(frame,"wrapForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "wrapLabel",av,ac);

    Widget wrap_before_label;
    Widget wrap_after_label;
    
    kids[i++] = m_wrap_toggle =
        XmCreateToggleButtonGadget(form, "wrapToggle", NULL, 0);

    kids[i++] = wrap_before_label =
        XmCreateLabelGadget(form, "wrapLengthLabel", NULL, 0);
    kids[i++] = m_wrap_length_text =
        XmCreateTextField(form, "wrapLengthText", NULL, 0);
    kids[i++] = wrap_after_label =
        XmCreateLabelGadget(form, "wrapLengthAfterLabel", NULL, 0);

    int max_height2 = XfeVaGetTallestWidget(wrap_before_label,
                                            m_wrap_length_text,
                                            wrap_after_label,
                                            NULL);
    XtVaSetValues(m_wrap_toggle,
                  XmNtopAttachment, XmATTACH_FORM,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtVaSetValues(wrap_before_label,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_WIDGET,
                  XmNtopWidget, m_wrap_toggle,
                  XmNleftAttachment, XmATTACH_FORM,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtVaSetValues(m_wrap_length_text,
                  XmNheight, max_height2,
                  XmNcolumns, 3,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, XmATTACH_WIDGET,
                  XmNtopWidget, wrap_before_label,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, wrap_before_label,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);
    
    XtVaSetValues(wrap_after_label,
                  XmNheight, max_height2,
                  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
                  XmNtopWidget, XmATTACH_WIDGET,
                  XmNtopWidget, m_wrap_length_text,
                  XmNleftAttachment, XmATTACH_WIDGET,
                  XmNleftWidget, m_wrap_length_text,
                  XmNrightAttachment, XmATTACH_NONE,
                  XmNbottomAttachment, XmATTACH_NONE,
                  NULL);

    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);
    
    return frame;
}

Widget XFE_PrefsPageMessages::createEightBitFrame (Widget parent,
                                                   Widget attachTo)
{
    Widget kids[3];
    int i=0;
    Arg av[10];
    int ac;

    Widget frame;
    ac=0;
    if (attachTo==NULL) {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    }
    else {
        XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
        XtSetArg(av[ac], XmNtopWidget, attachTo); ac++;
    }
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    frame =
        XmCreateFrame(parent,"eightbitFrame",av,ac);
  
    Widget form;
    ac=0;
    XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
    XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
    form =
        XmCreateRadioBox(frame,"eightbitForm",av,ac);

    Widget label;
    ac=0;
    XtSetArg(av[ac], XmNchildType, XmFRAME_TITLE_CHILD); ac++;
    label =
        XmCreateLabelGadget(frame, "eightbitLabel",av,ac);

    kids[i++] = m_eightbit_asis_toggle =
        XmCreateToggleButtonGadget(form, "eightbitAsIsToggle", NULL, 0);
    kids[i++] = m_eightbit_quoted_toggle =
        XmCreateToggleButtonGadget(form, "eightbitQuotedToggle", NULL, 0);

    XtManageChildren(kids,i);
    XtManageChild(form);
    XtManageChild(label);
    XtManageChild(frame);
    return frame;
}


void XFE_PrefsPageMessages::init()
{
    XP_Bool boolval;
    XP_Bool locked;
    int32 intval;
    char charval[10];           // this is only for the 2 or 3 character
                                // intvals - remember to enlarge if you
                                // need more

    PREF_GetIntPref(FORWARD, &intval);
    locked=PREF_PrefIsLocked(FORWARD);
    XtVaSetValues(m_forward_combo,
                  XmNsensitive, !locked,
                  NULL);

    XP_ASSERT(intval<FORWARD_ITEMS);
    if (intval<FORWARD_ITEMS)
        DtComboBoxSelectItem(m_forward_combo,
                             m_forward_strings[intval]);
    
    // autoquote?
    PREF_GetBoolPref(AUTOQUOTE, &boolval);
    locked=PREF_PrefIsLocked(AUTOQUOTE);
    XtVaSetValues(m_autoquote_toggle,
                  XmNset, boolval,
                  XmNsensitive,!locked,
                  NULL);

    // where to put quoted text?
    PREF_GetIntPref(AUTOQUOTE_STYLE, &intval);
    locked=PREF_PrefIsLocked(AUTOQUOTE_STYLE);
    XtVaSetValues(m_autoquote_style_combo,
                  XmNsensitive, !locked,
                  NULL);

    XP_ASSERT(intval<AUTOQUOTE_ITEMS);
    if (intval<AUTOQUOTE_ITEMS) 
        DtComboBoxSelectItem(m_autoquote_style_combo,
                             m_autoquote_strings[intval]);

    // spell check
    PREF_GetBoolPref(SPELLCHECK, &boolval);
    locked=PREF_PrefIsLocked(SPELLCHECK);
    XtVaSetValues(m_spellcheck_toggle,
                  XmNset, boolval,
                  XmNsensitive, !locked,
                  NULL);
    
    // should we wrap long lines?
    PREF_GetBoolPref(WRAP_LONG_LINES, &boolval);
    locked=PREF_PrefIsLocked(WRAP_LONG_LINES);
    XtVaSetValues(m_wrap_toggle,
                  XmNset, boolval,
                  XmNsensitive,!locked,
                  NULL);

    // where to wrap?
    PREF_GetIntPref(WRAP_LENGTH, &intval);
    PR_snprintf(charval, sizeof(charval),"%d",intval);
    locked=PREF_PrefIsLocked(WRAP_LENGTH);
    XtVaSetValues(m_wrap_length_text,
                  XmNsensitive, !locked,
                  NULL);
    fe_SetTextField(m_wrap_length_text, charval);

    // quote 8-bit or leave them alone?
    PREF_GetBoolPref(EIGHTBIT_MIME, &boolval);
    locked=PREF_PrefIsLocked(EIGHTBIT_MIME);
    XtVaSetValues(m_eightbit_asis_toggle,
                  XmNset, !boolval,
                  XmNsensitive,!locked,
                  NULL);
    XtVaSetValues(m_eightbit_quoted_toggle,
                  XmNset, boolval,
                  XmNsensitive,!locked,
                  NULL);

    setInitialized(TRUE);
}

void XFE_PrefsPageMessages::install()
{
}

void XFE_PrefsPageMessages::save()
{
    XP_Bool boolval;
    int32 intval;
    char *charval;

    XtVaGetValues(m_forward_combo,
                  XmNselectedPosition, &intval,
                  NULL);
    PREF_SetIntPref(FORWARD, intval);

    // should we autoquote?
    boolval=XmToggleButtonGadgetGetState(m_autoquote_toggle);
    PREF_SetBoolPref(AUTOQUOTE, boolval);

    // where to put quoted text
    XtVaGetValues(m_autoquote_style_combo,
                  XmNselectedPosition, &intval,
                  NULL);
    
    // convert Widget->pref value
    PREF_SetIntPref(AUTOQUOTE_STYLE, intval);

    // should we spellcheck?
    boolval=XmToggleButtonGadgetGetState(m_spellcheck_toggle);
    PREF_SetBoolPref(SPELLCHECK, boolval);

    // should we wrap?
    boolval=XmToggleButtonGadgetGetState(m_wrap_toggle);
    PREF_SetBoolPref(WRAP_LONG_LINES, boolval);

    // where to wrap?
    intval=getWrapLength();
    if (intval>0) PREF_SetIntPref(WRAP_LENGTH, intval);
    
    // MIME or as-is
    boolval=XmToggleButtonGadgetGetState(m_eightbit_quoted_toggle);
    PREF_SetBoolPref(EIGHTBIT_MIME, boolval);
    
}

Boolean XFE_PrefsPageMessages::verify()
{
    return True;
}


int32
XFE_PrefsPageMessages::getWrapLength()
{
    char *charval=XmTextFieldGetString(m_wrap_length_text);
    int32 intval=XP_ATOI(charval);
    XtFree(charval);
    return intval;
}
