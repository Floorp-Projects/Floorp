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
  g-navcenter-view.c -- navcenter views.
  Created: Chris Toshok <toshok@hungry.com>, 1-Jul-98.
*/

#include "xp_mem.h"
#include "g-navcenter-view.h"

static void
ctree_expand(GtkWidget *ctree,
             GList *node,
             MozNavCenterView *view)
{
  printf ("Expanding node %p\n", node);
}

static void
ctree_collapse(GtkWidget *ctree,
               GList *node,
               MozNavCenterView *view)
{
  printf ("Collapsing node %p\n", node);
}

static void
ctree_select(GtkWidget *ctree,
             gint node_index,
             MozNavCenterView *view)
{
  printf ("selecting node %d\n", node_index);
}

static void
ctree_deselect(GtkWidget *ctree,
               gint node_index,
               MozNavCenterView *view)
{
  printf ("deselecting node %d\n", node_index);
}

static void
get_column_titles(MozNavCenterView *view,
                  int *num,
                  char ***titles)
{
  HT_Cursor column_cursor;
  char *column_name;
  uint32 column_width;
  void *token;
  uint32 token_type;
  int cur_title;

  /* first we count them. */
  column_cursor = HT_NewColumnCursor(view->ht_view);
  *num = 0;
  while (HT_GetNextColumn(column_cursor, &column_name, &column_width,
                          &token, &token_type))
    {
      *num++;
    }
  HT_DeleteColumnCursor(column_cursor);

  /* then we fill in the array */
  column_cursor = HT_NewColumnCursor(view->ht_view);
  *titles = (char**)XP_CALLOC(*num, sizeof(char*));
  cur_title = 0;
  while (HT_GetNextColumn(column_cursor, &column_name, &column_width,
                          &token, &token_type))
    {
      *titles[cur_title++] = XP_STRDUP(column_name);
    }
  HT_DeleteColumnCursor(column_cursor);
}

static void
release_column_titles(MozNavCenterView *view,
                      int num,
                      char **titles)
{
  printf("release_column_titles (empty)\n");
}

static void
fill_tree(MozNavCenterView *view)
{
  char **column_titles;
  int num_column_titles;
  int i;

  get_column_titles(view, &num_column_titles, &column_titles);
  printf("There are %d titles.\n", num_column_titles);
  for (i = 0; i < num_column_titles; i++)
    printf ("Column[%d]: %s\n", i, column_titles[i]);
  release_column_titles(view, num_column_titles, column_titles);
}

static void
navcenter_ht_notify(HT_Notification ns, HT_Resource n,
                    HT_Event whatHappened)
{
  MozNavCenterView *view = (MozNavCenterView*)ns->data;

  switch (whatHappened)
    {
    case HT_EVENT_VIEW_CLOSED:
      break;
    case HT_EVENT_VIEW_SELECTED:
      {
        HT_View ht_view = HT_GetView(n);
        
        if (view->ht_view != ht_view)
          view->ht_view = ht_view;

        fill_tree(view);
        
        break;
      }
    case HT_EVENT_VIEW_ADDED:
      break;
    case HT_EVENT_NODE_ADDED:
    case HT_EVENT_NODE_DELETED_DATA:
    case HT_EVENT_NODE_DELETED_NODATA:
    case HT_EVENT_NODE_VPROP_CHANGED:
    case HT_EVENT_NODE_SELECTION_CHANGED:
    case HT_EVENT_NODE_OPENCLOSE_CHANGED: 
    case HT_EVENT_NODE_OPENCLOSE_CHANGING:
      break;
    default:
      printf("HT_Event(%d): Unknown type on %s\n",whatHappened,HT_GetNodeName(n));
      break;
    }
}

static void
createHTPane(MozNavCenterView *view)
{
  HT_Notification ns = XP_NEW_ZAP(HT_NotificationStruct);
  ns->notifyProc = navcenter_ht_notify;
  ns->data = view;

  view->ht_pane = HT_NewPane(ns);
  HT_SetPaneFEData(view->ht_pane, view);
}

void
moz_navcenter_view_init(MozNavCenterView *view,
                        MozFrame *parent_frame,
                        MWContext *context)
{
  /* call our superclass's init */
  moz_view_init(MOZ_VIEW(view), parent_frame, context);

  /* then do our stuff */
  moz_tagged_set_type(MOZ_TAGGED(view),
		      MOZ_TAG_NAVCENTER_VIEW);

  /* create the HTPane stuff */
  createHTPane(view);

#if 0
  view->ctree = gtk_ctree_new_with_titles(num_column_titles, 0, column_titles);
#else
  view->ctree = gtk_ctree_new_with_titles(0, 0, 0);
#endif

  gtk_signal_connect(GTK_OBJECT(view->ctree),
                     "tree_collapse",
                     (GtkSignalFunc)ctree_collapse, view);
  gtk_signal_connect(GTK_OBJECT(view->ctree),
                     "tree_expand",
                     (GtkSignalFunc)ctree_expand, view);
  gtk_signal_connect(GTK_OBJECT(view->ctree),
                     "tree_select",
                     (GtkSignalFunc)ctree_select, view);
  gtk_signal_connect(GTK_OBJECT(view->ctree),
                     "tree_deselect",
                     (GtkSignalFunc)ctree_select, view);

  moz_component_set_basewidget(MOZ_COMPONENT(view), view->ctree);
}

void
moz_navcenter_view_deinit(MozNavCenterView *view)
{
  /* do our stuff. */

  /* then call our superclass's deinit */
  moz_view_deinit(MOZ_VIEW(view));
}

MozNavCenterView*
moz_navcenter_view_create(MozFrame *parent_frame,
                          MWContext *context)
{
  MozNavCenterView* view;

  view = XP_NEW_ZAP(MozNavCenterView);
  XP_ASSERT(view);
  if (view == NULL) return NULL;

  moz_navcenter_view_init(view, parent_frame, context);
  return view;
}
