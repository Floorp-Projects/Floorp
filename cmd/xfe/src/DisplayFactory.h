/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   DisplayFactory.h -- class definition for the factory class
   Created: Spence Murray <spence@netscape.com>, 17-Sep-96.
 */



#ifndef _xfe_displayfactory_h
#define _xfe_displayfactory_h

#include "Frame.h"
#include "xfe.h"

class XFE_DisplayFactory
{
public:
  XFE_DisplayFactory(Widget toplevel);
  virtual ~XFE_DisplayFactory();

  /* return the colormap used by the "other" windows: anything
     not displaying html. In actuality, this colormap _is_
     this function returns the same value every time. */
  fe_colormap *getSharedColormap();

  /* return a newly allocated colormap.  This function is used
     by windows that need their own colormaps (those displaying
     html. */
  fe_colormap *getPrivateColormap();

  /* returns the visual we want to use for this display. */
  Visual *getVisual();

  /* returns the bpp of the default visual. */
  int getVisualDepth();

  int getVisualPixmapDepth();

  /* use this method to get the one instance of this class. */
  static XFE_DisplayFactory *theFactory();

  /* fe_DisposeColormap calls this method when it releases a colormap,
     so we can set m_sharedCmap to NULL, so it'll be recreated when needed. */
  void colormapGoingAway(fe_colormap *colormap);

private:
  fe_colormap *m_sharedCmap;
  Visual *m_visual;
  int m_visualDepth;

  Widget m_toplevel;
  Display *m_display;
  Screen *m_screen;

  XP_Bool m_alwaysInstallCmap;
};

extern "C" void fe_startDisplayFactory(Widget toplevel);
extern "C" void fe_DisplayFactoryColormapGoingAway(fe_colormap *colormap);
extern "C" fe_colormap * fe_GetSharedColormap(void);

#endif /* _xfe_displayfactory_h */
