/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Perignon: parse style information to store in the DOM.
 */

#ifndef LAY_DOM_H
#define LAY_DOM_H

#define AXIS_NONE       0
#define AXIS_X          1
#define AXIS_Y          2

#define FONT_WEIGHT_BOLDER		0x10000
#define FONT_WEIGHT_LIGHTER		0x20000

struct SSUnitContext {
  MWContext *context;
  uint32 enclosingVal;
  uint8 units;
  uint8 axisAdjust;  
};
 
typedef enum DOM_StyleUnits {
    STYLE_UNITS_UNKNOWN = -1,
    STYLE_UNITS_NONE,
    STYLE_UNITS_PERCENT,
    STYLE_UNITS_EMS,
    STYLE_UNITS_EXS,
    STYLE_UNITS_PTS, /* points */
    STYLE_UNITS_PXS, /* pixels */
    STYLE_UNITS_PCS,
    STYLE_UNITS_REL,
    STYLE_UNITS_INS, /* inches */
    STYLE_UNITS_CMS, /* centimeters */
    STYLE_UNITS_MMS, /* millimeters */
} DOM_StyleUnits;

extern JSBool
lo_atoi(const char *str, uint32 *num, void *closure);

extern JSBool
lo_ColorStringToData(const char *color, uint32 *data, void *closure);

extern JSBool
lo_SSUnitsToData(const char *units, uint32 *data, void *closure);

extern JSBool
FontWeightToData(const char *str, uint32 *data, void *closure);

extern JSBool
TextDecorationToData(const char *decors, uint32 *data, void *closure);

extern JSBool
PositionParser(const char *position, uint32 *data, void *closure);

extern JSBool
lo_ParseSSNumToData(const char *str, uint32 *data, void *closure);

extern JSBool
lo_ParseFontSizeToData(const char *str, uint32 *data, void *closure);

extern JSBool
lo_DeleteFontSize(DOM_AttributeEntry *entry);

#endif /* LAY_DOM_H */
