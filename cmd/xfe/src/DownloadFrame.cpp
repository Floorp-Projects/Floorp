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
   DownloadFrame.cpp -- class definition for the save to disk window.
   Created: Chris Toshok <toshok@netscape.com>, 21-Feb-97
 */



#include "DownloadFrame.h"
#include "MozillaApp.h"
#include "Dashboard.h"
#include "ViewGlue.h"
#include "Logo.h"

#include "xlate.h"

#include <Xm/XmP.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/Form.h>
#include <Xm/MessageB.h>
#include <Xm/PushB.h>

#include <Xfe/Xfe.h>

#include "xpgetstr.h"
#include "felocale.h"

extern int XFE_SAVE_AS_TYPE_ENCODING;
extern int XFE_SAVE_AS_TYPE;
extern int XFE_SAVE_AS_ENCODING;
extern int XFE_SAVE_AS;
extern int XFE_ERROR_OPENING_FILE;
extern int XFE_ERROR_READ_ONLY;
extern int XFE_COMMANDS_SAVE_AS_USAGE;
extern int XFE_COMMANDS_SAVE_AS_USAGE_TWO;

#ifdef DEBUG_toshok
#define D(x) x
#else
#define D(x)
#endif

/* Externs from dialog.c: */
extern "C" {
  void fe_set_scrolled_default_size(MWContext *context);
  int fe_await_synchronous_url (MWContext *context);
  int fe_GetSynchronousURL(MWContext *context,
						   Widget widget_to_grab,
						   const char *title,
						   URL_Struct *url,
						   int output_format,
						   void *call_data);
  void fe_save_top_frame_as_cb (Widget widget, XtPointer closure,
                                XtPointer call_data);
}

// Note:  isFileP - if true and the file_name is a directory,
//                  we return False...
//
//        existsP - if true, the file_name must exist or we return False,
//                  otherwise we just make sure we can read/write into the 
//                  directory specified in file_name...
//
XP_Bool
XFE_StatReadWrite(const char *file_name, XP_Bool isFileP, XP_Bool existsP) 
{
	// NOTE:  let's get the directory that this file resides in...
	//
	XP_StatStruct  dir_info;
	XP_StatStruct  file_info;
	XP_Bool        isDir = False;
	char*          p;
	char*          base_name = (char *) XP_ALLOC(XP_STRLEN(file_name) + 1);
	
	p = XP_STRRCHR(file_name, '/');
	
	if (p) {
		if (p == file_name) {
			XP_STRCPY(base_name, "/");
		}
		else {
			int32 len = p - file_name;

			if (len == XP_STRLEN(file_name)) {
				isDir = True;
				XP_STRCPY(base_name, file_name);
			}
			else {
				XP_MEMCPY(base_name, file_name, len);
				base_name[len] = '\0';
			}
		}
	}
	else {
		// WARNING... [ we may get into some trouble with this one ]
		//
		XP_STRCPY(base_name, ".");
	}
	
	// NOTE:  we need to verify that we can read/write the directory
	//        before we check the file...
	//
	//        - need to check all three mode groups... [ owner, group, other ]
	//
	if ( stat(base_name, &dir_info) >= 0 ) {
		XP_Bool gotcha = False;
		
		if ( isFileP && 
			 isDir
			) {
			// NOTE:  the specifed file_name is a directory... [ fail ]
			//
			XP_FREE(base_name);

			return False;
		}

		if (dir_info.st_uid == getuid()) {
			if ((dir_info.st_mode & S_IRUSR) &&
				(dir_info.st_mode & S_IWUSR)) {
				gotcha = True;
			}
		}
		
		if (!gotcha) {
			if (dir_info.st_gid == getgid()) {
				if ((dir_info.st_mode & S_IRGRP) &&
					(dir_info.st_mode & S_IWGRP)) {
					gotcha = True;
				}
			}
			
			if (!gotcha) {
				if ((dir_info.st_mode & S_IROTH) &&
					(dir_info.st_mode & S_IWOTH)) {
					gotcha = True;
				}
			}
		}

		if (gotcha) {
			if (isDir) {
				// NOTE: we're done... [ we can read/write this directory ]
				//
				XP_FREE(base_name);

				return True;
			}
			else {
				// NOTE:  let's do the same thing for the file...
				//
				if ( stat(file_name, &file_info) >= 0 ) {
					gotcha = False;
					
					if (isFileP &&
						(file_info.st_mode & S_IFDIR)) {
						// NOTE:  the specifed file_name is a directory...
						//
						XP_FREE(base_name);

						return False;
					}

					if (file_info.st_uid == getuid()) {
						if ((file_info.st_mode & S_IRUSR) &&
							(file_info.st_mode & S_IWUSR)) {
							gotcha = True;
						}
					}
					
					if (!gotcha) {
						if (file_info.st_gid == getgid()) {
							if ((file_info.st_mode & S_IRGRP) &&
								(file_info.st_mode & S_IWGRP)) {
								gotcha = True;
							}
						}
						
						if (!gotcha) {
							if ((file_info.st_mode & S_IROTH) &&
								(file_info.st_mode & S_IWOTH)) {
								gotcha = True;
							}
						}
					}

					if (gotcha) {
						// NOTE:  we can read/write this directory/file...
						//
						XP_FREE(base_name);
						
						return True;
					}
					else {
						// NOTE:  we failed the file check...
						//
						XP_FREE(base_name);
						
						return False;
					}
				}
				else {
					if (existsP) {
						// NOTE:  failed to stat the file...
						//
						XP_FREE(base_name);
					
						return False;
					}
					else {
						// NOTE:  we can read/write this directory...
						//
						XP_FREE(base_name);
					
						return True;
					}
				}
			}
		}
		else {
			// NOTE:  we failed the directory check...
			//
			XP_FREE(base_name);

			return False;
		}
	}
	else {
		// NOTE:  failed to stat the directory...
		//
		XP_FREE(base_name);

		return False;
	}
}


//////////////////////////////////////////////////////////////////////////
//
// XFE_DownloadView
//
//////////////////////////////////////////////////////////////////////////

XFE_DownloadFrame::XFE_DownloadFrame(Widget toplevel, XFE_Frame *parent_frame)
	: XFE_Frame("Download",			// name
				toplevel,			// top level
				parent_frame,		// parent frame
				FRAME_DOWNLOAD,		// frame type
				NULL,				// chrome
				False,				// have html display
				False,				// have menu bar
				False,				// have toolbar
				False,				// have dashboard (we'll make our own)
				True)				// destroy on close
{
	Widget msgb;
	XFE_Component *belowView;
	Arg av[10];
	int ac;

	XFE_DownloadView *v = new XFE_DownloadView(this, 
                                               getChromeParent(),
											   NULL, 
                                               m_context);

	ac = 0;
	XtSetArg(av[ac], XmNdialogType, XmDIALOG_TEMPLATE); ac++;
	msgb = XmCreateMessageBox(getChromeParent(), "messagebox", av, ac);

	/* We have to do this explicitly because of AIX versions come
	   with these buttons by default */
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_SEPARATOR));
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_OK_BUTTON));
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_CANCEL_BUTTON));
	fe_UnmanageChild_safe(XmMessageBoxGetChild(msgb, XmDIALOG_HELP_BUTTON));

	m_stopButton = XtVaCreateManagedWidget("stopLoading",
										   xmPushButtonWidgetClass,
										   msgb,
										   NULL);

	XtAddCallback(m_stopButton, XmNactivateCallback, activate_cb, this);

	belowView = new XFE_Component(msgb);

	setView(v);
	setBelowViewArea(belowView);

	m_dashboard = new XFE_Dashboard(this,				// top level
									getChromeParent(),	// parent
									this,				// parent frame
									False);				// have task bar

	m_dashboard->setShowStatusBar(True);
	m_dashboard->setShowProgressBar(True);

	fe_set_scrolled_default_size(m_context);
	v->show();
	belowView->show();


	m_dashboard->show();

	// Register the logo animation notifications with ourselves
	// We need to do this, cause the XFE_Frame class only does so
	// if the have_toolbars ctor parameter is true and in our case
	// it is not.
#if defined(GLUE_COMPO_CONTEXT)
	registerInterest(XFE_Component::logoStartAnimation,
					 this,
					 &XFE_Frame::logoAnimationStartNotice_cb);
	
	registerInterest(XFE_Component::logoStopAnimation,
					 this,
					 &XFE_Frame::logoAnimationStopNotice_cb);
#else
	registerInterest(XFE_Frame::logoStartAnimation,
					 this,
					 &XFE_Frame::logoAnimationStartNotice_cb);
	
	registerInterest(XFE_Frame::logoStopAnimation,
					 this,
					 &XFE_Frame::logoAnimationStopNotice_cb);
#endif /* GLUE_COMPO_CONTEXT */
}

XFE_DownloadFrame::~XFE_DownloadFrame()
{
	// Unregister the logo animation notifications with ourselves
	// We need to do this, cause the XFE_Frame class only does so
	// if the have_toolbars ctor parameter is true and in our case
	// it is not.
#if defined(GLUE_COMPO_CONTEXT)
	unregisterInterest(XFE_Component::logoStartAnimation,
					   this,
					   &XFE_Frame::logoAnimationStartNotice_cb);
	
	unregisterInterest(XFE_Component::logoStopAnimation,
					   this,
					   &XFE_Frame::logoAnimationStopNotice_cb);
#else
	unregisterInterest(XFE_Frame::logoStartAnimation,
					   this,
					   &XFE_Frame::logoAnimationStartNotice_cb);
	
	unregisterInterest(XFE_Frame::logoStopAnimation,
					   this,
					   &XFE_Frame::logoAnimationStopNotice_cb);
#endif /* GLUE_COMPO_CONTEXT */
}

XP_Bool
XFE_DownloadFrame::isCommandEnabled(CommandType cmd,
									void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		return fe_IsContextStoppable(m_context);
	else
		return False;
}

void
XFE_DownloadFrame::doCommand(CommandType cmd,
							 void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		{
			D( printf ("Canceling.\n"); )
			XP_InterruptContext(m_context);
			// allConnectionsComplete will destroy this frame.
		}
}

XP_Bool
XFE_DownloadFrame::handlesCommand(CommandType cmd,
								  void */*calldata*/, XFE_CommandInfo*)
{
	if (cmd == xfeCmdStopLoading)
		return True;
	else
		return False;
}

void
XFE_DownloadFrame::activate_cb(Widget w, XtPointer clientData, XtPointer)
{
	CommandType cmd = Command::intern(XtName(w));
	XFE_DownloadFrame *obj = (XFE_DownloadFrame*)clientData;

	if (obj->handlesCommand(cmd) && obj->isCommandEnabled(cmd))
		obj->doCommand(cmd);
}

void
XFE_DownloadFrame::allConnectionsComplete(MWContext  */* context */)
{
	D( printf ("in allConnectionsComplete\n");)
	XP_InterruptContext(m_context);

	delete_response();
}

void
XFE_DownloadFrame::setAddress(char *address)
{
	XFE_DownloadView *v = (XFE_DownloadView*)m_view;

	v->setAddress(address);
}

void
XFE_DownloadFrame::setDestination(char *destination)
{
	XFE_DownloadView *v = (XFE_DownloadView*)m_view;

	v->setDestination(destination);
}
//////////////////////////////////////////////////////////////////////////
XFE_Logo *
XFE_DownloadFrame::getLogo()
{
	XFE_Logo *			logo = NULL;
	XFE_DownloadView *	view = (XFE_DownloadView *) getView();
	
	if (view)
	{
		logo = view->getLogo();
	}

	return logo;
}
//////////////////////////////////////////////////////////////////////////
void
XFE_DownloadFrame::configureLogo()
{
}

//////////////////////////////////////////////////////////////////////////
//
// XFE_DownloadView
//
//////////////////////////////////////////////////////////////////////////
XFE_DownloadView::XFE_DownloadView(XFE_Component *toplevel, Widget parent,
								   XFE_View *parent_view, MWContext *context)
	: XFE_View (toplevel, parent_view, context)
{
	Widget form;
	Widget   pane = 0;
	Widget    saving_label, url_label;
	Arg av[10];
	int ac;

	form = XtVaCreateWidget("topArea",
							xmFormWidgetClass,
							parent,
							XmNrightAttachment,		XmATTACH_FORM,
							XmNtopAttachment,		XmATTACH_FORM,
							XmNleftAttachment,		XmATTACH_FORM,
							XmNbottomAttachment,	XmATTACH_NONE,
							NULL);

	CONTEXT_DATA(m_contextData)->top_area = form;
	

	m_logo = new XFE_Logo((XFE_Frame *) toplevel,form,"logo");
	
	pane = XtVaCreateManagedWidget("pane",
								   xmFormWidgetClass,
								   form,
								   NULL);
	url_label = XtVaCreateManagedWidget("downloadURLLabel",
										xmLabelGadgetClass,
										pane,
										NULL);
	ac = 0;
	XtSetArg (av [ac], XmNeditable, False); ac++;
	m_url_value = fe_CreateTextField (pane, "downloadURLValue", av, ac);
	XtManageChild(m_url_value);

	saving_label = XtVaCreateManagedWidget("downloadFileLabel",
										   xmLabelGadgetClass,
										   pane,
										   NULL);
	ac = 0;
	XtSetArg (av [ac], XmNeditable, False); ac++;
	m_saving_value = fe_CreateTextField(pane, "downloadFileValue", av, ac);
	XtManageChild(m_saving_value);

	/* =======================================================================
	   Attachments.  This has to be done after the widgets are created,
	   since they're interdependent, sigh.
	   =======================================================================
	   */
	XtVaSetValues(url_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_FORM,
				  0);
	XtVaSetValues(m_url_value,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, url_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, url_label,
				  0);
	XtVaSetValues(saving_label,
				  XmNleftAttachment, XmATTACH_FORM,
				  XmNtopAttachment, XmATTACH_WIDGET,
				  XmNtopWidget, m_url_value,
				  0);
	XtVaSetValues(m_saving_value,
				  XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET,
				  XmNtopWidget, saving_label,
				  XmNleftAttachment, XmATTACH_WIDGET,
				  XmNleftWidget, saving_label,
				  XmNrightAttachment, XmATTACH_FORM,
				  0);
	XP_ASSERT(XfeWidth(url_label) > 0 && XfeWidth(saving_label) > 0);
	if (XfeWidth(url_label) < XfeWidth(saving_label))
		XtVaSetValues (url_label, XmNwidth, XfeWidth(saving_label), 0);
	else
		XtVaSetValues (saving_label, XmNwidth, XfeWidth(url_label), 0);
	
	XtVaSetValues (url_label, XmNheight, XfeHeight(m_url_value), 0);
	XtVaSetValues (saving_label, XmNheight, XfeHeight(m_saving_value), 0);

	XtVaSetValues (pane,
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_FORM,
				   XmNbottomAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_WIDGET,
				   XmNrightWidget, m_logo->getBaseWidget(),
				   0);
	XtVaSetValues (m_logo->getBaseWidget(),
				   XmNtopAttachment, XmATTACH_FORM,
				   XmNrightAttachment, XmATTACH_FORM,
				   XmNleftAttachment, XmATTACH_NONE,
				   XmNbottomAttachment, XmATTACH_NONE,
				   0);

	m_logo->show();
	setBaseWidget(form);
}

XFE_DownloadView::~XFE_DownloadView()
{
	// nothing to do here.
}

void
XFE_DownloadView::setAddress(char *address)
{
	fe_SetTextFieldAndCallBack(m_url_value, address);
}

void
XFE_DownloadView::setDestination(char *destination)
{
	fe_SetTextFieldAndCallBack(m_saving_value, destination);
}

XFE_Logo *
XFE_DownloadView::getLogo()
{
	return m_logo;
}

MWContext*
fe_showDownloadWindow(Widget toplevel, XFE_Frame *parent_frame)
{
	XFE_DownloadFrame *frame = new XFE_DownloadFrame(toplevel, parent_frame);

	frame->show();

	return frame->getContext();
}


/* here is the stuff ripped out of commands.c, which is used to actually
   set up the stream.  It needs to be here instead of commands.c because
   we have do set values of the text fields, through member functions. */
extern char **fe_encoding_extensions; /* gag.  used by mkcache.c. */

static struct save_as_data *
make_save_as_data (MWContext *context, Boolean allow_conversion_p,
		   int type, URL_Struct *url, const char *output_file)
{
  const char *file_name;
  struct save_as_data *sad;
  FILE *file = NULL;
  char *suggested_name = 0;
  Boolean use_dialog_p = False;
  Boolean exists_p = False;
  Boolean nuke_p = True;
  const char *address = url ? url->address : 0;
  const char *content_type = url ? url->content_type : 0;
  const char *content_encoding = url ? url->content_encoding : 0;
  const char *title;
  char buf [255];

  if (url)
    {
#ifdef MOZ_MAIL_NEWS
      if (!url->content_name)
	url->content_name = MimeGuessURLContentName(context, url->address);
#endif
      if (url->content_name)
    address = url->content_name;
    }

  if (output_file)
    file_name = output_file;
  else
    {
      Boolean really_allow_conversion_p = allow_conversion_p;
      if (content_type &&
	  (!strcasecomp (content_type, UNKNOWN_CONTENT_TYPE) || 
	   !strcasecomp (content_type, INTERNAL_PARSER)))
	content_type = 0;

      if (content_type && !*content_type) content_type = 0;
      if (content_encoding && !*content_encoding) content_encoding = 0;
      if (content_type && content_encoding)
	PR_snprintf (buf, sizeof (buf), XP_GetString(XFE_SAVE_AS_TYPE_ENCODING),
		 content_type, content_encoding);
      else if (content_type)
	PR_snprintf (buf, sizeof (buf),
		XP_GetString(XFE_SAVE_AS_TYPE), content_type);
      else if (content_encoding)
	PR_snprintf (buf, sizeof (buf),
		XP_GetString(XFE_SAVE_AS_ENCODING), content_encoding);
      else
	PR_snprintf (buf, sizeof (buf), XP_GetString(XFE_SAVE_AS));
      title = buf;

      if (fe_encoding_extensions)
	{
	  int L = strlen (address);
	  int i = 0;
	  while (fe_encoding_extensions [i])
	    {
	      int L2 = strlen (fe_encoding_extensions [i]);
	      if (L2 < L &&
		  !strncmp (address + L - L2,
			    fe_encoding_extensions [i],
			    L2))
		{

		  /* The URL ends in ".Z" or ".gz" (or whatever.)
		     Take off that extension, and ask netlib what the type
		     of the resulting file is - if it is text/html or
		     text/plain (meaning it ends in ".html" or ".htm" or
		     ".txt" or god-knows-what-else) then (and only then)
		     strip off the .Z or .gz.  That we need to check the
		     extension like that is a kludge, but since we haven't
		     opened the URL yet, we don't have a content-type to
		     know for sure.  And it's just for the default file name
		     anyway...
		   */
#ifdef NEW_DECODERS
		  NET_cinfo *cinfo;

		  /* If the file ends in .html.Z, the suggested file
		     name is .html, but we will allow it to be saved
		     as .txt (uncompressed.)

		     If the file ends in .ps.Z, the suggested file name
		     is .ps.Z, and we will ignore a selection which says
		     to save it as text. */
		  really_allow_conversion_p = False;

		  suggested_name = strdup (address);
		  suggested_name [L - L2] = 0;
		  /* don't free cinfo. */
		  cinfo = NET_cinfo_find_type (suggested_name);
		  if (cinfo &&
		      cinfo->type &&
		      (!strcasecomp (cinfo->type, TEXT_PLAIN) ||
		       !strcasecomp (cinfo->type, TEXT_HTML) ||
		       !strcasecomp (cinfo->type, TEXT_MDL) ||
		       /* always treat unknown content types as text/plain */
		       !strcasecomp (cinfo->type, UNKNOWN_CONTENT_TYPE)))
		    {
		      /* then that's ok */
		      really_allow_conversion_p = allow_conversion_p;
		      break;
		    }

		  /* otherwise, continue. */
		  free (suggested_name);
		  suggested_name = 0;
#else  /* !NEW_DECODERS */
		  suggested_name = strdup (address);
		  suggested_name [L - L2] = 0;
		  break;
#endif /* !NEW_DECODERS */
		}
	      i++;
	    }
	}

      file_name = fe_ReadFileName (context, title,
				   (suggested_name ? suggested_name : address),
				   False,
				   (really_allow_conversion_p ? &type : 0));

      if (suggested_name)
	free ((char *) suggested_name);

      use_dialog_p = True;
    }

  if (! file_name)
    return 0;

#ifdef EDITOR 
  if (context->type == MWContextEditor) {
	  // NOTE:  let's check to make sure we can write into this file/directory
	  //        before we start nuking data...
	  //
	  if ( !XFE_StatReadWrite(file_name, True, False) ) {
		  // NOTE:  we can't write into this directory or file... [ PUNT ]
		  //
		  char buf [1024];
		  PR_snprintf (buf, sizeof (buf),
					   XP_GetString(XFE_ERROR_READ_ONLY), file_name);
		  fe_perror (context, buf);
		  
		  return 0;
	  }
  }
#endif
  
  /* If the file exists, confirm overwriting it. */
  {
    XP_StatStruct st;
    if (!stat (file_name, &st))
      {
	char *bp;
	int size;

	exists_p = True;

	size = XP_STRLEN (file_name) + 1;
	size += XP_STRLEN (fe_globalData.overwrite_file_message) + 1;
	bp = (char *) XP_ALLOC (size);
	if (!bp) return 0;
	PR_snprintf (bp, size,
			fe_globalData.overwrite_file_message, file_name);
	if (!FE_Confirm (context, bp))
	  {
	    XP_FREE (bp);
	    return 0;
	  }
	XP_FREE (bp);
      }
  }

#ifdef EDITOR 
  if (exists_p && (context->type == MWContextEditor)) {
	  // NOTE:  don't open the file or we'll destroy the ability to 
	  //        create the backup file in EDT...
	  //
	  nuke_p = False;
  }
#endif

  if (nuke_p) {
	  file = fopen (file_name, "w");
  }

  if (!file && nuke_p)
    {
      char buf [1024];
      PR_snprintf (buf, sizeof (buf),
		    XP_GetString(XFE_ERROR_OPENING_FILE), file_name);
      fe_perror (context, buf);
      sad = 0;
    }
  else
    {
      sad = (struct save_as_data *) malloc (sizeof (struct save_as_data));
      sad->context = context;
      sad->name = (char *) file_name;
      sad->file = file;
      sad->type = type;
      sad->done = NULL;
      sad->insert_base_tag = FALSE;
      sad->use_dialog_p = use_dialog_p;
      sad->content_length = 0;
      sad->bytes_read = 0;
      sad->url = url;
    }
  return sad;
}

static void
fe_save_as_complete (PrintSetup *ps)
{
  MWContext *context = (MWContext *) ps->carg;

  fclose(ps->out);

  fe_LowerSynchronousURLDialog (context);

  free (ps->filename);
  NET_FreeURLStruct (ps->url);
}


static int
fe_save_as_stream_write_method (NET_StreamClass *stream, const char *str, int32 len)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fwrite ((char *) str, 1, len, sad->file);

  sad->bytes_read += len;

  if (sad->content_length > 0)
    FE_SetProgressBarPercent (sad->context,
			      (sad->bytes_read * 100) /
			      sad->content_length);
#if 0
  /* Oops, this makes the size twice as large as it should be. */
  FE_GraphProgress (sad->context, 0 /* #### url_s */,
		    sad->bytes_read, len, sad->content_length);
#endif
  return 1;
}

static unsigned int
fe_save_as_stream_write_ready_method (NET_StreamClass *stream)
{
  return(MAX_WRITE_READY);
}

static void
fe_save_as_stream_complete_method (NET_StreamClass *stream)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fclose (sad->file);
  if (sad->done)
    (*sad->done)(sad);
  sad->file = 0;
  if (sad->name)
    {
      free (sad->name);
      sad->name = 0;
    }
  if (sad->use_dialog_p)
    fe_LowerSynchronousURLDialog (sad->context);

  FE_GraphProgressDestroy (sad->context, 0 /* #### url */,
			   sad->content_length, sad->bytes_read);
  NET_RemoveURLFromCache(sad->url); /* cleanup */
  NET_RemoveDiskCacheObjects(0); /* kick the cache to notice the cleanup */
  free (sad);
  return;
}

static void
fe_save_as_stream_abort_method (NET_StreamClass *stream, int /*status*/)
{
  struct save_as_data *sad = (struct save_as_data *) stream->data_object;
  fclose (sad->file);
  sad->file = 0;
  if (sad->name)
    {
      if (!unlink (sad->name))
	{
#if 0
	  char buf [1024];
	  PR_snprintf (buf, sizeof (buf),
			XP_GetString(XFE_ERROR_DELETING_FILE), sad->name);
	  fe_perror (sad->context, buf);
#endif
	}
      free (sad->name);
      sad->name = 0;
    }
  if (sad->use_dialog_p)
    fe_LowerSynchronousURLDialog (sad->context);

  FE_GraphProgressDestroy (sad->context, 0 /* #### url */,
			   sad->content_length, sad->bytes_read);
  /* We do *not* remove the URL from the cache at this point. */
  free (sad);
}

/* Creates and returns a stream object which writes the data read to a
   file.  If the file has not been prompted for / opened, it prompts the
   user.
 */
extern "C" NET_StreamClass *
fe_MakeSaveAsStream (int /*format_out*/, void */*data_obj*/,
		     URL_Struct *url_struct, MWContext *context)
{
	struct save_as_data *sad;
	NET_StreamClass* stream;
	
	if (url_struct->fe_data)
		{
			sad = (struct save_as_data*)url_struct->fe_data;
		}
	else
		{
			Boolean text_p = (url_struct->content_type &&
							  (!strcasecomp (url_struct->content_type, TEXT_HTML) ||
							   !strcasecomp (url_struct->content_type, TEXT_MDL) ||
							   !strcasecomp (url_struct->content_type, TEXT_PLAIN)));
			sad = make_save_as_data (context, text_p, fe_FILE_TYPE_HTML, url_struct,
									 0);
			if (! sad) return 0;
		}

	url_struct->fe_data = 0;
	
	stream = (NET_StreamClass *) calloc (sizeof (NET_StreamClass), 1);
	if (!stream) return 0;
	
	stream->name           = "SaveAs";
	stream->complete       = fe_save_as_stream_complete_method;
	stream->abort          = fe_save_as_stream_abort_method;
	stream->put_block      = fe_save_as_stream_write_method;
	stream->is_write_ready = fe_save_as_stream_write_ready_method;
	stream->data_object    = sad;
	stream->window_id      = context;
	
	if (sad->insert_base_tag && XP_STRCMP(url_struct->content_type, TEXT_HTML)
		== 0)
		{
			/*
			** This is here to that any relative URL's in the document
			** will continue to work even though we are moving the document
			** to another world.
			*/
			fe_save_as_stream_write_method(stream, "<BASE HREF=", 11);
			fe_save_as_stream_write_method(stream, url_struct->address,
										   XP_STRLEN(url_struct->address));
			fe_save_as_stream_write_method(stream, ">\n", 2);
		}

	sad->content_length = url_struct->content_length;
	FE_SetProgressBarPercent (context, -1);
    if( url_struct->server_can_do_byteranges || url_struct->server_can_do_restart )
        url_struct->must_cache = TRUE;
#if 0
	/* Oops, this makes the size twice as large as it should be. */
	FE_GraphProgressInit (context, url_struct, sad->content_length);
#endif
	
	if (sad->use_dialog_p)
		{
			/* make sure it is safe to open a new context */
			if (NET_IsSafeForNewContext(url_struct))
				{
					/* create a download frame here */
					XFE_Frame *parent;
					XFE_DownloadFrame *new_frame;
					MWContext *new_context;

					parent = ViewGlue_getFrame(XP_GetNonGridContext(context));
					XP_ASSERT(parent);
					new_frame = new XFE_DownloadFrame(XtParent(parent->getBaseWidget()), parent);
					new_context = new_frame->getContext();

					/* Set the values for location and filename */
					if (url_struct->address)
						new_frame->setAddress(url_struct->address);
					if (sad && sad->name)
						new_frame->setDestination(sad->name);

					new_frame->show();

					/* Set the url_count and enable all animation */
					CONTEXT_DATA (new_context)->active_url_count = 1;
					fe_StartProgressGraph (new_context);
	  
					/* register this new context associated with this stream
					 * with the netlib
					 */
					NET_SetNewContext(url_struct, new_context, fe_url_exit);

					/* Change the fe_data's context */
					sad->context = new_context;
				}
			else
				fe_RaiseSynchronousURLDialog (context, CONTEXT_WIDGET (context),
											  "saving");
		}

  return stream;
}

extern "C" NET_StreamClass *
fe_MakeSaveAsStreamNoPrompt (int format_out, void *data_obj,
			     URL_Struct *url_struct, MWContext *context)
{
  struct save_as_data *sad;

  assert (context->prSetup);
  if (! context->prSetup)
    return 0;

  sad = (struct save_as_data *) malloc (sizeof (struct save_as_data));
  sad->context = context;
  sad->name = strdup (context->prSetup->filename);
  sad->file = context->prSetup->out;
  sad->type = fe_FILE_TYPE_HTML;
  sad->done = 0;
  sad->insert_base_tag = FALSE;
  sad->use_dialog_p = FALSE;

  url_struct->fe_data = sad;

  sad->content_length = url_struct->content_length;
  FE_GraphProgressInit (context, url_struct, sad->content_length);

  return fe_MakeSaveAsStream (format_out, data_obj, url_struct, context);
}

static void
fe_save_as_nastiness (MWContext *context, URL_Struct *url,
		      struct save_as_data *sad,int synchronous)
{
  SHIST_SavedData saved_data;
  assert (sad);
  if (! sad) return;

  /* Make sure layout saves the current state of form elements. */
  LO_SaveFormData(context);

  /* Hold on to the saved data. */
  XP_MEMCPY(&saved_data, &url->savedData, sizeof(SHIST_SavedData));

  /* make sure the form_data slot is zero'd or else all
   * hell will break loose
   */
  XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));

  switch (sad->type)
    {
    case fe_FILE_TYPE_TEXT:
    case fe_FILE_TYPE_FORMATTED_TEXT:
      {
	PrintSetup p;
	fe_RaiseSynchronousURLDialog (context, CONTEXT_WIDGET (context),
				      "saving");
	XL_InitializeTextSetup (&p);
	p.out = sad->file;
	p.filename = sad->name;
	p.url = url;
	free(sad);
	p.completion = fe_save_as_complete;
	p.carg = context;
	XL_TranslateText (context, url, &p);
	fe_await_synchronous_url (context);
	break;
      }
    case fe_FILE_TYPE_PS:
      {
	PrintSetup p;
	fe_RaiseSynchronousURLDialog (context, CONTEXT_WIDGET (context),
				      "saving");
	XL_InitializePrintSetup (&p);
	p.out = sad->file;
	p.filename = sad->name;
	p.url = url;
	free (sad);
	p.completion = fe_save_as_complete;
	p.carg = context;
	XL_TranslatePostscript (context, url, &saved_data, &p);
	fe_await_synchronous_url (context);
	break;
      }

    case fe_FILE_TYPE_HTML:
      {
        if (!synchronous)
            fe_GetSecondaryURL (context, url, FO_CACHE_AND_SAVE_AS, sad, FALSE);
        else
            fe_GetSynchronousURL (context,
                                  CONTEXT_WIDGET (context),
                                  "saving",
                                  url,
                                  FO_CACHE_AND_SAVE_AS,
                                  (void *)sad);
	break;
      }
#ifdef SAVE_ALL
    case fe_FILE_TYPE_HTML_AND_IMAGES:
      {
	char *addr = strdup (url->address);
	char *filename = sad->name;
	FILE *file = sad->file;
	NET_FreeURLStruct (url);
	free (sad);
	SAVE_SaveTree (context, addr, filename, file);
	break;
      }
#endif /* SAVE_ALL */
    default:
      abort ();
    }
}


void
fe_SaveURL (MWContext *context, URL_Struct *url)
{
  struct save_as_data *sad;
  Boolean text_p = True;

  assert (url);
  if (!url) return;

#if 0
  if (url->content_type && *url->content_type)
    {
      if (! (!strcasecomp (url->content_type, TEXT_HTML) ||
	     !strcasecomp (url->content_type, TEXT_MDL) ||
	     !strcasecomp (url->content_type, TEXT_PLAIN)))
	text_p = False;
    }
#endif

  sad = (struct save_as_data*)make_save_as_data (context, text_p, fe_FILE_TYPE_HTML, url, 0);
  if (sad)
    fe_save_as_nastiness (context, url, sad, FALSE);
  else
    NET_FreeURLStruct (url);
}

void
fe_SaveSynchronousURL (MWContext *context, URL_Struct *url,const char *fname)
{
  struct save_as_data *sad;
  Boolean text_p = True;

  assert (url);
  if (!url) return;

  sad = (struct save_as_data*)make_save_as_data (context, text_p, fe_FILE_TYPE_HTML, url, fname);
  if (sad) {
      sad->use_dialog_p=FALSE;
      if (fname && strlen(fname)>0)
          sad->name = strdup((char *)fname);
      fe_save_as_nastiness (context, url, sad, TRUE);
  }
  else {
      NET_FreeURLStruct (url);
  }
}

#ifdef EDITOR

extern "C" Boolean
fe_SaveAsDialog(MWContext* context, char* buf, int type)
{
    URL_Struct*          url;
    struct save_as_data* sad;
	
    url = SHIST_CreateURLStructFromHistoryEntry(
					context,
					SHIST_GetCurrent(&context->hist)
					);

    if (url) 
		{
			sad = (struct save_as_data*)make_save_as_data(context, FALSE, type, url, 0);
			
			if (sad) 
				{
					// NOTE:  check to see if they opened the file...
					//
					if (sad->file) {
						fclose(sad->file);
					}
					
					strcpy(buf, sad->name);
					
					free(sad);
					
					return TRUE;
				}
		}
	
    return FALSE;
}

#endif

extern "C" void
fe_save_as_action (Widget widget, MWContext *context,
                   String *av, Cardinal *ac)
{
  /* See also fe_open_url_action() */
  XP_ASSERT (context);
  if (!context) return;
  fe_UserActivity (context);
  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;
  if ((*ac == 1 || *ac == 2) && av[0])
    {
      URL_Struct *url;
      struct save_as_data *sad;
      char *file;
      int type = fe_FILE_TYPE_HTML;

      if (*ac == 2 && av[1])
	{
	  if (!XP_STRCASECMP(av[1], "source"))
	    type = fe_FILE_TYPE_HTML;
	  else if (!XP_STRCASECMP(av[1], "html"))
	    type = fe_FILE_TYPE_HTML;
#ifdef SAVE_ALL
	  else if (!XP_STRCASECMP(av[1], "tree"))
	    type = fe_FILE_TYPE_HTML_AND_IMAGES;
	  else if (!XP_STRCASECMP(av[1], "html-and-images"))
	    type = fe_FILE_TYPE_HTML_AND_IMAGES;
	  else if (!XP_STRCASECMP(av[1], "source-and-images"))
	    type = fe_FILE_TYPE_HTML_AND_IMAGES;
#endif /* SAVE_ALL */
	  else if (!XP_STRCASECMP(av[1], "text"))
	    type = fe_FILE_TYPE_TEXT;
	  else if (!XP_STRCASECMP(av[1], "formatted-text"))
	    type = fe_FILE_TYPE_FORMATTED_TEXT;
	  else if (!XP_STRCASECMP(av[1], "ps"))
	    type = fe_FILE_TYPE_PS;
	  else if (!XP_STRCASECMP(av[1], "postscript"))
	    type = fe_FILE_TYPE_PS;
	  else
	    {
	      fprintf (stderr, 
		       XP_GetString(XFE_COMMANDS_SAVE_AS_USAGE),
		       fe_progname);
	      return;
	    }
	}

      url = SHIST_CreateWysiwygURLStruct (context,
                                          SHIST_GetCurrent (&context->hist));
      file = strdup (av[0]);
      sad = make_save_as_data (context, False, type, url, file);
      if (sad)
	fe_save_as_nastiness (context, url, sad, TRUE);
      else
	NET_FreeURLStruct (url);
    }
  else if (*ac > 2)
    {
		fprintf (stderr, 
				 XP_GetString(XFE_COMMANDS_SAVE_AS_USAGE_TWO),
				 fe_progname);
    }
  else
    {
      fe_save_top_frame_as_cb (widget, (XtPointer)context, (XtPointer)0);
    }
}

