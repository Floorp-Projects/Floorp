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
   RDFToolbox.cpp - Toolbars supplied by RDF
   Created: Stephen Lamm <slamm@netscape.com>, 13-Aug-1998
 */

#include "RDFToolbox.h"
#include "RDFToolbar.h"

#if DEBUG_slamm
#define D(x) x
#else
#define D(x)
#endif

XFE_RDFToolbox::XFE_RDFToolbox(XFE_Frame * frame,
                               XFE_Toolbox * toolbox)
    : XFE_RDFBase(),
      _frame(frame),
      _toolbox(toolbox)
{
    newToolbarPane();
}


XFE_RDFToolbox::~XFE_RDFToolbox() 
{
}
//////////////////////////////////////////////////////////////////////////

void
XFE_RDFToolbox::notify(HT_Resource n, HT_Event whatHappened)
{
  D(debugEvent(n, whatHappened,"Toolbox"););
  switch (whatHappened) {
  case HT_EVENT_VIEW_ADDED:
      {
          HT_View view = HT_GetView(n);

          // The destroy handler (see XFE_Component) handles deletion.
          new XFE_RDFToolbar(_frame, _toolbox, view);
      }
    break;
  case HT_EVENT_VIEW_DELETED:
      {
          HT_View view = HT_GetView(n);

          XFE_RDFToolbar * toolbar = (XFE_RDFToolbar *)HT_GetViewFEData(view);

          if (toolbar)
              delete toolbar;
      }
  default:
      // Fall through and let the base class handle this.
    break;
  }
  XFE_RDFBase::notify(n, whatHappened);
}
