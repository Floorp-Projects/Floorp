/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
   gnomeedit.c --- gnome functions for fe
                   specific editor stuff.
*/

#include "libimg.h"             /* Image Library public API. */
#include "xp_core.h"
#include "structs.h"
#include "ntypes.h"
#include "lo_ele.h"

#include "layers.h"

#include "g-util.h"
#include "g-font-select.h"

void
FE_ReleaseTextAttrFeData(MWContext *context,
			 LO_TextAttr *attr)
{
  printf ("FE_ReleaseTextAttrFeData\n");
  moz_font_release_text_attribute(attr);
}

PRBool
FE_HandleLayerEvent(MWContext *context,
		    CL_Layer *layer, 
		    CL_Event *event)
{
  MozHTMLView *view = find_html_view(context);

  switch(event->type)
    {
    case CL_EVENT_MOUSE_MOVE:
      html_view_handle_pointer_motion_for_layer(view, layer, event);
      return TRUE;
      break;
    case CL_EVENT_MOUSE_BUTTON_DOWN:
      html_view_handle_button_press_for_layer(view, layer, event);
      return TRUE;
      break;
    case CL_EVENT_MOUSE_BUTTON_UP:
      html_view_handle_button_release_for_layer(view, layer, event);
      return TRUE;
      break;
    default:
      return FALSE;
      break;
    }
}

PRBool
FE_HandleEmbedEvent(MWContext *context,
		    LO_EmbedStruct *embed,
		    CL_Event *event)
{
  printf ("FE_HandleEmbedEvent\n");
}

