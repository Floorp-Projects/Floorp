/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
/*
  g-component.h -- components.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_component_h
#define _moz_component_h

#include "gnome.h"
#include "g-tagged.h"

struct _MozComponent {
  MozTagged _tagged;

  GtkWidget *base_widget;

  gboolean is_shown;
};

extern void		moz_component_init(MozComponent *component);
extern void		moz_component_deinit(MozComponent *component);

extern void			moz_component_set_basewidget(MozComponent *component, GtkWidget *widget);
extern GtkWidget*	moz_component_get_basewidget(MozComponent *component);
extern gboolean		moz_component_is_shown(MozComponent *component);
extern void	  		moz_component_show(MozComponent *component);
extern void			moz_component_hide(MozComponent *component);

#endif /* _moz_component_h */
