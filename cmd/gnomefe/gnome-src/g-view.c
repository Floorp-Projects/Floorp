/*
  g-view.c -- views.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "g-view.h"

void
moz_view_init(MozView *view, MozFrame *parent_frame, MWContext *context)
{
  /* call our superclass's init first */
  moz_component_init(MOZ_COMPONENT(view));

  /* do our stuff. */
  view->parent_frame = parent_frame;
  view->subviews = g_list_alloc();
  view->context = context;
}

void
moz_view_deinit(MozView *view)
{
  /* do our stuff. */
  g_list_free(view->subviews);

  /* call our superclass's deinit last */
  moz_component_deinit(MOZ_COMPONENT(view));
}

void
moz_view_add_view(MozView *parent_view,
		  MozView *child)
{
  child->parent_view = parent_view;

  gtk_container_add(GTK_CONTAINER(parent_view->subview_parent),
		    MOZ_COMPONENT(child)->base_widget);

  parent_view->subviews = g_list_prepend(parent_view->subviews, child);
}

void
moz_view_set_context(MozView *view,
		     MWContext *context)
{
  view->context = context;
}

MWContext*
moz_view_get_context(MozView *view)
{
  return view->context;
}
