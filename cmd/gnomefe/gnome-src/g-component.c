/*
  g-component.c -- components.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#include "xpassert.h"
#include "g-component.h"

void 
moz_component_init(MozComponent *component)
{
  /* call our superclass's init */
  moz_tagged_init(MOZ_TAGGED(component));

  moz_tagged_set_type(MOZ_TAGGED(component),
		      MOZ_TAG_COMPONENT);

  component->is_shown = FALSE;
}

void 
moz_component_deinit(MozComponent *component)
{
  /* we destroy the widget here, if we have one. */
  if (component->base_widget)
    gtk_widget_destroy(component->base_widget);

  /* call our superclass's deinit */
  moz_tagged_deinit(MOZ_TAGGED(component));
}

void 
moz_component_set_basewidget(MozComponent *component, 
			     GtkWidget *widget)
{
  component->base_widget = widget;
}

GtkWidget* 
moz_component_get_basewidget(MozComponent *component)
{
  return component->base_widget;
}

gboolean
moz_component_is_shown(MozComponent *component)
{
  return component->is_shown;
}

void 
moz_component_show(MozComponent *component)
{
  XP_ASSERT(component->base_widget);

  gtk_widget_show(component->base_widget);
  component->is_shown = TRUE;
}

void 
moz_component_hide(MozComponent *component)
{
  XP_ASSERT(component->base_widget);

  gtk_widget_hide(component->base_widget);
  component->is_shown = FALSE;
}
