/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   commands.c --- menus and toolbar.
   Created: Jamie Zawinski <jwz@netscape.com>, 27-Jun-94.
 */


#include "mozilla.h"
#include "xfe.h"
#include "felocale.h"
#include "xlate.h"
#include "menu.h"
#include "net.h"
#include "e_kit.h"
#include "prefapi.h"
#include "intl_csi.h"
#include <netdb.h>
#ifdef FORTEZZA
#include "ssl.h"
#endif
#include "selection.h"

#include "outline.h"
#include "layers.h"


#ifdef EDITOR
#include "xeditor.h"
#include "edt.h" /* just for EDT_MailDocument() */
#endif /*EDITOR*/

#ifdef MOZ_MAIL_NEWS
#include "msgcom.h"
#include "msg_srch.h"
#include "mailnews.h"
#endif /* MOZ_MAIL_NEWS */

#include "il_icons.h"           /* Image icon enumeration. */

#include "mozjava.h"

#ifdef X_PLUGINS
#include "np.h"
#endif /* X_PLUGINS */

#ifndef NO_WEB_FONTS
#include "nf.h"
#endif /* NO_WEB_FONTS */

#include <Xm/RowColumn.h>
#include <XmL/Grid.h>

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_SAVE_AS_TYPE_ENCODING;
extern int XFE_SAVE_AS_TYPE;
extern int XFE_SAVE_AS_ENCODING;
extern int XFE_SAVE_AS;
extern int XFE_ERROR_OPENING_FILE;
extern int XFE_ERROR_DELETING_FILE;
extern int XFE_LOG_IN_AS;
extern int XFE_OUT_OF_MEMORY_URL;
extern int XFE_FILE_OPEN;
extern int XFE_PRINTING_OF_FRAMES_IS_CURRENTLY_NOT_SUPPORTED;
extern int XFE_ERROR_SAVING_OPTIONS;
extern int XFE_UNKNOWN_ESCAPE_CODE;
extern int XFE_COULDNT_FORK;
extern int XFE_EXECVP_FAILED;
extern int XFE_SAVE_FRAME_AS;
extern int XFE_SAVE_AS;
extern int XFE_PRINT_FRAME;
extern int XFE_PRINT;
extern int XFE_DOWNLOAD_FILE;
extern int XFE_COMPOSE_NO_SUBJECT;
extern int XFE_COMPOSE;
extern int XFE_NETSCAPE_UNTITLED;
extern int XFE_NETSCAPE;
extern int XFE_MAIL_FRAME;
extern int XFE_MAIL_DOCUMENT;
extern int XFE_NETSCAPE_MAIL;
extern int XFE_NETSCAPE_NEWS;
extern int XFE_BOOKMARKS;
extern int XFE_ADDRESS_BOOK;
extern int XFE_BACK;
#if 0		/* Disabling back/forward in frame - dp */
extern int XFE_BACK_IN_FRAME;
#endif /* 0 */
extern int XFE_FORWARD;
extern int XFE_FORWARD_IN_FRAME;
extern int XFE_CANNOT_SEE_FILE;
extern int XFE_CANNOT_READ_FILE;
extern int XFE_IS_A_DIRECTORY;
extern int XFE_REFRESH;
extern int XFE_REFRESH_FRAME;

extern int XFE_COMMANDS_ADD_BOOKMARK_USAGE;
extern int XFE_COMMANDS_FIND_USAGE;
extern int XFE_COMMANDS_HTML_HELP_USAGE;
extern int XFE_COMMANDS_MAIL_TO_USAGE;
extern int XFE_COMMANDS_OPEN_FILE_USAGE;
extern int XFE_COMMANDS_OPEN_URL_USAGE;
extern int XFE_COMMANDS_PRINT_FILE_USAGE;
extern int XFE_COMMANDS_SAVE_AS_USAGE;
extern int XFE_COMMANDS_SAVE_AS_USAGE_TWO;
extern int XFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC;
extern int XFE_COMMANDS_UPLOAD_FILE;

extern char* help_menu_names[];
extern char* directory_menu_names[];

extern MWContext * fe_reuseBrowser(MWContext *context,URL_Struct *url);

/* Local forward declarations */
extern void fe_InsertMessageCompositionText(MWContext *context, 
			const char *text,
			XP_Bool leaveCursorAtBeginning);

extern void fe_mailfilter_cb(Widget, XtPointer, XtPointer);

/* Externs from dialog.c: */
extern int fe_await_synchronous_url (MWContext *context);

/* Externs from mozilla.c */
extern char * fe_MakeSashGeometry(char *old_geom_str, int pane_config,
			unsigned int w, unsigned int h);


static Boolean
fe_hack_self_inserting_accelerator (Widget widget, XtPointer closure,
				    XtPointer call_data)
{
#if 1

  /* But actually we're not using this any more. */
  return False;

#else /* 0 */

/* This is completely disgusting.
   We want certain keys to be equivalent to menu items everywhere EXCEPT
   in text fields, where we would like them to do the obvious thing.
   This pile of code is what we need to do to implement that!!
 */

  XmPushButtonCallbackStruct *cd = (XmPushButtonCallbackStruct *) call_data;
  MWContext *context = (MWContext *) closure;
  Widget focus = XmGetFocusWidget (CONTEXT_WIDGET (context));
  Modifiers mods;
  KeySym k;
  char *s;

  if (!focus||
      (!XmIsText (focus) && !XmIsTextField (focus)))
    return False;

  if (!cd || !cd->event)	/* can this happen? */
    return False;

  if (cd->event->xany.type != KeyPress)
    return False;

  /* If any bits but control, shift, or lock are on, this can't be a
     self-inserting character. */
  if (cd->event->xkey.state & ~(ControlMask|ShiftMask|LockMask))
    return False;

  k = XtGetActionKeysym (cd->event, &mods);

  if (! k)
    return False;

  s = XKeysymToString (k);
  if (! s)
    return False;

  if (*s < ' ' || *s >= 127)  /* non-printing char */
    return False;

  if (s[1])		/* Not a self-inserting character - string is
			   probably "osfCancel" or some such nonsense. */
    return False;

/* fe_text_insert moved to selection.c */
  fe_text_insert (focus, s, CS_LATIN1);
  return True;

#endif /* 0 */
}

static void
fe_page_forward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->vscroll;
  XmScrollBarCallbackStruct cb;
  int pi = 0, v = 0, max = 1, min = 0;

  XP_ASSERT(sb);
  if (!sb) return;
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNpageIncrement, &pi, XmNvalue, &v,
		 XmNmaximum, &max, XmNminimum, &min, 0);
  cb.reason = XmCR_PAGE_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v + pi;
  if (cb.value > max - pi) cb.value = max - pi;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}


static void
fe_page_backward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->vscroll;
  XmScrollBarCallbackStruct cb;
  int pi = 0, v = 0, min = 0;

  XP_ASSERT(sb);
  if (!sb) return;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNpageIncrement, &pi, XmNvalue, &v, XmNminimum, &min, 0);
  cb.reason = XmCR_PAGE_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v - pi;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}


static void
fe_line_forward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->vscroll;
  XmScrollBarCallbackStruct cb;
  int li = 0, v = 0, max = 1, min = 0;

#ifdef EDITOR
  if (context->is_editor) return;
#endif /* EDITOR */
    
  XP_ASSERT(sb);
  if (!sb) return;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNincrement, &li, XmNvalue, &v,
		 XmNmaximum, &max, XmNminimum, &min, 0);
  cb.reason = XmCR_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v + li;
  if (cb.value > max - li) cb.value = max - li;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}


static void
fe_line_backward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->vscroll;
  XmScrollBarCallbackStruct cb;
  int li = 0, v = 0, min = 0;

#ifdef EDITOR
  if (context->is_editor) return;
#endif /* EDITOR */
    
  XP_ASSERT(sb);
  if (!sb) return;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNincrement, &li, XmNvalue, &v, XmNminimum, &min, 0);
  cb.reason = XmCR_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v - li;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}

static void
fe_column_forward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->hscroll;
  XmScrollBarCallbackStruct cb;
  int li = 0, v = 0, min = 0;

  XP_ASSERT(sb);
  if (!sb) return;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNincrement, &li, XmNvalue, &v, XmNminimum, &min, 0);
  cb.reason = XmCR_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v - li;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}


static void
fe_column_backward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget sb = CONTEXT_DATA (context)->hscroll;
  XmScrollBarCallbackStruct cb;
  int li = 0, v = 0, max = 1, min = 0;

  XP_ASSERT(sb);
  if (!sb) return;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  XtVaGetValues (sb, XmNincrement, &li, XmNvalue, &v,
		 XmNmaximum, &max, XmNminimum, &min, 0);
  cb.reason = XmCR_INCREMENT;
  cb.event = 0;
  cb.pixel = 0;
  cb.value = v + li;
  if (cb.value > max - li) cb.value = max - li;
  if (cb.value < min) cb.value = min;
  XtCallCallbacks (sb, XmNvalueChangedCallback, &cb);
}


/* File menu
 */

static void
fe_open_url_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  fe_OpenURLDialog(context);
}

void
fe_upload_file_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmString xm_title;
  char *title = XP_GetString(XFE_COMMANDS_UPLOAD_FILE);
  char *file, *msg;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  file = fe_ReadFileName (context, title, 0, True, 0);

  /* validate filename */
  if (file) {
    if (!fe_isFileExist(file)) {
      msg = PR_smprintf( XP_GetString( XFE_CANNOT_SEE_FILE ), file);
      if (msg) {
	XFE_Alert(context, msg);
	XP_FREE(msg);
      }
      XP_FREE (file);
      file = NULL;
    }
    else if (!fe_isFileReadable(file)) {
      msg = PR_smprintf( XP_GetString( XFE_CANNOT_READ_FILE ) , file);
      if (msg) {
	XFE_Alert(context, msg);
	XP_FREE(msg);
      }
      XP_FREE (file);
      file = NULL;
    }
    else if (fe_isDir(file)) {
      msg = PR_smprintf( XP_GetString( XFE_IS_A_DIRECTORY ), file);
      if (msg) {
	  XFE_Alert(context, msg);
	  if (msg) XP_FREE(msg);
      }
      XP_FREE (file);
      file = NULL;
    }
  }

  if (file)
    {
      History_entry *he = SHIST_GetCurrent (&context->hist);
      URL_Struct *url;

      if (he && he->address && (XP_STRNCMP (he->address, "ftp://", 6) == 0)
	  && he->address [strlen (he->address)-1] == '/')
	{
	  url = NET_CreateURLStruct (he->address, NET_SUPER_RELOAD);
	  if (!url)
	    {
	      XP_FREE (file);
	      return;
	    }
	  url->method = URL_POST_METHOD;
	  url->files_to_post = XP_ALLOC (2);
	  if (!url->files_to_post)
	    {
	      XP_FREE (file);
	      return;
	    }

	  url->files_to_post [0] = XP_STRDUP ((const char *) file);
	  url->files_to_post [1] = 0;

	  fe_GetURL (context, url, FALSE);
	}

      XP_FREE (file);
    }
}

void
fe_reload_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmPushButtonCallbackStruct *cd = (XmPushButtonCallbackStruct *) call_data;
  
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  
  if (cd && cd->event->xkey.state & ShiftMask)
    fe_ReLayout (context, NET_SUPER_RELOAD);
  else
    fe_ReLayout (context, NET_NORMAL_RELOAD);
}

/* not static -- it's needed by src/HTMLView.cpp */
void
fe_abort_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  fe_AbortCallback (widget, closure, call_data);
}

void
fe_refresh_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  Widget wid;
  Display *dpy;
  Window win;
  Dimension w = 0, h = 0;
  XGCValues gcv;
  GC gc;

  if (fe_IsGridParent (context)) context = fe_GetFocusGridOfContext (context);
  XP_ASSERT (context);
  if (!context) return;

  wid = CONTEXT_DATA (context)->drawing_area;
  if (wid == NULL) return;

  dpy = XtDisplay (wid);
  win = XtWindow (wid);

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  XtVaGetValues (wid, XmNbackground, &gcv.foreground,
		 XmNwidth, &w, XmNheight, &h, 0);
  gc = XCreateGC (dpy, win, GCForeground, &gcv);
  XFillRectangle (dpy, win, gc, 0, 0, w, h);
  XFreeGC (dpy, gc);

  if (context->compositor) {
    XP_Rect rect;
    
    rect.left = CONTEXT_DATA(context)->document_x;
    rect.top = CONTEXT_DATA(context)->document_y;
    rect.right = rect.left + w;
    rect.bottom = rect.top + h;
    CL_UpdateDocumentRect(context->compositor,
			  &rect, (PRBool)FALSE);
  }
}


char **fe_encoding_extensions = 0; /* gag.  used by mkcache.c. */



#ifdef EDITOR
/*
 *    Hack, hack, hack. These are in editor.c, please move the menu
 *    definition there.
 */
extern void fe_editor_view_source_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_edit_source_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_browse_doc_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_about_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_open_file_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_edit_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paragraph_style_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paragraph_style_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_new_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_new_from_wizard_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_new_from_template_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_open_file_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_save_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_save_as_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_edit_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_undo_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_redo_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_view_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_properties_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_hrule_properties_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_display_tables_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paragraph_marks_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_link_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_target_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_image_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_hrule_dialog_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_html_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_line_break_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_non_breaking_space_cb(Widget,XtPointer,XtPointer);
extern void fe_editor_char_props_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_toggle_char_props_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_char_props_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_clear_char_props_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_font_size_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_font_size_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paragraph_props_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paragraph_props_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_indent_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_properties_dialog_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_delete_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_refresh_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_insert_hrule_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_cut_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_copy_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_paste_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_insert_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_row_insert_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_column_insert_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_cell_insert_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_delete_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_row_delete_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_column_delete_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_table_cell_delete_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_file_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_document_properties_dialog_cb(Widget, XtPointer,
						    XtPointer);
extern void fe_editor_preferences_dialog_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_target_properties_dialog_cb(Widget, XtPointer,
						    XtPointer);
extern void fe_editor_html_properties_dialog_cb(Widget, XtPointer,
						XtPointer);
extern void fe_editor_table_properties_dialog_cb(Widget, XtPointer,
						 XtPointer);
extern void fe_editor_publish_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_find_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_find_again_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_windows_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_select_all_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_delete_item_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_select_table_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_reload_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_set_colors_dialog_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_default_color_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_remove_links_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_open_url_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_show_paragraph_toolbar_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_show_character_toolbar_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_options_menu_cb(Widget, XtPointer, XtPointer);
extern void fe_editor_browse_publish_location_cb(Widget, XtPointer, XtPointer);

#endif /* EDITOR */

/* not static -- it's needed by src/HTMLView.cpp */
void
fe_save_as_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  URL_Struct *url;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  {
    MWContext *ctx = fe_GetFocusGridOfContext (context);
    if (ctx)
      context = ctx;
  }

  url =	SHIST_CreateWysiwygURLStruct (context,
					    SHIST_GetCurrent (&context->hist));

  if (url)
    fe_SaveURL (context, url);
  else
    FE_Alert (context, fe_globalData.no_url_loaded_message);
}

/* not static -- it's needed by src/HTMLView.cpp */
void
fe_save_top_frame_as_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  URL_Struct *url;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  url =	SHIST_CreateWysiwygURLStruct (context,
					    SHIST_GetCurrent (&context->hist));

  if (url)
    fe_SaveURL (context, url);
  else
    FE_Alert (context, fe_globalData.no_url_loaded_message);
}

static unsigned int
fe_save_as_stream_write_ready_method (NET_StreamClass *stream)
{
  return(MAX_WRITE_READY);
}

struct view_source_data
{
  MWContext *context;
  Widget widget;
};


void
fe_view_source_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  History_entry *h;
  URL_Struct *url;
  char *new_url_add=0;

  h = SHIST_GetCurrent (&context->hist);
  url = SHIST_CreateURLStructFromHistoryEntry(context, h);
  if (! url)
    {
      FE_Alert (context, fe_globalData.no_url_loaded_message);
      return;
    }

  /* check to make sure it doesn't already have a view-source
   * prefix.  If it does then this window is already viewing
   * the source of another window.  In this case just
   * show the same thing by reloading the url...
   */
  if(strncmp(VIEW_SOURCE_URL_PREFIX, 
			 url->address, 
			 sizeof(VIEW_SOURCE_URL_PREFIX)-1))
    {
      /* prepend VIEW_SOURCE_URL_PREFIX to the url to
       * get the netlib to display the source view
       */
      StrAllocCopy(new_url_add, VIEW_SOURCE_URL_PREFIX);
      StrAllocCat(new_url_add, url->address);
      free(url->address);
      url->address = new_url_add;
    }

   /* make damn sure the form_data slot is zero'd or else all
    * hell will break loose
    */
   XP_MEMSET (&url->savedData, 0, sizeof (SHIST_SavedData));

#ifdef EDITOR
   if (EDT_IS_EDITOR(context) && !FE_CheckAndSaveDocument(context))
	   return;
#endif

/*
  fe_GetSecondaryURL (context, url, FO_PRESENT, NULL, FALSE);
*/
  fe_GetURL (context, url, FALSE);

}

static int
fe_view_source_stream_write_method (NET_StreamClass *stream, const char *str, int32 len)
{
  struct view_source_data *vsd = (struct view_source_data *) stream->data_object;
  char buf [1024];
  XmTextPosition pos, cpos;

  if (!vsd || !vsd->widget) return -1;
  pos = XmTextGetLastPosition (vsd->widget);
  cpos = 0;
  XtVaGetValues (vsd->widget, XmNcursorPosition, &cpos, 0);

  /* We copy the data first because XmTextInsert() needs a null-terminated
     string, and there isn't necessarily room on the end of `str' for us
     to plop down a null. */
  while (len > 0)
    {
      int i;
      int L = (len > (sizeof(buf)-1) ? (sizeof(buf)-1) : len);
      memcpy (buf, str, L);
      buf [L] = 0;
      str += L;
      len -= L;
      /* Crockishly translate CR to LF for the Motif text widget... */
      for (i = 0; i < L; i++)
	if (buf[i] == '\r' && buf[i+1] != '\n')
	  buf[i] = '\n';
      XmTextInsert (vsd->widget, pos, buf);
      pos += L;
    }
  XtVaSetValues (vsd->widget, XmNcursorPosition, cpos, 0);
  return 1;
}

static void
fe_view_source_stream_complete_method (NET_StreamClass *stream)
{
  struct view_source_data *vsd = (struct view_source_data *) stream->data_object;
  free (vsd);
}

static void
fe_view_source_stream_abort_method (NET_StreamClass *stream, int status)
{
  struct view_source_data *vsd = (struct view_source_data *) stream->data_object;
  free (vsd);
}


/* Creates and returns a stream object which writes the data read into a
   text widget.
 */
NET_StreamClass *
fe_MakeViewSourceStream (int format_out, void *data_obj,
			 URL_Struct *url_struct, MWContext *context)
{
  struct view_source_data *vsd;
  NET_StreamClass* stream;

  if (url_struct->is_binary)
    {
      FE_Alert (context, fe_globalData.binary_document_message);
      return 0;
    }

  vsd = url_struct->fe_data;
  if (! vsd) abort ();
  url_struct->fe_data = 0;

  stream = (NET_StreamClass *) calloc (sizeof (NET_StreamClass), 1);
  if (!stream) return 0;

  stream->name           = "ViewSource";
  stream->complete       = fe_view_source_stream_complete_method;
  stream->abort          = fe_view_source_stream_abort_method;
  stream->put_block      = fe_view_source_stream_write_method;
  stream->is_write_ready = fe_save_as_stream_write_ready_method;
  stream->data_object    = vsd;
  stream->window_id      = context;
  return stream;
}

#ifdef MOZ_MAIL_NEWS

/* Mailing documents
 */

void
fe_mailNew_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  MSG_Mail (context);
}


void
fe_mailto_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  /*
   * You cannot mail a frameset, you must mail the frame child
   * with focus
   */
  if (fe_IsGridParent (context))
    {
      MWContext *ctx = fe_GetFocusGridOfContext (context);
      if (ctx)
        context = ctx;
    }
#ifdef EDITOR
  if (EDT_IS_EDITOR(context))
	  EDT_MailDocument (context);
  else
#endif
	  MSG_MailDocument (context);

}

#endif  /* MOZ_MAIL_NEWS */

void
fe_print_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  URL_Struct *url;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

#ifdef EDITOR
  if (EDT_IS_EDITOR(context) && !FE_CheckAndSaveDocument(context))
      return;
#endif

  {
    MWContext *ctx = fe_GetFocusGridOfContext (context);
    if (ctx)
      context = ctx;
  }

  url = SHIST_CreateURLStructFromHistoryEntry (context, SHIST_GetCurrent(&context->hist));
  if (url) {
    /* Free the url struct we created */
    NET_FreeURLStruct(url);

#ifdef X_PLUGINS
    if (CONTEXT_DATA(context)->is_fullpage_plugin) {
      /* Full page plugins need a step here. We need to ask the plugin if
       * it wants to handle the printing.
       */
      NPPrint npprint;
      
      npprint.mode = NP_FULL;
      npprint.print.fullPrint.pluginPrinted = FALSE;
      npprint.print.fullPrint.printOne = TRUE;
      npprint.print.fullPrint.platformPrint = NULL;
      NPL_Print(context->pluginList, &npprint);
      if (npprint.print.fullPrint.pluginPrinted == TRUE)
	return;
    }
#endif /* X_PLUGINS */

    fe_PrintDialog (context);
  }
  else
    FE_Alert(context, fe_globalData.no_url_loaded_message);
}

#define fe_quit_cb fe_QuitCallback
void
fe_QuitCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
#ifdef MOZ_MAIL_NEWS
  if (!fe_CheckUnsentMail ())
    return;
  if (!fe_CheckDeferredMail ())
    return;
#endif
#ifdef EDITOR
  if (!fe_EditorCheckUnsaved(context))
      return;
#endif /*EDITOR*/
  if (!CONTEXT_DATA (context)->confirm_exit_p ||
      FE_Confirm (context, fe_globalData.really_quit_message))
    {
      fe_AbortCallback (widget, closure, call_data);
      fe_Exit (0);
    }
}


/* FIND */

void
fe_find_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  MWContext *top_context;

  fe_unset_findcommand_context();

  top_context = XP_GetNonGridContext(context);
  if (!top_context) top_context = context;

  fe_UserActivity (top_context);
  if (fe_hack_self_inserting_accelerator (CONTEXT_WIDGET(top_context),
					  closure, call_data))
    return;
  fe_FindDialog (top_context, False);
}


void
fe_find_again_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  MWContext *top_context;
  fe_FindData *find_data;

  fe_unset_findcommand_context();

  top_context = XP_GetNonGridContext(context);
  if (!top_context) top_context = context;

  fe_UserActivity (top_context);
  if (fe_hack_self_inserting_accelerator (CONTEXT_WIDGET(top_context),
					  closure, call_data))
    return;

  find_data = CONTEXT_DATA(top_context)->find_data;

  if ((top_context->type == MWContextBrowser
#ifdef MOZ_MAIL_NEWS
	|| top_context->type == MWContextMail
	|| top_context->type == MWContextNews
	|| top_context->type == MWContextMailMsg
#endif
	) &&
      find_data && find_data->string && find_data->string[0] != '\0')
    fe_FindDialog (top_context, True);
  else
    XBell (XtDisplay (widget), 0);
}


#ifdef FORTEZZA

static void
fe_fortezza_card_cb (Widget widget,
		       XtPointer closure,
		       XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  SSL_FortezzaMenu(context,SSL_FORTEZZA_CARD_SELECT);
}

static void
fe_fortezza_change_cb (Widget widget,
		       XtPointer closure,
		       XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  SSL_FortezzaMenu(context,SSL_FORTEZZA_CHANGE_PERSONALITY);
}

static void
fe_fortezza_view_cb (Widget widget,
		       XtPointer closure,
		       XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  SSL_FortezzaMenu(context,SSL_FORTEZZA_VIEW_PERSONALITY);
}

static void
fe_fortezza_info_cb (Widget widget,
		       XtPointer closure,
		       XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  SSL_FortezzaMenu(context,SSL_FORTEZZA_CARD_INFO);
}

static void
fe_fortezza_logout_cb (Widget widget,
		       XtPointer closure,
		       XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  SSL_FortezzaMenu(context,SSL_FORTEZZA_LOGOUT);
}
#endif


#define HTTP_OFF		42

	/* "about:document" */
#define DOCINFO			"about:document"

	/* ".netscape.com/" */
#define DOT_NETSCAPE_DOT_COM_SLASH \
				"\130\230\217\236\235\215\213\232\217\130\215"\
				"\231\227\131"

	/* "http://home.netscape.com/" */
#define HTTP_NETSCAPE		"\222\236\236\232\144\131\131\222\231\227\217"\
				"\130\230\217\236\235\215\213\232\217\130\215"\
				"\231\227\131"

	/* "http://cgi.netscape.com/cgi-bin/" */
#define HTTP_CGI		"\222\236\236\232\144\131\131\215\221\223\130"\
				"\230\217\236\235\215\213\232\217\130\215\231"\
				"\227\131\215\221\223\127\214\223\230\131"

        /* "http://home.netscape.com/home/" */
#define HTTP_HOME               "\222\236\236\232\144\131\131\222\231\227\217"\
                                "\130\230\217\236\235\215\213\232\217\130\215"\
                                "\231\227\131\222\231\227\217\131"

        /* "http://home.netscape.com/eng/mozilla/4.0/" */
#define HTTP_ENG                "\222\236\236\232\144\131\131\222\231\227\217"\
                                "\130\230\217\236\235\215\213\232\217\130\215"\
                                "\231\227\131\217\230\221\131\227\231\244\223"\
                		"\226\226\213\131\136\130\132\131"

        /* "http://home.netscape.com/eng/mozilla/2.0/" */
#define HTTP_ENG_2_0            "\222\236\236\232\144\131\131\222\231\227\217"\
                                "\130\230\217\236\235\215\213\232\217\130\215"\
                                "\231\227\131\217\230\221\131\227\231\244\223"\
                                "\226\226\213\131\134\130\132\131"

		/* "http://guide.netscape.com/" */
#define HTTP_GUIDE              "\222\236\236\232\144\131\131\221\237\223"\
                                "\216\217\130\230\217\236\235\215\213\232"\
                                "\217\130\215\231\227\131"

        /* "http://help.netscape.com/" */
#define HTTP_HELP         "\222\236\236\232\144\131\131\222\217\226\232"\
                          "\130\230\217\236\235\215\213\232\217\130\215"\
                          "\231\227\131"

        /* "http://home.netscape.com/info/" */
#define HTTP_INFO               "\222\236\236\232\144\131\131\222\231\227\217"\
                                "\130\230\217\236\235\215\213\232\217\130\215"\
                                "\231\227\131\223\230\220\231\131"

        /* "fishcam/" */
#define HTTP_FC                 HTTP_NETSCAPE \
                                "\220\223\235\222\215\213\227\131"

#ifdef MOZ_MAIL_NEWS
        /* "newsrc:" */
#define HTTP_NEWSRC             "\230\217\241\235\234\215\144"
#endif
 
        /* "whats_new/" */
#define HTTP_WHATS_NEW          HTTP_GUIDE \
                                "\241\222\213\236\235\211\230\217\241\131"
        /* "whats_cool/" */
#define HTTP_WHATS_COOL         HTTP_GUIDE \
                                "\241\222\213\236\235\211\215\231\231\226"\
                                "\131"
        /* "" */
#define HTTP_INET_DIRECTORY     HTTP_GUIDE
 
        /* "internet-search.html" */
#define HTTP_INET_SEARCH        HTTP_HOME "\223\230\236\217\234\230\217\236"\
                                "\127\235\217\213\234\215\222\130\222\236\227"\
                                "\226"
        /* "people/" */
#define HTTP_INET_WHITE         HTTP_GUIDE \
                                "\232\217\231\232\226\217\131"
 
        /* "yellow_pages/" */
#define HTTP_INET_YELLOW        HTTP_GUIDE \
                                "\243\217\226\226\231\241\211\232\213\221"\
                                "\217\235\131"

        /* "about-the-internet.html" */
#define HTTP_INET_ABOUT         HTTP_HOME "\213\214\231\237\236\127\236\222"\
                                "\217\127\223\230\236\217\234\230\217\236\130"\
                                "\222\236\227\226"
 
        /* "update.html" */
#define HTTP_SOFTWARE           HTTP_HOME "\237\232\216\213\236\217\130\222\236\227\226"
 
        /* "how-to-create-web-services.html" */
#define HTTP_HOWTO              HTTP_HOME "\222\231\241\127\236\231\127\215"\
                                "\234\217\213\236\217\127\241\217\214\127\235"\
                                "\217\234\240\223\215\217\235\130\222\236\227"\
                                "\226"
        /* "netscape-galleria.html" */
#define HTTP_GALLERIA           HTTP_HOME "\230\217\236\235\215\213\232\217"\
                                "\127\221\213\226\226\217\234\223\213\130\222"\
                                "\236\227\226"
 
        /* "relnotes/unix-" */
# define HTTP_REL_VERSION_PREFIX HTTP_ENG "\234\217\226\230\231\236\217\235"\
                                "\131\237\230\223\242\127"
 
# define HTTP_BETA_VERSION_PREFIX H_REL_VERSION_PREFIX
 
        /* "reginfo-x.cgi" */
#define HTTP_REG		HTTP_HOME "\234\217\221\223\235\236\217\234\130\222\236\227\226"

 
        /* "handbook/" */
#define HTTP_MANUAL             HTTP_ENG "\222\213\230\216\214\231\231\225\131"

	/* "starter.html" */
#define HTTP_STARTER            HTTP_HOME "\235\236\213\234\236\217\234\130"\
				"\222\236\227\226"

#ifdef NON_BETA
       /* "http://help.netscape.com/faqs.html" */
#define HTTP_FAQ                "\222\236\236\232\144\131\131\222\217\226\232"\
                                "\130\230\217\236\235\215\213\232\217\130\215"\
                                "\231\227\131\220\213\233\235\130\222\236\227"\
                                "\226" 
#else
        /* "eng/beta_central/faq/index.html" */
#define HTTP_FAQ		HTTP_NETSCAPE "\217\230\221\131\214\217\236"\
				"\213\211\215\217\230\236\234\213\226\131\220\213\233"\
				"\131\223\230\216\217\242\130\222\236\227\226"
#endif
 
        /* "security-doc.html" */
#define HTTP_SECURITY           HTTP_INFO "\235\217\215\237\234\223\236\243"\
                                "\127\216\231\215\130\222\236\227\226"
 
#ifdef NON_BETA
        /* "auto_bug.cgi" */
#define HTTP_FEEDBACK		HTTP_CGI "\213\237\236\231\211\214\237\221"\
				"\130\215\221\223"
#else
        /* "eng/beta_central" */
#define HTTP_FEEDBACK		HTTP_NETSCAPE "\217\230\221\131\214\217\236"\
				"\213\211\215\217\230\236\234\213\226"
#endif
 
#ifdef NON_BETA
        /* "http://help.netscape.com/" */
#define HTTP_SUPPORT            "\222\236\236\232\144\131\131\222\217\226\232"\
				"\130\230\217\236\235\215\213\232\217\130\215"\
				"\231\227\131"
#else
        /* "eng/beta_central" */
#define HTTP_SUPPORT		HTTP_NETSCAPE "\217\230\221\131\214\217\236"\
				"\213\211\215\217\230\236\234\213\226"
#endif

#ifdef MOZ_MAIL_NEWS
        /* "news/news.html" */
#define HTTP_USENET         HTTP_ENG_2_0 "\230\217\241\235\131\230\217\241"\
                                "\235\130\222\236\227\226"
#endif

        /* "about:plugins" */
#define HTTP_PLUGINS "\213\214\231\237\236\144\232\226\237\221\223\230\235"

        /* "about:fonts" */
#define HTTP_FONTS "\213\214\231\237\236\144\220\231\230\236\235"

#ifdef __sgi

	/* "http://www.sgi.com/surfer/silicon_sites.html" */
# define HTTP_SGI_MENU "\222\236\236\232\144\131\131\241\241\241\130\235\221"\
		  "\223\130\215\231\227\131\235\237\234\220\217\234\131\235"\
		  "\223\226\223\215\231\230\211\235\223\236\217\235\130\222"\
		  "\236\227\226"

	/* "http://www.sgi.com" */
# define HTTP_SGI_BUTTON "\222\236\236\232\144\131\131\241\241\241\130\235"\
			 "\221\223\130\215\231\227"

	/* "file:/usr/local/lib/netscape/docs/Welcome.html" */
# define HTTP_SGI_WELCOME "\220\223\226\217\144\131\237\235\234\131\226\231"\
			  "\215\213\226\131\226\223\214\131\230\217\236\235"\
			  "\215\213\232\217\131\216\231\215\235\131\201\217"\
			  "\226\215\231\227\217\130\222\236\227\226"

	/* "http://www.adobe.com" */
# define HTTP_ADOBE_MENU "\222\236\236\232\144\131\131\241\241\241\130\213"\
			 "\216\231\214\217\130\215\231\227"

	/* "http://www.sgi.com/surfer/cool_sites.html" */
# define SGI_WHATS_COOL  "\222\236\236\232\144\131\131\241\241\241\130\235"\
			 "\221\223\130\215\231\227\131\235\237\234\220\217"\
			 "\234\131\215\231\231\226\211\235\223\236\217\235"\
			 "\130\222\236\227\226"

	/* "http://www.sgi.com/surfer/netscape/relnotes-" */
# define SGI_VERSION_PREFIX	"\222\236\236\232\144\131\131\241\241\241\130"\
				"\235\221\223\130\215\231\227\131\235\237\234"\
				"\220\217\234\131\230\217\236\235\215\213\232"\
				"\217\131\234\217\226\230\231\236\217\235\127"

#endif /* __sgi */

	/* "products/client/communicator/" */
#define HTTP_PRODUCT_INFO HTTP_HELP "\232\234\231\216\237\215\236"\
				"\235\131\215\226\223\217\230\236\131\215\231\227"\
				"\227\237\230\223\215\213\236\231\234\131"

	/* "http://home.netscape.com/eng/intl/" */
#define HTTP_INTL "\222\236\236\232\144\131\131\222\231\227\217\130"\
				"\230\217\236\235\215\213\232\217\130\215\231\227"\
				"\131\217\230\221\131\223\230\236\226\131"

	/* "services.html" */
#define HTTP_SERVICES HTTP_HOME "\235\217\234\240\223\215\217\235"\
				"\130\222\236\227\226"


static char *
go_get_url(char *which)
{
	char		clas[128];
	char		name[128];
	char		*p;
	char		*ret;
	char		*type;
	XrmValue	value;

	PR_snprintf(clas, sizeof (clas), "%s.Url.Which", fe_progclass);
	PR_snprintf(name, sizeof (name), "%s.url.%s", fe_progclass, which);
	if (XrmGetResource(XtDatabase(fe_display), name, clas, &type, &value))
	{
		ret = strdup(value.addr);
		if (!ret)
		{
			return NULL;
		}
		p = ret;
		while (*p)
		{
			*p += HTTP_OFF;
			p++;
		}
		if (!strstr(ret, DOT_NETSCAPE_DOT_COM_SLASH))
		{
			free(ret);
			return NULL;
		}
		return ret;
	}

	return NULL;
}


#define GO_GET_URL(func, res)			\
	static char *				\
	func(char *builtin)			\
	{					\
		static char	*ret = NULL;	\
						\
		if (ret)			\
		{				\
			return ret;		\
		}				\
						\
		ret = go_get_url(res);		\
		if (ret)			\
		{				\
			return ret;		\
		}				\
						\
		return builtin;			\
	}


GO_GET_URL(go_get_url_plugins, "aboutplugins")
GO_GET_URL(go_get_url_fonts, "aboutfonts")
GO_GET_URL(go_get_url_whats_new, "whats_new")
GO_GET_URL(go_get_url_whats_cool, "whats_cool")
GO_GET_URL(go_get_url_inet_directory, "directory")
GO_GET_URL(go_get_url_inet_search, "search")
GO_GET_URL(go_get_url_inet_white, "white")
GO_GET_URL(go_get_url_inet_yellow, "yellow")
GO_GET_URL(go_get_url_inet_about, "about")
GO_GET_URL(go_get_url_software, "software")
GO_GET_URL(go_get_url_howto, "howto")
GO_GET_URL(go_get_url_netscape, "netscape")
GO_GET_URL(go_get_url_galleria, "galleria")
GO_GET_URL(go_get_url_starter, "starter")
GO_GET_URL(go_get_url_rel_version_prefix, "rel_notes")
GO_GET_URL(go_get_url_reg, "reg")
GO_GET_URL(go_get_url_manual, "manual")
GO_GET_URL(go_get_url_faq, "faq")
GO_GET_URL(go_get_url_usenet, "usenet")
GO_GET_URL(go_get_url_security, "security")
GO_GET_URL(go_get_url_feedback, "feedback")
GO_GET_URL(go_get_url_support, "support")
GO_GET_URL(go_get_url_product_info, "productInfo")
GO_GET_URL(go_get_url_intl, "intl")
GO_GET_URL(go_get_url_services, "services")


#define H_WHATS_NEW             go_get_url_whats_new(HTTP_WHATS_NEW)
#define H_WHATS_COOL            go_get_url_whats_cool(HTTP_WHATS_COOL)
#define H_INET_DIRECTORY        go_get_url_inet_directory(HTTP_INET_DIRECTORY)
#define H_INET_SEARCH           go_get_url_inet_search(HTTP_INET_SEARCH)
#define H_INET_WHITE            go_get_url_inet_white(HTTP_INET_WHITE)
#define H_INET_YELLOW           go_get_url_inet_yellow(HTTP_INET_YELLOW)
#define H_INET_ABOUT            go_get_url_inet_about(HTTP_INET_ABOUT)
#define H_SOFTWARE              go_get_url_software(HTTP_SOFTWARE)
#define H_HOWTO                 go_get_url_howto(HTTP_HOWTO)
#define H_NETSCAPE              go_get_url_netscape(HTTP_NETSCAPE)
#define H_GALLERIA              go_get_url_galleria(HTTP_GALLERIA)
#define H_REL_VERSION_PREFIX    go_get_url_rel_version_prefix(HTTP_REL_VERSION_PREFIX)
#define H_REG                   go_get_url_reg(HTTP_REG)
#define H_MANUAL                go_get_url_manual(HTTP_MANUAL)
#define H_STARTER               go_get_url_starter(HTTP_STARTER)
#define H_FAQ                   go_get_url_faq(HTTP_FAQ)
#define H_USENET                go_get_url_usenet(HTTP_USENET)
#define H_SECURITY              go_get_url_security(HTTP_SECURITY)
#define H_FEEDBACK              go_get_url_feedback(HTTP_FEEDBACK)
#define H_SUPPORT               go_get_url_support(HTTP_SUPPORT)
#define H_PRODUCT_INFO          go_get_url_product_info(HTTP_PRODUCT_INFO)
#define H_INTL                  go_get_url_intl(HTTP_INTL)
#define H_SERVICES              go_get_url_services(HTTP_SERVICES)

/*
 * EXPORT_URL creates a function that returns a localized URL.
 * It is needed by the e-kit in order to allow localized URL's.
 */
#define EXPORT_URL(func, builtin) char* xfe_##func(void) {return func(builtin);}

EXPORT_URL(go_get_url_plugins, HTTP_PLUGINS)
EXPORT_URL(go_get_url_fonts, HTTP_FONTS)
EXPORT_URL(go_get_url_whats_new, HTTP_WHATS_NEW)
EXPORT_URL(go_get_url_whats_cool, HTTP_WHATS_COOL)
EXPORT_URL(go_get_url_inet_directory, HTTP_INET_DIRECTORY)
EXPORT_URL(go_get_url_inet_search, HTTP_INET_SEARCH)
EXPORT_URL(go_get_url_inet_white, HTTP_INET_WHITE)
EXPORT_URL(go_get_url_inet_yellow, HTTP_INET_YELLOW)
EXPORT_URL(go_get_url_inet_about, HTTP_INET_ABOUT)
EXPORT_URL(go_get_url_software, HTTP_SOFTWARE)
EXPORT_URL(go_get_url_howto, HTTP_HOWTO)
EXPORT_URL(go_get_url_netscape, HTTP_NETSCAPE)
EXPORT_URL(go_get_url_galleria, HTTP_GALLERIA)
EXPORT_URL(go_get_url_reg, HTTP_REG)
EXPORT_URL(go_get_url_manual, HTTP_MANUAL)
EXPORT_URL(go_get_url_starter, HTTP_STARTER)
EXPORT_URL(go_get_url_faq, HTTP_FAQ)
#ifdef MOZ_MAIL_NEWS
EXPORT_URL(go_get_url_usenet, HTTP_USENET)
#endif
EXPORT_URL(go_get_url_security, HTTP_SECURITY)
EXPORT_URL(go_get_url_feedback, HTTP_FEEDBACK)
EXPORT_URL(go_get_url_support, HTTP_SUPPORT)
EXPORT_URL(go_get_url_product_info, HTTP_PRODUCT_INFO)
EXPORT_URL(go_get_url_intl, HTTP_INTL)
EXPORT_URL(go_get_url_services, HTTP_SERVICES)

/*
 * xfe_go_get_url_relnotes
 */
char*
xfe_go_get_url_relnotes(void)
{
  static char* url = NULL;

  if ( url == NULL ) {
      char buf[1024];
      char* ptr;
      char* prefix = (strchr(fe_version, 'a') || strchr(fe_version, 'b'))
                     ? HTTP_BETA_VERSION_PREFIX
                     : H_REL_VERSION_PREFIX;

#ifdef GOLD
      sprintf(buf, "%sGold.html", fe_version);
#else
      sprintf(buf, "%s.html", fe_version);
#endif

      for ( ptr = buf; *ptr; ptr++ ) {
          *ptr+= HTTP_OFF;
      }
      url = (char*) malloc(strlen(prefix)+strlen(buf)+1);
      strcpy(url, prefix);
      strcat(url, buf);
  }

  return url;
}

void
fe_docinfo_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = DOCINFO, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
    *out = 0;

#ifdef EDITOR
  if (EDT_IS_EDITOR(context) && !FE_CheckAndSaveDocument(context))
    return;
#endif
  /* @@@@ this is the proper way to do it but I dont'
   * know the encoding scheme
   * fe_GetURL (context,NET_CreateURLStruct(buf, FALSE), FALSE);
   */
  fe_GetURL (context,NET_CreateURLStruct(DOCINFO, FALSE), FALSE);

#ifdef EDITOR
  fe_EditorRefresh(context);
#endif
}

char *
xfe_get_netscape_home_page_url()
{
  static char buf [128];
  char *in, *out;
  for (in = H_NETSCAPE, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
  *out = 0;
  return buf;
}

static void
fe_net_showstatus_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char *rv;

  fe_UserActivity (context);
  rv = NET_PrintNetlibStatus();
  NET_ToggleTrace();  /* toggle trace mode on and off */
  XFE_Alert (context, rv);
  free(rv);
}

static void
fe_fishcam_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = HTTP_FC, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
  *out = 0;
  fe_GetURL (context,NET_CreateURLStruct(buf, FALSE), FALSE);
}

void
fe_SearchCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char* url;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  if ( PREF_GetUrl("internal_url.net_search", &url) ) {
    fe_GetURL (context,NET_CreateURLStruct(url, FALSE), FALSE);
  }
}

void
fe_GuideCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char* url = NULL;
  int ok;

  fe_UserActivity (context);

  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  ok = PREF_CopyConfigString("toolbar.places.default_url",&url);

  if (ok == PREF_NOERROR) 
  {
      FE_GetURL(context,NET_CreateURLStruct(url,NET_DONT_RELOAD));
  }                                                                       
  
  if (url)
  {
      XP_FREE(url);  
  }
}

void
fe_NetscapeCallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char* url;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  if ( PREF_GetUrl("toolbar.logo", &url) ) {
	  /*
	   *    Call this guy, it will sort out if the current context
	   *    is suitable, etc...
	   */
	  FE_GetURL(context,  NET_CreateURLStruct(url, NET_DONT_RELOAD));
  }
}

#ifdef __sgi
static void
fe_sgi_menu_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = HTTP_SGI_MENU, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
  *out = 0;
  fe_GetURL (context, NET_CreateURLStruct (buf, FALSE), FALSE);
}

static void
fe_adobe_menu_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = HTTP_ADOBE_MENU, out = buf; *in; in++, out++)
    *out = *in - HTTP_OFF;
  *out = 0;
  fe_GetURL (context, NET_CreateURLStruct (buf, FALSE), FALSE);
}

void
fe_SGICallback (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = HTTP_SGI_BUTTON, out = buf; *in; in++, out++)
    *out = *in - HTTP_OFF;
  *out = 0;
  fe_GetURL (context, NET_CreateURLStruct (buf, FALSE), FALSE);
}

void
fe_sgi_welcome_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char buf [1024], *in, *out;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  for (in = HTTP_SGI_WELCOME, out = buf; *in; in++, out++)
    *out = *in - HTTP_OFF;
  *out = 0;
  fe_GetURL (context, NET_CreateURLStruct (buf, FALSE), FALSE);
}
#endif /* __sgi */

#ifdef JAVA

void
fe_new_show_java_console_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  LJ_HideConsole();
  LJ_ShowConsole();
}

#endif

void
fe_load_images_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  fe_LoadDelayedImages (context);
}

/* Navigate menu
 */

/* not static -- it's needed by src/HTMLView.cpp */
void
fe_back_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  MWContext *top_context = XP_GetNonGridContext(context);
  URL_Struct *url;
  if (fe_IsGridParent(top_context))
    {
        if (LO_BackInGrid(top_context))
          {
            return;
          }
    }
  fe_UserActivity (top_context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  url = SHIST_CreateURLStructFromHistoryEntry (top_context,
					       SHIST_GetPrevious (top_context));
  if (url)
    {
      fe_GetURL (top_context, url, FALSE);
    }
  else
    {
      FE_Alert (top_context, fe_globalData.no_previous_url_message);
    }
}

/* not static -- it's needed by src/HTMLView.cpp */
void
fe_forward_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  MWContext *top_context = XP_GetNonGridContext(context);
  URL_Struct *url;
  if (fe_IsGridParent(top_context))
    {
        if (LO_ForwardInGrid(top_context))
          {
            return;
          }
    }
  fe_UserActivity (top_context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  url = SHIST_CreateURLStructFromHistoryEntry (top_context,
					       SHIST_GetNext (top_context));
  if (url)
    {
      fe_GetURL (top_context, url, FALSE);
    }
  else
    {
      FE_Alert (top_context, fe_globalData.no_next_url_message);
    }
}

/* not static -- it's needed by src/HTMLView.cpp ; BrowserFrame.cpp */
void
fe_home_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  URL_Struct *url;
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  if (!fe_globalPrefs.home_document || !*fe_globalPrefs.home_document)
    {
      XClearArea (XtDisplay (CONTEXT_WIDGET (context)),
		  XtWindow (CONTEXT_DATA (context)->drawing_area), 
		  0, 0,
		  CONTEXT_DATA (context)->scrolled_width,
		  CONTEXT_DATA (context)->scrolled_height, 
		  False);
      FE_Alert (context, fe_globalData.no_home_url_message);
      return;
    }
  url = NET_CreateURLStruct (fe_globalPrefs.home_document, FALSE);
  fe_GetURL ((MWContext *) closure, url, FALSE);
}

static void
fe_add_bookmark_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  fe_AddBookmarkCallback (widget, closure, call_data);
}

/* Help menu
 */
void
fe_about_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  fe_reuseBrowser(context, NET_CreateURLStruct ("about:", FALSE));
}

void
fe_manual_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  char* url;
  char buf[1024], *in, *out;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  url = HTTP_MANUAL;

  if ( url ) {
	for (in = url, out = buf; *in; in++, out++) *out = *in - HTTP_OFF;
	*out = 0;
	
	fe_reuseBrowser(context, NET_CreateURLStruct (buf, FALSE));
  }
}

#ifdef X_PLUGINS
void
fe_aboutPlugins_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  fe_GetURL (context, NET_CreateURLStruct ("about:plugins", FALSE), FALSE);
}
#endif /* X_PLUGINS */

void
fe_aboutFonts_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  fe_GetURL (context, NET_CreateURLStruct ("about:fonts", FALSE), FALSE);
}

/* Message composition stuff. */

void
fe_mailcompose_obeycb(MWContext *context, fe_MailComposeCallback cbid,
			void *call_data)
{
    switch (cbid) {
	case ComposeClearAllText_cb: {
	    Widget text = CONTEXT_DATA (context)->mcBodyText;
	    fe_SetTextFieldAndCallBack(text, "");
	    break;
	}
	case ComposeSelectAllText_cb: {
	    Widget text = CONTEXT_DATA (context)->mcBodyText;
	    XmPushButtonCallbackStruct *cb = (XmPushButtonCallbackStruct *)
						call_data;
	    if (cb)
		XmTextSetSelection(text, 0, XmTextGetLastPosition(text),
					cb->event->xkey.time);
	    break;
	}
    default:
      XP_ASSERT (0);
      break;
    }
}

#ifdef MOZ_MAIL_NEWS
void
FE_InsertMessageCompositionText(MSG_Pane* comppane,
				const char* text,
				XP_Bool leaveCursorAtBeginning) {

  MWContext* context = MSG_GetContext(comppane);
  /* call - function defined in src/ComposeView.cpp */
  fe_InsertMessageCompositionText(context, text, leaveCursorAtBeginning);
  return;
}
#endif  /* MOZ_MAIL_NEWS */

/* rlogin 
 */

void
FE_ConnectToRemoteHost (MWContext *context, int url_type, char *hostname,
			char *port, char *username)
{
  char *name;
  const char *command;
  const char *user_command;
  char *buf;
  const char *in;
  char *out;
  char **av, **ac, **ae;
  int na;
  Boolean cop_out_p = False;
  pid_t forked;

  switch (url_type)
    {
    case FE_TELNET_URL_TYPE:
      name = "telnet";
      command = fe_globalPrefs.telnet_command;
      user_command = 0 /* fe_globalPrefs.telnet_user_command */;
      break;
    case FE_TN3270_URL_TYPE:
      name = "tn3270";
      command = fe_globalPrefs.tn3270_command;
      user_command = 0 /* fe_globalPrefs.tn3270_user_command */;
      break;
    case FE_RLOGIN_URL_TYPE:
      name = "rlogin";
      command = fe_globalPrefs.rlogin_command;
      user_command = fe_globalPrefs.rlogin_user_command;
      break;
    default:
      abort ();
    }

  if (username && user_command && *user_command)
    command = user_command;
  else if (username)
    cop_out_p = True;

  buf = (char*) malloc( strlen(command) + 1
    + (hostname && *hostname ? strlen(hostname) : strlen("localhost"))
    + (username && *username ? strlen(username) : 0)
    + (port && *port ? strlen(port) : 0) );
  ac = av = (char**) malloc(10 * sizeof (char*));
  if (buf == NULL || av == NULL)
    goto malloc_lossage;
  ae = av + 10;
  in = command;
  *ac = out = buf;
  na = 0;
  while (*in)
    {
      if (*in == '%')
	{
	  in++;
	  if (*in == 'h')
	    {
	      char *s;
	      char *h = (hostname && *hostname ? hostname : "localhost");

	      /* Only allow the hostname to contain alphanumerics or
		 _ - and . to prevent people from embedding shell command
		 security holes in the host name. */
	      for (s = h; *s; s++)
		if (*s == '_' || (*s == '-' && s != h) || *s == '.' ||
		    isalpha(*s) || isdigit(*s))
		  *out++ = *s;
	    }
	  else if (*in == 'p')
	    {
	      if (port && *port)
		{
		  short port_num = atoi(port);

		  if (port_num > 0)
		    {
		      char buf1[6];
		      PR_snprintf (buf1, sizeof (buf1), "%.5d", port_num);
		      strcpy(out, buf1);
		      out += strlen(buf1);
		    }
		}
	    }
	  else if (*in == 'u')
	    {
	      char *s;
	      /* Only allow the user name to contain alphanumerics or
		 _ - and . to prevent people from embedding shell command
		 security holes in the host name. */
	      if (username && *username)
		{
		  for (s = username; *s; s++)
		    if (*s == '_' || (*s == '-' && s != username) ||
			*s == '.' || isalpha(*s) || isdigit(*s))
		      *out++ = *s;
		}
	    }
	  else if (*in == '%')
	    {
	      *out++ = '%';
	    }
	  else
	    {
	      char buf2 [255];
	      PR_snprintf (buf2, sizeof (buf2),
				 XP_GetString( XFE_UNKNOWN_ESCAPE_CODE ), name, *in);
	      FE_Alert (context, buf2);
	    }
	  if (*in)
	    in++;
	}
      else if (*in == ' ')
	{
	  if (out != *ac)
	    {
	      *out++ = '\0';
	      na++;
	      if (++ac == ae)
		{
		  av = (char**) realloc(av, (na + 10) * sizeof (char*));
		  if (av == NULL)
		    goto malloc_lossage;
		  ac = av + na;
		  ae = ac + 10;
		}
	      *ac = out;
	    }
	  in++;
	}
      else
	{
	  *out = *in;
	  out++;
	  in++;
	}
    }

  if (out != *ac)
    {
      *out = '\0';
      na++;
      ac++;
    }
  if (ac == ae)
    {
      av = (char**) realloc(av, (na + 1) * sizeof (char*));
      if (av == NULL)
	goto malloc_lossage;
      ac = av + na;
    }
  *ac = 0;

  if (cop_out_p)
    {
      char buf2 [1024];
      PR_snprintf (buf2, sizeof (buf2), XP_GetString(XFE_LOG_IN_AS), username);
      fe_Message (context, buf2);
    }

  switch (forked = fork ())
    {
    case -1:
      fe_perror (context, XP_GetString( XFE_COULDNT_FORK ) );
      break;
    case 0:
      {
	Display *dpy = XtDisplay (CONTEXT_WIDGET (context));
	close (ConnectionNumber (dpy));

	execvp (av [0], av);

	PR_snprintf (buf, sizeof (buf), XP_GetString( XFE_EXECVP_FAILED ),
			fe_progname, av[0]);
	perror (buf);
	exit (1);	/* Note that this only exits a child fork.  */
	break;
      }
    default:
      /* This is the "old" process (subproc pid is in `forked'.) */
      break;
    }
  free(av);
  free(buf);
  return;

malloc_lossage:
  if (av) free(av);
  if (buf) free(buf);
  fe_Message (context, XP_GetString(XFE_OUT_OF_MEMORY_URL));
}

/* The popup menu
 */

URL_Struct *fe_url_under_mouse = 0;
URL_Struct *fe_image_under_mouse = 0;

static void
fe_save_image_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmString xm_title = 0;
  char *title = 0;
  URL_Struct *url;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  XtVaGetValues (widget, XmNlabelString, &xm_title, 0);
  XmStringGetLtoR (xm_title, XmFONTLIST_DEFAULT_TAG, &title);
  XmStringFree(xm_title);

  url =	fe_image_under_mouse;
  if (! url)
    FE_Alert (context, fe_globalData.not_over_image_message);
  if (title) free (title);
  if (url)
    fe_SaveURL (context, url);
  fe_image_under_mouse = 0; /* it will be freed in the exit routine. */
}


static void
fe_save_link_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmString xm_title = 0;
  char *title = 0;
  URL_Struct *url;

  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;

  XtVaGetValues (widget, XmNlabelString, &xm_title, 0);
  XmStringGetLtoR (xm_title, XmFONTLIST_DEFAULT_TAG, &title);
  XmStringFree(xm_title);

  url =	fe_url_under_mouse;
  if (! url)
    FE_Alert (context, fe_globalData.not_over_link_message);
  if (title) free (title);
  if (url)
    fe_SaveURL (context, url);
  fe_url_under_mouse = 0; /* it will be freed in the exit routine. */
}

static void
fe_open_image_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  URL_Struct *url;
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data))
    return;
  url =	fe_image_under_mouse;
  if (! url)
    FE_Alert (context, fe_globalData.not_over_image_message);
  else
    {
      fe_GetURL (XP_GetNonGridContext(context), url, FALSE);
      fe_image_under_mouse = 0; /* it will be freed in the exit routine. */
    }
}

static void
fe_clipboard_url_1 (Widget widget, XtPointer closure, XtPointer call_data,
					URL_Struct *url, char* html)
{
  MWContext *context = (MWContext *) closure;
  XmPushButtonCallbackStruct *cb = (XmPushButtonCallbackStruct *) call_data;
  XEvent *event = (cb ? cb->event : 0);
  Time time = (event && (event->type == KeyPress ||
			 event->type == KeyRelease)
	       ? event->xkey.time :
	       event && (event->type == ButtonPress ||
			 event->type == ButtonRelease)
	       ? event->xbutton.time :
	       XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));
  fe_UserActivity (context);
  if (fe_hack_self_inserting_accelerator (widget, closure, call_data)) {
	  return;
  }
  if (! url)
    FE_Alert (context, fe_globalData.not_over_link_message);
  else
    {
		fe_OwnClipboard(widget, time,
						strdup (url->address), html, NULL, 0, False);
#ifdef NOT_rhess
		/*
		 * NOTE:  it's not really valid to make a primary selection if you
		 *        can't show the selection on the display...
		 *        [ this is a hidden selection ]
		 *
		 */
		fe_OwnPrimarySelection(widget, time, 
							   strdup (url->address), html, NULL, 0, context);
#endif
		NET_FreeURLStruct (url);
    }
}

static char fe_link_html_head[]   = "<A HREF=\"";
static char fe_link_html_middle[] = "\" >";
static char fe_link_html_tail[]   = "</A>";

static char fe_image_html_head[]  = "<IMG SRC=\"";
static char fe_image_html_tail[]  = "\" ALT=\"Image\" BORDER=0 >";

#define XFE_LINK_HTML_HEAD   (fe_link_html_head)
#define XFE_LINK_HTML_MIDDLE (fe_link_html_middle)
#define XFE_LINK_HTML_TAIL   (fe_link_html_tail)

#define XFE_IMAGE_HTML_HEAD  (fe_image_html_head)
#define XFE_IMAGE_HTML_TAIL  (fe_image_html_tail)

void
fe_clipboard_link_cb (Widget widget, XtPointer closure, 
					  XtPointer call_data,
					  URL_Struct *url)
{
	char *html = NULL;
	char *link = url->address;
	int32 len = strlen(link);
	char *head = XFE_LINK_HTML_HEAD;
	char *midd = XFE_LINK_HTML_MIDDLE;
	char *tail = XFE_LINK_HTML_TAIL;

	html = XP_ALLOC(len + len + 2 +
					strlen(head) + strlen(midd) + strlen(tail)
					);
	sprintf(html, "%s%s%s%s%s\0", head, link, midd, link, tail);

	fe_clipboard_url_1 (widget, closure, call_data, url, html);
	XP_FREE(html);
}

void
fe_clipboard_image_link_cb (Widget widget, XtPointer closure, 
							XtPointer call_data,
							URL_Struct *url, URL_Struct *img)
{
	char *html = NULL;
	char *link = url->address;
	int32 len = strlen(link);
	char *head = XFE_LINK_HTML_HEAD;
	char *midd = XFE_LINK_HTML_MIDDLE;
	char *tail = XFE_LINK_HTML_TAIL;

	if (!img) {
		html = XP_ALLOC(len + len + 2 +
						strlen(head) + strlen(midd) + strlen(tail)
						);
		sprintf(html, "%s%s%s%s%s\0", head, link, midd, link, tail);

		fe_clipboard_url_1 (widget, closure, call_data, url, html);
		XP_FREE(html);
	}
	else {
		char *image = img->address;
		char *ihead = XFE_IMAGE_HTML_HEAD;
		char *itail = XFE_IMAGE_HTML_TAIL;

		html = XP_ALLOC(len + 2 + strlen(image) +
						strlen(head) + strlen(midd) + strlen(tail) +
						strlen(ihead) + strlen(itail)
						);
		sprintf(html, "%s%s%s%s%s%s%s\0", 
				head, link, midd, ihead, image, itail, tail);

		fe_clipboard_url_1 (widget, closure, call_data, url, html);
		NET_FreeURLStruct (img);
	}
	XP_FREE(html);
}

void
fe_clipboard_image_cb (Widget widget, XtPointer closure, 
					   XtPointer call_data,
					   URL_Struct *url)
{
	char *html = NULL;
	char *link = url->address;
	int32 len = strlen(link);

	char *head = XFE_IMAGE_HTML_HEAD;
	char *tail = XFE_IMAGE_HTML_TAIL;

	html = XP_ALLOC(len + 2 + strlen(head) + strlen(tail));

	sprintf(html, "%s%s%s\0", head, link, tail);

	fe_clipboard_url_1 (widget, closure, call_data, url, html);
	XP_FREE(html);
}

/*
 * For scrolling the window with the arrow keys, we need to
 * insert a lookahead key eater into each action routine,
 * else it is possible to queue up too many key events, and
 * screw up our careful scroll/expose event timing.
 */
static void
fe_eat_window_key_events(Display *display, Window window)
{
  XEvent event;

  XSync(display, FALSE);
  while (XCheckTypedWindowEvent(display, window, KeyPress, &event) == TRUE);
}


static void
fe_column_forward_action (Widget widget, XEvent *event,
			  String *av, Cardinal *ac)
{
  XKeyEvent *kev = (XKeyEvent *)event;
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;

  fe_column_forward_cb (widget, (XtPointer)context, (XtPointer)0);
  fe_eat_window_key_events(XtDisplay(widget), kev->window);
}


static void
fe_column_backward_action (Widget widget, XEvent *event,
			  String *av, Cardinal *ac)
{
  XKeyEvent *kev = (XKeyEvent *)event;
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;

  fe_column_backward_cb (widget, (XtPointer)context, (XtPointer)0);
  fe_eat_window_key_events(XtDisplay(widget), kev->window);
}


static void
fe_line_forward_action (Widget widget, XEvent *event,
			  String *av, Cardinal *ac)
{
  XKeyEvent *kev = (XKeyEvent *)event;
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;

  fe_line_forward_cb (widget, (XtPointer)context, (XtPointer)0);
  fe_eat_window_key_events(XtDisplay(widget), kev->window);
}


static void
fe_line_backward_action (Widget widget, XEvent *event,
			  String *av, Cardinal *ac)
{
  XKeyEvent *kev = (XKeyEvent *)event;
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;

  fe_line_backward_cb (widget, (XtPointer)context, (XtPointer)0);
  fe_eat_window_key_events(XtDisplay(widget), kev->window);
}


/* Actions for use in translations tables. 
 */

#define DEFACTION(NAME) \
static void \
fe_##NAME##_action (Widget widget, XEvent *event, String *av, Cardinal *ac) \
{  \
  MWContext *context = fe_WidgetToMWContext (widget); \
  XP_ASSERT (context); \
  if (!context) return; \
  fe_##NAME##_cb (widget, (XtPointer)context, (XtPointer)0); \
}

/* DEFACTION (new) */
/*DEFACTION (open_url)*/
/*DEFACTION (open_file)*/
#ifdef EDITOR
/*DEFACTION (edit)*/
#endif /* EDITOR */
/*DEFACTION (save_as)*/
/*DEFACTION (mailto)*/
/*DEFACTION (print)*/
/* DEFACTION (docinfo) */
/*DEFACTION (delete)*/
/* DEFACTION (quit) */
/* DEFACTION (cut) */
/* DEFACTION (copy) */
/* DEFACTION (paste) */
/* DEFACTION (paste_quoted) */
/*DEFACTION (find)*/
/* DEFACTION (find_again) */
/* DEFACTION (reload) */
/* DEFACTION (load_images) */
/* DEFACTION (refresh) */
/* DEFACTION (view_source) */
/* DEFACTION (back) */
/* DEFACTION (forward) */
/* DEFACTION (history) */
/* DEFACTION (home) */
/*DEFACTION (save_options)*/
/* DEFACTION (view_bookmark) */
DEFACTION (fishcam)
DEFACTION (net_showstatus)
/* DEFACTION (abort) */
DEFACTION (page_forward)
DEFACTION (page_backward)
/*DEFACTION (line_forward)*/
/*DEFACTION (line_backward)*/
/*DEFACTION (column_forward)*/
/*DEFACTION (column_backward)*/

DEFACTION (save_image)
DEFACTION (open_image)
DEFACTION (save_link)
/*DEFACTION (follow_link)*/
/*DEFACTION (follow_link_new)*/
#undef DEFACTION

static void
fe_open_url_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  /* openURL() pops up a file requestor.
     openURL(http:xxx) opens that URL.
     Really only one arg is meaningful, but we also accept the form
     openURL(http:xxx, remote) for the sake of remote.c, and treat
     openURL(remote) the same as openURL().
   */
  MWContext *context = fe_WidgetToMWContext (widget);
  MWContext *old_context = NULL;
  Boolean other_p = False;
  char *windowName = 0;
  XP_ASSERT (context);
  if (!context) return;
  context = XP_GetNonGridContext (context);
  fe_UserActivity (context);

  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;

  if (*ac > 1 && av[*ac-1] )
  {
     if ( 
      (!strcasecomp (av[*ac-1], "new-window") ||
       !strcasecomp (av[*ac-1], "new_window") ||
       !strcasecomp (av[*ac-1], "newWindow") ||
       !strcasecomp (av[*ac-1], "new")))
    {
      other_p = True;
      (*ac)--;
    }
    else if ( (old_context = XP_FindNamedContextInList(context, av[*ac-1])))
    {
	context = old_context;
	other_p = False;
        (*ac)--;
    }
    else 
    {
	StrAllocCopy(windowName, av[*ac-1]);
	other_p = True;
        (*ac)--;
    }

   }

  if (*ac == 1 && av[0])
    {
      URL_Struct *url_struct = NET_CreateURLStruct (av[0], FALSE);

#ifdef MOZ_MAIL_NEWS
      /* Dont create new windows for Compose. fe_GetURL will take care of it */
      if (MSG_RequiresComposeWindow(url_struct->address))
	other_p = False;
#endif

      if (other_p)
      {
	context = fe_MakeWindow (XtParent (CONTEXT_WIDGET (context)),
		       context, url_struct, windowName, 
			MWContextBrowser, FALSE);
        XP_FREE(windowName);
      }
      else
	fe_GetURL (context, url_struct, FALSE);
    }
  else if (*ac > 1)
    {
      fprintf (stderr, 
	       XP_GetString(XFE_COMMANDS_OPEN_URL_USAGE),
	       fe_progname);
    }
  else
    {
      fe_open_url_cb (widget, (XtPointer)context, (XtPointer)0);
    }
}

static void
fe_print_remote_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  /* Silent print for remote. Valid invocations are
     print(filename, remote)           : print URL of context to filename
     print(remote)                     : print URL of context to printer
     print(remote)                     : print URL of context to printer
  */
  MWContext *context = fe_WidgetToMWContext (widget);
  URL_Struct *url_struct = NULL;
  Boolean toFile = False;
  char *filename = NULL;

  XP_ASSERT (context);
  if (!context) return;
  fe_UserActivity (context);

  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;
  else
    return;    /* this is valid only for remote */

  if (*ac && av[*ac-1]) {
    filename = av[*ac-1];
    if (*filename) toFile = True;
    (*ac)--;
  }

  if (*ac > 0)
    fprintf (stderr, "%s: usage: print([filename])\n", fe_progname);

  url_struct = SHIST_CreateWysiwygURLStruct (context,
                                       SHIST_GetCurrent (&context->hist));

  if (url_struct)
    {
      if (!toFile || filename)
    fe_Print(context, url_struct, toFile, filename);
      else
        NET_FreeURLStruct (url_struct);
    }
}

#ifdef MOZ_MAIL_NEWS
static void
fe_mailto_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  /* see also fe_open_url_action()

     #### This really ought to be implemented by opening a "mailto:" URL,
     instead of by calling MSG_ComposeMessage -- that'd be more abstract
     and stuff.  -jwz
   */

  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;
  fe_UserActivity (context);
  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;
  if (*ac >= 1)
    {
      char *to;
      int size = 0;
      int i;
      for (i = 0; i < *ac; i++)
	size += (strlen (av[i]) + 2);
      to = (char *) malloc (size + 1);
      *to = 0;
      for (i = 0; i < *ac; i++)
	{
	  strcat (to, av[i]);
	  if (i < (*ac)-1)
	    strcat (to, ", ");
	}

      MSG_ComposeMessage (context,
			  0, /* from */
			  0, /* reply_to */
			  to,
			  0, /* cc */
			  0, /* bcc */
			  0, /* fcc */
			  0, /* newsgroups */
			  0, /* followup_to */
			  0, /* organization */
			  0, /* subject */
			  0, /* references */
			  0, /* other_random_headers */
			  0, /* priority */
			  0, /* attachment */
			  0, /* newspost_url */
			  0, /* body */
			  FALSE, /* encrypt_p */
			  FALSE, /* sign_p */
			  FALSE, /* force_plain_text */
			  0 /* html_part */
			  );
    }
  else if (*ac > 1)
    {
      fprintf (stderr, 
			   XP_GetString(XFE_COMMANDS_MAIL_TO_USAGE),
			   fe_progname);
    }
  else
    {
      fe_mailto_cb (widget, (XtPointer)context, (XtPointer)0);
    }
}
#endif  /* MOZ_MAIL_NEWS */


static void
fe_add_bookmark_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  /* See also fe_open_url_action() */
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_ASSERT (context);
  if (!context) return;
  fe_UserActivity (context);
  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>"))
    (*ac)--;
  if ((*ac == 1 || *ac == 2) && av[0])
    {
      URL_Struct *url = NET_CreateURLStruct (av[0], FALSE);
      char *title = ((*ac == 2 && av[1]) ? av[1] : 0);
      fe_AddToBookmark (context, title, url, 0);
    }
  else if (*ac > 2)
    {
      fprintf (stderr,
			   XP_GetString(XFE_COMMANDS_ADD_BOOKMARK_USAGE),
			   fe_progname);
    }
  else
    {
      fe_add_bookmark_cb (widget, (XtPointer)context, (XtPointer)0);
    }
}

static void
fe_html_help_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  MWContext *context = fe_WidgetToMWContext (widget);
  XP_Bool remote_p = False;
  char *map_file_url;
  char *id;
  char *search_text;

  XP_ASSERT (context);
  if (!context) return;
  fe_UserActivity (context);
  if (*ac && av[*ac-1] && !strcmp (av[*ac-1], "<remote>")) {
    remote_p = True;
    (*ac)--;
  }

  if (*ac == 3) {
    map_file_url = av[0];
    id = av[1];
    search_text = av[2];
    NET_GetHTMLHelpFileFromMapFile(context, map_file_url, id, search_text);
  }
  else {
      fprintf (stderr,
			   XP_GetString(XFE_COMMANDS_HTML_HELP_USAGE),
			   fe_progname);
  }
}


static void
fe_deleteKey_action (Widget widget, XEvent *event, String *av, Cardinal *ac)
{
  MWContext *context = fe_WidgetToMWContext (widget);

  XP_ASSERT(context);
  if (!context) return;

#ifdef MOZ_MAIL_NEWS
  if (context->type == MWContextMail || context->type == MWContextNews)
    XP_ASSERT(0);
  else
#endif  /* MOZ_MAIL_NEWS */
    fe_page_backward_action(widget, event, av, ac);
}

static void
fe_undefined_key_action (Widget widget, XEvent *event,
			 String *av, Cardinal *ac)
{
  XBell (XtDisplay (widget), 0);
}

/* for the rest, look in src/Command.cpp -- for 'cmd_mapping mapping[]' */
XtActionsRec fe_CommandActions [] =
{
  { "addBookmark",	fe_add_bookmark_action	},

#ifdef MOZ_MAIL_NEWS
  { "mailto",		fe_mailto_action },
#endif

  { "print",        fe_print_remote_action },
  { "fishcam",		fe_fishcam_action	},
  { "net_showstatus",	fe_net_showstatus_action	},
  { "PageDown",		fe_page_forward_action	},
  { "PageUp",		fe_page_backward_action	},
  { "LineDown",		fe_line_forward_action	},
  { "LineUp",		fe_line_backward_action	},
  { "ColumnLeft",	fe_column_forward_action  },
  { "ColumnRight",	fe_column_backward_action },

  { "Delete",		fe_deleteKey_action },
  { "Backspace",	fe_deleteKey_action },

  { "SaveImage",	fe_save_image_action },
  { "OpenImage",	fe_open_image_action },
  { "SaveURL",		fe_save_link_action },
  { "OpenURL",		fe_open_url_action },
  { "OpenURLNewWindow",	fe_open_url_action },

  /* For Html Help */
  { "htmlHelp",		fe_html_help_action },

  { "undefined-key",	fe_undefined_key_action }
};

int fe_CommandActionsSize = countof (fe_CommandActions);

/**************************/
/* MessageCompose actions */
/* (Mail/News actions are */
/* defined in mailnews.c) */
/**************************/


void
fe_InitCommandActions ()
{
  XtAppAddActions(fe_XtAppContext, fe_CommandActions, fe_CommandActionsSize);
}


/* This installs all the usual translations on widgets which are a part
   of the main Netscape window -- it recursively decends the tree, 
   installing the appropriate translations everywhere.  This should
   not be called on popups/transients, but only on widgets which are
   children of the main Shell.
 */
void
fe_HackTranslations (MWContext *context, Widget widget)
{
  XtTranslations global_translations = 0;
  XtTranslations secondary_translations = 0;
  XP_Bool has_display_area = FALSE;

  if (XmIsGadget (widget))
    return;

 /* To prevent focus problems, dont enable translations on the menubar
    and its children. The problem was that when we had the translations
    in the menubar too, we could do a translation and popup a modal
    dialog when one of the menu's from the menubar was pulleddown. Now
    motif gets too confused about who holds pointer and keyboard focus.
 */

  if (XmIsRowColumn(widget)) {
    unsigned char type;
    XtVaGetValues(widget, XmNrowColumnType, &type, 0);
    if (type == XmMENU_BAR)
      return;
  }

  switch (context->type)
    {
#ifdef EDITOR
    case MWContextEditor:
      has_display_area = FALSE;
      global_translations = fe_globalData.global_translations;
      secondary_translations = fe_globalData.editor_global_translations;
      break;
#endif /*EDITOR*/

    case MWContextBrowser:
      has_display_area = TRUE;
      global_translations = fe_globalData.global_translations;
      secondary_translations = 0;
      break;

    case MWContextMail:
    case MWContextNews:
      has_display_area = TRUE;
      global_translations = fe_globalData.global_translations;
      secondary_translations = fe_globalData.mailnews_global_translations;
      break;

    case MWContextMailMsg:
      has_display_area = TRUE;
      global_translations = fe_globalData.global_translations;
      secondary_translations = fe_globalData.messagewin_global_translations;
      break;

    case MWContextMessageComposition:
      has_display_area = FALSE;
      global_translations = 0;
      secondary_translations = fe_globalData.mailcompose_global_translations;
      break;

    case MWContextBookmarks:
      has_display_area = FALSE;
      global_translations = 0;
      secondary_translations = fe_globalData.bm_global_translations;
      break;
    
    case MWContextHistory:
      has_display_area = FALSE;
      global_translations = 0;
      secondary_translations = fe_globalData.gh_global_translations;
      break;
    
    case MWContextAddressBook:
      has_display_area = FALSE;
      global_translations = 0;
      secondary_translations = fe_globalData.ab_global_translations;
      break;
    case MWContextDialog:
      has_display_area = TRUE;
      global_translations = 0;
      secondary_translations = fe_globalData.dialog_global_translations;
      break;

    default:
      break;
  }

  if (global_translations)
    XtOverrideTranslations (widget, global_translations);
  if (secondary_translations)
    XtOverrideTranslations (widget, secondary_translations);


  if (XmIsTextField (widget) || XmIsText (widget) || XmIsList(widget))
    {
      /* Set up the editing translations (all text fields, everywhere.) */
      if (XmIsTextField (widget) || XmIsText (widget))
	fe_HackTextTranslations (widget);

      /* Install globalTextFieldTranslations on single-line text fields in
	 windows which have an HTML display area (browser, mail, news) but
	 not in windows which don't have one (compose, download, bookmarks,
	 address book...)
       */
      if (has_display_area &&
	  XmIsTextField (widget) &&
	  fe_globalData.global_text_field_translations)
	XtOverrideTranslations (widget,
				fe_globalData.global_text_field_translations);
   }
  else
   {
       Widget *kids = 0;
       Cardinal nkids = 0;

      /* Not a Text or TextField.
       */
      /* Install globalNonTextTranslations on non-text widgets in windows which
	 have an HTML display area (browser, mail, news, view source) but not
	 in windows which don't have one (compose, download, bookmarks,
	 address book...)
       */
      if (has_display_area &&
	  fe_globalData.global_nontext_translations)
	XtOverrideTranslations (widget,
				fe_globalData.global_nontext_translations);

      /* Now recurse on the children.
       */
      XtVaGetValues (widget, XmNchildren, &kids, XmNnumChildren, &nkids, 0);
      while (nkids--)
	  fe_HackTranslations (context, kids [nkids]);
    }
}

/* This installs all the usual translations on Text and TextField widgets.
 */
void
fe_HackTextTranslations (Widget widget)
{ 
  Widget parent = widget;

  for (parent=widget; parent && !XtIsWMShell (parent); parent=XtParent(parent))
    if (XmLIsGrid(parent))
      /* We shouldn't be messing with Grid widget and its children */
      return;

  if (XmIsTextField(widget))
    {
      if (fe_globalData.editing_translations)
	XtOverrideTranslations (widget, fe_globalData.editing_translations);
      if (fe_globalData.single_line_editing_translations)
	XtOverrideTranslations (widget,
			      fe_globalData.single_line_editing_translations);
    }
  else if (XmIsText(widget))
    {
      if (fe_globalData.editing_translations)
	XtOverrideTranslations (widget, fe_globalData.editing_translations);
      if (fe_globalData.multi_line_editing_translations)
	XtOverrideTranslations (widget,
			      fe_globalData.multi_line_editing_translations);
    }
  else
    {
      XP_ASSERT(0);
    }
}

void
fe_HackFormTextTranslations(Widget widget)
{
  Widget parent = widget;

  for (parent=widget; parent && !XtIsWMShell (parent); parent=XtParent(parent))
    if (XmLIsGrid(parent))
      /* We shouldn't be messing with Grid widget and its children */
      return;

  if ((XmIsTextField(widget)) || (XmIsText(widget)) )
    {
      if (fe_globalData.form_elem_editing_translations)
	XtOverrideTranslations (widget, fe_globalData.form_elem_editing_translations);
    }
  else
    {
      XP_ASSERT(0);
    }
}


/* This installs all the usual translations for popups/transients.
   All it does is recurse the tree and call fe_HackTextTranslations
   on any Text or TextField widgets it finds.
 */
void
fe_HackDialogTranslations (Widget widget)
{
  if (XmIsGadget (widget))
    return;
  else if (XmIsText (widget) || XmIsTextField (widget))
    fe_HackTextTranslations (widget);
  else
    {
      Widget *kids = 0;
      Cardinal nkids = 0;
      XtVaGetValues (widget, XmNchildren, &kids, XmNnumChildren, &nkids, 0);
      while (nkids--)
	fe_HackDialogTranslations (kids [nkids]);
    }
}

/* these two are now in src/DownloadFrame.cpp */
extern NET_StreamClass *fe_MakeSaveAsStream (int format_out, void *data_obj,
											 URL_Struct *url_struct, MWContext *context);

extern NET_StreamClass *fe_MakeSaveAsStreamNoPrompt (int format_out, void *data_obj,
													 URL_Struct *url_struct, MWContext *context);

void
fe_RegisterConverters (void)
{
#ifdef NEW_DECODERS
  NET_ClearAllConverters ();
#endif /* NEW_DECODERS */

  if (fe_encoding_extensions)
    {
      int i = 0;
      while (fe_encoding_extensions [i])
	free (fe_encoding_extensions [i++]);
      free (fe_encoding_extensions);
      fe_encoding_extensions = 0;
    }

  /* register X specific decoders
   */
  if (fe_globalData.encoding_filters)
    {
      char *copy = strdup (fe_globalData.encoding_filters);
      char *rest = copy;
      char *end = rest + strlen (rest);

      int exts_count = 0;
      int exts_size = 10;
      char **all_exts = (char **) malloc (sizeof (char *) * exts_size);

      while (rest < end)
	{
	  char *start;
	  char *eol, *colon;
	  char *input, *output, *extensions, *command;
	  eol = strchr (rest, '\n');
	  if (eol) *eol = 0;

	  rest = fe_StringTrim (rest);
	  if (! *rest)
	    /* blank lines are ok */
	    continue;

	  start = rest;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  input = fe_StringTrim (rest);
	  rest = colon + 1;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  output = fe_StringTrim (rest);
	  rest = colon + 1;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  extensions = fe_StringTrim (rest);
	  rest = colon + 1;

	  command = fe_StringTrim (rest);
	  rest = colon + 1;
	  
	  if (*command)
	    {
	      /* First save away the extensions. */
	      char *rest = extensions;
	      while (*rest)
		{
		  char *start;
		  char *comma, *end;
		  while (isspace (*rest))
		    rest++;
		  start = rest;
		  comma = XP_STRCHR (start, ',');
		  end = (comma ? comma - 1 : start + strlen (start));
		  while (end >= start && isspace (*end))
		    end--;
		  if (comma) end++;
		  if (start < end)
		    {
		      all_exts [exts_count] =
			(char *) malloc (end - start + 1);
		      strncpy (all_exts [exts_count], start, end - start);
		      all_exts [exts_count][end - start] = 0;
		      if (++exts_count == exts_size)
			all_exts = (char **)
			  realloc (all_exts,
				   sizeof (char *) * (exts_size += 10));
		    }
		  rest = (comma ? comma + 1 : end);
		}
	      all_exts [exts_count] = 0;
	      fe_encoding_extensions = all_exts;

	      /* Now register the converter. */
	      NET_RegisterExternalDecoderCommand (input, output, command);
	    }
	  else
	    {
	  LOSER:
	      fprintf (stderr,
				   XP_GetString(XFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC),
				   fe_progname, start);
	    }
	  rest = (eol ? eol + 1 : end);
	}
      free (copy);
    }

  /* Register standard decoders
     This must come AFTER all calls to NET_RegisterExternalDecoderCommand(),
     (at least in the `NEW_DECODERS' world.)
   */
  NET_RegisterMIMEDecoders ();

  /* How to save to disk. */
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS, NULL,
				    fe_MakeSaveAsStream);

  /* Saving any binary format as type `text' should save as `source' instead.
   */
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS_TEXT, NULL,
				    fe_MakeSaveAsStreamNoPrompt);
  NET_RegisterContentTypeConverter ("*", FO_QUOTE_MESSAGE, NULL,
				    fe_MakeSaveAsStreamNoPrompt);

  /* default presentation converter - offer to save unknown types. */
  NET_RegisterContentTypeConverter ("*", FO_PRESENT, NULL,
				    fe_MakeSaveAsStream);

#if 0
  NET_RegisterContentTypeConverter ("*", FO_VIEW_SOURCE, NULL,
				    fe_MakeViewSourceStream);
#endif

#ifndef NO_MOCHA_CONVERTER_HACK
  /* libmocha:LM_InitMocha() installs this convert. We blow away all
   * converters that were installed and hence these mocha default converters
   * dont get recreated. And mocha has no call to re-register them.
   * So this hack. - dp/brendan
   */
  NET_RegisterContentTypeConverter(APPLICATION_JAVASCRIPT, FO_PRESENT, 0,
				   NET_CreateMochaConverter);
#endif /* NO_MOCHA_CONVERTER_HACK */

  /* Parse stuff out of the .mime.types and .mailcap files.
   * We dont have to check dates of files for modified because all that
   * would have been done by the caller. The only place time checking
   * happens is
   * (1) Helperapp page is created
   * (2) Helpers are being saved (OK button pressed on the General Prefs).
   */
  NET_InitFileFormatTypes (fe_globalPrefs.private_mime_types_file,
			   fe_globalPrefs.global_mime_types_file);
  fe_isFileChanged(fe_globalPrefs.private_mime_types_file, 0,
		   &fe_globalData.privateMimetypeFileModifiedTime);

  NET_RegisterConverters (fe_globalPrefs.private_mailcap_file,
			  fe_globalPrefs.global_mailcap_file);
  fe_isFileChanged(fe_globalPrefs.private_mailcap_file, 0,
		   &fe_globalData.privateMailcapFileModifiedTime);

#ifndef NO_WEB_FONTS
  /* Register webfont converters */
  NF_RegisterConverters();
#endif /* NO_WEB_FONTS */

  /* Plugins go on top of all this */
  fe_RegisterPluginConverters();
}

/* The Windows menu in the toolbar */

static void
fe_switch_context_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  struct fe_MWContext_cons *cntx = fe_all_MWContexts;

  /* We need to watchout here. If we were holding down the menu and
   * before we select, a context gets destroyed (ftp is done say), then
   * selecting on that item would give an invalid context here. Guard
   * against that case by validating the context.
   */
  
  while (cntx && (cntx->context != context))
    cntx = cntx->next;
  if (!cntx)
    /* Ah ha! invalid context. I told you! */
    return;
  
  XMapRaised(XtDisplay(CONTEXT_WIDGET(context)),
		   XtWindow(CONTEXT_WIDGET(context)));
  return;
}

void 
fe_SetString(Widget widget, const char* propname, char* str)
{
  XmString xmstr;
  xmstr = XmStringCreate(str, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(widget, propname, xmstr, 0);
  XmStringFree(xmstr);
}

static void
fe_addto_windows_menu(Widget w, MWContext *context, MWContextType lookfor)
{
  Widget menu = CONTEXT_DATA (context)->windows_menu;
  Widget button = w;
  char buf[1024], *title, *s;
  struct fe_MWContext_cons *cntx = NULL;
  Boolean more = True;		/* some contexts are always single. In those
				   cases, this will become False and stop
				   further search */
  INTL_CharSetInfo c;

  for (cntx = fe_all_MWContexts; more && cntx; cntx = cntx->next)
    {
    c = LO_GetDocumentCharacterSetInfo(cntx->context);
    if (cntx->context->type == lookfor) {
      /* Fix title */
      switch (lookfor) {
	case MWContextMail:
  	    title = XP_GetString( XFE_NETSCAPE_MAIL );
	    more = False;
	    break;
	case MWContextNews:
  	    title = XP_GetString( XFE_NETSCAPE_NEWS );
	    more = False;
	    break;
	case MWContextBookmarks:
  	    title = XP_GetString( XFE_BOOKMARKS );
	    more = False;
	    break;
	case MWContextAddressBook:
  	    title = XP_GetString( XFE_ADDRESS_BOOK );
	    more = False;
	    break;
	case MWContextSaveToDisk:
	    button = 0;
	    title =
		fe_GetTextField (CONTEXT_DATA(cntx->context)->url_label);
	    XP_ASSERT(title);
	    s = fe_Basename(title);
	    INTL_MidTruncateString (INTL_GetCSIWinCSID(c), s, s, 25);
	    PR_snprintf(buf, sizeof (buf), XP_GetString( XFE_DOWNLOAD_FILE ), s );
	    XtFree(title);
	    title = buf;
	    break;
	case MWContextMessageComposition:
	    button = 0;
	    title = cntx->context->title;
	    if (!title) {
		title = buf;
		PR_snprintf(buf, sizeof (buf), XP_GetString( XFE_COMPOSE_NO_SUBJECT ) );
	    }
	    else {
		s = XP_STRDUP(title);
	        INTL_MidTruncateString (INTL_GetCSIWinCSID(c), s, s, 25);
		PR_snprintf(buf, sizeof (buf), XP_GetString( XFE_COMPOSE ), s );
		XP_FREE(s);
		title = buf;
	    }
	    break;
	case MWContextBrowser:
	case MWContextEditor:
	    button = 0;
	    /* Dont process grid children */
	    if (cntx->context->is_grid_cell) continue;
	    title = cntx->context->title;
#if defined(EDITOR) && defined(EDITOR_UI)
	    if (!title || title[0] == '\0') {
	      History_entry *he = SHIST_GetCurrent(&cntx->context->hist);
	      char *url = (he && he->address ? he->address : 0);
	      if (url != NULL && strcmp(url, "file:///Untitled") != 0)
	        title = url;
	    }
#endif /*defined(EDITOR) && defined(EDITOR_UI)*/
	    if (!title || !(*title)) {
	      title = buf;
	      PR_snprintf(buf, sizeof (buf), XP_GetString( XFE_NETSCAPE_UNTITLED ) );
	    }
	    else {
		unsigned char *loc;

		s = strdup(title);
		title = buf;
		INTL_MidTruncateString (INTL_GetCSIWinCSID(c), s, s, 35);
		loc = fe_ConvertToLocaleEncoding(INTL_GetCSIWinCSID(c),
						 (unsigned char *) s);
		PR_snprintf(buf, sizeof (buf), XP_GetString( XFE_NETSCAPE ),
			    loc);
		if ((char *) loc != s) {
		    XP_FREE(loc);
		}
		XP_FREE(s);
	    }
	    break;
        default:
	  assert(0);
	  break;
      }
      if (!button)
	button = XmCreateToggleButtonGadget (menu, "windowButton", NULL, 0);
      XtVaSetValues(button, XmNset, (context == cntx->context),
			XmNvisibleWhenOff, False, 0);
      XtAddCallback (button, XmNvalueChangedCallback, fe_switch_context_cb,
			cntx->context);
      fe_SetString(button, XmNlabelString, title);
      XtManageChild(button);
      }
    }
    /* If nothing found, turn button off */
    if (w && !cntx)
      XtVaSetValues(w, XmNset, False, XmNvisibleWhenOff, False, 0);
}

void
fe_GenerateWindowsMenu (MWContext *context)
{
  Widget menu = CONTEXT_DATA (context)->windows_menu;
  Widget *kids = 0;
  Cardinal nkids = 0, kid;

  if (menu == NULL || CONTEXT_DATA (context)->windows_menu_up_to_date_p)
    return;

  XtVaGetValues (menu, XmNchildren, &kids, XmNnumChildren, &nkids, 0);

  if (context->type == MWContextBrowser)
    XP_ASSERT(nkids >= 7);
  else
    XP_ASSERT(nkids >= 6);

  kid = 0;

  fe_addto_windows_menu(kids[kid++], context, MWContextMail);

  /* next is separator */
  kid++;

  fe_addto_windows_menu(kids[kid++], context, MWContextAddressBook);
  fe_addto_windows_menu(kids[kid++], context, MWContextBookmarks);

  /* next is history for MWContextBrowser only */
  if (context->type == MWContextBrowser)
      XtVaSetValues(kids[kid++], XmNset, False, XmNvisibleWhenOff, False, 0);

  /* Benjie - added this btn to test mailfilter I will get rid of it soon */
  XtVaSetValues(kids[kid++], XmNset, False, XmNvisibleWhenOff, False, 0);

  /* next is separator */
  kid++;

  /* Destroy the rest of the children buttons */
  XtUnmanageChildren (&kids[kid], nkids-kid);
  for(; kid < nkids; kid++) XtDestroyWidget(kids[kid]);

  fe_addto_windows_menu(0, context, MWContextBrowser);
#if defined(EDITOR) && defined(EDITOR_UI)
  fe_addto_windows_menu(0, context, MWContextEditor);
#endif
  fe_addto_windows_menu(0, context, MWContextMessageComposition);
  fe_addto_windows_menu(0, context, MWContextSaveToDisk);

  CONTEXT_DATA (context)->windows_menu_up_to_date_p = True;
}

