/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIDOMCSS2Properties.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSS2PropertiesIID, NS_IDOMCSS2PROPERTIES_IID);

//
// CSS2Properties property ids
//
enum CSS2Properties_slots {
  CSS2PROPERTIES_AZIMUTH = -1,
  CSS2PROPERTIES_BACKGROUND = -2,
  CSS2PROPERTIES_BACKGROUNDATTACHMENT = -3,
  CSS2PROPERTIES_BACKGROUNDCOLOR = -4,
  CSS2PROPERTIES_BACKGROUNDIMAGE = -5,
  CSS2PROPERTIES_BACKGROUNDPOSITION = -6,
  CSS2PROPERTIES_BACKGROUNDREPEAT = -7,
  CSS2PROPERTIES_BORDER = -8,
  CSS2PROPERTIES_BORDERCOLLAPSE = -9,
  CSS2PROPERTIES_BORDERCOLOR = -10,
  CSS2PROPERTIES_BORDERSPACING = -11,
  CSS2PROPERTIES_BORDERSTYLE = -12,
  CSS2PROPERTIES_BORDERTOP = -13,
  CSS2PROPERTIES_BORDERRIGHT = -14,
  CSS2PROPERTIES_BORDERBOTTOM = -15,
  CSS2PROPERTIES_BORDERLEFT = -16,
  CSS2PROPERTIES_BORDERTOPCOLOR = -17,
  CSS2PROPERTIES_BORDERRIGHTCOLOR = -18,
  CSS2PROPERTIES_BORDERBOTTOMCOLOR = -19,
  CSS2PROPERTIES_BORDERLEFTCOLOR = -20,
  CSS2PROPERTIES_BORDERTOPSTYLE = -21,
  CSS2PROPERTIES_BORDERRIGHTSTYLE = -22,
  CSS2PROPERTIES_BORDERBOTTOMSTYLE = -23,
  CSS2PROPERTIES_BORDERLEFTSTYLE = -24,
  CSS2PROPERTIES_BORDERTOPWIDTH = -25,
  CSS2PROPERTIES_BORDERRIGHTWIDTH = -26,
  CSS2PROPERTIES_BORDERBOTTOMWIDTH = -27,
  CSS2PROPERTIES_BORDERLEFTWIDTH = -28,
  CSS2PROPERTIES_BORDERWIDTH = -29,
  CSS2PROPERTIES_BOTTOM = -30,
  CSS2PROPERTIES_CAPTIONSIDE = -31,
  CSS2PROPERTIES_CLEAR = -32,
  CSS2PROPERTIES_CLIP = -33,
  CSS2PROPERTIES_COLOR = -34,
  CSS2PROPERTIES_CONTENT = -35,
  CSS2PROPERTIES_COUNTERINCREMENT = -36,
  CSS2PROPERTIES_COUNTERRESET = -37,
  CSS2PROPERTIES_CUE = -38,
  CSS2PROPERTIES_CUEAFTER = -39,
  CSS2PROPERTIES_CUEBEFORE = -40,
  CSS2PROPERTIES_CURSOR = -41,
  CSS2PROPERTIES_DIRECTION = -42,
  CSS2PROPERTIES_DISPLAY = -43,
  CSS2PROPERTIES_ELEVATION = -44,
  CSS2PROPERTIES_EMPTYCELLS = -45,
  CSS2PROPERTIES_CSSFLOAT = -46,
  CSS2PROPERTIES_FONT = -47,
  CSS2PROPERTIES_FONTFAMILY = -48,
  CSS2PROPERTIES_FONTSIZE = -49,
  CSS2PROPERTIES_FONTSIZEADJUST = -50,
  CSS2PROPERTIES_FONTSTRETCH = -51,
  CSS2PROPERTIES_FONTSTYLE = -52,
  CSS2PROPERTIES_FONTVARIANT = -53,
  CSS2PROPERTIES_FONTWEIGHT = -54,
  CSS2PROPERTIES_HEIGHT = -55,
  CSS2PROPERTIES_LEFT = -56,
  CSS2PROPERTIES_LETTERSPACING = -57,
  CSS2PROPERTIES_LINEHEIGHT = -58,
  CSS2PROPERTIES_LISTSTYLE = -59,
  CSS2PROPERTIES_LISTSTYLEIMAGE = -60,
  CSS2PROPERTIES_LISTSTYLEPOSITION = -61,
  CSS2PROPERTIES_LISTSTYLETYPE = -62,
  CSS2PROPERTIES_MARGIN = -63,
  CSS2PROPERTIES_MARGINTOP = -64,
  CSS2PROPERTIES_MARGINRIGHT = -65,
  CSS2PROPERTIES_MARGINBOTTOM = -66,
  CSS2PROPERTIES_MARGINLEFT = -67,
  CSS2PROPERTIES_MARKEROFFSET = -68,
  CSS2PROPERTIES_MARKS = -69,
  CSS2PROPERTIES_MAXHEIGHT = -70,
  CSS2PROPERTIES_MAXWIDTH = -71,
  CSS2PROPERTIES_MINHEIGHT = -72,
  CSS2PROPERTIES_MINWIDTH = -73,
  CSS2PROPERTIES_ORPHANS = -74,
  CSS2PROPERTIES_OUTLINE = -75,
  CSS2PROPERTIES_OUTLINECOLOR = -76,
  CSS2PROPERTIES_OUTLINESTYLE = -77,
  CSS2PROPERTIES_OUTLINEWIDTH = -78,
  CSS2PROPERTIES_OVERFLOW = -79,
  CSS2PROPERTIES_PADDING = -80,
  CSS2PROPERTIES_PADDINGTOP = -81,
  CSS2PROPERTIES_PADDINGRIGHT = -82,
  CSS2PROPERTIES_PADDINGBOTTOM = -83,
  CSS2PROPERTIES_PADDINGLEFT = -84,
  CSS2PROPERTIES_PAGE = -85,
  CSS2PROPERTIES_PAGEBREAKAFTER = -86,
  CSS2PROPERTIES_PAGEBREAKBEFORE = -87,
  CSS2PROPERTIES_PAGEBREAKINSIDE = -88,
  CSS2PROPERTIES_PAUSE = -89,
  CSS2PROPERTIES_PAUSEAFTER = -90,
  CSS2PROPERTIES_PAUSEBEFORE = -91,
  CSS2PROPERTIES_PITCH = -92,
  CSS2PROPERTIES_PITCHRANGE = -93,
  CSS2PROPERTIES_PLAYDURING = -94,
  CSS2PROPERTIES_POSITION = -95,
  CSS2PROPERTIES_QUOTES = -96,
  CSS2PROPERTIES_RICHNESS = -97,
  CSS2PROPERTIES_RIGHT = -98,
  CSS2PROPERTIES_SIZE = -99,
  CSS2PROPERTIES_SPEAK = -100,
  CSS2PROPERTIES_SPEAKHEADER = -101,
  CSS2PROPERTIES_SPEAKNUMERAL = -102,
  CSS2PROPERTIES_SPEAKPUNCTUATION = -103,
  CSS2PROPERTIES_SPEECHRATE = -104,
  CSS2PROPERTIES_STRESS = -105,
  CSS2PROPERTIES_TABLELAYOUT = -106,
  CSS2PROPERTIES_TEXTALIGN = -107,
  CSS2PROPERTIES_TEXTDECORATION = -108,
  CSS2PROPERTIES_TEXTINDENT = -109,
  CSS2PROPERTIES_TEXTSHADOW = -110,
  CSS2PROPERTIES_TEXTTRANSFORM = -111,
  CSS2PROPERTIES_TOP = -112,
  CSS2PROPERTIES_UNICODEBIDI = -113,
  CSS2PROPERTIES_VERTICALALIGN = -114,
  CSS2PROPERTIES_VISIBILITY = -115,
  CSS2PROPERTIES_VOICEFAMILY = -116,
  CSS2PROPERTIES_VOLUME = -117,
  CSS2PROPERTIES_WHITESPACE = -118,
  CSS2PROPERTIES_WIDOWS = -119,
  CSS2PROPERTIES_WIDTH = -120,
  CSS2PROPERTIES_WORDSPACING = -121,
  CSS2PROPERTIES_ZINDEX = -122,
  CSS2PROPERTIES_MOZBINDING = -123,
  CSS2PROPERTIES_MOZOPACITY = -124
};

/***********************************************************************/
//
// CSS2Properties Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSS2PropertiesProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_AZIMUTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetAzimuth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUND:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUND, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackground(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDATTACHMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackgroundAttachment(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackgroundColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDIMAGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackgroundImage(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDPOSITION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackgroundPosition(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDREPEAT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBackgroundRepeat(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorder(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLLAPSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERCOLLAPSE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderCollapse(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERSPACING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderSpacing(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderTop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderRight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderBottom(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderLeft(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderTopColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderRightColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderBottomColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTCOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderLeftColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderTopStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderRightStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderBottomStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderLeftStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderTopWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderRightWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderBottomWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderLeftWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BORDERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBorderWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_BOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BOTTOM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetBottom(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CAPTIONSIDE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CAPTIONSIDE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCaptionSide(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CLEAR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CLEAR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetClear(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CLIP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CLIP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetClip(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_COLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CONTENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CONTENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetContent(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERINCREMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COUNTERINCREMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCounterIncrement(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERRESET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COUNTERRESET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCounterReset(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCue(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CUEAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUEAFTER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCueAfter(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CUEBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUEBEFORE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCueBefore(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CURSOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CURSOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCursor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_DIRECTION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_DIRECTION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDirection(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_DISPLAY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_DISPLAY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetDisplay(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_ELEVATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ELEVATION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetElevation(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_EMPTYCELLS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_EMPTYCELLS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetEmptyCells(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_CSSFLOAT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CSSFLOAT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetCssFloat(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFont(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTFAMILY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTFAMILY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontFamily(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSIZE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontSize(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZEADJUST:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSIZEADJUST, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontSizeAdjust(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTRETCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSTRETCH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontStretch(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTVARIANT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTVARIANT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontVariant(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_FONTWEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTWEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetFontWeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_HEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLeft(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LETTERSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LETTERSPACING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLetterSpacing(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LINEHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LINEHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetLineHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetListStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLEIMAGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetListStyleImage(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLEPOSITION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetListStylePosition(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLETYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLETYPE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetListStyleType(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARGIN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGIN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMargin(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARGINTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINTOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginTop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARGINRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINRIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginRight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARGINBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINBOTTOM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginBottom(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARGINLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINLEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarginLeft(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARKEROFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARKEROFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarkerOffset(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MARKS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARKS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMarks(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MAXHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MAXHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMaxHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MAXWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MAXWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMaxWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MINHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MINHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMinHeight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MINWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MINWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMinWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_ORPHANS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ORPHANS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOrphans(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOutline(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINECOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINECOLOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOutlineColor(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINESTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINESTYLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOutlineStyle(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINEWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINEWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOutlineWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_OVERFLOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OVERFLOW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetOverflow(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PADDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPadding(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGTOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPaddingTop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGRIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPaddingRight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGBOTTOM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPaddingBottom(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGLEFT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPaddingLeft(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPage(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKAFTER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPageBreakAfter(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKBEFORE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPageBreakBefore(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKINSIDE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPageBreakInside(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAUSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPause(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSEAFTER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPauseAfter(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSEBEFORE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPauseBefore(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PITCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PITCH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPitch(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PITCHRANGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PITCHRANGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPitchRange(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_PLAYDURING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PLAYDURING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPlayDuring(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_POSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_POSITION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetPosition(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_QUOTES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_QUOTES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetQuotes(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_RICHNESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_RICHNESS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetRichness(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_RIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_RIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetRight(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SIZE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSize(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SPEAK:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAK, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSpeak(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKHEADER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKHEADER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSpeakHeader(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKNUMERAL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKNUMERAL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSpeakNumeral(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKPUNCTUATION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSpeakPunctuation(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_SPEECHRATE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEECHRATE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetSpeechRate(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_STRESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_STRESS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetStress(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TABLELAYOUT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TABLELAYOUT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTableLayout(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TEXTALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTextAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TEXTDECORATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTDECORATION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTextDecoration(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TEXTINDENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTINDENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTextIndent(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TEXTSHADOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTSHADOW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTextShadow(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TEXTTRANSFORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTTRANSFORM, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTextTransform(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_TOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TOP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetTop(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_UNICODEBIDI:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_UNICODEBIDI, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetUnicodeBidi(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_VERTICALALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VERTICALALIGN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVerticalAlign(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_VISIBILITY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VISIBILITY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVisibility(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_VOICEFAMILY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VOICEFAMILY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVoiceFamily(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_VOLUME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VOLUME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetVolume(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_WHITESPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WHITESPACE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWhiteSpace(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_WIDOWS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WIDOWS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidows(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWidth(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_WORDSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WORDSPACING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetWordSpacing(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_ZINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ZINDEX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetZIndex(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MOZBINDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MOZBINDING, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMozBinding(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case CSS2PROPERTIES_MOZOPACITY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MOZOPACITY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetMozOpacity(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}

/***********************************************************************/
//
// CSS2Properties Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSS2PropertiesProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_AZIMUTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetAzimuth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUND:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUND, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackground(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDATTACHMENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackgroundAttachment(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackgroundColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDIMAGE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackgroundImage(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDPOSITION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackgroundPosition(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BACKGROUNDREPEAT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBackgroundRepeat(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorder(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLLAPSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERCOLLAPSE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderCollapse(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERSPACING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderSpacing(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderTop(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderRight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderBottom(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderLeft(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderTopColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderRightColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderBottomColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTCOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderLeftColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderTopStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderRightStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderBottomStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderLeftStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERTOPWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderTopWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERRIGHTWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderRightWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERBOTTOMWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderBottomWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERLEFTWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderLeftWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BORDERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BORDERWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBorderWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_BOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_BOTTOM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetBottom(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CAPTIONSIDE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CAPTIONSIDE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCaptionSide(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CLEAR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CLEAR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetClear(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CLIP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CLIP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetClip(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_COLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CONTENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CONTENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetContent(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERINCREMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COUNTERINCREMENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCounterIncrement(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERRESET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_COUNTERRESET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCounterReset(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CUE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCue(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CUEAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUEAFTER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCueAfter(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CUEBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CUEBEFORE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCueBefore(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CURSOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CURSOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCursor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_DIRECTION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_DIRECTION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDirection(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_DISPLAY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_DISPLAY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetDisplay(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_ELEVATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ELEVATION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetElevation(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_EMPTYCELLS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_EMPTYCELLS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetEmptyCells(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_CSSFLOAT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_CSSFLOAT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetCssFloat(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFont(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTFAMILY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTFAMILY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontFamily(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSIZE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontSize(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZEADJUST:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSIZEADJUST, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontSizeAdjust(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTRETCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSTRETCH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontStretch(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTVARIANT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTVARIANT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontVariant(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_FONTWEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_FONTWEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetFontWeight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_HEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_HEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetHeight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LEFT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLeft(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LETTERSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LETTERSPACING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLetterSpacing(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LINEHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LINEHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetLineHeight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetListStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLEIMAGE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetListStyleImage(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLEPOSITION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetListStylePosition(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLETYPE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_LISTSTYLETYPE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetListStyleType(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARGIN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGIN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMargin(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARGINTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINTOP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginTop(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARGINRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINRIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginRight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARGINBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINBOTTOM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginBottom(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARGINLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARGINLEFT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarginLeft(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARKEROFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARKEROFFSET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarkerOffset(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MARKS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MARKS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMarks(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MAXHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MAXHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMaxHeight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MAXWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MAXWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMaxWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MINHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MINHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMinHeight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MINWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MINWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMinWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_ORPHANS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ORPHANS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOrphans(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOutline(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINECOLOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINECOLOR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOutlineColor(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINESTYLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINESTYLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOutlineStyle(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINEWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OUTLINEWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOutlineWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_OVERFLOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_OVERFLOW, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetOverflow(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PADDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPadding(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGTOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGTOP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPaddingTop(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGRIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGRIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPaddingRight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGBOTTOM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGBOTTOM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPaddingBottom(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGLEFT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PADDINGLEFT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPaddingLeft(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPage(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKAFTER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPageBreakAfter(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKBEFORE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPageBreakBefore(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAGEBREAKINSIDE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPageBreakInside(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAUSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPause(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEAFTER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSEAFTER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPauseAfter(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEBEFORE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PAUSEBEFORE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPauseBefore(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PITCH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PITCH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPitch(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PITCHRANGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PITCHRANGE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPitchRange(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_PLAYDURING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_PLAYDURING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPlayDuring(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_POSITION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_POSITION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetPosition(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_QUOTES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_QUOTES, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetQuotes(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_RICHNESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_RICHNESS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetRichness(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_RIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_RIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetRight(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SIZE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSize(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SPEAK:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAK, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSpeak(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKHEADER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKHEADER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSpeakHeader(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKNUMERAL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKNUMERAL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSpeakNumeral(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEAKPUNCTUATION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSpeakPunctuation(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_SPEECHRATE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_SPEECHRATE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetSpeechRate(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_STRESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_STRESS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetStress(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TABLELAYOUT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TABLELAYOUT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTableLayout(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TEXTALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTextAlign(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TEXTDECORATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTDECORATION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTextDecoration(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TEXTINDENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTINDENT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTextIndent(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TEXTSHADOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTSHADOW, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTextShadow(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TEXTTRANSFORM:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TEXTTRANSFORM, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTextTransform(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_TOP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_TOP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetTop(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_UNICODEBIDI:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_UNICODEBIDI, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetUnicodeBidi(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_VERTICALALIGN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VERTICALALIGN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVerticalAlign(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_VISIBILITY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VISIBILITY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVisibility(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_VOICEFAMILY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VOICEFAMILY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVoiceFamily(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_VOLUME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_VOLUME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetVolume(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_WHITESPACE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WHITESPACE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWhiteSpace(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_WIDOWS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WIDOWS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidows(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_WIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWidth(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_WORDSPACING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_WORDSPACING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetWordSpacing(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_ZINDEX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_ZINDEX, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetZIndex(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MOZBINDING:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MOZBINDING, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMozBinding(prop);
          
        }
        break;
      }
      case CSS2PROPERTIES_MOZOPACITY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_CSS2PROPERTIES_MOZOPACITY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetMozOpacity(prop);
          
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, obj, id, vp);
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}


//
// CSS2Properties finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSS2Properties(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// CSS2Properties enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSS2Properties(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// CSS2Properties resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSS2Properties(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGenericResolve(cx, obj, id);
}


/***********************************************************************/
//
// class for CSS2Properties
//
JSClass CSS2PropertiesClass = {
  "CSS2Properties", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSS2PropertiesProperty,
  SetCSS2PropertiesProperty,
  EnumerateCSS2Properties,
  ResolveCSS2Properties,
  JS_ConvertStub,
  FinalizeCSS2Properties,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// CSS2Properties class properties
//
static JSPropertySpec CSS2PropertiesProperties[] =
{
  {"azimuth",    CSS2PROPERTIES_AZIMUTH,    JSPROP_ENUMERATE},
  {"background",    CSS2PROPERTIES_BACKGROUND,    JSPROP_ENUMERATE},
  {"backgroundAttachment",    CSS2PROPERTIES_BACKGROUNDATTACHMENT,    JSPROP_ENUMERATE},
  {"backgroundColor",    CSS2PROPERTIES_BACKGROUNDCOLOR,    JSPROP_ENUMERATE},
  {"backgroundImage",    CSS2PROPERTIES_BACKGROUNDIMAGE,    JSPROP_ENUMERATE},
  {"backgroundPosition",    CSS2PROPERTIES_BACKGROUNDPOSITION,    JSPROP_ENUMERATE},
  {"backgroundRepeat",    CSS2PROPERTIES_BACKGROUNDREPEAT,    JSPROP_ENUMERATE},
  {"border",    CSS2PROPERTIES_BORDER,    JSPROP_ENUMERATE},
  {"borderCollapse",    CSS2PROPERTIES_BORDERCOLLAPSE,    JSPROP_ENUMERATE},
  {"borderColor",    CSS2PROPERTIES_BORDERCOLOR,    JSPROP_ENUMERATE},
  {"borderSpacing",    CSS2PROPERTIES_BORDERSPACING,    JSPROP_ENUMERATE},
  {"borderStyle",    CSS2PROPERTIES_BORDERSTYLE,    JSPROP_ENUMERATE},
  {"borderTop",    CSS2PROPERTIES_BORDERTOP,    JSPROP_ENUMERATE},
  {"borderRight",    CSS2PROPERTIES_BORDERRIGHT,    JSPROP_ENUMERATE},
  {"borderBottom",    CSS2PROPERTIES_BORDERBOTTOM,    JSPROP_ENUMERATE},
  {"borderLeft",    CSS2PROPERTIES_BORDERLEFT,    JSPROP_ENUMERATE},
  {"borderTopColor",    CSS2PROPERTIES_BORDERTOPCOLOR,    JSPROP_ENUMERATE},
  {"borderRightColor",    CSS2PROPERTIES_BORDERRIGHTCOLOR,    JSPROP_ENUMERATE},
  {"borderBottomColor",    CSS2PROPERTIES_BORDERBOTTOMCOLOR,    JSPROP_ENUMERATE},
  {"borderLeftColor",    CSS2PROPERTIES_BORDERLEFTCOLOR,    JSPROP_ENUMERATE},
  {"borderTopStyle",    CSS2PROPERTIES_BORDERTOPSTYLE,    JSPROP_ENUMERATE},
  {"borderRightStyle",    CSS2PROPERTIES_BORDERRIGHTSTYLE,    JSPROP_ENUMERATE},
  {"borderBottomStyle",    CSS2PROPERTIES_BORDERBOTTOMSTYLE,    JSPROP_ENUMERATE},
  {"borderLeftStyle",    CSS2PROPERTIES_BORDERLEFTSTYLE,    JSPROP_ENUMERATE},
  {"borderTopWidth",    CSS2PROPERTIES_BORDERTOPWIDTH,    JSPROP_ENUMERATE},
  {"borderRightWidth",    CSS2PROPERTIES_BORDERRIGHTWIDTH,    JSPROP_ENUMERATE},
  {"borderBottomWidth",    CSS2PROPERTIES_BORDERBOTTOMWIDTH,    JSPROP_ENUMERATE},
  {"borderLeftWidth",    CSS2PROPERTIES_BORDERLEFTWIDTH,    JSPROP_ENUMERATE},
  {"borderWidth",    CSS2PROPERTIES_BORDERWIDTH,    JSPROP_ENUMERATE},
  {"bottom",    CSS2PROPERTIES_BOTTOM,    JSPROP_ENUMERATE},
  {"captionSide",    CSS2PROPERTIES_CAPTIONSIDE,    JSPROP_ENUMERATE},
  {"clear",    CSS2PROPERTIES_CLEAR,    JSPROP_ENUMERATE},
  {"clip",    CSS2PROPERTIES_CLIP,    JSPROP_ENUMERATE},
  {"color",    CSS2PROPERTIES_COLOR,    JSPROP_ENUMERATE},
  {"content",    CSS2PROPERTIES_CONTENT,    JSPROP_ENUMERATE},
  {"counterIncrement",    CSS2PROPERTIES_COUNTERINCREMENT,    JSPROP_ENUMERATE},
  {"counterReset",    CSS2PROPERTIES_COUNTERRESET,    JSPROP_ENUMERATE},
  {"cue",    CSS2PROPERTIES_CUE,    JSPROP_ENUMERATE},
  {"cueAfter",    CSS2PROPERTIES_CUEAFTER,    JSPROP_ENUMERATE},
  {"cueBefore",    CSS2PROPERTIES_CUEBEFORE,    JSPROP_ENUMERATE},
  {"cursor",    CSS2PROPERTIES_CURSOR,    JSPROP_ENUMERATE},
  {"direction",    CSS2PROPERTIES_DIRECTION,    JSPROP_ENUMERATE},
  {"display",    CSS2PROPERTIES_DISPLAY,    JSPROP_ENUMERATE},
  {"elevation",    CSS2PROPERTIES_ELEVATION,    JSPROP_ENUMERATE},
  {"emptyCells",    CSS2PROPERTIES_EMPTYCELLS,    JSPROP_ENUMERATE},
  {"cssFloat",    CSS2PROPERTIES_CSSFLOAT,    JSPROP_ENUMERATE},
  {"font",    CSS2PROPERTIES_FONT,    JSPROP_ENUMERATE},
  {"fontFamily",    CSS2PROPERTIES_FONTFAMILY,    JSPROP_ENUMERATE},
  {"fontSize",    CSS2PROPERTIES_FONTSIZE,    JSPROP_ENUMERATE},
  {"fontSizeAdjust",    CSS2PROPERTIES_FONTSIZEADJUST,    JSPROP_ENUMERATE},
  {"fontStretch",    CSS2PROPERTIES_FONTSTRETCH,    JSPROP_ENUMERATE},
  {"fontStyle",    CSS2PROPERTIES_FONTSTYLE,    JSPROP_ENUMERATE},
  {"fontVariant",    CSS2PROPERTIES_FONTVARIANT,    JSPROP_ENUMERATE},
  {"fontWeight",    CSS2PROPERTIES_FONTWEIGHT,    JSPROP_ENUMERATE},
  {"height",    CSS2PROPERTIES_HEIGHT,    JSPROP_ENUMERATE},
  {"left",    CSS2PROPERTIES_LEFT,    JSPROP_ENUMERATE},
  {"letterSpacing",    CSS2PROPERTIES_LETTERSPACING,    JSPROP_ENUMERATE},
  {"lineHeight",    CSS2PROPERTIES_LINEHEIGHT,    JSPROP_ENUMERATE},
  {"listStyle",    CSS2PROPERTIES_LISTSTYLE,    JSPROP_ENUMERATE},
  {"listStyleImage",    CSS2PROPERTIES_LISTSTYLEIMAGE,    JSPROP_ENUMERATE},
  {"listStylePosition",    CSS2PROPERTIES_LISTSTYLEPOSITION,    JSPROP_ENUMERATE},
  {"listStyleType",    CSS2PROPERTIES_LISTSTYLETYPE,    JSPROP_ENUMERATE},
  {"margin",    CSS2PROPERTIES_MARGIN,    JSPROP_ENUMERATE},
  {"marginTop",    CSS2PROPERTIES_MARGINTOP,    JSPROP_ENUMERATE},
  {"marginRight",    CSS2PROPERTIES_MARGINRIGHT,    JSPROP_ENUMERATE},
  {"marginBottom",    CSS2PROPERTIES_MARGINBOTTOM,    JSPROP_ENUMERATE},
  {"marginLeft",    CSS2PROPERTIES_MARGINLEFT,    JSPROP_ENUMERATE},
  {"markerOffset",    CSS2PROPERTIES_MARKEROFFSET,    JSPROP_ENUMERATE},
  {"marks",    CSS2PROPERTIES_MARKS,    JSPROP_ENUMERATE},
  {"maxHeight",    CSS2PROPERTIES_MAXHEIGHT,    JSPROP_ENUMERATE},
  {"maxWidth",    CSS2PROPERTIES_MAXWIDTH,    JSPROP_ENUMERATE},
  {"minHeight",    CSS2PROPERTIES_MINHEIGHT,    JSPROP_ENUMERATE},
  {"minWidth",    CSS2PROPERTIES_MINWIDTH,    JSPROP_ENUMERATE},
  {"orphans",    CSS2PROPERTIES_ORPHANS,    JSPROP_ENUMERATE},
  {"outline",    CSS2PROPERTIES_OUTLINE,    JSPROP_ENUMERATE},
  {"outlineColor",    CSS2PROPERTIES_OUTLINECOLOR,    JSPROP_ENUMERATE},
  {"outlineStyle",    CSS2PROPERTIES_OUTLINESTYLE,    JSPROP_ENUMERATE},
  {"outlineWidth",    CSS2PROPERTIES_OUTLINEWIDTH,    JSPROP_ENUMERATE},
  {"overflow",    CSS2PROPERTIES_OVERFLOW,    JSPROP_ENUMERATE},
  {"padding",    CSS2PROPERTIES_PADDING,    JSPROP_ENUMERATE},
  {"paddingTop",    CSS2PROPERTIES_PADDINGTOP,    JSPROP_ENUMERATE},
  {"paddingRight",    CSS2PROPERTIES_PADDINGRIGHT,    JSPROP_ENUMERATE},
  {"paddingBottom",    CSS2PROPERTIES_PADDINGBOTTOM,    JSPROP_ENUMERATE},
  {"paddingLeft",    CSS2PROPERTIES_PADDINGLEFT,    JSPROP_ENUMERATE},
  {"page",    CSS2PROPERTIES_PAGE,    JSPROP_ENUMERATE},
  {"pageBreakAfter",    CSS2PROPERTIES_PAGEBREAKAFTER,    JSPROP_ENUMERATE},
  {"pageBreakBefore",    CSS2PROPERTIES_PAGEBREAKBEFORE,    JSPROP_ENUMERATE},
  {"pageBreakInside",    CSS2PROPERTIES_PAGEBREAKINSIDE,    JSPROP_ENUMERATE},
  {"pause",    CSS2PROPERTIES_PAUSE,    JSPROP_ENUMERATE},
  {"pauseAfter",    CSS2PROPERTIES_PAUSEAFTER,    JSPROP_ENUMERATE},
  {"pauseBefore",    CSS2PROPERTIES_PAUSEBEFORE,    JSPROP_ENUMERATE},
  {"pitch",    CSS2PROPERTIES_PITCH,    JSPROP_ENUMERATE},
  {"pitchRange",    CSS2PROPERTIES_PITCHRANGE,    JSPROP_ENUMERATE},
  {"playDuring",    CSS2PROPERTIES_PLAYDURING,    JSPROP_ENUMERATE},
  {"position",    CSS2PROPERTIES_POSITION,    JSPROP_ENUMERATE},
  {"quotes",    CSS2PROPERTIES_QUOTES,    JSPROP_ENUMERATE},
  {"richness",    CSS2PROPERTIES_RICHNESS,    JSPROP_ENUMERATE},
  {"right",    CSS2PROPERTIES_RIGHT,    JSPROP_ENUMERATE},
  {"size",    CSS2PROPERTIES_SIZE,    JSPROP_ENUMERATE},
  {"speak",    CSS2PROPERTIES_SPEAK,    JSPROP_ENUMERATE},
  {"speakHeader",    CSS2PROPERTIES_SPEAKHEADER,    JSPROP_ENUMERATE},
  {"speakNumeral",    CSS2PROPERTIES_SPEAKNUMERAL,    JSPROP_ENUMERATE},
  {"speakPunctuation",    CSS2PROPERTIES_SPEAKPUNCTUATION,    JSPROP_ENUMERATE},
  {"speechRate",    CSS2PROPERTIES_SPEECHRATE,    JSPROP_ENUMERATE},
  {"stress",    CSS2PROPERTIES_STRESS,    JSPROP_ENUMERATE},
  {"tableLayout",    CSS2PROPERTIES_TABLELAYOUT,    JSPROP_ENUMERATE},
  {"textAlign",    CSS2PROPERTIES_TEXTALIGN,    JSPROP_ENUMERATE},
  {"textDecoration",    CSS2PROPERTIES_TEXTDECORATION,    JSPROP_ENUMERATE},
  {"textIndent",    CSS2PROPERTIES_TEXTINDENT,    JSPROP_ENUMERATE},
  {"textShadow",    CSS2PROPERTIES_TEXTSHADOW,    JSPROP_ENUMERATE},
  {"textTransform",    CSS2PROPERTIES_TEXTTRANSFORM,    JSPROP_ENUMERATE},
  {"top",    CSS2PROPERTIES_TOP,    JSPROP_ENUMERATE},
  {"unicodeBidi",    CSS2PROPERTIES_UNICODEBIDI,    JSPROP_ENUMERATE},
  {"verticalAlign",    CSS2PROPERTIES_VERTICALALIGN,    JSPROP_ENUMERATE},
  {"visibility",    CSS2PROPERTIES_VISIBILITY,    JSPROP_ENUMERATE},
  {"voiceFamily",    CSS2PROPERTIES_VOICEFAMILY,    JSPROP_ENUMERATE},
  {"volume",    CSS2PROPERTIES_VOLUME,    JSPROP_ENUMERATE},
  {"whiteSpace",    CSS2PROPERTIES_WHITESPACE,    JSPROP_ENUMERATE},
  {"widows",    CSS2PROPERTIES_WIDOWS,    JSPROP_ENUMERATE},
  {"width",    CSS2PROPERTIES_WIDTH,    JSPROP_ENUMERATE},
  {"wordSpacing",    CSS2PROPERTIES_WORDSPACING,    JSPROP_ENUMERATE},
  {"zIndex",    CSS2PROPERTIES_ZINDEX,    JSPROP_ENUMERATE},
  {"MozBinding",    CSS2PROPERTIES_MOZBINDING,    JSPROP_ENUMERATE},
  {"MozOpacity",    CSS2PROPERTIES_MOZOPACITY,    JSPROP_ENUMERATE},
  {0}
};


//
// CSS2Properties class methods
//
static JSFunctionSpec CSS2PropertiesMethods[] = 
{
  {0}
};


//
// CSS2Properties constructor
//
PR_STATIC_CALLBACK(JSBool)
CSS2Properties(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSS2Properties class initialization
//
extern "C" NS_DOM nsresult NS_InitCSS2PropertiesClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSS2Properties", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    if (NS_OK != NS_InitCSSStyleDeclarationClass(aContext, (void **)&parent_proto)) {
      return NS_ERROR_FAILURE;
    }
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSS2PropertiesClass,      // JSClass
                         CSS2Properties,            // JSNative ctor
                         0,             // ctor args
                         CSS2PropertiesProperties,  // proto props
                         CSS2PropertiesMethods,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (aPrototype) {
    *aPrototype = proto;
  }
  return NS_OK;
}


//
// Method for creating a new CSS2Properties JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSS2Properties(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSS2Properties");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSS2Properties *aCSS2Properties;

  if (nsnull == aParent) {
    parent = nsnull;
  }
  else if (NS_OK == aParent->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
    if (NS_OK != owner->GetScriptObject(aContext, (void **)&parent)) {
      NS_RELEASE(owner);
      return NS_ERROR_FAILURE;
    }
    NS_RELEASE(owner);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  if (NS_OK != NS_InitCSS2PropertiesClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSS2PropertiesIID, (void **)&aCSS2Properties);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSS2PropertiesClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSS2Properties);
  }
  else {
    NS_RELEASE(aCSS2Properties);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
