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
#include "nsIDOMCSS2Properties.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kICSS2PropertiesIID, NS_IDOMCSS2PROPERTIES_IID);

NS_DEF_PTR(nsIDOMCSS2Properties);

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
  CSS2PROPERTIES_OPACITY = -123
};

/***********************************************************************/
//
// CSS2Properties Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetCSS2PropertiesProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
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
      case CSS2PROPERTIES_BACKGROUND:
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
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
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
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
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
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
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
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
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
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
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
      case CSS2PROPERTIES_BORDER:
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
      case CSS2PROPERTIES_BORDERCOLLAPSE:
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
      case CSS2PROPERTIES_BORDERCOLOR:
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
      case CSS2PROPERTIES_BORDERSPACING:
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
      case CSS2PROPERTIES_BORDERSTYLE:
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
      case CSS2PROPERTIES_BORDERTOP:
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
      case CSS2PROPERTIES_BORDERRIGHT:
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
      case CSS2PROPERTIES_BORDERBOTTOM:
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
      case CSS2PROPERTIES_BORDERLEFT:
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
      case CSS2PROPERTIES_BORDERTOPCOLOR:
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
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
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
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
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
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
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
      case CSS2PROPERTIES_BORDERTOPSTYLE:
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
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
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
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
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
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
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
      case CSS2PROPERTIES_BORDERTOPWIDTH:
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
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
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
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
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
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
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
      case CSS2PROPERTIES_BORDERWIDTH:
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
      case CSS2PROPERTIES_BOTTOM:
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
      case CSS2PROPERTIES_CAPTIONSIDE:
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
      case CSS2PROPERTIES_CLEAR:
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
      case CSS2PROPERTIES_CLIP:
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
      case CSS2PROPERTIES_COLOR:
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
      case CSS2PROPERTIES_CONTENT:
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
      case CSS2PROPERTIES_COUNTERINCREMENT:
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
      case CSS2PROPERTIES_COUNTERRESET:
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
      case CSS2PROPERTIES_CUE:
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
      case CSS2PROPERTIES_CUEAFTER:
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
      case CSS2PROPERTIES_CUEBEFORE:
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
      case CSS2PROPERTIES_CURSOR:
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
      case CSS2PROPERTIES_DIRECTION:
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
      case CSS2PROPERTIES_DISPLAY:
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
      case CSS2PROPERTIES_ELEVATION:
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
      case CSS2PROPERTIES_EMPTYCELLS:
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
      case CSS2PROPERTIES_CSSFLOAT:
      {
        nsAutoString prop;
        if (NS_OK == a->GetCssFloat(prop)) {
          JSString *jsstring = JS_NewUCStringCopyN(cx, prop, prop.Length());
          // set the return value
          *vp = STRING_TO_JSVAL(jsstring);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONT:
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
      case CSS2PROPERTIES_FONTFAMILY:
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
      case CSS2PROPERTIES_FONTSIZE:
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
      case CSS2PROPERTIES_FONTSIZEADJUST:
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
      case CSS2PROPERTIES_FONTSTRETCH:
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
      case CSS2PROPERTIES_FONTSTYLE:
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
      case CSS2PROPERTIES_FONTVARIANT:
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
      case CSS2PROPERTIES_FONTWEIGHT:
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
      case CSS2PROPERTIES_HEIGHT:
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
      case CSS2PROPERTIES_LEFT:
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
      case CSS2PROPERTIES_LETTERSPACING:
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
      case CSS2PROPERTIES_LINEHEIGHT:
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
      case CSS2PROPERTIES_LISTSTYLE:
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
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
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
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
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
      case CSS2PROPERTIES_LISTSTYLETYPE:
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
      case CSS2PROPERTIES_MARGIN:
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
      case CSS2PROPERTIES_MARGINTOP:
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
      case CSS2PROPERTIES_MARGINRIGHT:
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
      case CSS2PROPERTIES_MARGINBOTTOM:
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
      case CSS2PROPERTIES_MARGINLEFT:
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
      case CSS2PROPERTIES_MARKEROFFSET:
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
      case CSS2PROPERTIES_MARKS:
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
      case CSS2PROPERTIES_MAXHEIGHT:
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
      case CSS2PROPERTIES_MAXWIDTH:
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
      case CSS2PROPERTIES_MINHEIGHT:
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
      case CSS2PROPERTIES_MINWIDTH:
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
      case CSS2PROPERTIES_ORPHANS:
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
      case CSS2PROPERTIES_OUTLINE:
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
      case CSS2PROPERTIES_OUTLINECOLOR:
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
      case CSS2PROPERTIES_OUTLINESTYLE:
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
      case CSS2PROPERTIES_OUTLINEWIDTH:
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
      case CSS2PROPERTIES_OVERFLOW:
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
      case CSS2PROPERTIES_PADDING:
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
      case CSS2PROPERTIES_PADDINGTOP:
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
      case CSS2PROPERTIES_PADDINGRIGHT:
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
      case CSS2PROPERTIES_PADDINGBOTTOM:
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
      case CSS2PROPERTIES_PADDINGLEFT:
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
      case CSS2PROPERTIES_PAGE:
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
      case CSS2PROPERTIES_PAGEBREAKAFTER:
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
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
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
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
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
      case CSS2PROPERTIES_PAUSE:
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
      case CSS2PROPERTIES_PAUSEAFTER:
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
      case CSS2PROPERTIES_PAUSEBEFORE:
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
      case CSS2PROPERTIES_PITCH:
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
      case CSS2PROPERTIES_PITCHRANGE:
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
      case CSS2PROPERTIES_PLAYDURING:
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
      case CSS2PROPERTIES_POSITION:
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
      case CSS2PROPERTIES_QUOTES:
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
      case CSS2PROPERTIES_RICHNESS:
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
      case CSS2PROPERTIES_RIGHT:
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
      case CSS2PROPERTIES_SIZE:
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
      case CSS2PROPERTIES_SPEAK:
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
      case CSS2PROPERTIES_SPEAKHEADER:
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
      case CSS2PROPERTIES_SPEAKNUMERAL:
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
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
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
      case CSS2PROPERTIES_SPEECHRATE:
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
      case CSS2PROPERTIES_STRESS:
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
      case CSS2PROPERTIES_TABLELAYOUT:
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
      case CSS2PROPERTIES_TEXTALIGN:
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
      case CSS2PROPERTIES_TEXTDECORATION:
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
      case CSS2PROPERTIES_TEXTINDENT:
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
      case CSS2PROPERTIES_TEXTSHADOW:
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
      case CSS2PROPERTIES_TEXTTRANSFORM:
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
      case CSS2PROPERTIES_TOP:
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
      case CSS2PROPERTIES_UNICODEBIDI:
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
      case CSS2PROPERTIES_VERTICALALIGN:
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
      case CSS2PROPERTIES_VISIBILITY:
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
      case CSS2PROPERTIES_VOICEFAMILY:
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
      case CSS2PROPERTIES_VOLUME:
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
      case CSS2PROPERTIES_WHITESPACE:
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
      case CSS2PROPERTIES_WIDOWS:
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
      case CSS2PROPERTIES_WIDTH:
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
      case CSS2PROPERTIES_WORDSPACING:
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
      case CSS2PROPERTIES_ZINDEX:
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
      case CSS2PROPERTIES_OPACITY:
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
        nsIJSScriptObject *object;
        if (NS_OK == a->QueryInterface(kIJSScriptObjectIID, (void**)&object)) {
          PRBool rval;
          rval =  object->GetProperty(cx, id, vp);
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
      rval =  object->GetProperty(cx, id, vp);
      NS_RELEASE(object);
      return rval;
    }
  }

  return PR_TRUE;
}

/***********************************************************************/
//
// CSS2Properties Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetCSS2PropertiesProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)JS_GetPrivate(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
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
      case CSS2PROPERTIES_BACKGROUND:
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
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
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
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
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
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
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
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
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
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
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
      case CSS2PROPERTIES_BORDER:
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
      case CSS2PROPERTIES_BORDERCOLLAPSE:
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
      case CSS2PROPERTIES_BORDERCOLOR:
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
      case CSS2PROPERTIES_BORDERSPACING:
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
      case CSS2PROPERTIES_BORDERSTYLE:
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
      case CSS2PROPERTIES_BORDERTOP:
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
      case CSS2PROPERTIES_BORDERRIGHT:
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
      case CSS2PROPERTIES_BORDERBOTTOM:
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
      case CSS2PROPERTIES_BORDERLEFT:
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
      case CSS2PROPERTIES_BORDERTOPCOLOR:
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
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
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
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
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
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
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
      case CSS2PROPERTIES_BORDERTOPSTYLE:
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
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
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
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
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
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
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
      case CSS2PROPERTIES_BORDERTOPWIDTH:
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
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
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
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
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
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
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
      case CSS2PROPERTIES_BORDERWIDTH:
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
      case CSS2PROPERTIES_BOTTOM:
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
      case CSS2PROPERTIES_CAPTIONSIDE:
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
      case CSS2PROPERTIES_CLEAR:
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
      case CSS2PROPERTIES_CLIP:
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
      case CSS2PROPERTIES_COLOR:
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
      case CSS2PROPERTIES_CONTENT:
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
      case CSS2PROPERTIES_COUNTERINCREMENT:
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
      case CSS2PROPERTIES_COUNTERRESET:
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
      case CSS2PROPERTIES_CUE:
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
      case CSS2PROPERTIES_CUEAFTER:
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
      case CSS2PROPERTIES_CUEBEFORE:
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
      case CSS2PROPERTIES_CURSOR:
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
      case CSS2PROPERTIES_DIRECTION:
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
      case CSS2PROPERTIES_DISPLAY:
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
      case CSS2PROPERTIES_ELEVATION:
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
      case CSS2PROPERTIES_EMPTYCELLS:
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
      case CSS2PROPERTIES_CSSFLOAT:
      {
        nsAutoString prop;
        JSString *jsstring;
        if ((jsstring = JS_ValueToString(cx, *vp)) != nsnull) {
          prop.SetString(JS_GetStringChars(jsstring));
        }
        else {
          prop.SetString((const char *)nsnull);
        }
      
        a->SetCssFloat(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONT:
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
      case CSS2PROPERTIES_FONTFAMILY:
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
      case CSS2PROPERTIES_FONTSIZE:
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
      case CSS2PROPERTIES_FONTSIZEADJUST:
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
      case CSS2PROPERTIES_FONTSTRETCH:
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
      case CSS2PROPERTIES_FONTSTYLE:
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
      case CSS2PROPERTIES_FONTVARIANT:
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
      case CSS2PROPERTIES_FONTWEIGHT:
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
      case CSS2PROPERTIES_HEIGHT:
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
      case CSS2PROPERTIES_LEFT:
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
      case CSS2PROPERTIES_LETTERSPACING:
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
      case CSS2PROPERTIES_LINEHEIGHT:
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
      case CSS2PROPERTIES_LISTSTYLE:
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
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
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
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
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
      case CSS2PROPERTIES_LISTSTYLETYPE:
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
      case CSS2PROPERTIES_MARGIN:
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
      case CSS2PROPERTIES_MARGINTOP:
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
      case CSS2PROPERTIES_MARGINRIGHT:
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
      case CSS2PROPERTIES_MARGINBOTTOM:
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
      case CSS2PROPERTIES_MARGINLEFT:
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
      case CSS2PROPERTIES_MARKEROFFSET:
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
      case CSS2PROPERTIES_MARKS:
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
      case CSS2PROPERTIES_MAXHEIGHT:
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
      case CSS2PROPERTIES_MAXWIDTH:
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
      case CSS2PROPERTIES_MINHEIGHT:
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
      case CSS2PROPERTIES_MINWIDTH:
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
      case CSS2PROPERTIES_ORPHANS:
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
      case CSS2PROPERTIES_OUTLINE:
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
      case CSS2PROPERTIES_OUTLINECOLOR:
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
      case CSS2PROPERTIES_OUTLINESTYLE:
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
      case CSS2PROPERTIES_OUTLINEWIDTH:
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
      case CSS2PROPERTIES_OVERFLOW:
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
      case CSS2PROPERTIES_PADDING:
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
      case CSS2PROPERTIES_PADDINGTOP:
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
      case CSS2PROPERTIES_PADDINGRIGHT:
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
      case CSS2PROPERTIES_PADDINGBOTTOM:
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
      case CSS2PROPERTIES_PADDINGLEFT:
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
      case CSS2PROPERTIES_PAGE:
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
      case CSS2PROPERTIES_PAGEBREAKAFTER:
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
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
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
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
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
      case CSS2PROPERTIES_PAUSE:
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
      case CSS2PROPERTIES_PAUSEAFTER:
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
      case CSS2PROPERTIES_PAUSEBEFORE:
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
      case CSS2PROPERTIES_PITCH:
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
      case CSS2PROPERTIES_PITCHRANGE:
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
      case CSS2PROPERTIES_PLAYDURING:
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
      case CSS2PROPERTIES_POSITION:
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
      case CSS2PROPERTIES_QUOTES:
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
      case CSS2PROPERTIES_RICHNESS:
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
      case CSS2PROPERTIES_RIGHT:
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
      case CSS2PROPERTIES_SIZE:
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
      case CSS2PROPERTIES_SPEAK:
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
      case CSS2PROPERTIES_SPEAKHEADER:
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
      case CSS2PROPERTIES_SPEAKNUMERAL:
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
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
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
      case CSS2PROPERTIES_SPEECHRATE:
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
      case CSS2PROPERTIES_STRESS:
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
      case CSS2PROPERTIES_TABLELAYOUT:
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
      case CSS2PROPERTIES_TEXTALIGN:
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
      case CSS2PROPERTIES_TEXTDECORATION:
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
      case CSS2PROPERTIES_TEXTINDENT:
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
      case CSS2PROPERTIES_TEXTSHADOW:
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
      case CSS2PROPERTIES_TEXTTRANSFORM:
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
      case CSS2PROPERTIES_TOP:
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
      case CSS2PROPERTIES_UNICODEBIDI:
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
      case CSS2PROPERTIES_VERTICALALIGN:
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
      case CSS2PROPERTIES_VISIBILITY:
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
      case CSS2PROPERTIES_VOICEFAMILY:
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
      case CSS2PROPERTIES_VOLUME:
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
      case CSS2PROPERTIES_WHITESPACE:
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
      case CSS2PROPERTIES_WIDOWS:
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
      case CSS2PROPERTIES_WIDTH:
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
      case CSS2PROPERTIES_WORDSPACING:
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
      case CSS2PROPERTIES_ZINDEX:
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
      case CSS2PROPERTIES_OPACITY:
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
// CSS2Properties finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeCSS2Properties(JSContext *cx, JSObject *obj)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)JS_GetPrivate(cx, obj);
  
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
// CSS2Properties enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateCSS2Properties(JSContext *cx, JSObject *obj)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)JS_GetPrivate(cx, obj);
  
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
// CSS2Properties resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveCSS2Properties(JSContext *cx, JSObject *obj, jsval id)
{
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)JS_GetPrivate(cx, obj);
  
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


/***********************************************************************/
//
// class for CSS2Properties
//
JSClass CSS2PropertiesClass = {
  "CSS2Properties", 
  JSCLASS_HAS_PRIVATE,
  JS_PropertyStub,
  JS_PropertyStub,
  GetCSS2PropertiesProperty,
  SetCSS2PropertiesProperty,
  EnumerateCSS2Properties,
  ResolveCSS2Properties,
  JS_ConvertStub,
  FinalizeCSS2Properties
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
  {"opacity",    CSS2PROPERTIES_OPACITY,    JSPROP_ENUMERATE},
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
nsresult NS_InitCSS2PropertiesClass(nsIScriptContext *aContext, void **aPrototype)
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
