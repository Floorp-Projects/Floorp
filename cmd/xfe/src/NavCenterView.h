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

class XFE_RDFChromeTreeView;

class XFE_NavCenterView : public XFE_View,
                          public XFE_RDFBase
{
public:
  XFE_NavCenterView(XFE_Component *toplevel_component, Widget parent, XFE_View *parent_view, MWContext *context);

  virtual ~XFE_NavCenterView();

  virtual void    notify            (HT_Resource n, HT_Event whatHappened);

  void            selectHTView             (HT_View view);
#ifdef MOZ_SELECTOR_BAR
  void            addHTView                (HT_View view);
  Widget          getSelector              ();
  static void     selector_activate_cb     (Widget,XtPointer,XtPointer);
#endif /*MOZ_SELECTORY_BAR*/


  virtual void    handleDisplayPixmap      (Widget, IL_Pixmap *, IL_Pixmap *,
                                            PRInt32 width, PRInt32 height);
  virtual void    handleNewPixmap          (Widget, IL_Pixmap *, Boolean mask);
  virtual void    handleImageComplete      (Widget, IL_Pixmap *);  
#ifdef MOZ_SELECTOR_BAR
  static void     image_complete_cb        (XtPointer);
  void setRdfTree(HT_View);
#endif

protected:

    // Override RDFBase methods
    virtual void          finishPaneCreate      ();
    virtual void          deletePane            ();

private:
  XFE_RDFChromeTreeView *    _rdftree;

#ifdef MOZ_SELECTOR_BAR
  Widget                     _selector;
#else
  // Terrible hack to pick the first added view (this will go away)
  int _firstViewAdded;
#endif /*!MOZ_SELECTORY_BAR*/

  // as oppposed to embedded in a browser
  XP_Bool                    _isStandalone;

#ifdef MOZ_SELECTOR_BAR
  void            createSelectorBar        ();
#endif
  void            createTree               ();
  void            doAttachments            ();
};


#endif /* _xfe_navcenterview_h */

