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

   dragdrop.h --- Very simplistic drag and drop support.
   Created: Terry Weissman <terry@netscape.com>, 22-Aug-95.
 */

#ifndef __xfe_dragdrop_h_
#define __xfe_dragdrop_h_


typedef enum {
  FE_DND_NONE,			/* Deliberate 'no dragging' value. */
  FE_DND_MAIL_MESSAGE,
  FE_DND_MAIL_FOLDER,
  FE_DND_NEWS_MESSAGE,
  FE_DND_NEWS_FOLDER,
  FE_DND_BOOKMARK,
  FE_DND_URL,
  FE_DND_COLUMN,
  FE_DND_ADDRESSBOOK,
  FE_DND_HISTORY,
  FE_DND_LDAP_ENTRY,
  FE_DND_BOOKS_DIRECTORIES
} fe_dnd_Type;



/* Tells a drop site what kind of event is happening to it. _START means that a
   new drag option has appeared.  _DRAG means that it is being moved around.
   _DROP means that it has potentially dropped on this site.  _END means that
   the drag is all done. */
typedef enum {
  FE_DND_START,
  FE_DND_DRAG,
  FE_DND_DROP,
  FE_DND_END
} fe_dnd_Event;

typedef struct fe_dnd_Source fe_dnd_Source;

/* messages sent from the drop site back to the source. */
typedef enum {
  FE_DND_MESSAGE_COPY,
  FE_DND_MESSAGE_MOVE,
  FE_DND_MESSAGE_DELETE
} fe_dnd_Message;

typedef void (*fe_dnd_SourceDropFunc)(fe_dnd_Source *source, fe_dnd_Message, void *closure);

struct fe_dnd_Source {
  Widget widget;		/* Toplevel widget to drag around. */
  int hotx;			/* Where in the widget the cursor should */
  int hoty;			/* "stick". */
  fe_dnd_Type type;		/* Type of thing being dragged. */
  void* closure;		/* Type-specific data about the thing being
				   dragged. */
  fe_dnd_SourceDropFunc func;	/* a way for the drop site to tell the source to do some operation. */
};


typedef void (*fe_dnd_DropFunc)(Widget dropw, void* closure, fe_dnd_Event,
				fe_dnd_Source*, XEvent* event);

XP_BEGIN_PROTOS

/* Register the given widget as a drop site. */
extern void fe_dnd_CreateDrop(Widget w, fe_dnd_DropFunc func, void* closure);

/* Inform the D&D internals that a drag is starting or dragging or dropping or
   finishing.  This will call the callback functions on all the drop sites,
   which have to decide for themselves if the given drag event applies.  The
   only trimming done is that _DROP events are not given to a widget unless
   the mouse is over that widget.

   Note that it is entirely up to the caller to create the source struct before
   calling this, and to destroy the source struct (and its widget) when
   done. */
extern void fe_dnd_DoDrag(fe_dnd_Source* source, XEvent* event,
			  fe_dnd_Event type);


XP_END_PROTOS

#endif /* __xfe_dragdrop_h_ */
