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

#include "selection.h"
#include <libi18n.h>
#include <layers.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <Xm/CutPaste.h>	/* for PasteQuote clipboard hackery */
/*#include <Xm/AtomMgr.h>*/

#include "intl_csi.h"
#include "felocale.h"

static char          *fe_clipboard_text   = NULL;
static char          *fe_clipboard_html   = NULL;
static char          *fe_clipboard_data   = NULL;
static unsigned long  fe_clipboard_dlen   = 0;
static Time           fe_clipboard_time   = 0;
static Widget         fe_clipboard_widget = NULL;
static XP_Bool        fe_clipboard_motif  = False;
static XP_Bool        fe_clipboard_empty  = True;

static char          *fe_selection_text   = NULL;
static char          *fe_selection_html   = NULL;
static char          *fe_selection_data   = NULL;
static unsigned long  fe_selection_dlen   = 0;
static Time           fe_selection_time   = 0;
static Widget         fe_selection_widget = NULL;
static MWContext     *fe_selection_ctx    = NULL;
static MWContext     *fe_selection_lost   = NULL;
static XP_Bool        fe_selection_empty  = True;

static char fe_clipboard_data_name[] = "NETSCAPE_HTML";
static char fe_clipboard_html_name[] = "HTML";
static char fe_clipboard_text_name[] = "STRING";
static char fe_clipboard_app_name[] = "MOZILLA";

#define XFE_CCP_DATA (fe_clipboard_data_name)
#define XFE_CCP_HTML (fe_clipboard_html_name)
#define XFE_CCP_TEXT (fe_clipboard_text_name)
#define XFE_CCP_NAME (fe_clipboard_app_name)

/* Selections
 */

static Boolean
fe_selection_converter (Widget widget, Atom *selection, Atom *target,
			Atom *type_ret, XtPointer *value_ret,
			unsigned long *length_ret, int *format_ret)
{
  Display *dpy = XtDisplay (widget);
  XTextProperty tmp_prop;
  int status;
  MWContext *context = fe_WidgetToMWContext (widget);

  Atom COMPOUND_TEXT = XmInternAtom(dpy, "COMPOUND_TEXT", False);
  Atom XA_TARGETS   = XmInternAtom (dpy, "TARGETS", False);
  Atom XA_TIMESTAMP = XmInternAtom (dpy, "TIMESTAMP", False);
  Atom XA_NETSCAPE  = XmInternAtom (dpy, XFE_CCP_DATA, False);
  Atom XA_HTML      = XmInternAtom (dpy, XFE_CCP_HTML, False);
  Atom XA_TEXT      = XmInternAtom (dpy, "TEXT", False);
  Atom XA_LENGTH    = XmInternAtom (dpy, "LENGTH", False);
  Atom XA_FILE_NAME = XmInternAtom (dpy, "FILE_NAME", False);
  Atom XA_OWNER_OS  = XmInternAtom (dpy, "OWNER_OS", False);
  Atom XA_HOST_NAME = XmInternAtom (dpy, "HOST_NAME", False);
  Atom XA_USER      = XmInternAtom (dpy, "USER", False);
  Atom XA_CLASS     = XmInternAtom (dpy, "CLASS", False);
  Atom XA_NAME      = XmInternAtom (dpy, "NAME", False);
  Atom XA_CLIENT_WINDOW = XmInternAtom (dpy, "CLIENT_WINDOW", False);
  Atom XA_PROCESS   = XmInternAtom (dpy, "PROCESS", False);
#if 0
  Atom XA_CHARACTER_POSITION = XmInternAtom (dpy, "CHARACTER_POSITION", False);
#endif

  char *text = 0;
  char *html = 0;
  char *data = 0;
  unsigned long dlen = 0;
  Time time = 0;
  char *loc;
  INTL_CharSetInfo c;

  XP_ASSERT (context);

  c = LO_GetDocumentCharacterSetInfo(context);

  if (*selection == XA_PRIMARY) {
      text = fe_selection_text;
      html = fe_selection_html;
	  data = fe_selection_data;
	  dlen = fe_selection_dlen;
      time = fe_selection_time;
  }
  else {
#ifdef DEBUG_rhess
	  fprintf(stderr, "fe_selection_converter::[ CLIPBOARD ] -- NOT\n");
#endif
	  return False;
  }

  if (!text && !html && !data) {
#ifdef DEBUG_rhess
	  fprintf(stderr, "fe_selection_converter::[ NULL ]\n");
#endif
      *value_ret = NULL;
      *length_ret = 0;
      *type_ret = None;
      *format_ret = 0;
      return False;
  }

  /* I can't believe the contortions we need to go through here!! */

  if (*target == XA_TARGETS)
    {
      Atom *av = (Atom *) malloc (sizeof (Atom) * 20);
      int ac = 0;
      av [ac++] = XA_TARGETS;
      av [ac++] = XA_TIMESTAMP;
      av [ac++] = XA_NETSCAPE;
      av [ac++] = XA_HTML;
      av [ac++] = XA_TEXT;
      av [ac++] = XA_STRING;
      av [ac++] = XA_LENGTH;
#if 0
      av [ac++] = XA_CHARACTER_POSITION;
#endif
      av [ac++] = XA_FILE_NAME;
      av [ac++] = XA_OWNER_OS;
      av [ac++] = XA_HOST_NAME;
      av [ac++] = XA_USER;
      av [ac++] = XA_CLASS;
      av [ac++] = XA_NAME;
      av [ac++] = XA_CLIENT_WINDOW;
      av [ac++] = XA_PROCESS;
      av [ac++] = COMPOUND_TEXT;

      /* Other types that might be interesting (from the R6 ICCCM):

	 XA_MULTIPLE		(this is a pain and nobody uses it)
	 XA_LINE_NUMBER		start and end lines of the selection
	 XA_COLUMN_NUMBER	start and end column of the selection
	 XA_BACKGROUND		a list of Pixel values
	 XA_FOREGROUND		a list of Pixel values
	 XA_PIXMAP		a list of pixmap IDs (of the images?)
	 XA_DRAWABLE		a list of drawable IDs (?)

	 XA_COMPOUND_TEXT	for text

	 Hairy Image Conversions:
	 XA_APPLE_PICT		for images
	 XA_POSTSCRIPT
	 XA_ENCAPSULATED_POSTSCRIPT aka _ADOBE_EPS
	 XA_ENCAPSULATED_POSTSCRIPT_INTERCHANGE aka _ADOBE_EPSI
	 XA_ODIF		ISO Office Document Interchange Format
       */
      *value_ret = av;
      *length_ret = ac;
      *type_ret = XA_ATOM;
      *format_ret = 32;
      return True;
   }
  else if (*target == XA_TIMESTAMP)
    {
      Time *timestamp;
      timestamp = (Time *) malloc (sizeof (Time));
      *timestamp = time;
      *value_ret = (char *) timestamp;
      *length_ret = 1;
      *type_ret = XA_TIMESTAMP;
      *format_ret = 32;
      return True;
    }
  else if (*target == XA_NETSCAPE)
	  {
		  if (dlen > 0) {
			  text = XP_ALLOC(dlen+1);
			  
			  memcpy(text, data, dlen);
			  
			  *value_ret = text;
			  *length_ret = dlen;
		  }
		  else {
			  *value_ret = NULL;
			  *length_ret = 0;
		  }
		  *type_ret = XA_NETSCAPE;
		  *format_ret = 8;
		  return True;
	  }
  else if (*target == XA_HTML)
	  {
		  *value_ret = strdup (html);
		  *length_ret = strlen (html);
		  *type_ret = XA_HTML;
		  *format_ret = 8;
		  return True;
	  }
  else if (((*target == XA_TEXT) && (INTL_GetCSIWinCSID(c) == CS_LATIN1)) ||
           (*target == XA_STRING))
    {
      *value_ret = strdup (text);
      *length_ret = strlen (text);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_TEXT)
    {
      loc = (char *) fe_ConvertToLocaleEncoding (INTL_GetCSIWinCSID(c),
                                                 (unsigned char *) text);
      status = XmbTextListToTextProperty (dpy, &loc, 1, XStdICCTextStyle,
                                          &tmp_prop);
      if (loc != text)
        {
          XP_FREE (loc);
        }
      if (status == Success)
        {
          *value_ret = (XtPointer) tmp_prop.value;
          *length_ret = tmp_prop.nitems;
          *type_ret = tmp_prop.encoding;    /* STRING or COMPOUND_TEXT */
          *format_ret = tmp_prop.format;
        }
      else
        {
          *value_ret = NULL;
          *length_ret = 0;
          return False;
        }
      return True;
    }
  else if (*target == COMPOUND_TEXT)
    {
      loc = (char *) fe_ConvertToLocaleEncoding (INTL_GetCSIWinCSID(c),
                                                 (unsigned char *) text);
      status = XmbTextListToTextProperty (dpy, &loc, 1, XCompoundTextStyle,
                                          &tmp_prop);
      if (loc != text)
        {
          XP_FREE (loc);
        }
      if (status == Success)
        {
          *value_ret = (XtPointer) tmp_prop.value;
          *length_ret = tmp_prop.nitems;
          *type_ret = COMPOUND_TEXT;
          *format_ret = 8;
        }
      else
        {
          *value_ret = NULL;
          *length_ret = 0;
          return False;
        }
      return True;
    }
  else if (*target == XA_LENGTH)
    {
      int *len = (int *) malloc (sizeof (int));
      *len = strlen (text);
      *value_ret = len;
      *length_ret = 1;
      *type_ret = XA_INTEGER;
      *format_ret = 32;
      return True;
    }
#if 0
  else if (*target == XA_CHARACTER_POSITION)
    {
      int32 *ends = (int32 *) malloc (sizeof (int32) * 2);
      LO_Element *s, *e;
      CL_Layer *layer;
      
      /* oops, this doesn't actually give me character positions -
	 just LO_Elements and indexes into them. */
      LO_GetSelectionEndpoints (context, &s, &e, &ends[0], &ends[1], &layer);
      *value_ret = ends;
      *length_ret = 2;
      *type_ret = XA_INTEGER;
      *format_ret = 32;
      return True;
    }
#endif
  else if (*target == XA_FILE_NAME)
    {
      History_entry *current = SHIST_GetCurrent (&context->hist);
      if (!current || !current->address)
	return False;
      *value_ret = strdup (current->address);
      *length_ret = strlen (current->address);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_OWNER_OS)
    {
      char *os;
      struct utsname uts;
      if (uname (&uts) < 0)
	os = "uname failed";
      else
	os = uts.sysname;
      *value_ret = strdup (os);
      *length_ret = strlen (os);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_HOST_NAME)
    {
      char name [255];
      if (gethostname (name, sizeof (name)))
	return False;
      *value_ret = strdup (name);
      *length_ret = strlen (name);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_USER)
    {
      struct passwd *pw = getpwuid (geteuid ());
      char *real_uid = (pw ? pw->pw_name : "");
      char *user_uid = getenv ("LOGNAME");
      if (! user_uid) user_uid = getenv ("USER");
      if (! user_uid) user_uid = real_uid;
      if (! user_uid)
	return False;
      *value_ret = strdup (user_uid);
      *length_ret = strlen (user_uid);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_CLASS)
    {
      *value_ret = strdup (fe_progclass);
      *length_ret = strlen (fe_progclass);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_NAME)
    {
      *value_ret = strdup (fe_progname);
      *length_ret = strlen (fe_progname);
      *type_ret = XA_STRING;
      *format_ret = 8;
      return True;
    }
  else if (*target == XA_CLIENT_WINDOW)
    {
      Window *window;
      window = (Window *) malloc (sizeof (Window));
      *window = XtWindow (CONTEXT_WIDGET (context));
      *value_ret = window;
      *length_ret = 1;
      *type_ret = XA_WINDOW;
      *format_ret = 32;
      return True;
    }
  else if (*target == XA_PROCESS)
    {
      pid_t *pid;
      pid = (pid_t *) malloc (sizeof (pid_t));
      *pid = getpid ();
      *value_ret = pid;
      *length_ret = 1;
      *type_ret = XA_INTEGER;
      *format_ret = 32;
      return True;
    }
  else if (*target == XA_COLORMAP)
    {
      Colormap *cmap;
      cmap = (Colormap *) malloc (sizeof (Colormap));
      *cmap = fe_cmap(context);
      *value_ret = cmap;
      *length_ret = 1;
      *type_ret = XA_COLORMAP;
      *format_ret = 32;
      return True;
    }

  return False;
}

static Boolean
fe_clipboard_converter (Widget widget, Atom *selection, Atom *target,
						Atom *type_ret, XtPointer *value_ret,
						unsigned long *length_ret, int *format_ret)
{
	Display *dpy = XtDisplay (widget);
	XTextProperty tmp_prop;
	int status;
	
	Atom XA_TARGETS       = XmInternAtom (dpy, "TARGETS", False);
	Atom XA_TIMESTAMP     = XmInternAtom (dpy, "TIMESTAMP", False);
	Atom XA_TEXT          = XmInternAtom (dpy, "TEXT", False);
	Atom XA_HTML          = XmInternAtom (dpy, XFE_CCP_HTML, False);
	Atom XA_NETSCAPE      = XmInternAtom (dpy, XFE_CCP_DATA, False);
	Atom XA_LENGTH        = XmInternAtom (dpy, "LENGTH", False);
	
	char *text = NULL;
	char *html = NULL;
	char *data = NULL;
	unsigned long dlen = 0;
	Time time = 0;
	
	if (*selection == XmInternAtom (dpy, "CLIPBOARD", False)) {
		text = fe_clipboard_text;
		html = fe_clipboard_html;
		data = fe_clipboard_data;
		dlen = fe_clipboard_dlen;
		time = fe_clipboard_time;
	}
	else {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clipboard_converter::[ PRIMARY ] -- NOT\n");
#endif
		return False;
	}
	
	if (!text && !html && !data) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clipboard_converter::[ NULL ]\n");
#endif
		return False;
	}

	if (*target == XA_TARGETS)
		{
			Atom *av = (Atom *) malloc (sizeof (Atom) * 20);
			int ac = 0;
			av [ac++] = XA_TARGETS;
			av [ac++] = XA_TIMESTAMP;
			av [ac++] = XA_TEXT;
			av [ac++] = XA_HTML;
			av [ac++] = XA_NETSCAPE;
			av [ac++] = XA_STRING;
			av [ac++] = XA_LENGTH;
			
			*value_ret = av;
			*length_ret = ac;
			*type_ret = XA_ATOM;
			*format_ret = 32;
			return True;
		}
	else if (*target == XA_TIMESTAMP) 
		{
			Time *timestamp;
			timestamp = (Time *) malloc (sizeof (Time));
			*timestamp = time;
			*value_ret = (char *) timestamp;
			*length_ret = 1;
			*type_ret = XA_TIMESTAMP;
			*format_ret = 32;
			return True;
		}
	else if ((*target == XA_TEXT) || 
			 (*target == XA_STRING)) 
		{
			if (text) {
				*value_ret = strdup (text);
				*length_ret = strlen (text);
				*type_ret = XA_STRING;
				*format_ret = 8;
				return True;
			}
			else {
				return False;
			}
		}
	else if (*target == XA_HTML)
		{
			if (html) {
				*value_ret = strdup (html);
				*length_ret = strlen (html);
				*type_ret = XA_HTML;
				*format_ret = 8;
				return True;
			}
			else {
				return False;
			}
		}
	else if (*target == XA_NETSCAPE)
		{
			if (dlen > 0) {
				text = XP_ALLOC(dlen+1);

				memcpy(text, data, dlen);

				*value_ret = text;
				*length_ret = dlen;
			}
			else {
				*value_ret = NULL;
				*length_ret = 0;
			}
			*type_ret = XA_NETSCAPE;
			*format_ret = 8;
			return True;
		}
	else if (*target == XA_LENGTH) 
		{
			int *len = (int *) malloc (sizeof (int));

			if (text) {
				*len = strlen(text);
				*value_ret = len;
				*length_ret = 1;
				*type_ret = XA_INTEGER;
				*format_ret = 32;
				return True;
			}
			else {
				return False;
			}
		}

	return False;
}

Boolean
fe_own_selection_p (MWContext *context, Boolean clip_p)
{
	Widget mainw = CONTEXT_WIDGET(context);
	Display *dpy = XtDisplay (mainw);

	if (clip_p) {

        /* Is there an internal clipboard entry? */
		if (fe_clipboard_text) {
            return True;
		} else {
			Atom atom = XmInternAtom (dpy, "CLIPBOARD", False);

			/* Is there an external clipboard owner? */
			if (XGetSelectionOwner (dpy, atom)) {
                return True;
			}
		}
	}
	else {
		/* Do we own the internal selection entry? */
		if (mainw == fe_selection_widget) {
            return True;
		} else {
			Window window = XGetSelectionOwner (dpy, XA_PRIMARY);
			Widget selection_widget = XtWindowToWidget(dpy, window);

			/* Is there a selection widget? */
			if (selection_widget) {
				XmTextPosition left, right;
				MWContext *ref;

                ref = fe_WidgetToMWContext(selection_widget);
                ref = XP_GetNonGridContext (ref);

				/* Does the selection widget belong to us?
                 * and is there a true selection?     
                 */
				if (ref == context
                    && (XmIsText(selection_widget)
                        || XmIsTextField(selection_widget))
                    && XmTextGetSelectionPosition(selection_widget,
                                                  &left, &right)
                    && ((right - left) > 0)) {
                    return True;
                }
            }
		}
	}
    return False;
}

/* Used by XFE_HTMLView::isCommandEnabled() */
Boolean
fe_can_cut (MWContext *context)
{
  return fe_own_selection_p(context, FALSE);
}

/* Used by XFE_HTMLView::isCommandEnabled() */
Boolean
fe_can_copy (MWContext *context)
{
  return fe_own_selection_p(context, FALSE);
}

/* Used by XFE_HTMLView::isCommandEnabled() */
Boolean
fe_can_paste (MWContext *context)
{
  return fe_own_selection_p(context, TRUE);
}

Window
xfe_GetClipboardWindow(Widget focus)
{
	Window target = NULL;

	if (fe_clipboard_widget)
		target = XtWindow(fe_clipboard_widget);
	else
		target = XtWindow(focus);

#ifdef DEBUG_rhess
	fprintf(stderr, "xfe_GetClipboardWindow::[ %p ][ %p ]\n", 
			XtWindow(focus), target);
#endif
	return target;
}

static void
fe_clipboard_loser (Widget widget, Atom *selection)
{
	Display *dpy = XtDisplay (widget);

	if (*selection == XmInternAtom (dpy, "CLIPBOARD", False)) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clipboard_loser::[ ]\n");
#endif
		fe_DisownClipboard (widget, 0);
	}
	else {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clipboard_loser::[ invalid atom ]\n");
#endif
	}
}

static void
fe_selection_loser (Widget widget, Atom *selection)
{
	Display *dpy = XtDisplay (widget);
	MWContext *context = fe_WidgetToMWContext (widget);

	if (! context) {
		/* Are you talking to me? */
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_selection_loser::[ NULL ]\n");
#endif
	}
	else if (*selection == XA_PRIMARY) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_selection_loser::[ PRIMARY ]\n");
#endif
		fe_DisownPrimarySelection(widget, 0);
	}
	else {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_selection_loser::[ ERROR ][ invalid target ]\n");
#endif
	}
}

static void
fe_set_clipboard(Widget focus, Time time, 
				 char *text, char* html, char* data, unsigned long data_len,
				 XP_Bool motif_p)
{
	if (fe_clipboard_text) {
		XP_FREE(fe_clipboard_text);
		fe_clipboard_text = NULL;
	}
	
	if (fe_clipboard_html) {
		XP_FREE(fe_clipboard_html);
		fe_clipboard_html = NULL;
	}
	
	if (fe_clipboard_data) {
		XP_FREE(fe_clipboard_data);
		fe_clipboard_data = NULL;
	}

	if (text) {
		fe_clipboard_text = XP_STRDUP(text);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_clipboard::[ %d ] TEXT\n", strlen(text));
#endif
	}

	if (html) {
		fe_clipboard_html = XP_STRDUP(html);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_clipboard::[ %d ] HTML\n", strlen(html));
#endif
	}

	if (data) {
		fe_clipboard_data = XP_ALLOC(data_len+1);
		memcpy(fe_clipboard_data, data, data_len);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_clipboard::[ %d ] DATA\n", data_len);
#endif
	}

	fe_clipboard_dlen   = data_len;
	fe_clipboard_time   = time;
	fe_clipboard_widget = focus;
	fe_clipboard_motif  = motif_p;
	fe_clipboard_empty  = False;
}

static void
fe_set_selection(Widget focus, Time time, 
				 char *text, char* html, char* data, unsigned long data_len,
				 MWContext* ctx
				 )
{
	if (fe_selection_text) {
		XP_FREE(fe_selection_text);
		fe_selection_text = NULL;
	}

	if (fe_selection_html) {
		XP_FREE(fe_selection_html);
		fe_selection_html = NULL;
	}

	if (fe_selection_data) {
		XP_FREE(fe_selection_data);
		fe_selection_data = NULL;
	}

	if (text) {
		fe_selection_text = XP_STRDUP(text);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_selection::[ %d ] TEXT\n", strlen(text));
#endif
	}

	if (html) {
		fe_selection_html = XP_STRDUP(html);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_selection::[ %d ] HTML\n", strlen(html));
#endif
	}

	if (data) {
		fe_selection_data = XP_ALLOC(data_len+1);
		memcpy(fe_selection_data, data, data_len);
#ifdef DEBUG_rhess
		fprintf(stderr, 
				"fe_set_selection::[ %d ] DATA\n", data_len);
#endif
	}

	if (fe_selection_ctx) {
		if (fe_selection_lost) {
#ifdef DEBUG_rhess
			fprintf(stderr, "WARNING... [ lost in fe_set_selection ][ %p ]\n",
					fe_selection_lost);
#endif			
		}
		fe_selection_lost = fe_selection_ctx;
	}

	fe_selection_ctx    = ctx;
	fe_selection_dlen   = data_len;
	fe_selection_time   = time;
	fe_selection_widget = focus;
	fe_selection_empty  = False;
}

static void
fe_clear_clipboard()
{
	if (fe_clipboard_empty) {
	}
	else {
		fe_clipboard_empty  = True;
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clear_clipboard::[ ]\n");
#endif
		if (fe_clipboard_text) {
			XP_FREE( fe_clipboard_text );
			fe_clipboard_text = NULL;
		}
	
		if (fe_clipboard_html) {
			XP_FREE( fe_clipboard_html );
			fe_clipboard_html = NULL;
		}
	
		if (fe_clipboard_data) {
			XP_FREE( fe_clipboard_data );
			fe_clipboard_data = NULL;
		}
	
		fe_clipboard_dlen   = 0;
		fe_clipboard_time   = 0;
		fe_clipboard_widget = NULL;
		fe_clipboard_motif  = False;
	}
}

static void
fe_clear_selection()
{
	if (fe_selection_empty) {
	}
	else {
		fe_selection_empty  = True;
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_clear_selection::[ ]\n");
#endif
		if (fe_selection_text) {
			XP_FREE( fe_selection_text );
			fe_selection_text = NULL;
		}
		
		if (fe_selection_html) {
			XP_FREE( fe_selection_html );
			fe_selection_html = NULL;
		}
		
		if (fe_selection_data) {
			XP_FREE( fe_selection_data );
			fe_selection_data = NULL;
		}
	
		if (fe_selection_ctx) {
			if (fe_selection_lost) {
#ifdef DEBUG_rhess
				fprintf(stderr, 
						"WARING... [ lost in fe_clear_selection ][ %p ]\n",
						fe_selection_lost);
#endif			
			}
			fe_selection_lost = fe_selection_ctx;
		}

		fe_selection_ctx    = NULL;
		fe_selection_dlen   = 0;
		fe_selection_time   = 0;
		fe_selection_widget = NULL;
	}
}

#ifdef EDITOR

static void
fe_edt_selection_loser (Widget widget, Atom *selection)
{
	Display *dpy = XtDisplay (widget);
	MWContext *context = fe_WidgetToMWContext (widget);

	if (! context) {
		/* Are you talking to me? */
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_edt_selection_loser::[ NULL ]\n");
#endif
	}
	else if (*selection == XA_PRIMARY) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_edt_selection_loser::[ PRIMARY ]\n");
#endif
		fe_EditorDisownSelection (context, 0, False);
	}
	else if (*selection == XmInternAtom (dpy, "CLIPBOARD", False)) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_edt_selection_loser::[ CLIPBOARD ]\n");
#endif
		fe_EditorDisownSelection (context, 0, True);
	}
}

void
fe_OwnEDTSelection (MWContext *context, Time time, Boolean clip_p, 
					char *text, char *html, char *data, unsigned long data_len)
{
	Widget mainw = CONTEXT_WIDGET(context);
	Display *dpy = XtDisplay (mainw);
	Atom atom = (clip_p ? XmInternAtom (dpy, "CLIPBOARD", False) : XA_PRIMARY);

	if (clip_p)
		{
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_own_edt_selection::[ CLIPBOARD ]\n");
#endif
			fe_clear_clipboard();

			fe_set_clipboard(mainw, time, 
							 text, html, data, data_len, False);

			if (text) {
				XtOwnSelection (mainw, atom, time,
								fe_clipboard_converter,  /* conversion proc */
								fe_edt_selection_loser,  /* lost proc */
								0);	                     /* ACK proc */
			}
		}
	else
		{
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_own_edt_selection::[ PRIMARY ]\n");
#endif
			fe_clear_selection();

			fe_set_selection(mainw, time, 
							 text, html, data, data_len, context);

			if (text) {
				XtOwnSelection (mainw, atom, time,
								fe_selection_converter,  /* conversion proc */
								fe_edt_selection_loser,  /* lost proc */
								0);	                     /* ACK proc */
			}
		}
}

#endif /* EDITOR */

void
fe_OwnClipboard(Widget focus, Time time,
				char* text, char* html, char* data, unsigned long data_len,
				XP_Bool motif_p)
{
  Display *dpy = XtDisplay (focus);
  Atom atom = XmInternAtom (dpy, "CLIPBOARD", False);

  if ((text && (strlen(text) > 0)) || 
	  (html && (strlen(html) > 0)) || 
	  (data && (data_len > 0))
	  ) {
	  fe_clear_clipboard();

	  fe_set_clipboard(focus, time, text, html, data, data_len, motif_p);

#ifdef DEBUG_rhess
	  if (data) {
		  if (html) {
			  if (text) {
				  fprintf(stderr, "fe_OwnClipboard::[ %p ][ %d, %d, %d ]\n", 
						  focus, strlen(text), strlen(html), data_len);
			  }
			  else {
				  fprintf(stderr, "fe_OwnClipboard::[ %p ][ NULL, %d, %d ]\n", 
						  focus, strlen(html), data_len);
			  }
		  }
		  else {
			  if (text) {
				  fprintf(stderr, "fe_OwnClipboard::[ %p ][ %d, NULL, %d ]\n", 
						  focus, strlen(text), data_len);
			  }
			  else {
				  fprintf(stderr, 
						  "fe_OwnClipboard::[ %p ][ NULL, NULL, %d ]\n", 
						  focus, data_len);
			  }
		  }
	  }
	  else {
		  if (html) {
			  if (text) {
				  fprintf(stderr, "fe_OwnClipboard::[ %p ][ %d, %d, NULL ]\n",
						  focus, strlen(text), strlen(html));
			  }
			  else {
				  fprintf(stderr, 
						  "fe_OwnClipboard::[ %p ][ NULL, %d, NULL ]\n", 
						  focus, strlen(html));
			  }
		  }
		  else {
			  if (text) {
				  fprintf(stderr, 
						  "fe_OwnClipboard::[ %p ][ %d, NULL, NULL ]\n",
						  focus, strlen(text));
			  }
			  else {
				  fprintf(stderr, 
						  "fe_OwnClipboard::[ %p ][ NULL, NULL, NULL ]\n", 
						  focus);
			  }
		  }
	  }
#endif
	  XtOwnSelection (focus, atom, time,
					  fe_clipboard_converter,	/* conversion proc */
					  fe_clipboard_loser,		/* lost proc */
					  0);						/* ACK proc */
  }

}

void
fe_DisownClipboard(Widget focus, Time time)
{
	Display *dpy = XtDisplay (focus);
	Atom atom = XmInternAtom (dpy, "CLIPBOARD", False);
	XtDisownSelection (focus, atom, time);

	if (focus == fe_clipboard_widget) {
#ifdef DEBUG_rhess
		fprintf(stderr, "fe_DisownClipboard::[ %p ]\n", focus);
#endif
		fe_clear_clipboard();
	}
}

void
fe_OwnPrimarySelection(Widget focus, Time time,
					   char* text, char* html, 
					   char* data, unsigned long data_len,
					   MWContext* context)
{
  Display *dpy = XtDisplay (focus);
  Atom atom = XA_PRIMARY;

  if ((text && (strlen(text) > 0)) || 
	  (html && (strlen(html) > 0)) || 
	  (data && (data_len > 0))
	  ) {
	  fe_clear_selection();

	  fe_set_selection(focus, time, text, html, data, data_len, context);

#ifdef DEBUG_rhess
	  if (text) {
		  if (html) {
			  fprintf(stderr, 
					  "fe_OwnPrimarySelection::[ %p ][ %d, %d, %d ]\n", 
					  focus, strlen(text), strlen(html), data_len);
		  }
		  else {
			  fprintf(stderr, 
					  "fe_OwnPrimarySelection::[ %p ][ %d, 0, %d ]\n", 
					  focus, strlen(text), data_len);
		  }
	  }
	  else {
		  if (html) {
			  fprintf(stderr, "fe_OwnPrimarySelection::[ %p ][ 0, %d, %d ]\n", 
					  focus, strlen(html), data_len);
		  }
		  else {
			  fprintf(stderr, "fe_OwnPrimarySelection::[ %p ][ 0, 0, %d ]\n", 
					  focus, data_len);
		  }
	  }
#endif
	  XtOwnSelection (focus, atom, time,
					  fe_selection_converter,	/* conversion proc */
					  fe_selection_loser,		/* lost proc */
					  0);						/* ACK proc */
  }

}

void
fe_DisownPrimarySelection(Widget focus, Time time)
{
	Display *dpy = XtDisplay (focus);
	Atom atom = XA_PRIMARY;

	XtDisownSelection (focus, atom, time);

	if (fe_selection_lost) {
		LO_ClearSelection(fe_selection_lost);
		fe_selection_lost = NULL;
	}
	else {
		if (focus == fe_selection_widget) {
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_DisownPrimarySelection::[ %p ]\n", focus);
#endif
			if (fe_selection_empty) {
#ifdef DEBUG_rhess
				fprintf(stderr, "fe_DisownPrimarySelection::[ punt ]\n");
#endif
			}
			else {
				if (fe_selection_ctx) {
#ifdef EDITOR
					if ((fe_selection_ctx->type == 
						 MWContextMessageComposition) ||
						(fe_selection_ctx->type == MWContextEditor)) {
						if (!fe_ContextHasPopups(fe_selection_ctx)) {
#ifdef DEBUG_rhess
							fprintf(stderr, 
									"fe_DisownPrimarySelection::[ %p ] EDT\n",
									fe_selection_ctx);
#endif
							EDT_ClearSelection(fe_selection_ctx);
						}
					}
					else {
#endif /* EDITOR */

#ifdef DEBUG_rhess
						fprintf(stderr, 
								"fe_DisownPrimarySelection::[ %p ] LO\n",
								fe_selection_ctx);
#endif
						LO_ClearSelection(fe_selection_ctx);
#ifdef EDITOR
					}
#endif
					fe_selection_ctx = NULL;
				}
				fe_clear_selection();
			}
		}
	}
}

void
fe_OwnSelection (MWContext *context, Time time, Boolean clip_p)
{
	Widget mainw = CONTEXT_WIDGET(context);
	char *text = (char *) LO_GetSelectionText (context);

	if (clip_p) {
		fe_OwnClipboard(mainw, time, 
						text, NULL, NULL, 0, False);
	}
	else {
		fe_OwnPrimarySelection(mainw, time, 
							   text, NULL, NULL, 0, context);
	}
}

void
fe_DisownSelection (MWContext *context, Time time, Boolean clip_p)
{
	Widget mainw = CONTEXT_WIDGET(context);

	if (clip_p) {
		if (fe_clipboard_widget) {
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_DisownSelection::[ CLIPBOARD ]\n");
#endif
			fe_DisownClipboard(mainw, time);
		}
	}
	else {
		if (fe_selection_widget) {
#ifdef DEBUG_rhess
			fprintf(stderr, "fe_DisownSelection::[ PRIMARY ]\n");
#endif
			fe_DisownPrimarySelection(mainw, time);
		}
	}
}


int
xfe_clipboard_retrieve_work(Widget focus,
							char* name,
							char** buf, unsigned long* buf_len_a,
							Time timestamp) 
{
    Display* display  = XtDisplay(focus);
    Window   window   = xfe_GetClipboardWindow(focus);
    int      cb_result;
    long     private_id;
	int      i;

	char    *clip_data = NULL;
	char     work_data[512];

	unsigned long length   = 0;
	unsigned long clip_len = 0;

	cb_result = XmClipboardLock(display, window);

	if (cb_result != ClipboardSuccess) {
#ifdef DEBUG_rhess
		fprintf(stderr, "xfe_clipboard_retrieve::[ FAILED LOCK ]\n");
#endif
		*buf_len_a = 0;
		*buf       = NULL;
		return cb_result;
	}
		
#ifdef DEBUG_rhess
	fprintf(stderr, "xfe_clipboard_retrieve::[ LOCK ]\n");
#endif

	cb_result = XmClipboardInquireLength(display, window,
										 name, &length);

	if (cb_result != ClipboardSuccess) {
#ifdef DEBUG_rhess
		fprintf(stderr, "xfe_clipboard_retrieve::[ FAILED INQUIRE ]\n");
#endif
		XmClipboardUnlock(display, window, TRUE);

		*buf_len_a = 0;
		*buf       = NULL;
		return cb_result;
	}
		
	clip_data = XP_ALLOC(length+1);
	clip_len  = 0;

#ifdef DEBUG_rhess
	fprintf(stderr, "xfe_clipboard_retrieve::[ %d ]\n", length);
#endif

	cb_result = XmClipboardRetrieve(display, window,
									name,
									(XtPointer) clip_data,
									length,
									&clip_len,
									&private_id);

#ifdef DEBUG_rhess
	fprintf(stderr, "xfe_clipboard_retrieve::[ %d ]\n", clip_len);
#endif
	clip_data[clip_len] = '\0';

	*buf_len_a = clip_len;
	*buf       = clip_data;

	XmClipboardUnlock(display, window, TRUE);

#ifdef DEBUG_rhess
	fprintf(stderr, "xfe_clipboard_retrieve::[ UNLOCK ]\n");
#endif

	return cb_result;
}

char *
xfe2_GetClipboardText (Widget focus, int32 *internal)
{
    unsigned long length;
    int       cb_result;

    Display*  display   = XtDisplay(focus);
    Window    window    = xfe_GetClipboardWindow(focus);
	Time      timestamp = XtLastTimestampProcessed (display);

    struct fe_MWContext_cons *rest;

	char     *clip_data = NULL;
	char     *clip_name = NULL;
	int       i;

	if (fe_clipboard_motif || !fe_clipboard_widget) {
		*internal = 0;
	}
	else {
		MWContext *context = fe_WidgetToMWContext(fe_clipboard_widget);

		*internal = (int) context;
	}

	/*
	 * NOTE:  look in our "internal" clipboard cache...
	 *        - [ shortcut for internal paste ]
	 *
	 */
	if (!clip_data) {
		if (fe_clipboard_text) {
#ifdef DEBUG_rhess
			fprintf(stderr, "xfe2_GetClipboardText::[ cache ]\n");
#endif
			clip_data = XP_STRDUP(fe_clipboard_text);
		}
	}

    if (!clip_data) { /* go look on real CLIPBOARD */

		clip_name = XFE_CCP_TEXT;
#ifdef DEBUG_rhess
		fprintf(stderr, "xfe2_GetClipboardText::[ retrieve ]\n");
#endif
		cb_result = xfe_clipboard_retrieve_work(focus,
												clip_name,
												&clip_data, &length,
												timestamp);

		if (cb_result != ClipboardSuccess) {
#ifdef DEBUG_rhess
			fprintf(stderr, "xfe2_GetClipboardText::[ failed ]\n");
#endif
			XP_FREE(clip_data);
			clip_data = NULL;
			clip_name = NULL;
		}
    } 

	return clip_data;
}

static void
fe_text_insert (Widget text, const char *s, int16 charset)
{
  /* Gee, I wonder what other gratuitous junk I need to worry about? */
  Boolean pdel = True;
  XtVaGetValues (text, XmNpendingDelete, &pdel, 0);

  if (XmIsText (text))
    {
      char *s2 = strdup (s); /* This is nuts: it wants to modify the string! */
      char *loc;
      if (pdel) XmTextRemove (text);
      loc = (char *) fe_ConvertToLocaleEncoding (charset,
                                                 (unsigned char *) s2);
      XmTextInsert (text, XmTextGetInsertionPosition (text), loc);
      if (loc != s2)
        {
          free (loc);
        }
      free (s2);
    }
  else if (XmIsTextField (text))
    {
      char *s2 = strdup (s); /* This is nuts: it wants to modify the string! */
      char *loc;
      if (pdel) XmTextFieldRemove (text);
      loc = (char *) fe_ConvertToLocaleEncoding (charset,
                                                 (unsigned char *) s2);
      XmTextFieldInsert (text, XmTextFieldGetInsertionPosition (text), loc);
      if (loc != s2)
        {
          free (loc);
        }
      free (s2);
    }
  else
    abort ();
}

XP_Bool
fe_InternalSelection(XP_Bool clip_p, 
					 unsigned long *text_len, 
					 unsigned long *html_len,
					 unsigned long *data_len) 
{
	XP_Bool gotcha = False;
	char *text = NULL;
	char *html = NULL;
	char *data = NULL;
	unsigned long dlen = 0;

	if (clip_p) {
		text = fe_clipboard_text;
		html = fe_clipboard_html;
		data = fe_clipboard_data;
		dlen = fe_clipboard_dlen;
	}
	else {
		text = fe_selection_text;
		html = fe_selection_html;
		data = fe_selection_data;
		dlen = fe_selection_dlen;
	}

	if (text) {
		*text_len = strlen(text);
		gotcha = True;
	}
	else {
		*text_len = 0;
	}

	if (html) {
		*html_len = strlen(html);
		gotcha = True;
	}
	else {
		*html_len = 0;
	}

	if (dlen > 0) {
		*data_len = dlen;
		gotcha = True;
	}
	else {
		*data_len = 0;
	}
		
	return gotcha;
}

char *
fe_GetPrimaryText(void)
{
#ifdef DEBUG_rhess
	if (fe_selection_text) 
		fprintf(stderr, 
				"fe_GetPrimaryText::[ %d ]\n", strlen(fe_selection_text));
	else
		fprintf(stderr,
				"fe_GetPrimaryText::[ NULL ]\n");
#endif
	return fe_selection_text;
}

char *
fe_GetPrimaryHtml(void)
{
#ifdef DEBUG_rhess
	if (fe_selection_html)
		fprintf(stderr, 
				"fe_GetPrimaryHtml::[ %d ]\n", strlen(fe_selection_html));
	else
		fprintf(stderr,
				"fe_GetPrimaryHtml::[ NULL ]\n");
#endif
	return fe_selection_html;
}

char *
fe_GetPrimaryData(unsigned long *length)
{
#ifdef DEBUG_rhess
	if (fe_selection_data && (fe_selection_dlen > 0)) 
		fprintf(stderr, 
				"fe_GetPrimaryData::[ %d ]\n", fe_selection_dlen);
	else
		fprintf(stderr,
				"fe_GetPrimaryData::[ NULL ]\n");
#endif
	*length = fe_selection_dlen;

	if (fe_selection_dlen > 0) {
		return fe_selection_data;
	}
	else {
		return NULL;
	}
}

char *
fe_GetInternalText(void)
{
#ifdef DEBUG_rhess
	if (fe_clipboard_text) 
		fprintf(stderr, 
				"fe_GetInternalText::[ %d ]\n", strlen(fe_clipboard_text));
	else
		fprintf(stderr,
				"fe_GetInternalText::[ NULL ]\n");
#endif
	return fe_clipboard_text;
}

char *
fe_GetInternalHtml(void)
{
#ifdef DEBUG_rhess
	if (fe_clipboard_html) 
		fprintf(stderr, 
				"fe_GetInternalHtml::[ %d ]\n", strlen(fe_clipboard_html));
	else
		fprintf(stderr,
				"fe_GetInternalHtml::[ NULL ]\n");
#endif
	return fe_clipboard_html;
}

char *
fe_GetInternalData(unsigned long *length)
{
#ifdef DEBUG_rhess
	if (fe_clipboard_data && (fe_clipboard_dlen > 0)) 
		fprintf(stderr, 
				"fe_GetInternalData::[ %d ]\n", fe_clipboard_dlen);
	else
		fprintf(stderr,
				"fe_GetInternalData::[ NULL ]\n");
#endif
	*length = fe_clipboard_dlen;

	if (fe_clipboard_dlen > 0) {
		return fe_clipboard_data;
	}
	else {
		return NULL;
	}
}

/*
 * NOTE:  we need this to support the unified cut/copy/paste mechanism...
 *
 *        - this allows translations to call cut/copy on a specific widget
 *
 */
void
xfe_TextCopy(MWContext *context, Widget focus, XP_Bool cut)
{
	XP_Bool text_wp = False;
	XP_Bool text_fp = False;
	char      *clip = NULL;
	Time       time = 0;

	if (!focus) 
		return;

	text_wp = XmIsText(focus);
	text_fp = XmIsTextField(focus);

	if (text_wp || text_fp) {

		clip = fe_GetTextSelection(focus);
		if (clip) {
			time = XtLastTimestampProcessed(XtDisplay(focus));

			fe_OwnClipboard(focus, time, 
							clip, NULL, NULL, 0, True);
			XP_FREE(clip);
			
			if (cut) {
				if (text_wp)
					XmTextRemove(focus);
				else
					XmTextFieldRemove(focus);
#ifdef DEBUG_rhess
				fprintf(stderr, "xfe_TextCopy::[ CUT ]\n");
#endif
			}
#ifdef DEBUG_rhess
			else {
				fprintf(stderr, "xfe_TextCopy::[ ]\n");
			}
#endif			
		}
#ifdef DEBUG_rhess
		else {
			fprintf(stderr, "xfe_TextCopy::[ failed to get selection ]\n");
		}
#endif
	}
}

/* Motif sucks!  XmTextPaste() gets stuck if you're pasting from one
   widget to another in the same application! */
void
xfe_TextPaste (MWContext *context, Widget text, XP_Bool quote_p)
{
  struct fe_MWContext_cons *rest;
  char *clip = 0;
  int16 charset = CS_LATIN1;
  INTL_CharSetInfo csi;

  int32 internal = 0;
  MWContext *con = NULL;

  clip = xfe2_GetClipboardText(text, &internal);

  if (internal) {
	  con = (MWContext *) internal;
	  csi = LO_GetDocumentCharacterSetInfo(con);
	  charset = INTL_GetCSIWinCSID(csi);
  }
  else {
	  charset = INTL_DefaultWinCharSetID(NULL);
  }

#ifdef MOZ_MAIL_NEWS
  if (clip && quote_p)
    {
      char *n = MSG_ConvertToQuotation (clip);
      XP_ASSERT(n);
      XP_FREE(clip);
      clip = n;
      if (!clip) return;
    }
#endif

  if (clip)
    {
#ifdef DEBUG_rhess
	  fprintf(stderr, "xfe_TextPaste::[ insert ][ %d ]--[ %d ] [ %s ]\n", 
			  charset, CS_LATIN1, clip);
#endif
      fe_text_insert (text, clip, charset);
      XP_FREE(clip);
    }
  else
    XBell (XtDisplay (text), 0);
}


typedef enum { fe_Cut, fe_Copy, fe_Paste, fe_PasteQuoted } fe_CCP;

static void
fe_ccp (MWContext *context, XEvent *event, fe_CCP ccp)
{
  Widget focus = XmGetFocusWidget (CONTEXT_WIDGET (context));
  char *clip = NULL;
  Time time = (event && (event->type == KeyPress ||
			 event->type == KeyRelease)
	       ? event->xkey.time :
	       event && (event->type == ButtonPress ||
			 event->type == ButtonRelease)
	       ? event->xbutton.time :
	       XtLastTimestampProcessed (XtDisplay(CONTEXT_WIDGET (context))));

  if ((CONTEXT_WIDGET(context) != fe_selection_widget
       || (ccp != fe_Cut && ccp != fe_Copy))
      && focus && (XmIsText (focus) || XmIsTextField (focus)))
    switch (ccp)
      {
      case fe_Cut:	   
		  xfe_TextCopy (context, focus, TRUE);
		  break;
      case fe_Copy:	   
		  xfe_TextCopy (context, focus, FALSE);
		  break;
      case fe_Paste:	   
		  xfe_TextPaste (context, focus, FALSE); 
		  break;
      case fe_PasteQuoted: 
		  xfe_TextPaste (context, focus, TRUE); 
		  break;
      default:		   
		  XP_ASSERT(0); 
		  break;
      }
  else
    {
		if (focus && ccp == fe_Copy) {
			fe_OwnSelection(context, time, True);
		}
		else {
			XBell (XtDisplay (CONTEXT_WIDGET (context)), 0);
		}
    }
}

void
fe_cut_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
  fe_UserActivity (context);

  fe_ccp (context, (cb ? cb->event : 0), fe_Cut);
}

void
fe_copy_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
  fe_UserActivity (context);

  fe_ccp (context, (cb ? cb->event : 0), fe_Copy);
}

void
fe_paste_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
  fe_UserActivity (context);

  fe_ccp (context, (cb ? cb->event : 0), fe_Paste);
}

void
fe_paste_quoted_cb (Widget widget, XtPointer closure, XtPointer call_data)
{
  MWContext *context = (MWContext *) closure;
  XmAnyCallbackStruct *cb = (XmAnyCallbackStruct *) call_data;
  fe_UserActivity (context);

  fe_ccp (context, (cb ? cb->event : 0), fe_PasteQuoted);
}
