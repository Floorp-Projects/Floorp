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
/* */
/* remote-s.c --- remote control of netscape.
   Created: Jamie Zawinski <jwz@netscape.com>
 */


#include "mozilla.h"
#include "xfe.h"

#define progname fe_progname
#define mozilla_remote_init_atoms fe_init_atoms
#define mozilla_remote_commands fe_RemoteCommands
static const char *expected_mozilla_version = fe_version;
#include "remote.c"

/* for XP_GetString() */
#include <xpgetstr.h>
extern int XFE_REMOTE_S_UNABLE_TO_READ_PROPERTY;
extern int XFE_REMOTE_S_INVALID_DATA_ON_PROPERTY;
extern int XFE_REMOTE_S_509_INTERNAL_ERROR;
extern int XFE_REMOTE_S_500_UNPARSABLE_COMMAND;
extern int XFE_REMOTE_S_501_UNRECOGNIZED_COMMAND;
extern int XFE_REMOTE_S_502_NO_APPROPRIATE_WINDOW;
extern int XFE_REMOTE_S_200_EXECUTED_COMMAND;


/* server side */

static void fe_property_change_action (Widget widget, XEvent *event,
				       String *av, Cardinal *ac);

static char *fe_server_handle_command (Display *dpy, Window window,
				       XEvent *event, char *command);

static XtTranslations fe_prop_translations;

void
fe_InitRemoteServer (Display *dpy)
{
  static Boolean done = False;
  static XtActionsRec actions [] =
    { { "HandleMozillaCommand", fe_property_change_action } };

  if (done) return;
  done = True;
  fe_init_atoms (dpy);

  XtAppAddActions (fe_XtAppContext, actions, countof (actions));

  fe_prop_translations =
    XtParseTranslationTable ("<PropertyNotify>: HandleMozillaCommand()");
}

void
fe_InitRemoteServerWindow (MWContext *context)
{
  Widget widget = CONTEXT_WIDGET (context);
  Display *dpy = XtDisplay (widget);
  Window window = XtWindow (widget);
  XWindowAttributes attrs;
  unsigned char *data = (unsigned char *) fe_version;

  XtOverrideTranslations (widget, fe_prop_translations);

  XChangeProperty (dpy, window, XA_MOZILLA_VERSION, XA_STRING,
		   8, PropModeReplace, data, strlen (data));

  XGetWindowAttributes (dpy, window, &attrs);
  if (! (attrs.your_event_mask & PropertyChangeMask))
    XSelectInput (dpy, window, attrs.your_event_mask | PropertyChangeMask);
}


void
fe_server_handle_property_change (Display *dpy, Window window, XEvent *event)
{
  if (event->xany.type == PropertyNotify &&
      event->xproperty.state == PropertyNewValue &&
      event->xproperty.window == window &&
      event->xproperty.atom == XA_MOZILLA_COMMAND)
    {
      int result;
      Atom actual_type;
      int actual_format;
      unsigned long nitems, bytes_after;
      unsigned char *data = 0;

      result = XGetWindowProperty (dpy, window, XA_MOZILLA_COMMAND,
				   0, (65536 / sizeof (long)),
				   True, /* atomic delete after */
				   XA_STRING,
				   &actual_type, &actual_format,
				   &nitems, &bytes_after,
				   &data);
      if (result != Success)
	{
	  fprintf (stderr,
		   XP_GetString(XFE_REMOTE_S_UNABLE_TO_READ_PROPERTY),
		   fe_progname,
		   MOZILLA_COMMAND_PROP);
	  return;
	}
      else if (!data || !*data)
	{
	  fprintf (stderr, 
		   XP_GetString(XFE_REMOTE_S_INVALID_DATA_ON_PROPERTY),
		   fe_progname,
		   MOZILLA_COMMAND_PROP,
		   (unsigned int) window);
	  return;
	}
      else
	{
	  char *response =
	    fe_server_handle_command (dpy, window, event, (char *) data);
	  if (! response) abort ();
	  XChangeProperty (dpy, window, XA_MOZILLA_RESPONSE, XA_STRING,
			   8, PropModeReplace, response, strlen (response));
	  free (response);
	}

      if (data)
	XFree (data);
    }
#ifdef DEBUG_PROPS
  else if (event->xany.type == PropertyNotify &&
	   event->xproperty.window == window &&
	   event->xproperty.state == PropertyDelete &&
	   event->xproperty.atom == XA_MOZILLA_RESPONSE)
    {
      fprintf (stderr, "%s: (client has accepted the response on 0x%x.)\n",
	       fe_progname, (unsigned int) window);
    }
  else if (event->xany.type == PropertyNotify &&
	   event->xproperty.window == window &&
	   event->xproperty.atom == XA_MOZILLA_LOCK)
    {
      fprintf (stderr, "%s: (client has %s a lock on 0x%x.)\n",
	       fe_progname,
	       (event->xproperty.state == PropertyNewValue
		? "obtained" : "freed"),
	       (unsigned int) window);
    }
#endif /* DEBUG_PROPS */

}

static void
fe_property_change_action (Widget widget, XEvent *event,
			   String *av, Cardinal *ac)
{
  MWContext *context = fe_WidgetToMWContext (widget);
  if (! context) return;
  fe_server_handle_property_change (XtDisplay (CONTEXT_WIDGET (context)),
				    XtWindow (CONTEXT_WIDGET (context)),
				    event);
}

static char *
fe_server_handle_command (Display *dpy, Window window, XEvent *event,
			  char *command)
{
	char *name = 0;
	char **av = 0;
	int ac = 0;
	int avsize = 0;
	Boolean raise_p = True;
	int i;
	char *buf;
	char *buf2;
	int32 buf2_size;
	char *head, *tail;
	XtActionProc action = 0;
	Widget widget = XtWindowToWidget (dpy, window);
	MWContext *context = fe_WidgetToMWContext (widget);
	XP_Bool mail_or_news_required = FALSE;
	MWContextType required_type = (MWContextType) (~0);
	XP_Bool make_context_if_necessary = FALSE;
    MWContext *target_context = context;

	XP_ASSERT(context);

	buf = fe_StringTrim (strdup (command));
	buf2_size = strlen (buf) + 200;
	buf2 = (char *) malloc (buf2_size);

	head = buf;
	tail = buf;

	if (! widget)
		{
			PR_snprintf (buf2, buf2_size,
						 XP_GetString(XFE_REMOTE_S_509_INTERNAL_ERROR),
						 (unsigned int) window);
			free (buf);
			return buf2;
		}

	/* extract the name (everything before the first '(', trimmed.) */
	while (1)
		if (*tail == '(' || isspace (*tail) || !*tail)
			{
				*tail = 0;
				tail++;
				name = fe_StringTrim (head);
				break;
			}
		else
			tail++;

	if (!name || !*name)
		{
			PR_snprintf (buf2, buf2_size,
						 XP_GetString(XFE_REMOTE_S_500_UNPARSABLE_COMMAND), 
						 command);
			free (buf);
			return buf2;
		}

	/* look for it in the old remote actions. */
	for (i = 0; i < fe_CommandActionsSize; i++)
		if (!XP_STRCASECMP(name, fe_CommandActions [i].string))
			{
				name = fe_CommandActions [i].string;
				action = fe_CommandActions [i].proc;
				break;
			}

	if (!av)
		{
			avsize = 20;
			av = (char **) calloc (avsize, sizeof (char *));

			/* if it's not an old action, we need to know the name of the
			   command, so we stick it on the front.  This will be dealt
			   with in xfeDoRemoteCommand. */
			if (!action)
				av[ ac++ ] = name;

			while (*tail == '(' || isspace (*tail))
				tail++;

			head = tail;
			while (1)
				{
					if (*tail == ')' || *tail == ',' || *tail == 0)
						{
							char delim = *tail;
							if (ac >= (avsize - 2))
								{
									avsize += 20;
									av = (char **) realloc (av, avsize * sizeof (char *));
								}
							*tail = 0;

							av [ac++] = fe_StringTrim (head);

							if (delim != ',' && !*av[ac-1])
								ac--;
							else if (!strcasecomp (av [ac-1], "noraise"))
								{
									raise_p = False;
									ac--;
								}
							else if (!strcasecomp (av [ac-1], "raise"))
								{
									raise_p = True;
									ac--;
								}

							head = tail+1;
							if (delim != ',')
								break;
						}
					tail++;
				}

			av [ac++] = "<remote>";
		}

	/* If this is GetURL or something like it, make sure the context we pick
	   matches the URL.
	   */
	if (strstr(name, "URL"))
		{
			const char *url = av[0];
			mail_or_news_required = FALSE;
			required_type = (MWContextType) (~0);
#ifdef MOZ_MAIL_NEWS
			if (MSG_RequiresMailWindow (url))
				required_type = MWContextMail;
			else if (MSG_RequiresNewsWindow (url))
				required_type = MWContextNews;
			else if (MSG_RequiresBrowserWindow (url))
#endif
				required_type = MWContextBrowser;
			/* Nothing to do for MSG_RequiresComposeWindow compose. */

			if (required_type != (MWContextType) (~0))
				{
					make_context_if_necessary = TRUE;
				}
		}
    else if (!strcasecomp(name, "openfile"))
      {
        required_type = MWContextBrowser;
        make_context_if_necessary = TRUE;
      }

	if (raise_p)
		XMapRaised (dpy, window);

    if (required_type != (MWContextType) (~0))
      target_context = XP_FindContextOfType(context, required_type);
			
    if (make_context_if_necessary && !target_context)
      target_context = FE_MakeNewWindow(context, NULL, NULL, NULL);

    if (target_context) 
      {
        Cardinal ac2 = ac; /* why is this passed as a pointer??? */
        if (name && action)
          {
            (*action) (CONTEXT_WIDGET(target_context), event, av, &ac2);
          }
        else
          /* now we call our new xfe2 interface to the command mechanism */
          {
            xfeDoRemoteCommand (CONTEXT_WIDGET(target_context),
                                event, av, &ac2);
          }
      }

	PR_snprintf (buf2, buf2_size,
				 XP_GetString(XFE_REMOTE_S_200_EXECUTED_COMMAND),
				 name);
	for (i = 0; i < ac-1; i++)
		{
			strcat (buf2, av [i]);
			if (i < ac-2)
				strcat (buf2, ", ");
		}
	strcat (buf2, ")");

	free (av);

	free (buf);
	return buf2;
}
