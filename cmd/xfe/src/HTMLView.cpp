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
   HTMLView.cpp -- class definition for the HTML view class
   Created: Spence Murray <spence@netscape.com>, 23-Oct-96.
 */



#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

#include "HTMLView.h"
#include "BookmarkFrame.h"
#include "MozillaApp.h"
#ifdef EDITOR
#include "EditorFrame.h"
#endif
#include "bkmks.h"
#include "net.h"
#include "layers.h"
#include "ntypes.h"
#include "libevent.h"
#include "il_icons.h"
#include "xpgetstr.h"
#include "MozillaApp.h"
#include "ViewGlue.h"

#include "fe_proto.h"
#include "felocale.h"
#include <libi18n.h>
#include "intl_csi.h"

#include "prefs.h"
#include "xfe2_extern.h"
#include "prefapi.h"
#include "selection.h"

#include <Xm/Label.h> 
#include <Xfe/Xfe.h>
#include "fonts.h"

#include <unistd.h>   // for getcwd()

extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_COMMANDS_OPEN_FILE_USAGE;
#ifdef EDITOR
extern int XFE_EDITOR_ALERT_ABOUT_DOCUMENT;
extern int XFE_EDITOR_ALERT_FRAME_DOCUMENT;
#endif

/* handle leave notify event
 */
#define HANDLE_LEAVE_WIN          1

// baggage from the old world.
extern "C" {
  void fe_upload_file_cb (Widget widget, XtPointer closure,
				      XtPointer call_data);
  void fe_SaveURL (MWContext *context, URL_Struct *url);
  MWContext *fe_MakeWindow(Widget toplevel, MWContext *context_to_copy,
			       URL_Struct *url, char *window_name, 
			       MWContextType type, Boolean skip_get_url);
  void fe_UserActivity (MWContext *context);
  MWContext *fe_MotionWidgetToMWContext (Widget widget);
  void fe_get_context_resources (MWContext *context);
  void fe_load_default_font (MWContext *context);
  void fe_find_scrollbar_sizes (MWContext *context);
  void fe_get_final_context_resources (MWContext *context);
  int fe_GetURL(MWContext *context, URL_Struct *url, Boolean skip_get_url);
  CL_Compositor *fe_create_compositor(MWContext *context);
  MWContext *fe_GetFocusGridOfContext (MWContext *context);
  void fe_back_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_forward_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_home_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_abort_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_save_as_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_save_top_frame_as_cb (Widget widget, XtPointer closure,
                                XtPointer call_data);
  void fe_save_as_action (Widget widget, MWContext *context,
                          String *av, Cardinal *ac);
  void fe_open_file_action (Widget widget, MWContext *context,
                            String *av, Cardinal *ac);
  void fe_mailNew_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_mailto_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_print_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_find_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_find_again_cb (Widget widget, XtPointer closure, 
                         XtPointer call_data);
/*  void fe_reload_cb (Widget widget, XtPointer closure, XtPointer call_data);*/
  void fe_load_images_cb (Widget widget, XtPointer closure, 
                          XtPointer call_data);
  void fe_refresh_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_view_source_cb (Widget widget, XtPointer closure, 
                          XtPointer call_data);
  void fe_docinfo_cb (Widget widget, XtPointer closure, XtPointer call_data);
  void fe_map_notify_eh (Widget w, XtPointer closure, XEvent *ev, Boolean *cont);
  void fe_sec_logo_cb (Widget widget, XtPointer closure, XtPointer call_data);
#ifdef EDITOR
  void fe_editor_edit_cb(Widget widget, XtPointer closure, XtPointer call_data);
#endif
  void fe_doc_enc_cb(Widget widget, XtPointer closure, XtPointer call_data);

#if HANDLE_LEAVE_WIN
  /* Tooltips
   */
  void fe_Add_HTMLViewTooltips_eh(MWContext *context);
  void fe_Remove_HTMLViewTooltips_eh(MWContext *context);
#endif
}

const char *XFE_HTMLView::newURLLoading = "XFE_HTMLView::newURLLoading";
const char *XFE_HTMLView::spacebarAtPageBottom = "XFE_HTMLView::spacebarAtPageBottom";
const char *XFE_HTMLView::popupNeedsShowing = "XFE_HTMLView::popupNeedsShowing";

MenuSpec XFE_HTMLView::separator_spec[] = {
  MENU_SEPARATOR,
  { NULL },
};
MenuSpec XFE_HTMLView::openLinkNew_spec[] = {
  { xfeCmdOpenLinkNew , PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::openFrameNew_spec[] = {
  { xfeCmdOpenFrameNew , PUSHBUTTON },
  { xfeCmdOpenFrameInWindow , PUSHBUTTON },
  { NULL },
};
#ifdef EDITOR
MenuSpec XFE_HTMLView::openLinkEdit_spec[] = {
  { xfeCmdOpenLinkEdit, PUSHBUTTON },
  { NULL },
};
#endif
MenuSpec XFE_HTMLView::go_spec[] = {
  { xfeCmdBack, PUSHBUTTON },
  { xfeCmdForward, PUSHBUTTON },
  { xfeCmdFrameReload     , PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::showImage_spec[] = {
  { xfeCmdShowImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::stopLoading_spec[] = {
  { xfeCmdStopLoading, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::page_details_spec[] = {
  { xfeCmdViewFrameSource, PUSHBUTTON },
  { xfeCmdViewFrameInfo, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::openImage_spec[] = {
  { xfeCmdOpenImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::addLinkBookmark_spec[] = {
  { xfeCmdAddLinkBookmark, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::addFrameBookmark_spec[] = {
  { xfeCmdAddFrameBookmark, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::addBookmark_spec[] = {
  { xfeCmdAddBookmark, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::sendPage_spec[] = {
  { xfeCmdSendPage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::saveLink_spec[] = {
  { xfeCmdSaveLink, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::saveImage_spec[] = {
  { xfeCmdSaveImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::saveBGImage_spec[] = {
  { xfeCmdSaveBGImage, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::copy_spec[] = {
  { xfeCmdCopy, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::copyLink_spec[] = {
  { xfeCmdCopyLink, PUSHBUTTON },
  { NULL },
};
MenuSpec XFE_HTMLView::copyImage_spec[] = {
  { xfeCmdCopyImage, PUSHBUTTON },
  { NULL },
};

extern Boolean fe_IsPageLoaded (MWContext *context);

XFE_HTMLView::XFE_HTMLView(XFE_Component *toplevel_component,
			   Widget parent, XFE_View *parent_view,
			   MWContext *context) 
  : XFE_View(toplevel_component, parent_view, context)
{
  makeScroller (parent);

  CONTEXT_DATA(m_contextData)->fancy_ftp_p = True;

  toplevel_component->registerInterest(XFE_Component::afterRealizeCallback,
				       this,
				       (XFE_FunctionNotification)beforeToplevelShow_cb);

  toplevel_component->registerInterest(XFE_Frame::encodingChanged,
				       this,
				       (XFE_FunctionNotification)DocEncoding_cb);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::linksAttributeChanged,
											 this,
											 (XFE_FunctionNotification)ChangeLinksAttribute_cb);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::defaultColorsChanged,
											 this,
											 (XFE_FunctionNotification)ChangeDefaultColors_cb);

  XFE_MozillaApp::theApp()->registerInterest(XFE_MozillaApp::defaultFontChanged,
											 this,
											 (XFE_FunctionNotification)ChangeDefaultFont_cb);

  m_popup = NULL;

  setBaseWidget(m_scrollerForm);
}

XFE_HTMLView::~XFE_HTMLView()
{
#if HANDLE_LEAVE_WIN
  /* Tooltips
   */
  fe_Remove_HTMLViewTooltips_eh(m_contextData);
#endif /* HANDLE_LEAVE_WIN */

  if (m_popup) {
    delete m_popup;
    m_popup = NULL;
  }

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::linksAttributeChanged,
											   this,
                                               (XFE_FunctionNotification)ChangeLinksAttribute_cb);

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::defaultFontChanged,
											   this,
                                               (XFE_FunctionNotification)ChangeDefaultFont_cb);

  XFE_MozillaApp::theApp()->unregisterInterest(XFE_MozillaApp::defaultColorsChanged,
											   this,
                                               (XFE_FunctionNotification)ChangeDefaultColors_cb);
}

void
XFE_HTMLView::makeScroller(Widget parent)
{
  Widget pane;
//  Widget line1;
//  Widget line2;
  Widget scroller;
  Arg av[20];
  int ac;


  ac = 0;
  m_scrollerForm = XmCreateForm (parent, "scrollerForm", av, ac);

//   ac = 0;
//   XtSetArg (av[ac], XmNseparatorType, XmSHADOW_ETCHED_IN); ac++;
//   XtSetArg (av[ac], XmNshadowThickness, 5); ac++;
//   line1 = XmCreateSeparatorGadget (m_scrollerForm, "line1", av, ac);

//   XtVaSetValues (line1,
// 		 XmNtopAttachment, XmATTACH_FORM,
// 		 XmNbottomAttachment, XmATTACH_NONE,
// 		 XmNleftAttachment, XmATTACH_FORM,
// 		 XmNrightAttachment, XmATTACH_FORM,
// 		 0);


//   ac = 0;
//   XtSetArg (av[ac], XmNseparatorType, XmSHADOW_ETCHED_IN); ac++;
//   XtSetArg (av[ac], XmNshadowThickness, 5); ac++;
//   line2 = XmCreateSeparatorGadget (m_scrollerForm, "line2", av, ac);

//   XtVaSetValues (line2,
// 		 XmNtopAttachment, XmATTACH_NONE,
// 		 XmNbottomAttachment, XmATTACH_FORM,
// 		 XmNleftAttachment, XmATTACH_FORM,
// 		 XmNrightAttachment, XmATTACH_FORM,
// 		 0);

  ac = 0;
  XtSetArg (av[ac], XmNborderWidth, 0); ac++;
  XtSetArg (av[ac], XmNmarginWidth, 0); ac++;
  XtSetArg (av[ac], XmNmarginHeight, 0); ac++;
  XtSetArg (av[ac], XmNborderColor, 0); ac++;
	    // CONTEXT_DATA (m_contextData)->default_bg_pixel); ac++;
  pane = XmCreatePanedWindow (m_scrollerForm, "pane", av, ac);
  
  XtVaSetValues(pane,
				XmNtopAttachment,		XmATTACH_FORM,
				XmNbottomAttachment,	XmATTACH_FORM,
				XmNleftAttachment,		XmATTACH_FORM,
				XmNrightAttachment,		XmATTACH_FORM,
				XmNtopOffset,			0,
				XmNbottomOffset,		0,
				XmNleftOffset,			0,
				XmNrightOffset,			0,

// 		 XmNtopAttachment, XmATTACH_WIDGET,
// 		 XmNtopWidget, line1,
// 		 XmNbottomAttachment, XmATTACH_WIDGET,
// 		 XmNbottomWidget, line2,
// 		 XmNleftAttachment, XmATTACH_FORM,
// 		 XmNrightAttachment, XmATTACH_FORM,
		 0);

  /* The actual work area */
  scroller = fe_MakeScrolledWindow (m_contextData, pane, "scroller");
  XtVaSetValues (CONTEXT_DATA (m_contextData)->scrolled,
				 XmNborderWidth, 0, 
#if defined(__FreeBSD__)||defined(BSDI)||defined(LINUX)||defined(IRIX)
				 // Allow for resolutions > 1000 pixels.
				 // This causes the vertical scrollbar not to show
				 // up on Solaris 2.4, bug in Motif (77998).  
				 XmNpaneMaximum, 6000,
#endif
				 0);
  
//  XtManageChild (line1);
  XtManageChild (scroller);
//  XtManageChild (line2);
  XtManageChild (pane);

  CONTEXT_DATA (m_contextData)->main_pane = m_scrollerForm;

  fe_load_default_font (m_contextData);
  fe_get_context_resources (m_contextData);   /* Do other resource db hackery. */

  /* Figure out how much space the horizontal and vertical scrollbars take up.
     It's basically impossible to determine this before creating them...
   */
  {
    Dimension w1 = 0, w2 = 0, h1 = 0, h2 = 0;

    XtManageChild (CONTEXT_DATA (m_contextData)->hscroll);
    XtManageChild (CONTEXT_DATA (m_contextData)->vscroll);
    XtVaGetValues (CONTEXT_DATA (m_contextData)->drawing_area,
                                 XmNwidth, &w1,
                                 XmNheight, &h1,
                                 0);

    XtUnmanageChild (CONTEXT_DATA (m_contextData)->hscroll);
    XtUnmanageChild (CONTEXT_DATA (m_contextData)->vscroll);
    XtVaGetValues (CONTEXT_DATA (m_contextData)->drawing_area,
                                 XmNwidth, &w2,
                                 XmNheight, &h2,
                                 0);

    CONTEXT_DATA (m_contextData)->sb_w = w2 - w1;
    CONTEXT_DATA (m_contextData)->sb_h = h2 - h1;

    /* Now that we know, we don't need to leave them managed. */
  }

  XtVaSetValues (CONTEXT_DATA (m_contextData)->scrolled, XmNinitialFocus,
                 CONTEXT_DATA (m_contextData)->drawing_area, 0);

#if HANDLE_LEAVE_WIN
  /* add event handler
   */
  fe_Add_HTMLViewTooltips_eh(m_contextData);
#endif
}

XFE_CALLBACK_DEFN(XFE_HTMLView, beforeToplevelShow)(XFE_NotificationCenter *,
						    void *, void*)
{
  m_contextData->compositor = fe_create_compositor(m_contextData);

  if (m_contextData->is_grid_cell)
    fe_SetGridFocus (m_contextData);  /* Give this grid focus */
}

int
XFE_HTMLView::getURL(URL_Struct *url, Boolean skip_get_url)
{
  if (url)
    {
      XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));
      return fe_GetURL (m_contextData, url, skip_get_url);
    }

  return 0; // ###ct ???  Is this what we want?
}

void
XFE_HTMLView::doCommand(CommandType cmd, void *callData, XFE_CommandInfo* info)
{
#define IS_CMD(command) (cmd == (command))
  if (IS_CMD(xfeCmdBack))
    {
      fe_back_cb(NULL, m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdForward))
    {
      fe_forward_cb(NULL, m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdHome))
    {
      fe_home_cb(NULL, m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdDestinations))
    {
		fe_GuideCallback (CONTEXT_WIDGET (m_contextData), 
						  (XtPointer) m_contextData, NULL);

		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdFindInObject))
    {
      fe_find_cb(NULL, m_contextData, NULL);
      return;
    }
  else if (IS_CMD(xfeCmdStopLoading))
    {
      //if (fe_IsContextStoppable(m_contextData)) {
        fe_abort_cb(NULL, m_contextData, NULL);
    /*} else {
        XP_InterruptContext (m_contextData);

        CONTEXT_DATA(m_contextData)->looping_images_p = False;

      }
      */
      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdStopAnimations))
    {
      XP_InterruptContext (m_contextData);
      return;
    }
  else if (IS_CMD(xfeCmdOpenUrl))
    {
      URL_Struct *url = (URL_Struct*)callData;

      getURL(url, False);
      
      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdOpenUrlFromSelection))
    {
		fe_PrimarySelectionFetchURL(m_contextData);

		return;
    }
  else if (IS_CMD(xfeCmdOpenPage))
    {
	  fe_OpenURLDialog(m_contextData);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdOpenPageChooseFile))
    {
      openFileAction(info->params, info->nparams);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdSaveAs))
    {
      fe_save_as_action(CONTEXT_WIDGET(m_contextData), m_contextData,
                        info->params, info->nparams);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdSaveFrameAs))
    {
      fe_save_as_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
#ifdef MOZ_MAIL_NEWS
  else if (IS_CMD(xfeCmdSendPage))
    {
      fe_mailto_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdSendLink))
    {
      MSG_Pane *pane;
      char *new_title = NULL;

      History_entry *h = SHIST_GetCurrent (&m_contextData->hist);

      if (!h) return;

      if (h->title && *h->title) 
        new_title = h->title;
      else 
        new_title = h->address;

      MSG_CompositionFields *fields = 
        MSG_CreateCompositionFields(NULL,      // from
									NULL,      // reply_to
									NULL,      // to
									NULL,      // cc
									NULL,      // bcc
									NULL,      // fcc
									NULL,      // newsgroups
									NULL,      // followup_to
									NULL,      // organization
									new_title, // subject
									NULL,      // references
									NULL,      // other_random_headers
									NULL,      // priority
									NULL,      // attachment
									NULL,      // newspost_url
									FALSE,     // encrypt_p
									FALSE);    // sign_p

      // Since they are only sending the link,
      // I am guessing that they want a plaintext editor.
      // Otherwise, why wouldn't they just send the page.  -slamm
      pane = fe_showCompose(getBaseWidget(), NULL, m_contextData, fields, 
                            h->address,   MSG_PLAINTEXT_EDITOR, False);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
#endif  // MOZ_MAIL_NEWS
  else if (IS_CMD(xfeCmdCut))
    {
		MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
		if (!context) context = m_contextData;
		
		fe_cut_cb (CONTEXT_WIDGET (m_contextData), 
				   (XtPointer) context, NULL);
		
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
        return;
    }
  else if (IS_CMD(xfeCmdCopy))
    {
		MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
		if (!context) context = m_contextData;

		fe_copy_cb (CONTEXT_WIDGET (m_contextData), 
					(XtPointer)context,
					NULL);
		
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
        return;
    }
  else if (IS_CMD(xfeCmdPaste))
    {
		MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
		if (!context) context = m_contextData;

		fe_paste_cb (CONTEXT_WIDGET (m_contextData), 
					 (XtPointer) context, NULL);
		
		getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
        return;
    }
  else if (IS_CMD(xfeCmdFindInObject))
    {
      fe_find_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdFindAgain))
    {
      fe_find_again_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdEditPreferences))
    {
	  fe_showPreferences(getToplevel(), getContext());
      // fe_PrefsDialog (m_contextData);
      

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdReload))
    {
      XP_ASSERT(info);
      reload(info->widget, info->event);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdFrameReload))
    {
      XP_ASSERT(info);

      // pass true to only reload frame
      reload(info->widget, info->event, TRUE);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdSearch)) 
    {
      fe_SearchCallback (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdShowImages))
    {
      MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
      if (!context) context = m_contextData;

      fe_LoadDelayedImages (context);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdShowImage))
    {
      if (m_imageUnderMouse) 
        {
          MWContext *context = fe_GetFocusGridOfContext(m_contextData);
          
          if (!context) context = m_contextData;

          fe_LoadDelayedImage (context, 
                               m_imageUnderMouse->address);

          getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      
          m_imageUnderMouse = NULL;
        }
      return;
    }
  else if (IS_CMD(xfeCmdRefresh))
    {
      fe_refresh_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdViewPageSource))
    {
      fe_view_source_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdViewPageInfo))
    {
      fe_docinfo_cb (CONTEXT_WIDGET (m_contextData), 
			  (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdViewFrameSource))
    {
      MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
      if (!context) context = m_contextData;

      fe_view_source_cb (CONTEXT_WIDGET (m_contextData), 
                         (XtPointer) context, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdViewFrameInfo))
    {
      MWContext *context = fe_GetFocusGridOfContext(m_contextData);
      
      if (!context) context = m_contextData;

      fe_docinfo_cb (CONTEXT_WIDGET (m_contextData), 
                     (XtPointer) context, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdViewSecurity))
    {
      fe_sec_logo_cb(NULL, m_contextData, NULL);
      return;
    }
  else if (IS_CMD(xfeCmdAddBookmark))
    {
      History_entry *h = SHIST_GetCurrent (&m_contextData->hist);
      BM_Entry *bm;
      char *new_title;

      if (!h) return;
      
      if (!h->title || !*h->title) new_title = h->address;
      else new_title = h->title;
      bm = (BM_Entry*)BM_NewUrl( new_title, h->address, NULL, h->last_access);
      
      BM_AppendToHeader (XFE_BookmarkFrame::main_bm_context,
			 BM_GetAddHeader(XFE_BookmarkFrame::main_bm_context), bm);
      return;
    }
  else if (IS_CMD(xfeCmdAddFrameBookmark))
    {
      fe_UserActivity (m_contextData);
      
      MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
      if (!context) context = m_contextData;

      History_entry *h = SHIST_GetCurrent (&context->hist);

      if (!h) return;
      
      BM_Entry *bm;
      char *new_title;

      if (!h->title || !*h->title) new_title = h->address;
      else new_title = h->title;

      bm = (BM_Entry*)BM_NewUrl( new_title, h->address, NULL, h->last_access);
      
      BM_AppendToHeader (XFE_BookmarkFrame::main_bm_context,
			 BM_GetAddHeader(XFE_BookmarkFrame::main_bm_context), bm);
      return;
    }
  else if (IS_CMD(xfeCmdAddLinkBookmark))
    {
      if (!m_urlUnderMouse) return;
      
      BM_Entry *bm;
      char *new_title;

      new_title = m_urlUnderMouse->address; /* Maybe we can do something
                                               smart someday. -slamm */

      bm = (BM_Entry*)BM_NewUrl( new_title, m_urlUnderMouse->address,
                                 NULL, 0);
      
      BM_AppendToHeader (XFE_BookmarkFrame::main_bm_context,
			 BM_GetAddHeader(XFE_BookmarkFrame::main_bm_context), bm);
      return;
    }
  else if (IS_CMD(xfeCmdPrint))
    {
      fe_print_cb  (CONTEXT_WIDGET (m_contextData), 
                    (XtPointer) m_contextData, NULL);

      getToplevel()->notifyInterested(XFE_View::chromeNeedsUpdating);
      return;
    }
  else if (IS_CMD(xfeCmdSpacebar))
    {
		MWContext *focus_context = fe_GetFocusGridOfContext(m_contextData);
		if (!focus_context) focus_context = m_contextData;

		Widget sb = CONTEXT_DATA(focus_context)->vscroll;
		XmScrollBarCallbackStruct cb;
		int pi = 0, v = 0, max = 1, min = 0;
		
		XP_ASSERT(sb);
		if (!sb) return;
		
		XtVaGetValues (sb, XmNpageIncrement, &pi, XmNvalue, &v,
					   XmNmaximum, &max, XmNminimum, &min, 0);
		cb.reason = XmCR_PAGE_INCREMENT;
		cb.event = 0;
		cb.pixel = 0;
		cb.value = v + pi;
		if (cb.value > max - pi) cb.value = max - pi;
		if (cb.value < min) cb.value = min;
		
		if ((unsigned long)cb.value == CONTEXT_DATA (focus_context)->document_y)
			{
				/* if we're staying at the same document_y, we must be at the bottom
				   of the page. */
				notifyInterested(spacebarAtPageBottom);
			}
		else
			{
				XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
			}
		return;
    }
  else if (IS_CMD (xfeCmdShowPopup))
    {
      D(printf("xfeCmdShowPopup - HTMLView\n");)
      XEvent *event = (XEvent *) info->event;

      if (m_popup) {
		  delete m_popup;
		  m_popup = NULL;
      }

      Widget w = XtWindowToWidget (event->xany.display, event->xany.window);
      if (!w)
	w = m_widget;

      findLayerForPopupMenu (w, event);
      return;
    }
  else if (IS_CMD(xfeCmdOpenLinkNew))
    {
      fe_UserActivity (m_contextData);

      if (!m_urlUnderMouse)
	return;

      if (
#ifdef MOZ_MAIL_NEWS
          MSG_RequiresComposeWindow(m_urlUnderMouse->address) ||
#endif  // MOZ_MAIL_NEWS
          ! XP_STRNCMP ("telnet:", m_urlUnderMouse->address, 7) ||
          ! XP_STRNCMP ("tn3270:", m_urlUnderMouse->address, 7) ||
          ! XP_STRNCMP ("rlogin:", m_urlUnderMouse->address, 7)
         )
	getURL (m_urlUnderMouse, FALSE);
      else
	fe_MakeWindow (XtParent (CONTEXT_WIDGET (m_contextData)),
		       m_contextData, m_urlUnderMouse, NULL, 
		       MWContextBrowser, FALSE);
      m_urlUnderMouse = NULL; /* it will be freed in the exit routine. */
      return;
    }
  else if (IS_CMD(xfeCmdOpenFrameNew))
    {
      fe_UserActivity (m_contextData);
      
      MWContext *context = fe_GetFocusGridOfContext(m_contextData);
		
      if (!context) context = m_contextData;

      History_entry *h = SHIST_GetCurrent (&context->hist);

      if (!h) return;
      
      URL_Struct *url_s = NET_CreateURLStruct (h->address,
                                               NET_DONT_RELOAD);

      fe_MakeWindow (XtParent (CONTEXT_WIDGET (m_contextData)),
                     m_contextData, url_s, NULL, 
                     MWContextBrowser, FALSE);
    }
  else if (IS_CMD(xfeCmdOpenFrameInWindow))
    {
      fe_UserActivity (m_contextData);

      MWContext * grid_context = fe_GetFocusGridOfContext(m_contextData);

      History_entry * h = SHIST_GetCurrent (&grid_context->hist);

      if (!h) return;

      if (!grid_context) grid_context = m_contextData;
	  
	  MWContext * top_context = XP_GetNonGridContext(grid_context);

      URL_Struct * url = NET_CreateURLStruct(h->address,NET_DONT_RELOAD);

	  fe_GetURL(top_context,url,False);
    }
#ifdef EDITOR
  else if (IS_CMD(xfeCmdOpenLinkEdit))
    {
      fe_UserActivity (m_contextData);

      if (!m_urlUnderMouse)
	return;

	/* If this is a grid doc, bail ... ...but we are editing the link, not this frame
	if (m_contextData->is_grid_cell || fe_IsGridParent (m_contextData)) {
	    FE_Alert(m_contextData, 
		     XP_GetString(XFE_EDITOR_ALERT_FRAME_DOCUMENT));
	    return;
	}
    */

	if (strncmp(m_urlUnderMouse->address, "about:", 6) == 0) {
	    FE_Alert(m_contextData, 
		     XP_GetString(XFE_EDITOR_ALERT_ABOUT_DOCUMENT));
	    return;
	}
#ifdef MOZ_MAIL_NEWS
      if (MSG_RequiresComposeWindow(m_urlUnderMouse->address))
	getURL (m_urlUnderMouse, FALSE);
      else
#endif
        fe_EditorNew(m_contextData, (XFE_Frame *)m_toplevel,
                     NULL, m_urlUnderMouse->address);

      m_urlUnderMouse = NULL; /* it will be freed in the exit routine. */
      return;
    }
#endif  // EDITOR
  else if (IS_CMD(xfeCmdSaveLink))
    {
      fe_UserActivity (m_contextData);

      if (!m_urlUnderMouse)
	FE_Alert (m_contextData, fe_globalData.not_over_link_message);
      else
	fe_SaveURL (m_contextData, m_urlUnderMouse);

      m_urlUnderMouse = NULL; /* it will be freed in the exit routine. */
      return;
    }
  else if (IS_CMD(xfeCmdCopyLink))
    {
      if (!m_urlUnderMouse)
        FE_Alert (m_contextData, fe_globalData.not_over_link_message);
      else {
		  if (m_imageUnderMouse) {
			  fe_clipboard_image_link_cb (CONTEXT_WIDGET (m_contextData),
										  (XtPointer) m_contextData, NULL,
										  m_urlUnderMouse, m_imageUnderMouse);
			  m_imageUnderMouse = NULL;
		  }
		  else {
			  fe_clipboard_link_cb (CONTEXT_WIDGET (m_contextData),
									(XtPointer) m_contextData, NULL,
									m_urlUnderMouse);
		  }
	  }
      m_urlUnderMouse = NULL;
      return;
    }
  else if (IS_CMD(xfeCmdCopyImage))
    {
      if (!m_imageUnderMouse)
        FE_Alert (m_contextData, fe_globalData.not_over_image_message);
      else
        fe_clipboard_image_cb (CONTEXT_WIDGET (m_contextData),
							   (XtPointer) m_contextData, NULL,
							   m_imageUnderMouse);
      m_imageUnderMouse = NULL;
      return;
    }
  else if (IS_CMD(xfeCmdSaveImage))
    {
      fe_UserActivity (m_contextData);

      if (!m_imageUnderMouse)
	FE_Alert (m_contextData, fe_globalData.not_over_image_message);
      else
	fe_SaveURL (m_contextData, m_imageUnderMouse);

      m_imageUnderMouse = NULL; /* it will be freed in the exit routine. */
      return;
    }
  else if (IS_CMD(xfeCmdSaveBGImage))
    {
      fe_UserActivity (m_contextData);

      if (!m_backgroundUnderMouse)
        FE_Alert (m_contextData, fe_globalData.not_over_image_message);
      else
        fe_SaveURL (m_contextData, m_backgroundUnderMouse);

      m_backgroundUnderMouse = NULL; /* it will be freed in the exit routine. */
      return;
    }
  else if (IS_CMD(xfeCmdOpenImage))
    {
      if (!m_imageUnderMouse)
        FE_Alert (m_contextData, fe_globalData.not_over_image_message);
      else
        getURL(m_imageUnderMouse, False);

      m_imageUnderMouse = NULL; /* it will be freed in the exit routine. */
    }
  else if (IS_CMD(xfeCmdSelectAll))
    {
      Widget focus = XmGetFocusWidget (CONTEXT_WIDGET (m_contextData));

      if (focus && XmIsTextField (focus) || XmIsTextField(focus)) {
        char *text = fe_GetTextField(focus);

        if (text == NULL) return;

        XmTextFieldSetSelection(focus, 0, XP_STRLEN(text), 0);

        XtFree(text);

      } else {

        MWContext *context = fe_GetFocusGridOfContext(m_contextData);
        XEvent *event = (XEvent *) info->event;
        Time time = XtLastTimestampProcessed(event->xany.display);
		
        if (!context) context = m_contextData;
        
        LO_SelectAll (context);

        fe_OwnSelection(context, time, False);
      }
      return;
    }
  else if (IS_CMD(xfeCmdUploadFile))
    {
      fe_upload_file_cb (CONTEXT_WIDGET (m_contextData), 
			 (XtPointer) m_contextData, NULL);
      return;
    }
  else if (IS_CMD(xfeCmdPageServices))
    {
      if (SHIST_CurrentHandlesPageServices(m_contextData))
        {
          char *url = SHIST_GetCurrentPageServicesURL(m_contextData);

          if (url)
            {
              URL_Struct *url_s = NET_CreateURLStruct (url, NET_DONT_RELOAD);
              getURL(url_s, False);
            }
        }
      return;
    }
  else if (IS_CMD(xfeCmdChangeDocumentEncoding))
    {
      int/*16*/ new_doc_csid = (int/*16*/)callData;
      INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_contextData);
      
      if (new_doc_csid != m_contextData->fe.data->xfe_doc_csid) 
        {
          m_contextData->fe.data->xfe_doc_csid = new_doc_csid;
          INTL_SetCSIWinCSID(c, INTL_DocToWinCharSetID(new_doc_csid));
          // now let our observers know that the encoding
          // for this window needs to be changed.
          getToplevel()->notifyInterested(XFE_Frame::encodingChanged, callData);
        }
    }
  else if (IS_CMD(xfeCmdSetDefaultDocumentEncoding))
    {
      fe_globalPrefs.doc_csid = m_contextData->fe.data->xfe_doc_csid;
      /* clumsy to save all prefs but ... */
      if (!XFE_SavePrefs((char *)fe_globalData.user_prefs_file,&fe_globalPrefs))
        {
          fe_perror(m_contextData,XP_GetString(XFE_ERROR_SAVING_OPTIONS));
        }
    }
  else
    {
      XFE_View::doCommand(cmd);
    }
#undef IS_CMD
}

XP_Bool
XFE_HTMLView::isCommandSelected(CommandType cmd,
							 void *calldata, XFE_CommandInfo* info)
{
#define IS_CMD(command) (cmd == (command))
	XFE_Command* handler = getCommand(cmd);

	if (handler != NULL)
		return handler->isSelected(this, info);

	/* This method is designed for toggle button */
	/* We want to keep the toggle button to have the same state
     as its matched view */
  if (IS_CMD(xfeCmdChangeDocumentEncoding))
    {
      int/*16*/ test_doc_csid = (int/*16*/)calldata;
      if (test_doc_csid == m_contextData->fe.data->xfe_doc_csid) 
        return True;
      else
        return False;
    }
  return (XFE_View::isCommandSelected(cmd, calldata, info));
#undef IS_CMD
}

Boolean
XFE_HTMLView::isCommandEnabled(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) (cmd == (command))
  //  Boolean framesArePresent = hasSubViews();

  if (IS_CMD(xfeCmdBack))
    {
      return SHIST_CanGoBack (m_contextData);
    }
  else if (IS_CMD(xfeCmdForward))
    {
      return SHIST_CanGoForward (m_contextData);
    }
  else if (IS_CMD(xfeCmdAddBookmark))
    {
      return (SHIST_GetCurrent(&m_contextData->hist) != NULL);
    }
  else if (IS_CMD(xfeCmdHome))
    {
      return (fe_globalPrefs.home_document && *fe_globalPrefs.home_document);
    }
  else if (IS_CMD(xfeCmdDestinations))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdStopLoading))
    {
      return fe_IsContextStoppable(m_contextData)
        || CONTEXT_DATA (m_contextData)->active_url_count > 0;
    }
  else if (IS_CMD(xfeCmdStopAnimations))
    {
      return fe_IsContextLooping(m_contextData);
    }
  else if (IS_CMD(xfeCmdFindInObject))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdOpenPage)
           || IS_CMD(xfeCmdOpenPageChooseFile))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdSaveAs))
    {
      // Always save the topmost frame
      return True; //hasSubViews() == False;
    }
  else if (IS_CMD(xfeCmdSaveFrameAs))
    {
      // Save the frame with focus
      return hasSubViews() == True;
    }
  else if (IS_CMD(xfeCmdSendPage)
           || IS_CMD(xfeCmdSendLink))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdCut))
    {
      return fe_can_cut(m_contextData);
    }
  else if (IS_CMD(xfeCmdCopy))
    {
      return fe_can_copy(m_contextData);
    }
  else if (IS_CMD(xfeCmdPaste))
    {
      return fe_can_paste(m_contextData);
    }
  else if (IS_CMD(xfeCmdFindInObject))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdFindAgain))
    {
      fe_FindData *find_data = CONTEXT_DATA(m_contextData)->find_data;
      if (find_data && find_data->string && find_data->string[0] != '\0')
        return True;
      else
        return False;
    }
  else if (IS_CMD(xfeCmdEditPreferences))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdReload)
           || IS_CMD(xfeCmdFrameReload))
    {
      //return !fe_IsContextStoppable(m_contextData);
      return True;
    }
  else if (IS_CMD(xfeCmdSearch)) 
    {
      return True;
    }
  else if (IS_CMD(xfeCmdShowImages) 
           || IS_CMD(xfeCmdShowImage))
    {
      // Enable if autoload images is off or if there are delayed images.
      return (!fe_globalPrefs.autoload_images_p ||
              CONTEXT_DATA (m_contextData)->delayed_images_p);
    }
  else if (IS_CMD(xfeCmdRefresh))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdViewPageSource))
    {
      return fe_IsPageLoaded(m_contextData);
    }
  else if (IS_CMD(xfeCmdViewPageInfo))
    {
      return fe_IsPageLoaded(m_contextData);
    }
  else if (IS_CMD(xfeCmdViewFrameSource)
           ||IS_CMD(xfeCmdViewFrameInfo))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdViewSecurity))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdOpenUrl))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdOpenUrlFromSelection))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdPrint))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdSpacebar))
    {
      return True; // should this always be enabled?
    }
  else if (IS_CMD(xfeCmdOpenLinkNew)
           || IS_CMD(xfeCmdOpenFrameNew)
           || IS_CMD(xfeCmdOpenFrameInWindow))
    {
      return True;
    }
#ifdef EDITOR
  else if (IS_CMD(xfeCmdOpenLinkEdit))
    {
      return True;
    }
#endif
  else if (IS_CMD(xfeCmdSaveLink))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdCopyLink)
           || IS_CMD(xfeCmdCopyImage))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdSaveImage)
           || IS_CMD(xfeCmdSaveBGImage))
    {
      return True;
    } 
  else if (IS_CMD(xfeCmdSelectAll))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdUploadFile))
    {
      History_entry *he = SHIST_GetCurrent (&m_contextData->hist);
      Boolean b = False;

      if (he && he->address && (XP_STRNCMP (he->address, "ftp://", 6) == 0)
	  && (he->address[XP_STRLEN(he->address)-1] == '/'))
	{
	  b = True;
	}
      return b;
    }
  else if (cmd == xfeCmdChangeDocumentEncoding)
    {
      /* bstell: need to check if there are fonts */
      int/*16*/ doc_csid = (int/*16*/)calldata;
      return fe_IsCharSetSupported((int16)doc_csid);
    }
  else if (IS_CMD(xfeCmdSetDefaultDocumentEncoding))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdShowPopup)
           || IS_CMD(xfeCmdOpenImage)
           || IS_CMD(xfeCmdAddLinkBookmark)
           || IS_CMD(xfeCmdAddFrameBookmark))
    {
      return True;
    }
  else if (IS_CMD(xfeCmdPageServices))
    {
      return SHIST_CurrentHandlesPageServices(m_contextData);
    }
  else
    {
      return XFE_View::isCommandEnabled(cmd, calldata);
    }
#undef IS_CMD
}

Boolean
XFE_HTMLView::handlesCommand(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) (cmd == (command))
  if (IS_CMD(xfeCmdBack)
      || IS_CMD(xfeCmdForward)
      || IS_CMD(xfeCmdHome)
      || IS_CMD(xfeCmdDestinations)
      || IS_CMD(xfeCmdFindInObject)
      || IS_CMD(xfeCmdStopLoading)
      || IS_CMD(xfeCmdStopAnimations)
      || IS_CMD(xfeCmdOpenPage)
      || IS_CMD(xfeCmdOpenPageChooseFile)
      || IS_CMD(xfeCmdExit)
      || IS_CMD(xfeCmdSaveAs)
      || IS_CMD(xfeCmdSaveFrameAs)
      || IS_CMD(xfeCmdSendPage)
      || IS_CMD(xfeCmdSendLink)
      || IS_CMD(xfeCmdCut)
      || IS_CMD(xfeCmdCopy)
      || IS_CMD(xfeCmdPaste)
      || IS_CMD(xfeCmdFindInObject)
      || IS_CMD(xfeCmdFindAgain)
      || IS_CMD(xfeCmdEditPreferences)
      || IS_CMD(xfeCmdReload)
      || IS_CMD(xfeCmdFrameReload)
      || IS_CMD(xfeCmdShowImages)
      || IS_CMD(xfeCmdRefresh)
      || IS_CMD(xfeCmdViewPageSource)
      || IS_CMD(xfeCmdViewPageInfo)
      || IS_CMD(xfeCmdViewSecurity)
      || IS_CMD(xfeCmdAddBookmark)
      || IS_CMD(xfeCmdOpenUrl)
      || IS_CMD(xfeCmdOpenUrlFromSelection)
      || IS_CMD(xfeCmdPrint)
      || IS_CMD(xfeCmdSelectAll)
      || IS_CMD(xfeCmdUploadFile)

      || IS_CMD(xfeCmdChangeDocumentEncoding)
      || IS_CMD(xfeCmdSetDefaultDocumentEncoding)

      /* moving around the page */
      || IS_CMD(xfeCmdSpacebar)
      || IS_CMD(xfeCmdSearch)
      || IS_CMD(xfeCmdShowPopup)

      // context menu items
      || IS_CMD(xfeCmdOpenLinkNew)
      || IS_CMD(xfeCmdOpenFrameNew)
      || IS_CMD(xfeCmdOpenFrameInWindow)
#ifdef EDITOR
      || IS_CMD(xfeCmdOpenLinkEdit)
#endif
      || IS_CMD(xfeCmdSaveLink)
      || IS_CMD(xfeCmdCopyLink)
      || IS_CMD(xfeCmdCopyImage)
      || IS_CMD(xfeCmdSaveImage)
      || IS_CMD(xfeCmdSaveBGImage)
      || IS_CMD(xfeCmdViewFrameSource)
      || IS_CMD(xfeCmdViewFrameInfo)
      || IS_CMD(xfeCmdOpenImage)
      || IS_CMD(xfeCmdAddLinkBookmark)
      || IS_CMD(xfeCmdAddFrameBookmark)
      || IS_CMD(xfeCmdShowImage)
      || IS_CMD(xfeCmdPageServices)
      )
    {
      return True;
    }
  else
    {
      return XFE_View::handlesCommand(cmd, calldata);
    }
#undef IS_CMD
}

char *
XFE_HTMLView::commandToString(CommandType cmd, void *calldata, XFE_CommandInfo*)
{
#define IS_CMD(command) (cmd == (command))

  Boolean framesArePresent = hasSubViews();

  if (IS_CMD(xfeCmdSaveAs)) {
    if (framesArePresent) 
      return stringFromResource("saveFramesetAsCmdString");
    else
      return stringFromResource("saveAsCmdString");
  } else if (IS_CMD(xfeCmdEditPage)) {
	  char* name = cmd;

	  if (framesArePresent) 
		  name = "editFrameSet";

	  return getLabelString(name);
  } else if (IS_CMD(xfeCmdPrint)) {
    if (framesArePresent) 
      return stringFromResource("printFrameCmdString");
    else
      return stringFromResource("printCmdString");
/* Don't use "Select All in Frame" because a textfield may have focus
  } else if (IS_CMD(xfeCmdSelectAll)) {
    if (framesArePresent) 
      return stringFromResource("selectAllInFrameCmdString");
    else
      return stringFromResource("selectAllCmdString");
*/
  } else if (IS_CMD(xfeCmdFindInObject)) {
    if (framesArePresent) 
      return stringFromResource("findInFrameCmdString");
    else
      return stringFromResource("findInObjectCmdString");
  } else if (IS_CMD(xfeCmdStopLoading)) {
    if (fe_IsContextLooping(m_contextData))
      return stringFromResource("stopAnimationsCmdString");
    else
      return stringFromResource("stopLoadingCmdString");
  } else if (IS_CMD(xfeCmdFrameReload)) {
    if (framesArePresent)
      return stringFromResource("reloadWithFrameCmdString");
    else
      return stringFromResource("reloadNonFrameCmdString");
  } else if (IS_CMD(xfeCmdViewFrameSource)) {
    if (framesArePresent)
      return stringFromResource("pageSourceWithFrameCmdString");
    else
      return stringFromResource("pageSourceNonFrameCmdString");
  } else if (IS_CMD(xfeCmdViewFrameInfo)) {
    if (framesArePresent)
      return stringFromResource("pageInfoWithFrameCmdString");
    else
      return stringFromResource("pageInfoNonFrameCmdString");
  } else if (IS_CMD(xfeCmdOpenImage)) {
    if (m_imageUnderMouse && m_imageUnderMouse->address) {

#define BUFMAX 100
#define FILEBUFMAX 25
      static char buf[BUFMAX];
      static char filebuf[FILEBUFMAX];
      char *filename = XP_STRRCHR(m_imageUnderMouse->address, '/');

      if (filename) {
        filename++;

        if (*filename) {
          char *cmdLabel = stringFromResource("openImageCmdString");
          int cmdLength = XP_STRLEN(cmdLabel);
          int lengthLeft = BUFMAX - cmdLength - 3; //count " ()"
          int maxFileLength = FILEBUFMAX;

          if (lengthLeft <= 0) return cmdLabel;

          if (maxFileLength > lengthLeft)
            maxFileLength = lengthLeft;

          INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo(m_contextData);
          INTL_MidTruncateString(INTL_GetCSIWinCSID(c), filename, filebuf, maxFileLength);
          
          PR_snprintf(buf, sizeof(buf), "%s (%s)", 
                      stringFromResource("openImageCmdString"),
                      filebuf);

          return buf;
        }
      }
    }
    return stringFromResource("openImageCmdString");
  }
  else if (IS_CMD(xfeCmdChangeDocumentEncoding))
    {
      char *res = NULL;
      int doc_csid = (int)calldata;

      switch (doc_csid)
        {
          case CS_LATIN1:
            res = "latin1EncCmdString";
            break;
          case CS_LATIN2:
            res = "latin2EncCmdString";
            break;
          case CS_CP_1250:
            res = "Win1250EncCmdString";
            break;
          case CS_EUCJP_AUTO:
            res = "jaAutoEncCmdString";
            break;
          case CS_SJIS:
            res = "jaSJISEncCmdString";
            break;
          case CS_EUCJP:
            res = "jaEUCEncCmdString";
            break;
          case CS_BIG5:
            res = "twBig5EncCmdString";
            break;
          case CS_CNS_8BIT:
            res = "twEUCEncCmdString";
            break;
          case CS_GB_8BIT:
            res = "gbEUCEncCmdString";
            break;
          case CS_KSC_8BIT_AUTO:
            res = "krEUCEncCmdString";
            break;
          case CS_2022_KR:
            res = "2022krEncCmdString";
            break;
          case CS_KOI8_R:
            res = "koi8rEncCmdString";
            break;
          case CS_CP_1251:
            res = "Win1251EncCmdString";
            break;
          case CS_8859_5:
            res = "88595EncCmdString";
            break;
          case CS_ARMSCII8:
            res = "armenianEncCmdString";
            break;
          case CS_8859_7:
            res = "greekEncCmdString";
            break;
          case CS_CP_1253:
            res = "Win1253EncCmdString";
            break;
          case CS_8859_9:
            res = "88599EncCmdString";
            break;
          case CS_UTF8:
            res = "unicode_utf8EncCmdString";
            break;
          case CS_UTF7:
            res = "unicode_utf7EncCmdString";
            break;
          case CS_USRDEF2:
            res = "otherEncCmdString";
            break;
          default:
            XP_ASSERT(0);
            break;
        }
      
      if (res)
        return stringFromResource(res);
      else
        return NULL;
    }
  else if (IS_CMD(xfeCmdSetDefaultDocumentEncoding))
    {
      char *res;
      res = "setDefaultDocCSID";
      return stringFromResource(res);
    }
  return NULL;
#undef IS_CMD
}

void XFE_HTMLView::setScrollbarsActive(XP_Bool b)
{
  unsigned long x;
  unsigned long y;
  XFE_View::setScrollbarsActive(b);
  if (m_contextData) {
	x = CONTEXT_DATA(m_contextData)->document_x;
	y = CONTEXT_DATA(m_contextData)->document_y;
	fe_SetDocPosition(m_contextData, x, y);
  }
}


XFE_CALLBACK_DEFN(XFE_HTMLView, DocEncoding)(XFE_NotificationCenter *,
					     void *, void */*callData*/)
{
  fe_ReLayout(m_contextData, NET_DONT_RELOAD);
}

XFE_CALLBACK_DEFN(XFE_HTMLView, ChangeLinksAttribute)(XFE_NotificationCenter *,
													  void *, void */*callData*/)
{
	Dimension w = 0, h = 0;
    XtVaGetValues(CONTEXT_DATA(m_contextData)->drawing_area,
				  XmNwidth, &w,
				  XmNheight, &h,
				  0);

	fe_RefreshArea(m_contextData,
				   CONTEXT_DATA(m_contextData)->document_x,
				   CONTEXT_DATA(m_contextData)->document_y,
				   w, h);
}

XFE_CALLBACK_DEFN(XFE_HTMLView, ChangeDefaultColors)(XFE_NotificationCenter *,
													 void *, void */*callData*/)
{
	LO_Color *color;

	color = &fe_globalPrefs.links_color;
	CONTEXT_DATA(m_contextData)->link_pixel = 
		fe_GetPixel(m_contextData, color->red, color->green, color->blue);
	LO_SetDefaultColor(LO_COLOR_LINK, color->red, color->green, color->blue);

	color = &fe_globalPrefs.vlinks_color;
	CONTEXT_DATA(m_contextData)->vlink_pixel = 
		fe_GetPixel(m_contextData, color->red, color->green, color->blue);
	LO_SetDefaultColor(LO_COLOR_VLINK, color->red, color->green, color->blue);

	color = &fe_globalPrefs.text_color;
	CONTEXT_DATA(m_contextData)->default_fg_pixel = 
		fe_GetPixel(m_contextData, color->red, color->green, color->blue);
	LO_SetDefaultColor(LO_COLOR_FG, color->red, color->green, color->blue);

	color = &fe_globalPrefs.background_color;
	CONTEXT_DATA(m_contextData)->default_bg_pixel = 
		fe_GetPixel(m_contextData, color->red, color->green, color->blue);
	LO_SetDefaultColor(LO_COLOR_BG, color->red, color->green, color->blue);

	fe_ReLayout(m_contextData, NET_RESIZE_RELOAD);
}

XFE_CALLBACK_DEFN(XFE_HTMLView, ChangeDefaultFont)(XFE_NotificationCenter *,
												   void *, void */*callData*/)
{
	LO_InvalidateFontData(m_contextData);
	fe_ReLayout(m_contextData, NET_RESIZE_RELOAD);
}

void
XFE_HTMLView::translateToDocCoords(int x, int y,
				   int *doc_x, int *doc_y)
{
  *doc_x = x + CONTEXT_DATA(m_contextData)->document_x;
  *doc_y = y + CONTEXT_DATA(m_contextData)->document_y;
}

/* caller must free the returned URL_Struct */
URL_Struct *
XFE_HTMLView::URLAtPosition(int x, int y, CL_Layer *layer)
{
  LO_Element *le = layoutElementAtPosition(x, y, layer);
  URL_Struct *url_s = NULL;
  History_entry *h;

  if (le)
    {
      h = SHIST_GetCurrent(&m_contextData->hist);

      switch (le->type)
	{
	case LO_TEXT:
	  if (le->lo_text.anchor_href)
	    url_s = NET_CreateURLStruct ((char *)le->lo_text.anchor_href->anchor, NET_DONT_RELOAD);
	  break;
	case LO_IMAGE:
            // check for client-side image map
          if (le->lo_image.image_attr->usemap_name != NULL) {
              LO_AnchorData *anchor_href;

              long ix = le->lo_image.x + le->lo_image.x_offset;
              long iy = le->lo_image.y + le->lo_image.y_offset;
              long mx = x - ix - le->lo_image.border_width;
              long my = y - iy - le->lo_image.border_width;

              anchor_href = LO_MapXYToAreaAnchor(m_contextData, (LO_ImageStruct *)le,mx, my);
              url_s = NET_CreateURLStruct ((char *)anchor_href->anchor, NET_DONT_RELOAD);
          }
          // check for regular image map
          else if (le->lo_image.anchor_href) {
            url_s = NET_CreateURLStruct ((char *)le->lo_image.anchor_href->anchor, NET_DONT_RELOAD);
            /* if this is an image map (and not an about: link), add the coordinates to the URL */    
            if (strncmp (url_s->address, "about:", 6)!=0 &&
              le->lo_image.image_attr->attrmask & LO_ATTR_ISMAP) {
                int doc_x, doc_y;
                int ix = le->lo_image.x + le->lo_image.x_offset;
                int iy = le->lo_image.y + le->lo_image.y_offset;
                int x_coord, y_coord;
                
                translateToDocCoords(x, y, &doc_x, &doc_y);
		  
                x_coord = doc_x - ix - le->lo_image.border_width;
                y_coord = doc_y - iy - le->lo_image.border_width;
                NET_AddCoordinatesToURLStruct (url_s,
                                               ((x_coord < 0) ? 0 : x_coord),
                                               ((y_coord < 0) ? 0 : y_coord));
            }
          }
            
	  break;
	}

      if (h && url_s)
		  url_s->referer = fe_GetURLForReferral(h);
    }

  return url_s;
}

/* caller must free the returned URL_Struct */
URL_Struct *
XFE_HTMLView::imageURLAtPosition(int x, int y, CL_Layer *layer)
{
  LO_Element *le = layoutElementAtPosition(x, y, layer);
  URL_Struct *image_url_s = NULL;
  History_entry *h;

  if (le && le->type == LO_IMAGE)
    {
      h = SHIST_GetCurrent(&m_contextData->hist);

      /* if we're currently looking at an image, construct the URL out of the history entry */
      if (h && h->is_binary)
	{
	  image_url_s = NET_CreateURLStruct(h->address, NET_DONT_RELOAD);
	}
      else /* otherwise, construct it out of the info that layout gives us. */
	{
	  char *image_url = (char*)le->lo_image.image_url;;
	  
	  /* If this is an internal-external-reconnect image, then the *real* url
	     follows the "internal-external-reconnect:" prefix. Gag gag gag. */
	  if (image_url) 
	    {
	      if (!XP_STRNCMP (image_url, "internal-external-reconnect:", 28))
		image_url += 28;
	      
	      image_url_s = NET_CreateURLStruct (image_url, NET_DONT_RELOAD);
	    }
	  
	  if (image_url_s)
	    {
	      /* if this is an image map (and not an about: link), add the coordinates to the URL */
	      if (strncmp (image_url_s->address, "about:", 6) &&
		  le->lo_image.image_attr->attrmask & LO_ATTR_ISMAP)
		{
		  int doc_x, doc_y;
		  int ix = le->lo_image.x + le->lo_image.x_offset;
		  int iy = le->lo_image.y + le->lo_image.y_offset;
		  int x_coord, y_coord;
		  
		  translateToDocCoords(x, y, &doc_x, &doc_y);
		  
		  x_coord = doc_x - ix - le->lo_image.border_width;
		  y_coord = doc_y - iy - le->lo_image.border_width;
		  NET_AddCoordinatesToURLStruct (image_url_s,
						 ((x_coord < 0) ? 0 : x_coord),
						 ((y_coord < 0) ? 0 : y_coord));
		}
	    }
	}

      if (h && image_url_s)
		  image_url_s->referer = 
			  fe_GetURLForReferral((History_entry *)&m_contextData->hist);
    }

  return image_url_s;
}

LO_Element *
XFE_HTMLView::layoutElementAtPosition(int x, int y, CL_Layer *layer)
{
  int doc_x, doc_y;
  LO_Element *le;

  translateToDocCoords(x, y, &doc_x, &doc_y);

  le = LO_XYToElement (m_contextData, x, y, layer);

  return le;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_HTMLView::reload(Widget, XEvent *event, XP_Bool onlyReloadFrame)
{
  fe_UserActivity (m_contextData);
/*  if (fe_hack_self_inserting_accelerator (widget, NULL, NULL))
    return; */
  
  MWContext *context = m_contextData;

  if (onlyReloadFrame) {
    context = fe_GetFocusGridOfContext(m_contextData);
    if (context == NULL)
      context = m_contextData;
  }

  if (event->xkey.state & ShiftMask)
    fe_ReLayout (context, NET_SUPER_RELOAD);
  else
    fe_ReLayout (context, NET_NORMAL_RELOAD);
}
//////////////////////////////////////////////////////////////////////////
void
XFE_HTMLView::findLayerForPopupMenu(Widget widget, XEvent *event)
{
  MWContext *context;
  CL_Event layer_event;
  fe_EventStruct fe_event;

  /* If this is a grid cell, set focus to it */
  context = fe_MotionWidgetToMWContext (widget);
  if (!context)
	return;
  if (context && context->is_grid_cell)
    fe_UserActivity (context);

  /* Now grab the toplevel context and go. */
  context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (! context) return;

  /* Popup menu not valid for MWContextDialog */
  if (context->type == MWContextDialog) return;

  /* Fill in FE part of layer_event. */
#ifdef LAYERS_FULL_FE_EVENT
  fe_event.event = event;
  // fe_event.av = cav;
  // fe_event.ac = cac;
  fe_event.mouse_action = FE_POPUP_MENU;
#else
  fe_event_stuff(context,&fe_event,event,0,0,FE_POPUP_MENU);
  layer_event.fe_event_size = sizeof(fe_event);
#endif
  fe_event.data = (void *) this;

  layer_event.fe_event = (void *)&fe_event;

  layer_event.type = CL_EVENT_MOUSE_BUTTON_DOWN;
  layer_event.which = event->xbutton.button;
  layer_event.modifiers = xfeToLayerModifiers(event->xbutton.state);

  if (context->compositor)
      {
          unsigned long x, y;

          fe_EventLOCoords (context, event, &x, &y);
          layer_event.x = x;
          layer_event.y = y;
          CL_DispatchEvent(context->compositor, &layer_event);
      }
  else
      {
          doPopup (context, NULL, &layer_event);
      }
}

URL_Struct*
XFE_HTMLView::URLUnderMouse()
{
	return m_urlUnderMouse;
}

URL_Struct*
XFE_HTMLView::ImageUnderMouse()
{
	return m_imageUnderMouse;
}

URL_Struct*
XFE_HTMLView::BackgroundUnderMouse()
{
	return m_backgroundUnderMouse;
}
 
void
XFE_HTMLView::doPopup(MWContext *context, CL_Layer *layer,
		      CL_Event *layer_event)
{ 
  fe_EventStruct *fe_event = (fe_EventStruct *) layer_event->fe_event;
#ifdef LAYERS_FULL_FE_EVENT
  XEvent *event = fe_event->event;
#else
  XEvent *event = fe_event_extract(fe_event,NULL,NULL,NULL);
#endif
  Boolean image_delayed_p = False;
  History_entry *h = NULL;
  char *image_url;
  Widget w = CONTEXT_WIDGET (context);

  if (!w)
    w = m_widget;

  m_urlUnderMouse = NULL;
  m_imageUnderMouse = NULL;
  m_backgroundUnderMouse = NULL;

  // Add dynamic entries dependent on what's under the mouse
  int x = layer_event->x;
  int y = layer_event->y;
  LO_Element *le = LO_XYToElement (context, x, y, layer);

  if (le)
    switch (le->type)
      {
      case LO_TEXT:
        if (le->lo_text.anchor_href)
          m_urlUnderMouse =
            NET_CreateURLStruct ((char *) le->lo_text.anchor_href->anchor,
                                 NET_DONT_RELOAD);
        break;
      case LO_IMAGE:
        if (le->lo_image.anchor_href)
          m_urlUnderMouse =
            NET_CreateURLStruct ((char *) le->lo_image.anchor_href->anchor,
                                 NET_DONT_RELOAD);

        /* If this is an internal-external-reconnect image, then the *real*
           url follows the "internal-external-reconnect:" prefix. Gag gag.*/
        image_url = (char*)le->lo_image.image_url;
        if (image_url) {
          if (!XP_STRNCMP (image_url, "internal-external-reconnect:", 28))
            image_url += 28;
          m_imageUnderMouse =
            NET_CreateURLStruct (image_url, NET_DONT_RELOAD);
        }
        
        image_delayed_p = le->lo_image.is_icon &&
          le->lo_image.icon_number == IL_IMAGE_DELAYED;
        
        if (m_urlUnderMouse &&
            strncmp (m_urlUnderMouse->address, "about:", 6) &&
            le->lo_image.image_attr->attrmask & LO_ATTR_ISMAP)
          {
            /* cribbed from fe_activate_link_action() */
            long cx = layer_event->x + CONTEXT_DATA (context)->document_x;
            long cy = layer_event->y + CONTEXT_DATA (context)->document_y;
            long ix = le->lo_image.x + le->lo_image.x_offset;
            long iy = le->lo_image.y + le->lo_image.y_offset;
            long x = cx - ix - le->lo_image.border_width;
            long y = cy - iy - le->lo_image.border_width;
            NET_AddCoordinatesToURLStruct (m_urlUnderMouse,
                                           ((x < 0) ? 0 : x),
                                           ((y < 0) ? 0 : y));
          }
        break;
      default:
        // must be something we don't care about
        break;
      }

  if (m_imageUnderMouse == NULL)
    {
      const char *backgroundURL;
      CL_Layer *parent_layer;
      char *layer_name;
      layer_name = CL_GetLayerName(layer);
      if (layer_name && ((XP_STRCMP(layer_name, LO_BODY_LAYER_NAME) == 0) ||
                         (XP_STRCMP(layer_name, LO_BACKGROUND_LAYER_NAME) == 0)))
        {
          parent_layer = CL_GetLayerParent(layer);
          //pLOImage = LO_GetLayerBackdropImage(parent_layer);
          backgroundURL = LO_GetLayerBackdropURL(parent_layer);
        }
      else
        {
          //pLOImage = LO_GetLayerBackdropImage(layer);
          backgroundURL = LO_GetLayerBackdropURL(layer);
        }

      if (backgroundURL) 
        {
          m_backgroundUnderMouse =
            NET_CreateURLStruct (backgroundURL, NET_DONT_RELOAD);
        }
    }

  if (h && h->is_binary)
    {
      if (m_imageUnderMouse)
        NET_FreeURLStruct (m_imageUnderMouse);
      m_imageUnderMouse = NET_CreateURLStruct (h->address, NET_DONT_RELOAD);
    }

  /* Add the referer to the URLs. */
  /* Ahh! h is never set.  Fix on 5.0 tip, can't risk
	 fixing this for 4.04. -mcafee */
  if (h && h->address)
    {
      if (m_urlUnderMouse)
        m_urlUnderMouse->referer = fe_GetURLForReferral(h);
      if (m_imageUnderMouse)
        m_imageUnderMouse->referer = fe_GetURLForReferral(h);
      if (m_backgroundUnderMouse)
        m_backgroundUnderMouse->referer = fe_GetURLForReferral(h);
    }


  if (hasInterested(XFE_HTMLView::popupNeedsShowing))
	  {
		  notifyInterested(XFE_HTMLView::popupNeedsShowing, event);
	  }
  else
	  {
		  // Delete the old popup if needed
		  if (m_popup != NULL)
		  {
			  delete m_popup;
		  }

		  // Create and initialize the popup
		  m_popup = new XFE_PopupMenu ("popup",
									   (XFE_Frame *) m_toplevel, 
									   XfeAncestorFindApplicationShell(w));
		  
		  XP_Bool isBrowserLink = FALSE;
		  
		  if (m_urlUnderMouse != NULL
			  && XP_STRNCMP ("mailto:", m_urlUnderMouse->address, 7) != 0
			  && XP_STRNCMP ("telnet:", m_urlUnderMouse->address, 7) != 0
			  && XP_STRNCMP ("tn3270:", m_urlUnderMouse->address, 7) != 0
			  && XP_STRNCMP ("rlogin:", m_urlUnderMouse->address, 7) != 0)
			  {
				  isBrowserLink = TRUE;
			  }
		  
		  XP_Bool isLink = m_urlUnderMouse != NULL;
		  XP_Bool isImage = m_imageUnderMouse != NULL;
		  XP_Bool isFrame = hasSubViews();
		  XP_Bool isBrowser = m_contextData->type == MWContextBrowser;
		  XP_Bool isBackground = m_backgroundUnderMouse != NULL;
		  
		  XP_Bool needSeparator = FALSE;
		  XP_Bool haveAddedItems = FALSE;
		  
#define ADD_MENU_SEPARATOR {                     \
    if (haveAddedItems) {                        \
      needSeparator = TRUE;                      \
      haveAddedItems = FALSE;                    \
    }                                            \
}
#define ADD_SPEC(theSpec) {                      \
  if (needSeparator)                             \
    m_popup->addMenuSpec(separator_spec);        \
  m_popup->addMenuSpec(theSpec);                 \
  needSeparator = FALSE;                         \
  haveAddedItems = TRUE;                         \
}

		  if (isBrowser)                           ADD_SPEC ( go_spec );
		  if (isImage && image_delayed_p)          ADD_SPEC ( showImage_spec );
		  if (isCommandEnabled(xfeCmdStopLoading)) ADD_SPEC ( stopLoading_spec );
		  ADD_MENU_SEPARATOR;
		  if (isBrowserLink)                       ADD_SPEC ( openLinkNew_spec );
		  if (isFrame)                             ADD_SPEC ( openFrameNew_spec );
#ifdef EDITOR
		  if (isBrowserLink)                       ADD_SPEC ( openLinkEdit_spec );
#endif
		  ADD_MENU_SEPARATOR;
		  if (isBrowser)                           ADD_SPEC ( page_details_spec );
		  if (isImage)                             ADD_SPEC ( openImage_spec );
		  ADD_MENU_SEPARATOR;
		  if (isLink) {                            ADD_SPEC ( addLinkBookmark_spec );
		  } else if (isFrame) {                    ADD_SPEC ( addFrameBookmark_spec );
		  } else if (isBrowser) {                  ADD_SPEC ( addBookmark_spec );
		  }
		  if (isBrowser)                           ADD_SPEC ( sendPage_spec );
		  ADD_MENU_SEPARATOR;
		  if (isBrowserLink)                       ADD_SPEC ( saveLink_spec );
		  if (isImage)                             ADD_SPEC ( saveImage_spec );
		  if (isBackground)                        ADD_SPEC ( saveBGImage_spec );
		  ADD_MENU_SEPARATOR;
		  if (isCommandEnabled(xfeCmdCopy))        ADD_SPEC ( copy_spec );
		  if (isLink)                              ADD_SPEC ( copyLink_spec );
		  if (isImage)                             ADD_SPEC ( copyImage_spec );
		  // Finish up the popup
		  m_popup->position (event);
		  m_popup->show();
	  }
}
#if DO_NOT_PASS_LAYER_N_EVENT
void 
XFE_HTMLView::tipCB(MWContext *context, int x, int y, char* alt_text,
					XtPointer cb_data)
{
	XFE_TipStringCallbackStruct* cbs = (XFE_TipStringCallbackStruct*)cb_data;
	Widget w = CONTEXT_WIDGET (context);
	
	if (!w)
		w = m_widget;

	// tip string
	if (alt_text) {
		/* to be free by the caller
		 */
		*cbs->string = XP_STRDUP((char *)alt_text);
	}/* if */

	// positioning
	cbs->x = x;
	cbs->y = y+ 20;

	// correct offsets
	if (CONTEXT_DATA(context)->drawing_area &&
		CONTEXT_DATA(context)) {
		cbs->x -= CONTEXT_DATA (context)->document_x;
		cbs->y -= CONTEXT_DATA (context)->document_y;
	}/* if */
}
#else
void 
XFE_HTMLView::tipCB(MWContext *context, CL_Layer *layer,
					CL_Event *layer_event,	XtPointer cb_data)
{
	XFE_TipStringCallbackStruct* cbs = (XFE_TipStringCallbackStruct*)cb_data;
	Widget w = CONTEXT_WIDGET (context);
	
	if (!w)
		w = m_widget;

	/*
	 */
	int x = layer_event->x;
	int y = layer_event->y;

	/* Tao: fix BSR in XFE_HTMLView::tipCB; 
	 * allocatedfrom fe_HTMLViewTooltipsEH
	 */
	XP_FREEIF(layer_event);

	LO_Element *le = LO_XYToElement (context, x, y, layer);

	// tip string
	if (le && 
		le->type == LO_IMAGE &&
		le->lo_image.alt &&
		le->lo_image.alt_len ) {

		/* to be free by the caller
		 */
		*cbs->string = XP_STRDUP((char *)le->lo_image.alt);
	}/* if */

	// positioning
	cbs->x = x;
	cbs->y = y+ 20;

	// correct offsets
	if (CONTEXT_DATA(context)->drawing_area &&
		CONTEXT_DATA(context)) {
		cbs->x -= CONTEXT_DATA (context)->document_x;
		cbs->y -= CONTEXT_DATA (context)->document_y;
	}/* if */
}
#endif

extern "C" void
fe_HTMLViewDoPopup (MWContext *context, CL_Layer *layer,
		    CL_Event *layer_event)
{
  fe_EventStruct *fe_event = (fe_EventStruct *) layer_event->fe_event;
  XFE_HTMLView *htmlview = (XFE_HTMLView *) fe_event->data;
  if (htmlview)
	htmlview->doPopup (context, layer, layer_event);
#ifdef DEBUG
  else
	fprintf(stderr,"fe_HTMLViewDoPopup(): htmlview is NULL\n");
#endif
}

#if DO_NOT_PASS_LAYER_N_EVENT
extern "C" void
fe_HTMLViewTooltips(MWContext *context, 
					int x, int y, char *alt_text, 
					XFE_TipStringCallbackStruct *cb_info)
{
	/* let's get htmlview from context
	 */
	Widget drawingArea = CONTEXT_DATA(context)->drawing_area;
	if (drawingArea) {
		/* Get view from widget
		 */
		XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));
		if (f) {
			XFE_View *view = f->widgetToView(drawingArea);
			if (view) {
				if (f->getType() == FRAME_BROWSER) {
					XFE_HTMLView *htmlview = (XFE_HTMLView *) view;
					htmlview->tipCB(context, x, y, alt_text, 
									(XtPointer)cb_info);
				}/* if */
			}/* if */
		}/* if f */
	}/* if drawingArea */
}/* fe_HTMLViewTooltips() */
#else
extern "C" void
fe_HTMLViewTooltips(MWContext *context, 
					CL_Layer *layer,
					CL_Event *layer_event, 
					XFE_TipStringCallbackStruct *cb_info)
{
	/* let's get htmlview from context
	 */
	Widget drawingArea = CONTEXT_DATA(context)->drawing_area;
	if (drawingArea) {
		/* Get view from widget
		 */
		XFE_Frame *f = ViewGlue_getFrame(XP_GetNonGridContext(context));
		if (f) {
#if defined(DEBUG_tao)
			printf("\n fe_HTMLViewTooltips, type=%d\n", 
					   f->getType());
#endif
			XFE_View *view = f->widgetToView(drawingArea);
			if (view) {
				if (f->getType() == FRAME_BROWSER) {
#if defined(DEBUG_tao)
					printf("\n drawingArea=%x, viewWidget=%x\n", 
						   drawingArea, view->getBaseWidget());
#endif
					XFE_HTMLView *htmlview = (XFE_HTMLView *) view;
					htmlview->tipCB(context, layer, layer_event, 
									(XtPointer)cb_info);
				}/* if */
			}/* if */
#ifdef DEBUG
			else
				fprintf(stderr,"fe_HTMLViewTooltips(): htmlview is NULL\n");
#endif
		}/* if f */
	}/* if drawingArea */
}/* fe_HTMLViewTooltips() */
#endif

// Need to be able to access the popup menu from XFE_Command handlers.
// I've made HTML_View->m_popup visible to EditorView sub-class. These
// getter/setters will allow the XFE_Command handlers access...djw
XFE_PopupMenu*
XFE_HTMLView::getPopupMenu()
{
	return m_popup;
}

void
XFE_HTMLView::setPopupMenu(XFE_PopupMenu* popup)
{
	if (m_popup != NULL)
		delete m_popup;
	m_popup = popup;
}


static void fe_raise_frame(MWContext * context)
{
	if (!context)
	{
		return;
	}

	/* Find the frame from the context */
	XFE_Frame * frame = ViewGlue_getFrame(XP_GetNonGridContext(context));

	if (!frame)
	{
		return;
	}
		
	if (frame && frame->isAlive())
	{
		Widget w = frame->getBaseWidget();
		
		XMapRaised(XtDisplay(w),XtWindow(w));
		
		XtPopup(w,XtGrabNone);
	}
}

#define APP_SET_BUSY(busy)							\
{													\
	if (busy)										\
	{												\
		XFE_MozillaApp::theApp()->notifyInterested(	\
			XFE_MozillaApp::appBusyCallback);		\
	}												\
	else											\
	{												\
		XFE_MozillaApp::theApp()->notifyInterested(	\
			XFE_MozillaApp::appNotBusyCallback);	\
	}												\
}

/*
 * Open a target url.  
 * The context can be NULL, but anchor_data must be valid.
 * anchor_data->anchor can be anything understood by either browser or 
 * mail news frames.
 */
void
fe_openTargetUrl(MWContext * context,LO_AnchorData * anchor_data)
{
	URL_Struct *	url = NULL;
	int				url_type;
	XP_Bool			is_msg_url = False;
	XP_Bool			is_message_center = False;
	XFE_Frame *		parent_frame = NULL;

	XP_ASSERT( anchor_data != NULL );

	if (context)
	{
		parent_frame = ViewGlue_getFrame(context);
	}

	url = NET_CreateURLStruct((char *) anchor_data->anchor, NET_NORMAL_RELOAD);

	/* url type */
	url_type = NET_URL_Type(url->address);

	/* Is this a mail/news url ? */
	is_msg_url = (url_type == MAILBOX_TYPE_URL || url_type == NEWS_TYPE_URL || url_type == IMAP_TYPE_URL);

	/* Determine if this is the message center */
	is_message_center = 
		(parent_frame && parent_frame->getType() == FRAME_MAILNEWS_FOLDER);

	/*
	 * In order for mail/news bookmarks to work, at least one messenger 
	 * has to exist.
	 */
	if (is_msg_url)
	{
#ifdef MOZ_MAIL_NEWS
		XP_List * messenger_list = 
			XFE_MozillaApp::theApp()->getFrameList(FRAME_MAILNEWS_MSG);

		/* Create a messenger if none exist */
		if (!XP_ListCount(messenger_list))
		{
			APP_SET_BUSY(True);

			fe_showInbox(FE_GetToplevelWidget(),
						 NULL,
						 NULL,
						 fe_globalPrefs.reuse_thread_window,
						 False);

			APP_SET_BUSY(False);
		}
#endif  /* MOZ_MAIL_NEWS */
	}

	/* A msg url being opened from either a message frame or the messenger */
	if (context &&
		(context->type == MWContextMailMsg) &&
		is_msg_url && 
		!is_message_center)
	{
		/* Nothing, fe_GetURL() below will deal with it */
	}
	/* A msg url being opened from any other frame (even message center) */
	else if (is_msg_url)
	{
#ifdef MOZ_MAIL_NEWS
		/*
		 * Trying to be clever about re-using messages frames.  Needs more
		 * work and maybe a better method so its out.
		 */
#if 0
		XP_Bool found = False;

		/* Try to reuse a message frame */
		if (fe_globalPrefs.reuse_msg_window)
		{
			XP_List * msg_list = 
				XFE_MozillaApp::theApp()->getFrameList(FRAME_MAILNEWS_MSG);

			if (XP_ListCount(msg_list))
			{
				XFE_Frame * msg_frame = (XFE_Frame *) 
					XP_ListGetObjectNum(msg_list,0);
				
				context = msg_frame->getContext();

				found = True;
			}
		}
#endif
		APP_SET_BUSY(True);
		
		/* Get a mail message context */
		context = fe_showMsg(FE_GetToplevelWidget(),
							 parent_frame,
							 NULL,
							 NULL,
							 MSG_MESSAGEKEYNONE,
							 fe_globalPrefs.reuse_msg_window);

		APP_SET_BUSY(False);
#endif  /* MOZ_MAIL_NEWS */
	}
	/* URLS which fe_GetURL() can handle and which require no extra windows */
	else if (url_type == MAILTO_TYPE_URL ||
			 url_type == RLOGIN_TYPE_URL ||
			 url_type == TN3270_TYPE_URL ||
			 url_type == TELNET_TYPE_URL)
	{
		fe_GetURL(context,url,False);

		return;
	}
#ifdef MOZ_MAIL_NEWS
	/* addbook: */
	else if (url_type == ADDRESS_BOOK_TYPE_URL)
	{
		APP_SET_BUSY(True);

		fe_showAddrBook(FE_GetToplevelWidget(),parent_frame,NULL);

		APP_SET_BUSY(False);

 		return;
	}
#endif  /* MOZ_MAIL_NEWS */
	/* Anything else which probably requires a browser */
	else
	{
		/* Check for non-browser contexts */
		if (!context || (context->type != MWContextBrowser))
		{
			/* Reuse a browser if possible */
			MWContext * reuse_context=fe_FindNonCustomBrowserContext(context);
			
			if (reuse_context)
			{
				context = reuse_context;
			}
			/* Otherwise create a new browser */
			else
			{
				APP_SET_BUSY(True);

				/* If the target window doesn't exist, create it. */
				MWContext * new_context =
					fe_MakeWindow(FE_GetToplevelWidget(),
								  context,
								  url,
								  (char *) anchor_data->target,
								  MWContextBrowser, 
								  False);

				/* Raise the frame */
				fe_raise_frame(new_context);
				
				APP_SET_BUSY(False);

				/*
				 * fe_MakeWindow() loads the url, so we are done.  We dont
				 * need the extra fe_GetURL() below.
				 */
				return;
			}
			
			/* If there is a target window, deal with it. */
			if (anchor_data->target)
			{
				MWContext * target_context;
				
				target_context = 
					XP_FindNamedContextInList(context,
											  (char *)anchor_data->target);
				
				/* This prevents the parser from double-guessing us. */
				url->window_target = strdup ("");
				
				if (target_context)
				{
					context = target_context;
				}
			}
		}
	}

	XP_ASSERT( context != NULL );
	XP_ASSERT( url != NULL );

	if (!context || !url)
	{
		return;
	}

	/* Raise the frame */
	fe_raise_frame(context);

	/* Do the magic */
	fe_GetURL(context,url,False);
}

void
XFE_HTMLView::openFileAction (String *av, Cardinal *ac)
{
  MWContext *context = m_contextData;
  MWContext *old_context = NULL;

  /* See also fe_open_url_action() */
  Boolean other_p = False;

  fe_UserActivity (context);
  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;

  if (*ac > 1 && av[*ac-1] )
    {
      if ((!strcasecomp (av[*ac-1], "new-window") ||
           !strcasecomp (av[*ac-1], "new_window") ||
           !strcasecomp (av[*ac-1], "newWindow") ||
           !strcasecomp (av[*ac-1], "new")))
        {
          other_p = True;
          (*ac)--;
        }
      else if ((old_context = XP_FindNamedContextInList(context, av[*ac-1])))
        {
          context = old_context;
          other_p = False;
          (*ac)--;
        }
      else 
        {
          other_p = True;
          (*ac)--;
        }
    }

  if (*ac == 1 && av[0])
    {
#ifndef PATH_MAX
#define PATH_MAX 1024     
#endif
      char newURL [PATH_MAX];
      URL_Struct *url_struct;
      
      if (av[0][0] == '/')
        {
          PR_snprintf (newURL, sizeof (newURL), "file:%.900s", av[0]);
        }
      else
        {
          char cwd_buf[PATH_MAX];
          getcwd(cwd_buf, PATH_MAX);

          PR_snprintf (newURL, sizeof (newURL), "file:%s/%.900s",
                       cwd_buf, av[0]);
        }
#ifdef DEBUG_slamm
      fprintf(stderr,"%s",newURL);
#endif
      url_struct = NET_CreateURLStruct (newURL, NET_DONT_RELOAD);
      if (other_p) 
        {
          fe_MakeWindow (XtParent (CONTEXT_WIDGET (context)),
                         context, url_struct, NULL, 
                         MWContextBrowser, FALSE);
        }
      else
        fe_GetURL (context, url_struct, FALSE);
    }
  else if (*ac > 1)
    {
      fprintf (stderr,
	       XP_GetString(XFE_COMMANDS_OPEN_FILE_USAGE),
	       fe_progname);
    }
  else
    {
      fe_OpenURLChooseFileDialog (context);
    }
}
