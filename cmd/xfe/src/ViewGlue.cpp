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
   ViewGlue.cpp -- glue to go from views to MWContexts, and back again.
   Created: Chris Toshok <toshok@netscape.com>, 25-Sep-96
   */



#include "structs.h"
#include "xfe.h"
#include "ViewGlue.h"
#include "xpassert.h"

void 
ViewGlue_addMapping(XFE_Frame *frame,
		    void *context)
{
  MWContext *mwcontext = (MWContext*)context;

  CONTEXT_DATA(mwcontext)->view = frame;
}

void 
ViewGlue_addMappingForCompo(XFE_Component *compo,
							void *context)
{
  MWContext *mwcontext = (MWContext*)context;

  CONTEXT_DATA(mwcontext)->view = compo;
}


// Get a view associated with a context.
// Toshok: "the name is a throwback to when we had
//          one context per view.  Now we have one per frame."
XFE_Frame *
ViewGlue_getFrame(void *context)
{
  MWContext *mwcontext = (MWContext*)context;
  if (mwcontext == NULL) return NULL;

#if defined(GLUE_COMPO_CONTEXT)
  XFE_Component *compo = (XFE_Component *) (CONTEXT_DATA(mwcontext)->view);
  if (compo &&
	  compo->isClassOf("Frame"))
	  return ((XFE_Frame*) compo); 
  else
	  return NULL;
#else
  return (XFE_Frame*)(CONTEXT_DATA(mwcontext)->view);
#endif /* GLUE_COMPO_CONTEXT */
}

// Get a view associated with a context.
//          one context per view.  Now we have one per frame."
XFE_Component *
ViewGlue_getCompo(void *context)
{
  MWContext *mwcontext = (MWContext*)context;
  if (mwcontext == NULL) return NULL;

  return (XFE_Component*)(CONTEXT_DATA(mwcontext)->view);
}
