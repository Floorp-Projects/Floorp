/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * laydom.c - Parse CSS/DOM values, constructors and destructors for data storage
 *
 */

#ifdef DOM /* For the Mac Build */

#include "xp.h"
#include "layout.h"
#include "dom.h"
#include "laydom.h"
#include "laystyle.h"

JSBool
lo_atoi(const char *str, uint32 *num, void *closure)
{ 
  *num = XP_ATOI(str);
  return JS_TRUE;
}

JSBool
lo_SSUnitsToData(const char *str, uint32 *data, void *closure)
{
  /* XXX NYI */
  *data = XP_ATOI(str);
  return JS_TRUE;
}

JSBool
FontWeightToData(const char *str, uint32 *data, void *closure)
{
  uint32 weight;
  /* XXX use proper CSS-value parsing stuff */
  if (!strcasecomp(str, "bolder")) {
    weight = FONT_WEIGHT_BOLDER;
  } else if (!strcasecomp(str, "lighter")) {
    weight = FONT_WEIGHT_LIGHTER;
  } else if (!strcasecomp(str, "bold")) {
    weight = 700;
  } else if (!strcasecomp(str, "normal")) {
    weight = 400;
  } else {
    weight = XP_ATOI(str);
    weight -= weight % 100;
  }
  *data = weight;
  return JS_TRUE;
}

#define SET_ATTR_BIT_IF(style, bit)                                           \
if (!strcasecomp(decors, style))                                              \
    attrs |= bit;

JSBool
TextDecorationToData(const char *decors, uint32 *data, void *closure)
{
  uint32 attrs = 0;
  /* XXX handle multiple tokens */
  SET_ATTR_BIT_IF(BLINK_STYLE, LO_ATTR_BLINK)
    else
  SET_ATTR_BIT_IF(STRIKEOUT_STYLE, LO_ATTR_STRIKEOUT)
    else
  SET_ATTR_BIT_IF(UNDERLINE_STYLE, LO_ATTR_UNDERLINE)
    else
  if (!strcasecomp(decors, "none"))
    attrs = 0;

  *data = attrs; 
  return JS_TRUE;
}

#undef SET_ATTR_BIT_IF

JSBool
PositionParser(const char *position, uint32 *data, void *closure)
{ 
  if (!XP_STRCASECMP(position, ABSOLUTE_STYLE))
    *data = 0;
  else if (!XP_STRCASECMP(position, RELATIVE_STYLE))
    *data = 1;
  else
    *data = 2;
  return JS_TRUE;
}

JSBool
lo_ColorStringToData(const char *color, uint32 *data, void *closure)
{ 
  LO_Color col; 
  if (!LO_ParseStyleSheetRGB((char *)color, &col.red, &col.green, &col.blue))
    return JS_FALSE;
  *data = *(uint32 *)&col;
  return JS_TRUE;
}  

/* 
 * Update LO_AdjustSSUnits() in laystyle.c to deal with DOM_StyleUnits
 */

JSBool   
lo_ParseSSNum(const char *str, uint32 *num, DOM_StyleUnits *units)
{ 
  const char *ptr, *num_ptr;

  ptr = str;

  /* skip any whitespace */
  while(XP_IS_SPACE(*ptr)) ptr++;
        
  /* save a pointer to the first non white char */
  num_ptr = ptr;
        
  /* go past any sign in front of the number */
  if(*ptr == '-' || *ptr == '+') ptr++;

  /* go forward until a non number is encountered */
  while(XP_IS_DIGIT(*ptr)) ptr++;                               
    
  /* go past a decimal */
  if(*ptr == '.') ptr++;
        
  while(XP_IS_DIGIT(*ptr)) ptr++;
        
  /* skip any whitespace between the number and units */
  while(XP_IS_SPACE(*ptr)) ptr++;
        
  /*
   * no need to clear out the string at the end since
   * atof will do that for us, and writting to the string
   * will make us crash
   *
   * ptr_value = *ptr;
   * *ptr = '\0';
   * *ptr = ptr_value;
   */
  *num = XP_ATOI(num_ptr);

  if(!XP_STRNCASECMP(ptr, "em", 2))
    *units = STYLE_UNITS_EMS;
  else if(!XP_STRNCASECMP(ptr, "ex", 2))
    *units = STYLE_UNITS_EXS;
  else if(!XP_STRNCASECMP(ptr, "px", 2))
    *units = STYLE_UNITS_PXS;
  else if(!XP_STRNCASECMP(ptr, "pt", 2))
    *units = STYLE_UNITS_PTS;
  else if(!XP_STRNCASECMP(ptr, "pc", 2))
    *units = STYLE_UNITS_PCS;
  else if(!XP_STRNCASECMP(ptr, "in", 2))
    *units = STYLE_UNITS_INS;
  else if(!XP_STRNCASECMP(ptr, "cm", 2))
    *units = STYLE_UNITS_CMS;
  else if(!XP_STRNCASECMP(ptr, "mm", 2))
    *units = STYLE_UNITS_MMS;
  else if(!XP_STRNCMP(ptr, "%", 1))
    *units = STYLE_UNITS_PERCENT;  
  else if(!*ptr)
    *units = STYLE_UNITS_REL;
  else
    *units = STYLE_UNITS_PXS;

  return JS_TRUE;
}

JSBool  
lo_ParseSSNumToData(const char *str, uint32 *data, void *closure)
{ 
  struct SSUnitContext *argp = closure;
  int32 num;

  if (!lo_ParseSSNum(str, &num, (DOM_StyleUnits *)&argp->units))
    return JS_FALSE;

  if (argp->units == STYLE_UNITS_PERCENT) {
    num = argp->enclosingVal * num / 100;
  }
        
  if (argp->axisAdjust == AXIS_X)
    num = FEUNITS_X(num, argp->context);
  else if (argp->axisAdjust == AXIS_Y)
    num = FEUNITS_Y(num, argp->context);
            
  *data = num;
  
  return JS_TRUE;
}        

/* XXX Merge with lo_ParseSSNum */

JSBool
lo_ParseFontSize(const char *str, uint32 *num, DOM_StyleUnits *units)
{ 
  const char *ptr, *num_ptr;
  double *value = XP_NEW(double);

  if (!value)
    return JS_FALSE;

  ptr = str;

  /* skip any whitespace */
  while(XP_IS_SPACE(*ptr)) ptr++;
        
  /* save a pointer to the first non white char */
  num_ptr = ptr;
        
  /* go past any sign in front of the number */
  if(*ptr == '-' || *ptr == '+') ptr++;

  /* go forward until a non number is encountered */
  while(XP_IS_DIGIT(*ptr)) ptr++;                               
    
  /* go past a decimal */
  if(*ptr == '.') ptr++;
        
  while(XP_IS_DIGIT(*ptr)) ptr++;
        
  /* skip any whitespace between the number and units */
  while(XP_IS_SPACE(*ptr)) ptr++;
        
  /*
   * no need to clear out the string at the end since
   * atof will do that for us, and writting to the string
   * will make us crash
   *
   * ptr_value = *ptr;
   * *ptr = '\0';
   * *ptr = ptr_value;
   */
  *value = atof(num_ptr);  /* Calculate value */
  *num = (int32)value;     /* Save pointer to value in data */

  if(!XP_STRNCASECMP(ptr, "em", 2))
    *units = STYLE_UNITS_EMS;
  else if(!XP_STRNCASECMP(ptr, "ex", 2))
    *units = STYLE_UNITS_EXS;
  else if(!XP_STRNCASECMP(ptr, "px", 2))
    *units = STYLE_UNITS_PXS;
  else if(!XP_STRNCASECMP(ptr, "pt", 2))
    *units = STYLE_UNITS_PTS;
  else if(!XP_STRNCASECMP(ptr, "pc", 2))
    *units = STYLE_UNITS_PCS;
  else if(!XP_STRNCASECMP(ptr, "in", 2))
    *units = STYLE_UNITS_INS;
  else if(!XP_STRNCASECMP(ptr, "cm", 2))
    *units = STYLE_UNITS_CMS;
  else if(!XP_STRNCASECMP(ptr, "mm", 2))
    *units = STYLE_UNITS_MMS;
  else if(!XP_STRNCMP(ptr, "%", 1))
    *units = STYLE_UNITS_PERCENT;  
  else if(!*ptr)
    *units = STYLE_UNITS_REL;
  else
    *units = STYLE_UNITS_PXS;

  return JS_TRUE;
}

JSBool  
lo_ParseFontSizeToData(const char *str, uint32 *data, void *closure)
{ 
  struct SSUnitContext *argp = closure;
  int32 num;

  if (!lo_ParseFontSize(str, &num, (DOM_StyleUnits *)&argp->units))
    return JS_FALSE;

  if (argp->units == STYLE_UNITS_PERCENT) {
    num = argp->enclosingVal * num / 100;
  }
        
  if (argp->axisAdjust == AXIS_X)
    num = FEUNITS_X(num, argp->context);
  else if (argp->axisAdjust == AXIS_Y)
    num = FEUNITS_Y(num, argp->context);
            
  *data = num;
  
  return JS_TRUE;
}        

JSBool
lo_DeleteFontSize(DOM_AttributeEntry *entry)
{
  if (!entry)
    return JS_FALSE;

  XP_FREEIF((double *)entry->data);
  return JS_TRUE;
}

#endif /* DOM: For the Mac build */
