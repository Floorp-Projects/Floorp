/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPtr.h"
#include "nsString.h"
#include "nsIDOMCSSStyleDeclaration.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);

NS_DEF_PTR(nsIDOMCSSStyleDeclaration);

//
// CSSStyleDeclaration property ids
//
enum CSSStyleDeclaration_slots {
  CSSSTYLEDECLARATION_LENGTH = -1,
  CSSSTYLEDECLARATION_AZIMUTH = -2,
  CSSSTYLEDECLARATION_BACKGROUND = -3,
  CSSSTYLEDECLARATION_BACKGROUNDATTACHMENT = -4,
  CSSSTYLEDECLARATION_BACKGROUNDCOLOR = -5,
  CSSSTYLEDECLARATION_BACKGROUNDIMAGE = -6,
  CSSSTYLEDECLARATION_BACKGROUNDPOSITION = -7,
  CSSSTYLEDECLARATION_BACKGROUNDREPEAT = -8,
  CSSSTYLEDECLARATION_BORDER = -9,
  CSSSTYLEDECLARATION_BORDERCOLLAPSE = -10,
  CSSSTYLEDECLARATION_BORDERCOLOR = -11,
  CSSSTYLEDECLARATION_BORDERSPACING = -12,
  CSSSTYLEDECLARATION_BORDERSTYLE = -13,
  CSSSTYLEDECLARATION_BORDERTOP = -14,
  CSSSTYLEDECLARATION_BORDERRIGHT = -15,
  CSSSTYLEDECLARATION_BORDERBOTTOM = -16,
  CSSSTYLEDECLARATION_BORDERLEFT = -17,
  CSSSTYLEDECLARATION_BORDERTOPCOLOR = -18,
  CSSSTYLEDECLARATION_BORDERRIGHTCOLOR = -19,
  CSSSTYLEDECLARATION_BORDERBOTTOMCOLOR = -20,
  CSSSTYLEDECLARATION_BORDERLEFTCOLOR = -21,
  CSSSTYLEDECLARATION_BORDERTOPSTYLE = -22,
  CSSSTYLEDECLARATION_BORDERRIGHTSTYLE = -23,
  CSSSTYLEDECLARATION_BORDERBOTTOMSTYLE = -24,
  CSSSTYLEDECLARATION_BORDERLEFTSTYLE = -25,
  CSSSTYLEDECLARATION_BORDERTOPWIDTH = -26,
  CSSSTYLEDECLARATION_BORDERRIGHTWIDTH = -27,
  CSSSTYLEDECLARATION_BORDERBOTTOMWIDTH = -28,
  CSSSTYLEDECLARATION_BORDERLEFTWIDTH = -29,
  CSSSTYLEDECLARATION_BORDERWIDTH = -30,
  CSSSTYLEDECLARATION_BOTTOM = -31,
  CSSSTYLEDECLARATION_CAPTIONSIDE = -32,
  CSSSTYLEDECLARATION_CLEAR = -33,
  CSSSTYLEDECLARATION_CLIP = -34,
  CSSSTYLEDECLARATION_COLOR = -35,
  CSSSTYLEDECLARATION_CONTENT = -36,
  CSSSTYLEDECLARATION_COUNTERINCREMENT = -37,
  CSSSTYLEDECLARATION_COUNTERRESET = -38,
  CSSSTYLEDECLARATION_CUE = -39,
  CSSSTYLEDECLARATION_CUEAFTER = -40,
  CSSSTYLEDECLARATION_CUEBEFORE = -41,
  CSSSTYLEDECLARATION_CURSOR = -42,
  CSSSTYLEDECLARATION_DIRECTION = -43,
  CSSSTYLEDECLARATION_DISPLAY = -44,
  CSSSTYLEDECLARATION_ELEVATION = -45,
  CSSSTYLEDECLARATION_EMPTYCELLS = -46,
  CSSSTYLEDECLARATION_STYLEFLOAT = -47,
  CSSSTYLEDECLARATION_FONT = -48,
  CSSSTYLEDECLARATION_FONTFAMILY = -49,
  CSSSTYLEDECLARATION_FONTSIZE = -50,
  CSSSTYLEDECLARATION_FONTSIZEADJUST = -51,
  CSSSTYLEDECLARATION_FONTSTRETCH = -52,
  CSSSTYLEDECLARATION_FONTSTYLE = -53,
  CSSSTYLEDECLARATION_FONTVARIANT = -54,
  CSSSTYLEDECLARATION_FONTWEIGHT = -55,
  CSSSTYLEDECLARATION_HEIGHT = -56,
  CSSSTYLEDECLARATION_LEFT = -57,
  CSSSTYLEDECLARATION_LETTERSPACING = -58,
  CSSSTYLEDECLARATION_LINEHEIGHT = -59,
  CSSSTYLEDECLARATION_LISTSTYLE = -60,
  CSSSTYLEDECLARATION_LISTSTYLEIMAGE = -61,
  CSSSTYLEDECLARATION_LISTSTYLEPOSITION = -62,
  CSSSTYLEDECLARATION_LISTSTYLETYPE = -63,
  CSSSTYLEDECLARATION_MARGIN = -64,
  CSSSTYLEDECLARATION_MARGINTOP = -65,
  CSSSTYLEDECLARATION_MARGINRIGHT = -66,
  CSSSTYLEDECLARATION_MARGINBOTTOM = -67,
  CSSSTYLEDECLARATION_MARGINLEFT = -68,
  CSSSTYLEDECLARATION_MARKEROFFSET = -69,
  CSSSTYLEDECLARATION_MARKS = -70,
  CSSSTYLEDECLARATION_MAXHEIGHT = -71,
  CSSSTYLEDECLARATION_MAXWIDTH = -72,
  CSSSTYLEDECLARATION_MINHEIGHT = -73,
  CSSSTYLEDECLARATION_MINWIDTH = -74,
  CSSSTYLEDECLARATION_ORPHANS = -75,
  CSSSTYLEDECLARATION_OUTLINE = -76,
  CSSSTYLEDECLARATION_OUTLINECOLOR = -77,
  CSSSTYLEDECLARATION_OUTLINESTYLE = -78,
  CSSSTYLEDECLARATION_OUTLINEWIDTH = -79,
  CSSSTYLEDECLARATION_OVERFLOW = -80,
  CSSSTYLEDECLARATION_PADDING = -81,
  CSSSTYLEDECLARATION_PADDINGTOP = -82,
  CSSSTYLEDECLARATION_PADDINGRIGHT = -83,
  CSSSTYLEDECLARATION_PADDINGBOTTOM = -84,
  CSSSTYLEDECLARATION_PADDINGLEFT = -85,
  CSSSTYLEDECLARATION_PAGE = -86,
  CSSSTYLEDECLARATION_PAGEBREAKAFTER = -87,
  CSSSTYLEDECLARATION_PAGEBREAKBEFORE = -88,
  CSSSTYLEDECLARATION_PAGEBREAKINSIDE = -89,
  CSSSTYLEDECLARATION_PAUSE = -90,
  CSSSTYLEDECLARATION_PAUSEAFTER = -91,
  CSSSTYLEDECLARATION_PAUSEBEFORE = -92,
  CSSSTYLEDECLARATION_PITCH = -93,
  CSSSTYLEDECLARATION_PITCHRANGE = -94,
  CSSSTYLEDECLARATION_PLAYDURING = -95,
  CSSSTYLEDECLARATION_POSITION = -96,
  CSSSTYLEDECLARATION_QUOTES = -97,
  CSSSTYLEDECLARATION_RICHNESS = -98,
  CSSSTYLEDECLARATION_RIGHT = -99,
  CSSSTYLEDECLARATION_SIZE = -100,
  CSSSTYLEDECLARATION_SPEAK = -101,
  CSSSTYLEDECLARATION_SPEAKHEADER = -102,
  CSSSTYLEDECLARATION_SPEAKNUMERAL = -103,
  CSSSTYLEDECLARATION_SPEAKPUNCTUATION = -104,
  CSSSTYLEDECLARATION_SPEECHRATE = -105,
  CSSSTYLEDECLARATION_STRESS = -106,
  CSSSTYLEDECLARATION_TABLELAYOUT = -107,
  CSSSTYLEDECLARATION_TEXTALIGN = -108,
  CSSSTYLEDECLARATION_TEXTDECORATION = -109,
  CSSSTYLEDECLARATION_TEXTINDENT = -110,
  CSSSTYLEDECLARATION_TEXTSHADOW = -111,
  CSSSTYLEDECLARATION_TEXTTRANSFORM = -112,
  CSSSTYLEDECLARATION_TOP = -113,
  CSSSTYLEDECLARATION_UNICODEBIDI = -114,
  CSSSTYLEDECLARATION_VERTICALALIGN = -115,
  CSSSTYLEDECLARATION_VISIBILITY = -116,
  CSSSTYLEDECLARATION_VOICEFAMILY = -117,
  CSSSTYLEDECLARATION_VOLUME = -118,
  CSSSTYLEDECLARATION_WHITESPACE = -119,
  CSSSTYLEDECLARATION_WIDOWS = -120,
  CSSSTYLEDECLARATION_WIDTH = -121,
  CSSSTYLEDECLARATION_WORDSPACING = -122,
  CSSSTYLEDECLARATION_ZINDEX = -123,
  CSSSTYLEDECLARATION_OPACITY = -124
};

/***********************************************************************/
//
// CSSStyleDeclaration Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSSStyleDeclarationProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleDeclaration *a = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSSSTYLEDECLARATION_LENGTH:
      {
        PRUint32 prop;
        if (NS_OK == a->GetLength(&prop)) {
          *vp = INT_TO_JSVAL(prop);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_AZIMUTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetAzimuth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUND:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackground(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDATTACHMENT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackgroundAttachment(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackgroundColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDIMAGE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackgroundImage(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDPOSITION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackgroundPosition(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDREPEAT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBackgroundRepeat(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDER:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorder(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERCOLLAPSE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderCollapse(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERSPACING:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderSpacing(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOP:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderTop(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderRight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOM:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderBottom(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderLeft(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderTopColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderRightColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderBottomColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTCOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderLeftColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderTopStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderRightStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderBottomStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderLeftStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderTopWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderRightWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderBottomWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderLeftWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BORDERWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBorderWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_BOTTOM:
      {
        nsAutoString prop;
        if (NS_OK == a->GetBottom(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CAPTIONSIDE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCaptionSide(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CLEAR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetClear(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CLIP:
      {
        nsAutoString prop;
        if (NS_OK == a->GetClip(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_COLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CONTENT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetContent(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_COUNTERINCREMENT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCounterIncrement(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_COUNTERRESET:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCounterReset(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CUE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCue(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CUEAFTER:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCueAfter(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CUEBEFORE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCueBefore(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_CURSOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCursor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_DIRECTION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetDirection(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_DISPLAY:
      {
        nsAutoString prop;
        if (NS_OK == a->GetDisplay(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_ELEVATION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetElevation(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_EMPTYCELLS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetEmptyCells(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_STYLEFLOAT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetStyleFloat(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFont(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTFAMILY:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontFamily(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTSIZE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontSize(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTSIZEADJUST:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontSizeAdjust(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTSTRETCH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontStretch(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTVARIANT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontVariant(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_FONTWEIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetFontWeight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_HEIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetHeight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LEFT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetLeft(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LETTERSPACING:
      {
        nsAutoString prop;
        if (NS_OK == a->GetLetterSpacing(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LINEHEIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetLineHeight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetListStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLEIMAGE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetListStyleImage(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLEPOSITION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetListStylePosition(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLETYPE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetListStyleType(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARGIN:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMargin(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARGINTOP:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarginTop(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARGINRIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarginRight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARGINBOTTOM:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarginBottom(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARGINLEFT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarginLeft(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARKEROFFSET:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarkerOffset(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MARKS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMarks(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MAXHEIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMaxHeight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MAXWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMaxWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MINHEIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMinHeight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_MINWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetMinWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_ORPHANS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOrphans(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOutline(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINECOLOR:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOutlineColor(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINESTYLE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOutlineStyle(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINEWIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOutlineWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OVERFLOW:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOverflow(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PADDING:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPadding(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGTOP:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPaddingTop(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGRIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPaddingRight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGBOTTOM:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPaddingBottom(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGLEFT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPaddingLeft(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAGE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPage(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKAFTER:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPageBreakAfter(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKBEFORE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPageBreakBefore(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKINSIDE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPageBreakInside(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAUSE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPause(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAUSEAFTER:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPauseAfter(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PAUSEBEFORE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPauseBefore(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PITCH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPitch(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PITCHRANGE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPitchRange(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_PLAYDURING:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPlayDuring(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_POSITION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetPosition(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_QUOTES:
      {
        nsAutoString prop;
        if (NS_OK == a->GetQuotes(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_RICHNESS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRichness(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_RIGHT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetRight(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SIZE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSize(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SPEAK:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSpeak(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKHEADER:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSpeakHeader(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKNUMERAL:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSpeakNumeral(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKPUNCTUATION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSpeakPunctuation(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_SPEECHRATE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetSpeechRate(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_STRESS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetStress(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TABLELAYOUT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTableLayout(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TEXTALIGN:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTextAlign(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TEXTDECORATION:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTextDecoration(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TEXTINDENT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTextIndent(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TEXTSHADOW:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTextShadow(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TEXTTRANSFORM:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTextTransform(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_TOP:
      {
        nsAutoString prop;
        if (NS_OK == a->GetTop(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_UNICODEBIDI:
      {
        nsAutoString prop;
        if (NS_OK == a->GetUnicodeBidi(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_VERTICALALIGN:
      {
        nsAutoString prop;
        if (NS_OK == a->GetVerticalAlign(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_VISIBILITY:
      {
        nsAutoString prop;
        if (NS_OK == a->GetVisibility(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_VOICEFAMILY:
      {
        nsAutoString prop;
        if (NS_OK == a->GetVoiceFamily(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_VOLUME:
      {
        nsAutoString prop;
        if (NS_OK == a->GetVolume(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_WHITESPACE:
      {
        nsAutoString prop;
        if (NS_OK == a->GetWhiteSpace(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_WIDOWS:
      {
        nsAutoString prop;
        if (NS_OK == a->GetWidows(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_WIDTH:
      {
        nsAutoString prop;
        if (NS_OK == a->GetWidth(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_WORDSPACING:
      {
        nsAutoString prop;
        if (NS_OK == a->GetWordSpacing(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_ZINDEX:
      {
        nsAutoString prop;
        if (NS_OK == a->GetZIndex(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSSSTYLEDECLARATION_OPACITY:
      {
        nsAutoString prop;
        if (NS_OK == a->GetOpacity(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
      {
        nsAutoString prop;
        if (NS_OK == a->Item(JSVAL_TO_INT(id), prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
      }
    }
  }
  else {
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      PRBool rval;
      rval =  object->GetProperty(cx, id, vp);
      NS_RELEASE(object);
      return rval;
    }
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// CSSStyleDeclaration Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSSStyleDeclarationProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSSStyleDeclaration *a = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSSSTYLEDECLARATION_AZIMUTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetAzimuth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUND:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackground(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDATTACHMENT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackgroundAttachment(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackgroundColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDIMAGE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackgroundImage(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDPOSITION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackgroundPosition(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BACKGROUNDREPEAT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBackgroundRepeat(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDER:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorder(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERCOLLAPSE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderCollapse(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERSPACING:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderSpacing(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOP:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderTop(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderRight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOM:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderBottom(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderLeft(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderTopColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderRightColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderBottomColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTCOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderLeftColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderTopStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderRightStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderBottomStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderLeftStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERTOPWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderTopWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERRIGHTWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderRightWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERBOTTOMWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderBottomWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERLEFTWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderLeftWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BORDERWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBorderWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_BOTTOM:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetBottom(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CAPTIONSIDE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCaptionSide(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CLEAR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetClear(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CLIP:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetClip(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_COLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CONTENT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetContent(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_COUNTERINCREMENT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCounterIncrement(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_COUNTERRESET:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCounterReset(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CUE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCue(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CUEAFTER:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCueAfter(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CUEBEFORE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCueBefore(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_CURSOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCursor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_DIRECTION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetDirection(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_DISPLAY:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetDisplay(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_ELEVATION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetElevation(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_EMPTYCELLS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetEmptyCells(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_STYLEFLOAT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetStyleFloat(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFont(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTFAMILY:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontFamily(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTSIZE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontSize(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTSIZEADJUST:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontSizeAdjust(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTSTRETCH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontStretch(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTVARIANT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontVariant(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_FONTWEIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetFontWeight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_HEIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetHeight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LEFT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetLeft(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LETTERSPACING:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetLetterSpacing(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LINEHEIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetLineHeight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetListStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLEIMAGE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetListStyleImage(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLEPOSITION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetListStylePosition(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_LISTSTYLETYPE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetListStyleType(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARGIN:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMargin(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARGINTOP:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarginTop(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARGINRIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarginRight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARGINBOTTOM:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarginBottom(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARGINLEFT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarginLeft(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARKEROFFSET:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarkerOffset(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MARKS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMarks(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MAXHEIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMaxHeight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MAXWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMaxWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MINHEIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMinHeight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_MINWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetMinWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_ORPHANS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOrphans(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOutline(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINECOLOR:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOutlineColor(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINESTYLE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOutlineStyle(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OUTLINEWIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOutlineWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OVERFLOW:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOverflow(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PADDING:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPadding(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGTOP:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPaddingTop(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGRIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPaddingRight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGBOTTOM:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPaddingBottom(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PADDINGLEFT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPaddingLeft(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAGE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPage(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKAFTER:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPageBreakAfter(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKBEFORE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPageBreakBefore(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAGEBREAKINSIDE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPageBreakInside(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAUSE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPause(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAUSEAFTER:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPauseAfter(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PAUSEBEFORE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPauseBefore(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PITCH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPitch(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PITCHRANGE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPitchRange(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_PLAYDURING:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPlayDuring(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_POSITION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetPosition(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_QUOTES:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetQuotes(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_RICHNESS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetRichness(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_RIGHT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetRight(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SIZE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSize(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SPEAK:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSpeak(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKHEADER:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSpeakHeader(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKNUMERAL:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSpeakNumeral(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SPEAKPUNCTUATION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSpeakPunctuation(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_SPEECHRATE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetSpeechRate(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_STRESS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetStress(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TABLELAYOUT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTableLayout(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TEXTALIGN:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTextAlign(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TEXTDECORATION:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTextDecoration(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TEXTINDENT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTextIndent(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TEXTSHADOW:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTextShadow(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TEXTTRANSFORM:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTextTransform(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_TOP:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetTop(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_UNICODEBIDI:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetUnicodeBidi(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_VERTICALALIGN:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetVerticalAlign(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_VISIBILITY:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetVisibility(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_VOICEFAMILY:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetVoiceFamily(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_VOLUME:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetVolume(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_WHITESPACE:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetWhiteSpace(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_WIDOWS:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetWidows(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_WIDTH:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetWidth(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_WORDSPACING:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetWordSpacing(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_ZINDEX:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetZIndex(prop);
        
        break;
      }
      case CSSSTYLEDECLARATION_OPACITY:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetOpacity(prop);
        
        break;
      }
      default:
      {
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->SetProperty(cx, id, vp);
          NS_RELEASE(object);
          return rval;
        }
      }
    }
  }
  else {
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      PRBool rval;
      rval =  object->SetProperty(cx, id, vp);
      NS_RELEASE(object);
      return rval;
    }
  }

  return PR_TRUE;
}


//
// CSSStyleDeclaration finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSSStyleDeclaration(JSContext *cx, JSObject *obj)
{
  nsIDOMCSSStyleDeclaration *a = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIScriptObjectOwner *owner = nsnull;
    if (NS_OK == a->QueryInterface(kIScriptObjectOwnerIID, (void**)&owner)) {
      owner->SetScriptObject(nsnull);
      NS_RELEASE(owner);
    }

    NS_RELEASE(a);
  }
}


//
// CSSStyleDeclaration enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSSStyleDeclaration(JSContext *cx, JSObject *obj)
{
  nsIDOMCSSStyleDeclaration *a = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->EnumerateProperty(cx);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


//
// CSSStyleDeclaration resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSSStyleDeclaration(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMCSSStyleDeclaration *a = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  
  if (nsnull != a) {
    // get the js object
    nsIJSScriptObject *object;
    if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
      object->Resolve(cx, id);
      NS_RELEASE(object);
    }
  }
  return JS_TRUE;
}


//
// Native method GetPropertyValue
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleDeclarationGetPropertyValue(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleDeclaration *nativeThis = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->GetPropertyValue(b0, nativeRet)) {
      return JS_FALSE;
    }

    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }
  else {
    JS_ReportError(cx, "Function getPropertyValue requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method GetPropertyPriority
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleDeclarationGetPropertyPriority(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleDeclaration *nativeThis = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;
  nsAutoString b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->GetPropertyPriority(b0, nativeRet)) {
      return JS_FALSE;
    }

    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }
  else {
    JS_ReportError(cx, "Function getPropertyPriority requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method SetProperty
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleDeclarationSetProperty(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleDeclaration *nativeThis = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString b0;
  nsAutoString b1;
  nsAutoString b2;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 3) {

    JSString *jsstring0 = JS_ValueToString(cx, argv[0]);
    if (nsnull != jsstring0) {
      b0.SetString(JS_GetStringChars(jsstring0));
    }
    else {
      b0.SetString("");   // Should this really be null?? 
    }

    JSString *jsstring1 = JS_ValueToString(cx, argv[1]);
    if (nsnull != jsstring1) {
      b1.SetString(JS_GetStringChars(jsstring1));
    }
    else {
      b1.SetString("");   // Should this really be null?? 
    }

    JSString *jsstring2 = JS_ValueToString(cx, argv[2]);
    if (nsnull != jsstring2) {
      b2.SetString(JS_GetStringChars(jsstring2));
    }
    else {
      b2.SetString("");   // Should this really be null?? 
    }

    if (NS_OK != nativeThis->SetProperty(b0, b1, b2)) {
      return JS_FALSE;
    }

    *rval = JSVAL_VOID;
  }
  else {
    JS_ReportError(cx, "Function setProperty requires 3 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


//
// Native method Item
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleDeclarationItem(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMCSSStyleDeclaration *nativeThis = (nsIDOMCSSStyleDeclaration*)JS_GetPrivate(cx, obj);
  JSBool rBool = JS_FALSE;
  nsAutoString nativeRet;
  PRUint32 b0;

  *rval = JSVAL_NULL;

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  if (argc >= 1) {

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      JS_ReportError(cx, "Parameter must be a number");
      return JS_FALSE;
    }

    if (NS_OK != nativeThis->Item(b0, nativeRet)) {
      return JS_FALSE;
    }

    JSString *jsstring = JS_NewUCStringCopyN(cx, nativeRet, nativeRet.Length());
    // set the return value
    *rval = STRING_TO_JSVAL(jsstring);
  }
  else {
    JS_ReportError(cx, "Function item requires 1 parameters");
    return JS_FALSE;
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for CSSStyleDeclaration
//
JSClass CSSStyleDeclarationClass = {
  "CSSStyleDeclaration", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSSStyleDeclarationProperty,
  SetCSSStyleDeclarationProperty,
  EnumerateCSSStyleDeclaration,
  ResolveCSSStyleDeclaration,
  JS_ConvertStub,
  FinalizeCSSStyleDeclaration
};


//
// CSSStyleDeclaration class properties
//
static JSPropertySpec CSSStyleDeclarationProperties[] =
{
  {"length",    CSSSTYLEDECLARATION_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"azimuth",    CSSSTYLEDECLARATION_AZIMUTH,    JSPROP_ENUMERATE},
  {"background",    CSSSTYLEDECLARATION_BACKGROUND,    JSPROP_ENUMERATE},
  {"backgroundAttachment",    CSSSTYLEDECLARATION_BACKGROUNDATTACHMENT,    JSPROP_ENUMERATE},
  {"backgroundColor",    CSSSTYLEDECLARATION_BACKGROUNDCOLOR,    JSPROP_ENUMERATE},
  {"backgroundImage",    CSSSTYLEDECLARATION_BACKGROUNDIMAGE,    JSPROP_ENUMERATE},
  {"backgroundPosition",    CSSSTYLEDECLARATION_BACKGROUNDPOSITION,    JSPROP_ENUMERATE},
  {"backgroundRepeat",    CSSSTYLEDECLARATION_BACKGROUNDREPEAT,    JSPROP_ENUMERATE},
  {"border",    CSSSTYLEDECLARATION_BORDER,    JSPROP_ENUMERATE},
  {"borderCollapse",    CSSSTYLEDECLARATION_BORDERCOLLAPSE,    JSPROP_ENUMERATE},
  {"borderColor",    CSSSTYLEDECLARATION_BORDERCOLOR,    JSPROP_ENUMERATE},
  {"borderSpacing",    CSSSTYLEDECLARATION_BORDERSPACING,    JSPROP_ENUMERATE},
  {"borderStyle",    CSSSTYLEDECLARATION_BORDERSTYLE,    JSPROP_ENUMERATE},
  {"borderTop",    CSSSTYLEDECLARATION_BORDERTOP,    JSPROP_ENUMERATE},
  {"borderRight",    CSSSTYLEDECLARATION_BORDERRIGHT,    JSPROP_ENUMERATE},
  {"borderBottom",    CSSSTYLEDECLARATION_BORDERBOTTOM,    JSPROP_ENUMERATE},
  {"borderLeft",    CSSSTYLEDECLARATION_BORDERLEFT,    JSPROP_ENUMERATE},
  {"borderTopColor",    CSSSTYLEDECLARATION_BORDERTOPCOLOR,    JSPROP_ENUMERATE},
  {"borderRightColor",    CSSSTYLEDECLARATION_BORDERRIGHTCOLOR,    JSPROP_ENUMERATE},
  {"borderBottomColor",    CSSSTYLEDECLARATION_BORDERBOTTOMCOLOR,    JSPROP_ENUMERATE},
  {"borderLeftColor",    CSSSTYLEDECLARATION_BORDERLEFTCOLOR,    JSPROP_ENUMERATE},
  {"borderTopStyle",    CSSSTYLEDECLARATION_BORDERTOPSTYLE,    JSPROP_ENUMERATE},
  {"borderRightStyle",    CSSSTYLEDECLARATION_BORDERRIGHTSTYLE,    JSPROP_ENUMERATE},
  {"borderBottomStyle",    CSSSTYLEDECLARATION_BORDERBOTTOMSTYLE,    JSPROP_ENUMERATE},
  {"borderLeftStyle",    CSSSTYLEDECLARATION_BORDERLEFTSTYLE,    JSPROP_ENUMERATE},
  {"borderTopWidth",    CSSSTYLEDECLARATION_BORDERTOPWIDTH,    JSPROP_ENUMERATE},
  {"borderRightWidth",    CSSSTYLEDECLARATION_BORDERRIGHTWIDTH,    JSPROP_ENUMERATE},
  {"borderBottomWidth",    CSSSTYLEDECLARATION_BORDERBOTTOMWIDTH,    JSPROP_ENUMERATE},
  {"borderLeftWidth",    CSSSTYLEDECLARATION_BORDERLEFTWIDTH,    JSPROP_ENUMERATE},
  {"borderWidth",    CSSSTYLEDECLARATION_BORDERWIDTH,    JSPROP_ENUMERATE},
  {"bottom",    CSSSTYLEDECLARATION_BOTTOM,    JSPROP_ENUMERATE},
  {"captionSide",    CSSSTYLEDECLARATION_CAPTIONSIDE,    JSPROP_ENUMERATE},
  {"clear",    CSSSTYLEDECLARATION_CLEAR,    JSPROP_ENUMERATE},
  {"clip",    CSSSTYLEDECLARATION_CLIP,    JSPROP_ENUMERATE},
  {"color",    CSSSTYLEDECLARATION_COLOR,    JSPROP_ENUMERATE},
  {"content",    CSSSTYLEDECLARATION_CONTENT,    JSPROP_ENUMERATE},
  {"counterIncrement",    CSSSTYLEDECLARATION_COUNTERINCREMENT,    JSPROP_ENUMERATE},
  {"counterReset",    CSSSTYLEDECLARATION_COUNTERRESET,    JSPROP_ENUMERATE},
  {"cue",    CSSSTYLEDECLARATION_CUE,    JSPROP_ENUMERATE},
  {"cueAfter",    CSSSTYLEDECLARATION_CUEAFTER,    JSPROP_ENUMERATE},
  {"cueBefore",    CSSSTYLEDECLARATION_CUEBEFORE,    JSPROP_ENUMERATE},
  {"cursor",    CSSSTYLEDECLARATION_CURSOR,    JSPROP_ENUMERATE},
  {"direction",    CSSSTYLEDECLARATION_DIRECTION,    JSPROP_ENUMERATE},
  {"display",    CSSSTYLEDECLARATION_DISPLAY,    JSPROP_ENUMERATE},
  {"elevation",    CSSSTYLEDECLARATION_ELEVATION,    JSPROP_ENUMERATE},
  {"emptyCells",    CSSSTYLEDECLARATION_EMPTYCELLS,    JSPROP_ENUMERATE},
  {"styleFloat",    CSSSTYLEDECLARATION_STYLEFLOAT,    JSPROP_ENUMERATE},
  {"font",    CSSSTYLEDECLARATION_FONT,    JSPROP_ENUMERATE},
  {"fontFamily",    CSSSTYLEDECLARATION_FONTFAMILY,    JSPROP_ENUMERATE},
  {"fontSize",    CSSSTYLEDECLARATION_FONTSIZE,    JSPROP_ENUMERATE},
  {"fontSizeAdjust",    CSSSTYLEDECLARATION_FONTSIZEADJUST,    JSPROP_ENUMERATE},
  {"fontStretch",    CSSSTYLEDECLARATION_FONTSTRETCH,    JSPROP_ENUMERATE},
  {"fontStyle",    CSSSTYLEDECLARATION_FONTSTYLE,    JSPROP_ENUMERATE},
  {"fontVariant",    CSSSTYLEDECLARATION_FONTVARIANT,    JSPROP_ENUMERATE},
  {"fontWeight",    CSSSTYLEDECLARATION_FONTWEIGHT,    JSPROP_ENUMERATE},
  {"height",    CSSSTYLEDECLARATION_HEIGHT,    JSPROP_ENUMERATE},
  {"left",    CSSSTYLEDECLARATION_LEFT,    JSPROP_ENUMERATE},
  {"letterSpacing",    CSSSTYLEDECLARATION_LETTERSPACING,    JSPROP_ENUMERATE},
  {"lineHeight",    CSSSTYLEDECLARATION_LINEHEIGHT,    JSPROP_ENUMERATE},
  {"listStyle",    CSSSTYLEDECLARATION_LISTSTYLE,    JSPROP_ENUMERATE},
  {"listStyleImage",    CSSSTYLEDECLARATION_LISTSTYLEIMAGE,    JSPROP_ENUMERATE},
  {"listStylePosition",    CSSSTYLEDECLARATION_LISTSTYLEPOSITION,    JSPROP_ENUMERATE},
  {"listStyleType",    CSSSTYLEDECLARATION_LISTSTYLETYPE,    JSPROP_ENUMERATE},
  {"margin",    CSSSTYLEDECLARATION_MARGIN,    JSPROP_ENUMERATE},
  {"marginTop",    CSSSTYLEDECLARATION_MARGINTOP,    JSPROP_ENUMERATE},
  {"marginRight",    CSSSTYLEDECLARATION_MARGINRIGHT,    JSPROP_ENUMERATE},
  {"marginBottom",    CSSSTYLEDECLARATION_MARGINBOTTOM,    JSPROP_ENUMERATE},
  {"marginLeft",    CSSSTYLEDECLARATION_MARGINLEFT,    JSPROP_ENUMERATE},
  {"markerOffset",    CSSSTYLEDECLARATION_MARKEROFFSET,    JSPROP_ENUMERATE},
  {"marks",    CSSSTYLEDECLARATION_MARKS,    JSPROP_ENUMERATE},
  {"maxHeight",    CSSSTYLEDECLARATION_MAXHEIGHT,    JSPROP_ENUMERATE},
  {"maxWidth",    CSSSTYLEDECLARATION_MAXWIDTH,    JSPROP_ENUMERATE},
  {"minHeight",    CSSSTYLEDECLARATION_MINHEIGHT,    JSPROP_ENUMERATE},
  {"minWidth",    CSSSTYLEDECLARATION_MINWIDTH,    JSPROP_ENUMERATE},
  {"orphans",    CSSSTYLEDECLARATION_ORPHANS,    JSPROP_ENUMERATE},
  {"outline",    CSSSTYLEDECLARATION_OUTLINE,    JSPROP_ENUMERATE},
  {"outlineColor",    CSSSTYLEDECLARATION_OUTLINECOLOR,    JSPROP_ENUMERATE},
  {"outlineStyle",    CSSSTYLEDECLARATION_OUTLINESTYLE,    JSPROP_ENUMERATE},
  {"outlineWidth",    CSSSTYLEDECLARATION_OUTLINEWIDTH,    JSPROP_ENUMERATE},
  {"overflow",    CSSSTYLEDECLARATION_OVERFLOW,    JSPROP_ENUMERATE},
  {"padding",    CSSSTYLEDECLARATION_PADDING,    JSPROP_ENUMERATE},
  {"paddingTop",    CSSSTYLEDECLARATION_PADDINGTOP,    JSPROP_ENUMERATE},
  {"paddingRight",    CSSSTYLEDECLARATION_PADDINGRIGHT,    JSPROP_ENUMERATE},
  {"paddingBottom",    CSSSTYLEDECLARATION_PADDINGBOTTOM,    JSPROP_ENUMERATE},
  {"paddingLeft",    CSSSTYLEDECLARATION_PADDINGLEFT,    JSPROP_ENUMERATE},
  {"page",    CSSSTYLEDECLARATION_PAGE,    JSPROP_ENUMERATE},
  {"pageBreakAfter",    CSSSTYLEDECLARATION_PAGEBREAKAFTER,    JSPROP_ENUMERATE},
  {"pageBreakBefore",    CSSSTYLEDECLARATION_PAGEBREAKBEFORE,    JSPROP_ENUMERATE},
  {"pageBreakInside",    CSSSTYLEDECLARATION_PAGEBREAKINSIDE,    JSPROP_ENUMERATE},
  {"pause",    CSSSTYLEDECLARATION_PAUSE,    JSPROP_ENUMERATE},
  {"pauseAfter",    CSSSTYLEDECLARATION_PAUSEAFTER,    JSPROP_ENUMERATE},
  {"pauseBefore",    CSSSTYLEDECLARATION_PAUSEBEFORE,    JSPROP_ENUMERATE},
  {"pitch",    CSSSTYLEDECLARATION_PITCH,    JSPROP_ENUMERATE},
  {"pitchRange",    CSSSTYLEDECLARATION_PITCHRANGE,    JSPROP_ENUMERATE},
  {"playDuring",    CSSSTYLEDECLARATION_PLAYDURING,    JSPROP_ENUMERATE},
  {"position",    CSSSTYLEDECLARATION_POSITION,    JSPROP_ENUMERATE},
  {"quotes",    CSSSTYLEDECLARATION_QUOTES,    JSPROP_ENUMERATE},
  {"richness",    CSSSTYLEDECLARATION_RICHNESS,    JSPROP_ENUMERATE},
  {"right",    CSSSTYLEDECLARATION_RIGHT,    JSPROP_ENUMERATE},
  {"size",    CSSSTYLEDECLARATION_SIZE,    JSPROP_ENUMERATE},
  {"speak",    CSSSTYLEDECLARATION_SPEAK,    JSPROP_ENUMERATE},
  {"speakHeader",    CSSSTYLEDECLARATION_SPEAKHEADER,    JSPROP_ENUMERATE},
  {"speakNumeral",    CSSSTYLEDECLARATION_SPEAKNUMERAL,    JSPROP_ENUMERATE},
  {"speakPunctuation",    CSSSTYLEDECLARATION_SPEAKPUNCTUATION,    JSPROP_ENUMERATE},
  {"speechRate",    CSSSTYLEDECLARATION_SPEECHRATE,    JSPROP_ENUMERATE},
  {"stress",    CSSSTYLEDECLARATION_STRESS,    JSPROP_ENUMERATE},
  {"tableLayout",    CSSSTYLEDECLARATION_TABLELAYOUT,    JSPROP_ENUMERATE},
  {"textAlign",    CSSSTYLEDECLARATION_TEXTALIGN,    JSPROP_ENUMERATE},
  {"textDecoration",    CSSSTYLEDECLARATION_TEXTDECORATION,    JSPROP_ENUMERATE},
  {"textIndent",    CSSSTYLEDECLARATION_TEXTINDENT,    JSPROP_ENUMERATE},
  {"textShadow",    CSSSTYLEDECLARATION_TEXTSHADOW,    JSPROP_ENUMERATE},
  {"textTransform",    CSSSTYLEDECLARATION_TEXTTRANSFORM,    JSPROP_ENUMERATE},
  {"top",    CSSSTYLEDECLARATION_TOP,    JSPROP_ENUMERATE},
  {"unicodeBidi",    CSSSTYLEDECLARATION_UNICODEBIDI,    JSPROP_ENUMERATE},
  {"verticalAlign",    CSSSTYLEDECLARATION_VERTICALALIGN,    JSPROP_ENUMERATE},
  {"visibility",    CSSSTYLEDECLARATION_VISIBILITY,    JSPROP_ENUMERATE},
  {"voiceFamily",    CSSSTYLEDECLARATION_VOICEFAMILY,    JSPROP_ENUMERATE},
  {"volume",    CSSSTYLEDECLARATION_VOLUME,    JSPROP_ENUMERATE},
  {"whiteSpace",    CSSSTYLEDECLARATION_WHITESPACE,    JSPROP_ENUMERATE},
  {"widows",    CSSSTYLEDECLARATION_WIDOWS,    JSPROP_ENUMERATE},
  {"width",    CSSSTYLEDECLARATION_WIDTH,    JSPROP_ENUMERATE},
  {"wordSpacing",    CSSSTYLEDECLARATION_WORDSPACING,    JSPROP_ENUMERATE},
  {"zIndex",    CSSSTYLEDECLARATION_ZINDEX,    JSPROP_ENUMERATE},
  {"opacity",    CSSSTYLEDECLARATION_OPACITY,    JSPROP_ENUMERATE},
  {0}
};


//
// CSSStyleDeclaration class methods
//
static JSFunctionSpec CSSStyleDeclarationMethods[] = 
{
  {"getPropertyValue",          CSSStyleDeclarationGetPropertyValue,     1},
  {"getPropertyPriority",          CSSStyleDeclarationGetPropertyPriority,     1},
  {"setProperty",          CSSStyleDeclarationSetProperty,     3},
  {"item",          CSSStyleDeclarationItem,     1},
  {0}
};


//
// CSSStyleDeclaration constructor
//
PR_STATIC_CALLBACK(JSBool)
CSSStyleDeclaration(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  return JS_FALSE;
}


//
// CSSStyleDeclaration class initialization
//
nsresult NS_InitCSSStyleDeclarationClass(nsIScriptContext *aContext, void **aPrototype)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "CSSStyleDeclaration", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {

    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &CSSStyleDeclarationClass,      // JSClass
                         CSSStyleDeclaration,            // JSNative ctor
                         0,             // ctor args
                         CSSStyleDeclarationProperties,  // proto props
                         CSSStyleDeclarationMethods,     // proto funcs
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
// Method for creating a new CSSStyleDeclaration JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptCSSStyleDeclaration(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null argument to NS_NewScriptCSSStyleDeclaration");
  JSObject *proto;
  JSObject *parent;
  nsIScriptObjectOwner *owner;
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  nsresult result = NS_OK;
  nsIDOMCSSStyleDeclaration *aCSSStyleDeclaration;

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

  if (NS_OK != NS_InitCSSStyleDeclarationClass(aContext, (void **)&proto)) {
    return NS_ERROR_FAILURE;
  }

  result = aSupports->QueryInterface(kICSSStyleDeclarationIID, (void **)&aCSSStyleDeclaration);
  if (NS_OK != result) {
    return result;
  }

  // create a js object for this class
  *aReturn = JS_NewObject(jscontext, &CSSStyleDeclarationClass, proto, parent);
  if (nsnull != *aReturn) {
    // connect the native object to the js object
    JS_SetPrivate(jscontext, (JSObject *)*aReturn, aCSSStyleDeclaration);
  }
  else {
    NS_RELEASE(aCSSStyleDeclaration);
    return NS_ERROR_FAILURE; 
  }

  return NS_OK;
}
