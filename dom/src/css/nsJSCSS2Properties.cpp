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
#include "nsJSUtils.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
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
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.azimuth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetAzimuth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUND:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.background", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackground(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundattachment", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackgroundAttachment(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackgroundColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundimage", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackgroundImage(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundposition", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackgroundPosition(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundrepeat", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBackgroundRepeat(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.border", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorder(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLLAPSE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordercollapse", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderCollapse(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordercolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderSpacing(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderTop(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderRight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderBottom(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderLeft(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderTopColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderRightColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderBottomColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderLeftColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderTopStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderRightStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderBottomStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderLeftStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERTOPWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderTopWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderRightWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderBottomWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderLeftWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BORDERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBorderWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_BOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetBottom(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CAPTIONSIDE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.captionside", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCaptionSide(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CLEAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.clear", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetClear(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CLIP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.clip", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetClip(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_COLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.color", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CONTENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.content", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetContent(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERINCREMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.counterincrement", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCounterIncrement(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_COUNTERRESET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.counterreset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCounterReset(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CUE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cue", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCue(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CUEAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cueafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCueAfter(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CUEBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cuebefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCueBefore(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CURSOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cursor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCursor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_DIRECTION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.direction", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetDirection(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_DISPLAY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.display", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetDisplay(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_ELEVATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.elevation", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetElevation(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_EMPTYCELLS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.emptycells", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetEmptyCells(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_CSSFLOAT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cssfloat", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetCssFloat(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.font", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFont(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTFAMILY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontfamily", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontFamily(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontsize", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontSize(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTSIZEADJUST:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontsizeadjust", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontSizeAdjust(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTRETCH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontstretch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontStretch(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTVARIANT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontvariant", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontVariant(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_FONTWEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontweight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetFontWeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.height", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.left", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetLeft(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LETTERSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.letterspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetLetterSpacing(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LINEHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.lineheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetLineHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetListStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyleimage", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetListStyleImage(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyleposition", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetListStylePosition(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_LISTSTYLETYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyletype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetListStyleType(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARGIN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.margin", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMargin(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARGINTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.margintop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginTop(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARGINRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginRight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARGINBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginBottom(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARGINLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarginLeft(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARKEROFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.markeroffset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarkerOffset(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MARKS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marks", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMarks(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MAXHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.maxheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMaxHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MAXWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.maxwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMaxWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MINHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.minheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMinHeight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_MINWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.minwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetMinWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_ORPHANS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.orphans", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOrphans(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outline", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOutline(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINECOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinecolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOutlineColor(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINESTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinestyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOutlineStyle(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OUTLINEWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinewidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOutlineWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OVERFLOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.overflow", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOverflow(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PADDING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.padding", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPadding(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingtop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPaddingTop(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPaddingRight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPaddingBottom(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PADDINGLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPaddingLeft(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.page", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPage(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPageBreakAfter(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakbefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPageBreakBefore(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakinside", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPageBreakInside(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAUSE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pause", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPause(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pauseafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPauseAfter(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PAUSEBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pausebefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPauseBefore(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PITCH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pitch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPitch(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PITCHRANGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pitchrange", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPitchRange(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_PLAYDURING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.playduring", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPlayDuring(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_POSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.position", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetPosition(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_QUOTES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.quotes", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetQuotes(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_RICHNESS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.richness", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetRichness(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_RIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.right", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetRight(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SIZE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.size", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSize(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SPEAK:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speak", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSpeak(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKHEADER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speakheader", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSpeakHeader(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKNUMERAL:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speaknumeral", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSpeakNumeral(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speakpunctuation", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSpeakPunctuation(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_SPEECHRATE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speechrate", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetSpeechRate(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_STRESS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.stress", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetStress(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TABLELAYOUT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.tablelayout", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTableLayout(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TEXTALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textalign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTextAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TEXTDECORATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textdecoration", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTextDecoration(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TEXTINDENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textindent", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTextIndent(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TEXTSHADOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textshadow", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTextShadow(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TEXTTRANSFORM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.texttransform", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTextTransform(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_TOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.top", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetTop(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_UNICODEBIDI:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.unicodebidi", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetUnicodeBidi(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_VERTICALALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.verticalalign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVerticalAlign(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_VISIBILITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.visibility", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVisibility(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_VOICEFAMILY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.voicefamily", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVoiceFamily(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_VOLUME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.volume", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetVolume(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_WHITESPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.whitespace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWhiteSpace(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_WIDOWS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.widows", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWidows(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.width", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWidth(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_WORDSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.wordspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetWordSpacing(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_ZINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.zindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetZIndex(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      case CSS2PROPERTIES_OPACITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.opacity", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        if (NS_SUCCEEDED(a->GetOpacity(prop))) {
          nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
        }
        else {
          return JS_FALSE;
        }
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectGetProperty(a, cx, id, vp);
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
  nsIDOMCSS2Properties *a = (nsIDOMCSS2Properties*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  if (JSVAL_IS_INT(id)) {
    nsIScriptContext *scriptCX = (nsIScriptContext *)JS_GetContextPrivate(cx);
    nsCOMPtr<nsIScriptSecurityManager> secMan;
    if (NS_OK != scriptCX->GetSecurityManager(getter_AddRefs(secMan))) {
      return JS_FALSE;
    }
    switch(JSVAL_TO_INT(id)) {
      case CSS2PROPERTIES_AZIMUTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.azimuth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetAzimuth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUND:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.background", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackground(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDATTACHMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundattachment", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackgroundAttachment(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackgroundColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDIMAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundimage", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackgroundImage(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDPOSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundposition", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackgroundPosition(prop);
        
        break;
      }
      case CSS2PROPERTIES_BACKGROUNDREPEAT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.backgroundrepeat", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBackgroundRepeat(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.border", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorder(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERCOLLAPSE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordercollapse", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderCollapse(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordercolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderSpacing(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderTop(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderRight(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderBottom(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderLeft(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERTOPCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderTopColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderRightColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderBottomColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTCOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftcolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderLeftColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERTOPSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderTopStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderRightStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderBottomStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderLeftStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERTOPWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bordertopwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderTopWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERRIGHTWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderrightwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderRightWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERBOTTOMWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderbottomwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderBottomWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERLEFTWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderleftwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderLeftWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BORDERWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.borderwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBorderWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_BOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.bottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetBottom(prop);
        
        break;
      }
      case CSS2PROPERTIES_CAPTIONSIDE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.captionside", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCaptionSide(prop);
        
        break;
      }
      case CSS2PROPERTIES_CLEAR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.clear", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetClear(prop);
        
        break;
      }
      case CSS2PROPERTIES_CLIP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.clip", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetClip(prop);
        
        break;
      }
      case CSS2PROPERTIES_COLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.color", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_CONTENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.content", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetContent(prop);
        
        break;
      }
      case CSS2PROPERTIES_COUNTERINCREMENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.counterincrement", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCounterIncrement(prop);
        
        break;
      }
      case CSS2PROPERTIES_COUNTERRESET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.counterreset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCounterReset(prop);
        
        break;
      }
      case CSS2PROPERTIES_CUE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cue", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCue(prop);
        
        break;
      }
      case CSS2PROPERTIES_CUEAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cueafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCueAfter(prop);
        
        break;
      }
      case CSS2PROPERTIES_CUEBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cuebefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCueBefore(prop);
        
        break;
      }
      case CSS2PROPERTIES_CURSOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cursor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCursor(prop);
        
        break;
      }
      case CSS2PROPERTIES_DIRECTION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.direction", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetDirection(prop);
        
        break;
      }
      case CSS2PROPERTIES_DISPLAY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.display", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetDisplay(prop);
        
        break;
      }
      case CSS2PROPERTIES_ELEVATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.elevation", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetElevation(prop);
        
        break;
      }
      case CSS2PROPERTIES_EMPTYCELLS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.emptycells", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetEmptyCells(prop);
        
        break;
      }
      case CSS2PROPERTIES_CSSFLOAT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.cssfloat", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetCssFloat(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.font", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFont(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTFAMILY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontfamily", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontFamily(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTSIZE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontsize", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontSize(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTSIZEADJUST:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontsizeadjust", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontSizeAdjust(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTSTRETCH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontstretch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontStretch(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontstyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTVARIANT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontvariant", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontVariant(prop);
        
        break;
      }
      case CSS2PROPERTIES_FONTWEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.fontweight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetFontWeight(prop);
        
        break;
      }
      case CSS2PROPERTIES_HEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.height", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetHeight(prop);
        
        break;
      }
      case CSS2PROPERTIES_LEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.left", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLeft(prop);
        
        break;
      }
      case CSS2PROPERTIES_LETTERSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.letterspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLetterSpacing(prop);
        
        break;
      }
      case CSS2PROPERTIES_LINEHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.lineheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetLineHeight(prop);
        
        break;
      }
      case CSS2PROPERTIES_LISTSTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetListStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEIMAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyleimage", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetListStyleImage(prop);
        
        break;
      }
      case CSS2PROPERTIES_LISTSTYLEPOSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyleposition", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetListStylePosition(prop);
        
        break;
      }
      case CSS2PROPERTIES_LISTSTYLETYPE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.liststyletype", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetListStyleType(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARGIN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.margin", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMargin(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARGINTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.margintop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginTop(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARGINRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginRight(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARGINBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginBottom(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARGINLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marginleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarginLeft(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARKEROFFSET:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.markeroffset", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarkerOffset(prop);
        
        break;
      }
      case CSS2PROPERTIES_MARKS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.marks", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMarks(prop);
        
        break;
      }
      case CSS2PROPERTIES_MAXHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.maxheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMaxHeight(prop);
        
        break;
      }
      case CSS2PROPERTIES_MAXWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.maxwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMaxWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_MINHEIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.minheight", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMinHeight(prop);
        
        break;
      }
      case CSS2PROPERTIES_MINWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.minwidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetMinWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_ORPHANS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.orphans", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOrphans(prop);
        
        break;
      }
      case CSS2PROPERTIES_OUTLINE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outline", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOutline(prop);
        
        break;
      }
      case CSS2PROPERTIES_OUTLINECOLOR:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinecolor", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOutlineColor(prop);
        
        break;
      }
      case CSS2PROPERTIES_OUTLINESTYLE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinestyle", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOutlineStyle(prop);
        
        break;
      }
      case CSS2PROPERTIES_OUTLINEWIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.outlinewidth", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOutlineWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_OVERFLOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.overflow", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOverflow(prop);
        
        break;
      }
      case CSS2PROPERTIES_PADDING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.padding", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPadding(prop);
        
        break;
      }
      case CSS2PROPERTIES_PADDINGTOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingtop", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPaddingTop(prop);
        
        break;
      }
      case CSS2PROPERTIES_PADDINGRIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingright", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPaddingRight(prop);
        
        break;
      }
      case CSS2PROPERTIES_PADDINGBOTTOM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingbottom", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPaddingBottom(prop);
        
        break;
      }
      case CSS2PROPERTIES_PADDINGLEFT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.paddingleft", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPaddingLeft(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.page", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPage(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPageBreakAfter(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakbefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPageBreakBefore(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAGEBREAKINSIDE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pagebreakinside", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPageBreakInside(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAUSE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pause", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPause(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAUSEAFTER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pauseafter", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPauseAfter(prop);
        
        break;
      }
      case CSS2PROPERTIES_PAUSEBEFORE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pausebefore", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPauseBefore(prop);
        
        break;
      }
      case CSS2PROPERTIES_PITCH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pitch", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPitch(prop);
        
        break;
      }
      case CSS2PROPERTIES_PITCHRANGE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.pitchrange", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPitchRange(prop);
        
        break;
      }
      case CSS2PROPERTIES_PLAYDURING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.playduring", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPlayDuring(prop);
        
        break;
      }
      case CSS2PROPERTIES_POSITION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.position", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetPosition(prop);
        
        break;
      }
      case CSS2PROPERTIES_QUOTES:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.quotes", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetQuotes(prop);
        
        break;
      }
      case CSS2PROPERTIES_RICHNESS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.richness", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetRichness(prop);
        
        break;
      }
      case CSS2PROPERTIES_RIGHT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.right", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetRight(prop);
        
        break;
      }
      case CSS2PROPERTIES_SIZE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.size", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSize(prop);
        
        break;
      }
      case CSS2PROPERTIES_SPEAK:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speak", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSpeak(prop);
        
        break;
      }
      case CSS2PROPERTIES_SPEAKHEADER:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speakheader", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSpeakHeader(prop);
        
        break;
      }
      case CSS2PROPERTIES_SPEAKNUMERAL:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speaknumeral", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSpeakNumeral(prop);
        
        break;
      }
      case CSS2PROPERTIES_SPEAKPUNCTUATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speakpunctuation", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSpeakPunctuation(prop);
        
        break;
      }
      case CSS2PROPERTIES_SPEECHRATE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.speechrate", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetSpeechRate(prop);
        
        break;
      }
      case CSS2PROPERTIES_STRESS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.stress", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetStress(prop);
        
        break;
      }
      case CSS2PROPERTIES_TABLELAYOUT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.tablelayout", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTableLayout(prop);
        
        break;
      }
      case CSS2PROPERTIES_TEXTALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textalign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTextAlign(prop);
        
        break;
      }
      case CSS2PROPERTIES_TEXTDECORATION:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textdecoration", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTextDecoration(prop);
        
        break;
      }
      case CSS2PROPERTIES_TEXTINDENT:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textindent", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTextIndent(prop);
        
        break;
      }
      case CSS2PROPERTIES_TEXTSHADOW:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.textshadow", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTextShadow(prop);
        
        break;
      }
      case CSS2PROPERTIES_TEXTTRANSFORM:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.texttransform", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTextTransform(prop);
        
        break;
      }
      case CSS2PROPERTIES_TOP:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.top", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetTop(prop);
        
        break;
      }
      case CSS2PROPERTIES_UNICODEBIDI:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.unicodebidi", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetUnicodeBidi(prop);
        
        break;
      }
      case CSS2PROPERTIES_VERTICALALIGN:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.verticalalign", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVerticalAlign(prop);
        
        break;
      }
      case CSS2PROPERTIES_VISIBILITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.visibility", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVisibility(prop);
        
        break;
      }
      case CSS2PROPERTIES_VOICEFAMILY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.voicefamily", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVoiceFamily(prop);
        
        break;
      }
      case CSS2PROPERTIES_VOLUME:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.volume", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetVolume(prop);
        
        break;
      }
      case CSS2PROPERTIES_WHITESPACE:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.whitespace", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWhiteSpace(prop);
        
        break;
      }
      case CSS2PROPERTIES_WIDOWS:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.widows", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWidows(prop);
        
        break;
      }
      case CSS2PROPERTIES_WIDTH:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.width", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWidth(prop);
        
        break;
      }
      case CSS2PROPERTIES_WORDSPACING:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.wordspacing", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetWordSpacing(prop);
        
        break;
      }
      case CSS2PROPERTIES_ZINDEX:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.zindex", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetZIndex(prop);
        
        break;
      }
      case CSS2PROPERTIES_OPACITY:
      {
        PRBool ok = PR_FALSE;
        secMan->CheckScriptAccess(scriptCX, obj, "css2properties.opacity", &ok);
        if (!ok) {
          //Need to throw error here
          return JS_FALSE;
        }
        nsAutoString prop;
        nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
        a->SetOpacity(prop);
        
        break;
      }
      default:
        return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
    }
  }
  else {
    return nsJSUtils::nsCallJSScriptObjectSetProperty(a, cx, id, vp);
  }

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
