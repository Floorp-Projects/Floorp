/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "mozilla.h"
#include "xfe.h"
#include "fonts.h"

#include <Xm/LabelG.h>
#include <Xm/Label.h>
#include <XmL/Folder.h>

#include <Xfe/Xfe.h>			/* for xfe widgets and utilities */

extern void*
fe_compose_getData(MWContext* context);
extern void*
fe_compose_setData(MWContext* context, void* data);

extern void
fe_mc_field_lostfocus(Widget widget, XtPointer closure, XtPointer call_data);

extern void
fe_mc_field_changed(Widget widget, XtPointer closure, XtPointer call_data);

extern WidgetList
fe_create_composition_widgets(MWContext* context, Widget pane, int *numkids);


typedef enum {

  MailComposeLayoutAddress,
  MailComposeLayoutAttach,
  MailComposeLayoutCompose

} MailComposeLayoutType;


/* fe_MailComposeContextData *mailcomposer = 0;*/ /*Temporary*/

#define MAILCOMPOSE_CONTEXT_DATA(context) ((fe_MailComposeContextData*)(fe_compose_getData(context)))

#include <xpgetstr.h>  /* for XP_GetString() */
extern int XFE_PRI_URGENT;
extern int XFE_PRI_IMPORTANT;
extern int XFE_PRI_NORMAL;
extern int XFE_PRI_FYI;
extern int XFE_PRI_JUNK;
extern int XFE_PRI_PRIORITY;
extern int XFE_COMPOSE_LABEL;
extern int XFE_COMPOSE_ADDRESSING;
extern int XFE_COMPOSE_ATTACHMENT;
extern int XFE_COMPOSE_COMPOSE;

static void createDummy(Widget parent, char *name)
{
    XtVaCreateManagedWidget(name,
        xmLabelWidgetClass, parent,
        XmNtopAttachment, XmATTACH_FORM,
        XmNbottomAttachment, XmATTACH_FORM,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        XmNmarginWidth, 80,
        XmNmarginHeight, 60,
        NULL);
}

static Widget
createOptionMenu(MWContext *context,
                 Widget parent, 
		 char* widgetName,
		 char* labelName,
                 Widget *popup)
{
  Cardinal ac;
  Arg      av[10];
  Visual   *v = 0;
  Colormap cmap = 0;
  Cardinal depth =0;
  Widget option_menu;
  XmString xmStr = 0;

  Widget shell = parent;
 

  while (shell && !XtIsShell(shell)) shell = XtParent(shell);
  XtVaGetValues(shell,
         XtNvisual, &v, XtNcolormap, &cmap, XtNdepth, &depth, 0);
 
  ac = 0;
  XtSetArg(av[ac], XmNvisual, v); ac++;
  XtSetArg(av[ac], XmNdepth, depth); ac++;
  XtSetArg(av[ac], XmNcolormap, cmap); ac ++;
  *popup= XmCreatePulldownMenu(parent, widgetName, av, ac);
 
  ac = 0;
  XtSetArg(av[ac], XmNsubMenuId, *popup); ac++;
  XtSetArg(av[ac], XmNmarginWidth, 0); ac++;
  if (labelName && *labelName)
  {
   xmStr = XmStringCreateSimple(labelName);
   XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
  }
  option_menu = XmCreateOptionMenu(parent, widgetName, av, ac);
  if (xmStr) XmStringFree(xmStr);
 
  return option_menu;
}

Widget 
makeOptionMenu(MWContext *context, Widget parent)
{
   int i;
   int ac = 0;
   Arg av[20];
   Widget optionW = 0;
   Widget popUpW = 0;
   XmString xmStr;
   Widget btn;

   int priorityCnt= 5; /* May need to get from somewhere */
   optionW = createOptionMenu(context, parent, "priorityOption", 
		XP_GetString(XFE_PRI_PRIORITY), &popUpW);

   for ( i = 0; i < priorityCnt; i++ )
   {
	xmStr = XmStringCreateSimple(XP_GetString(XFE_PRI_URGENT+i));
        ac = 0;
  	XtSetArg(av[ac], XmNuserData, i); ac++;
   	XtSetArg(av[ac], XmNlabelString, xmStr); ac++;
 	btn = XmCreatePushButtonGadget(popUpW, "attrBtn", av, ac);
  	XtManageChild(btn);
        if ( i == 2  ) /* Normal */
	XtVaSetValues(optionW, XmNmenuHistory, btn, 0);
	XmStringFree(xmStr);
   }


   XtManageChild(optionW);

   return optionW;
}



static Widget
createManagedCompose(MWContext *context, Widget pane)
{

  int ac = 0;
  char  name[30];
  char  buf[100]; 
  fe_ContextData* data = CONTEXT_DATA(context);
  Widget form;
  Widget secureW;
  Widget optionW;
  Widget subjectTextW;
  Widget subjectLabelW;
  XmFontList fontList;
  Arg av[20];
  Widget composeform;
  
  ac = 0;
  XtSetArg(av[ac], XmNresizePolicy, XmRESIZE_GROW); ac++;
  composeform = XmCreateForm(pane, "mailto_field", av, ac);

  ac = 0;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_NONE); ac++;
  form = XmCreateForm(composeform, "mailto_field", av, ac);
  XtManageChild(form);

  /* Create a secure button */
  ac = 0;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_NONE); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  secureW = XmCreateToggleButtonGadget(form, "secureToggle", av, ac);
  XtManageChild(secureW);

  /* Create a priority option */
  optionW = makeOptionMenu(context, form);

  XtVaSetValues( optionW, 
  	XmNleftAttachment, XmATTACH_NONE,
  	XmNrightAttachment, XmATTACH_WIDGET,
  	XmNrightWidget, secureW,
	XmNrightOffset, 5,
  	XmNtopAttachment, XmATTACH_FORM,
  	XmNbottomAttachment, XmATTACH_FORM,
  	NULL);
  
  /* Create the text field */

  PR_snprintf(name, sizeof (name), "%s", "subject");
  ac = 0;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(av[ac], XmNrightOffset, 5); ac++;
  XtSetArg(av[ac], XmNrightWidget, optionW); ac++;
  subjectTextW = fe_CreateTextField(form, name, av, ac);
  if (fe_globalData.nonterminal_text_translations) {
    XtOverrideTranslations(subjectTextW,
		       fe_globalData.nonterminal_text_translations);
  }
  XtAddCallback(subjectTextW, XmNvalueChangedCallback, fe_mc_field_changed,
	    (XtPointer)MSG_SUBJECT_HEADER_MASK);
  XtAddCallback(subjectTextW, XmNlosingFocusCallback, fe_mc_field_lostfocus,
	    (XtPointer)MSG_SUBJECT_HEADER_MASK );

  ac = 0;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(av[ac], XmNrightWidget, subjectTextW); ac++;
  PR_snprintf(buf, sizeof (buf), XP_GetString(XFE_COMPOSE_LABEL), name);
  subjectLabelW = XmCreateLabelGadget(form, buf, av, ac);

  data->mcSubject = subjectTextW;


  /* Subject is not a drop site */

  XtVaSetValues(subjectTextW, XmNleftOffset, XfeWidth(subjectLabelW), 0);

  /* Manage All children here */

  XtManageChild(subjectTextW);
  XtManageChild(subjectLabelW);


  ac = 0;
  XtSetArg (av[ac], XmNeditMode, XmMULTI_LINE_EDIT); ac++;
  XtSetArg(av[ac], XmNleftAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNrightAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopAttachment, XmATTACH_WIDGET); ac++;
  XtSetArg(av[ac], XmNtopWidget, form); ac++;
  XtSetArg(av[ac], XmNbottomAttachment, XmATTACH_FORM); ac++;
  XtSetArg(av[ac], XmNtopOffset, 5); ac++;
  data->mcBodyText = 
	XmCreateScrolledText(composeform, "mailto_bodyText", av, ac);
  XtManageChild(data->mcBodyText);

  XtVaSetValues(XtParent(data->mcBodyText),
	XmNtopAttachment, XmATTACH_WIDGET,
	XmNtopWidget, form,
	XmNleftAttachment, XmATTACH_FORM,
	XmNbottomAttachment, XmATTACH_FORM,
	XmNrightAttachment, XmATTACH_FORM, NULL);

  XtManageChild(composeform);
  return composeform;
}


extern Widget
fe_MailComposeAddress_CreateManaged(MWContext* context, Widget parent)
{
  Widget frame;
  WidgetList formList;
  int k = 0;
  Arg av[10];
  int ac = 0;
  Widget pane;

  pane = XmCreatePanedWindow(parent, "pane2", av, ac);
  XtVaSetValues(pane, XmNseparatorOn, True, 0);

  ac =0;
  frame = XmCreateFrame(pane, "frame1", av, ac);
  XtVaSetValues(frame, XmNtopAttachment, XmATTACH_FORM, 
		XmNbottomAttachment, XmATTACH_NONE,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		0);

  XtManageChild(frame);

  formList = fe_create_composition_widgets(context, frame, &k);
  XtManageChildren(formList, k);
  XtManageChild(frame);

  return pane; 
}

extern Widget fe_make_managed_attach_form(MWContext* context, Widget parent);
extern Widget
fe_MailComposeAttach_CreateManaged(MWContext* context, Widget parent)
{
  Widget messb;

  messb = fe_make_managed_attach_form(context, parent);

  return messb;
}

static Widget
fe_MailCompose_CreateManaged(MWContext* context, Widget parent)
{
  Widget compose;

  compose = 
  createManagedCompose(context, parent);

  return compose;
}

static Widget
fe_MailComposeContainer_CreateManaged(MWContext* context)
{
  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);
  Widget container;
  Widget address;
  Widget attach;
  Arg av[10];
  int ac = 0;

  data->container = container = 
		XtVaCreateManagedWidget("mailcompose_container",
		xmFormWidgetClass, data->parentFolder,
		XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment,XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM,
		XmNresizePolicy, XmRESIZE_GROW,
		0);

  data->address = fe_MailComposeAddress_CreateManaged(context, container);
  data->attach = fe_MailComposeAttach_CreateManaged(context, container);
  data->compose  = fe_MailCompose_CreateManaged(context, container);
  return container;
}


static Widget
fe_MailComposeWin_AddTab( MWContext* context, char *tabName)
{
  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);
  Widget parentFolder;
  Widget tab;
  XmString xmstr = 0;

  if (!data) return NULL;
  parentFolder = data->parentFolder;

  xmstr = XmStringCreateSimple(tabName);
  tab = XmLFolderAddTabForm(parentFolder, xmstr);
  XtVaSetValues(tab, XmNtopAttachment, XmATTACH_FORM,
		XmNbottomAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		XmNrightAttachment, XmATTACH_FORM, 
		XmNresizePolicy, XmRESIZE_GROW,
		NULL);


  XmStringFree(xmstr);
  return tab;
}


static Widget
fe_MailComposeWin_CreateFolders(MWContext* context)
{

  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);

  Widget folder;
  Widget parent;


  if (!data) return NULL;

  parent = data->parent;

  folder = XtVaCreateManagedWidget("composeFolder",
            xmlFolderWidgetClass, parent,
	    NULL);

  return folder;
}

static fe_MailComposeContextData *
fe_MailComposeWin_CreateContext(MWContext *context)
{
  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);

  if ( !data )
    data = (fe_MailComposeContextData*)
		malloc(sizeof(fe_MailComposeContextData));

  data->tab_number = 0;

  data->parent = 0;
  data->container = 0;
  data->parentFolder = 0;

  data->address = 0;
  data->attach = 0;
  data->compose = 0;

  data->address_tab = 0;
  data->attach_tab = 0;
  data->compose_tab = 0;

  return data;
}

static void
fe_MailComposeWin_DestroyContext(MWContext *context)
{
  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);

  if ( !data ) 
     return;

  XtDestroyWidget(data->parentFolder);

  /* Remember to destroy mad from the context */
}


static void
mailcompose_layout(fe_MailComposeContextData *data, 
		MailComposeLayoutType type)
{
   Widget manage= 0, unmanage = 0;
   
   switch (type) 
   {
 	case MailComposeLayoutAddress:
		manage = data->address;	
		unmanage = data->attach;
	break;
	case MailComposeLayoutAttach:
		manage = data->attach;	
		unmanage = data->address;
	break;
	case MailComposeLayoutCompose:
	default: 
		/* Do nothing */
	break;
   }

   if ( manage && unmanage )
   {
   	XtUnmanageChild(unmanage);

   	XtVaSetValues(manage,
	 XmNtopAttachment, XmATTACH_FORM,
	 XmNbottomAttachment, XmATTACH_NONE,
	 XmNleftAttachment, XmATTACH_FORM,
	 XmNrightAttachment, XmATTACH_FORM,
	0);

   	XtVaSetValues(data->compose,
	 XmNtopAttachment, XmATTACH_WIDGET,
	 XmNtopWidget, manage,
	 XmNtopOffset, 6,
	 XmNbottomAttachment, XmATTACH_FORM,
	 XmNleftAttachment, XmATTACH_FORM,
	 XmNrightAttachment, XmATTACH_FORM,
	0);
   	XtManageChild(manage);
   }
   else if (type ==MailComposeLayoutCompose )
   {
	XtUnmanageChild(data->address);
	XtUnmanageChild(data->attach);
   	XtVaSetValues(data->compose,
	 XmNtopAttachment, XmATTACH_FORM,
	 XmNbottomAttachment, XmATTACH_FORM,
	 XmNleftAttachment, XmATTACH_FORM,
	 XmNrightAttachment, XmATTACH_FORM,
	0);
   }
}

extern void 
fe_make_new_attach_list(MWContext *context);

static void
activateTab(Widget w, XtPointer clientData, XtPointer callData)
{
  XmLFolderCallbackStruct *cb = (XmLFolderCallbackStruct *)callData;

  MWContext *context = (MWContext *)clientData;
  fe_ContextData* data = CONTEXT_DATA(context);
  String text;
  XmTextPosition pos;
  fe_MailComposeContextData *maildata = MAILCOMPOSE_CONTEXT_DATA(context);

  XtVaSetValues(maildata->address_tab, XmNtabManagedWidget, NULL, 0);
  XtVaSetValues(maildata->attach_tab, XmNtabManagedWidget, NULL, 0);
  XtVaSetValues(maildata->compose_tab, XmNtabManagedWidget, NULL, 0);
  XmProcessTraversal(w, XmTRAVERSE_DOWN);

  if ( cb->pos == 0 )
  {
     mailcompose_layout(maildata, MailComposeLayoutAddress);
     XtVaSetValues(maildata->address_tab, XmNtabManagedWidget, maildata->container,0);
     XmProcessTraversal (data->mcSubject, XmTRAVERSE_CURRENT);
  }
  else if (cb->pos == 1 )
  {
     mailcompose_layout(maildata, MailComposeLayoutAttach);
     fe_make_new_attach_list(context);
     XtVaSetValues(maildata->attach_tab, XmNtabManagedWidget, maildata->container,0);
     XmProcessTraversal (data->mcSubject, XmTRAVERSE_CURRENT);
  }
  else 
  {
     mailcompose_layout(maildata, MailComposeLayoutCompose);
     XtVaSetValues(maildata->compose_tab, XmNtabManagedWidget, maildata->container,0);
     XmProcessTraversal (data->mcSubject, XmTRAVERSE_CURRENT);
  }
}


Widget
fe_MailComposeWin_Create(MWContext* context, Widget parent)
{

  XmFontList fontList;
  Widget mainForm = 0;
  fe_MailComposeContextData *data = MAILCOMPOSE_CONTEXT_DATA(context);
  WidgetList formList2;
  int j;


  if ( !data ) 
	/* Create a new one */
        data = fe_MailComposeWin_CreateContext(context);
  fe_compose_setData(context, (void *)data);
  data->parent = parent;

  mainForm = fe_MailComposeWin_CreateFolders(context);
  data->parentFolder = mainForm;
  /* Create the Container */
  {
     data->container = 
       fe_MailComposeContainer_CreateManaged(context);
  }
  /* Create Addressing */
  {
    XmString xmStr;

    xmStr = XmStringCreateSimple(XP_GetString(XFE_COMPOSE_ADDRESSING));
    data->address_tab = XmLFolderAddTab(data->parentFolder, xmStr);
    XmStringFree(xmStr);
  }

  /* Create Attachment */
  {
    XmString xmStr;

    xmStr = XmStringCreateSimple(XP_GetString(XFE_COMPOSE_ATTACHMENT));
    data->attach_tab = XmLFolderAddTab(data->parentFolder, xmStr);
    XmStringFree(xmStr);
  }

  /* Create Compose */
  {
    XmString xmStr;

    xmStr = XmStringCreateSimple(XP_GetString(XFE_COMPOSE_COMPOSE));
    data->compose_tab = XmLFolderAddTab(data->parentFolder, xmStr);
    XmStringFree(xmStr);
  }

  XtAddCallback(data->parentFolder, XmNactivateCallback, 
		activateTab, context);

  return (mainForm);
}

void
fe_MailComposeWin_Activate(MWContext *context)
{
  XmLFolderSetActiveTab(MAILCOMPOSE_CONTEXT_DATA(context)->parentFolder, 0, True);
}

void
fe_MailComposeWin_ActivateFolder(MWContext *context, int pos)
{
  XmLFolderSetActiveTab(MAILCOMPOSE_CONTEXT_DATA(context)->parentFolder, pos, True);
}

