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
   NavCenterView.h - Aurora/NavCenter view class
   Created: Stephen Lamm <slamm@netscape.com>, 23-Oct-97.
 */



#ifndef _xfe_navcenterview_h
#define _xfe_navcenterview_h

#include "View.h"
#include "htrdf.h"
#include "RDFBase.h"
#include "RDFImage.h"

class XFE_HTMLView;
class XFE_RDFView;



class XFE_NavCenterView : public XFE_View,
                          public XFE_RDFBase
{
public:
  XFE_NavCenterView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_NavCenterView();

  virtual void notify(HT_Resource n, HT_Event whatHappened);

#ifdef USE_SELECTOR_BAR
  void setRDFView(HT_View view);
  void addRDFView(HT_View view);
  Widget  getSelector(void);
  static void selector_activate_cb(Widget,XtPointer,XtPointer);
  static void selector_destroy_cb(Widget,XtPointer,XtPointer);
#endif /*USE_SELECTORY_BAR*/


  virtual void handleDisplayPixmap(Widget, IL_Pixmap *, IL_Pixmap *, PRInt32 width, PRInt32 height);
  virtual void handleNewPixmap(Widget, IL_Pixmap *, Boolean mask);
  virtual void handleImageComplete(Widget, IL_Pixmap *);  

private:
  XFE_HTMLView * m_htmlview;
  XFE_RDFView  * m_rdfview;
#ifdef USE_SELECTOR_BAR
  Widget         m_selector;
#endif /*USE_SELECTORY_BAR*/
  Widget         rdf_parent;
  XP_Bool        m_isStandalone; // as oppposed to embedded in a browser
};


#endif /* _xfe_navcenterview_h */

