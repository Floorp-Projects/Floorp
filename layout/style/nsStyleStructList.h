/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// IWYU pragma: private, include "nsStyleStructFwd.h"

/*
 * list of structs that contain the data provided by ComputedStyle, the
 * internal API for computed style data for an element
 */

/*
 * This file is intended to be used by different parts of the code, with
 * the STYLE_STRUCT macro (or the STYLE_STRUCT_INHERITED and
 * STYLE_STRUCT_RESET pair of macros) defined in different ways.
 */

#ifndef STYLE_STRUCT_INHERITED
#define STYLE_STRUCT_INHERITED(name) STYLE_STRUCT(name)
#define UNDEF_STYLE_STRUCT_INHERITED
#endif

#ifndef STYLE_STRUCT_RESET
#define STYLE_STRUCT_RESET(name) STYLE_STRUCT(name)
#define UNDEF_STYLE_STRUCT_RESET
#endif

// The inherited structs are listed before the Reset structs.
// nsStyleStructID assumes this is the case, and callers other than
// nsStyleStructFwd.h that want the structs in id-order just define
// STYLE_STRUCT rather than including the file twice.

STYLE_STRUCT_INHERITED(Font)
STYLE_STRUCT_INHERITED(List)
STYLE_STRUCT_INHERITED(Text)
STYLE_STRUCT_INHERITED(Visibility)
STYLE_STRUCT_INHERITED(UI)
STYLE_STRUCT_INHERITED(TableBorder)
STYLE_STRUCT_INHERITED(SVG)

STYLE_STRUCT_RESET(Background)
STYLE_STRUCT_RESET(Position)
STYLE_STRUCT_RESET(TextReset)
STYLE_STRUCT_RESET(Display)
STYLE_STRUCT_RESET(Content)
STYLE_STRUCT_RESET(UIReset)
STYLE_STRUCT_RESET(Table)
STYLE_STRUCT_RESET(Margin)
STYLE_STRUCT_RESET(Padding)
STYLE_STRUCT_RESET(Border)
STYLE_STRUCT_RESET(Outline)
STYLE_STRUCT_RESET(XUL)
STYLE_STRUCT_RESET(SVGReset)
STYLE_STRUCT_RESET(Column)
STYLE_STRUCT_RESET(Effects)
STYLE_STRUCT_RESET(Page)

#ifdef UNDEF_STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_INHERITED
#undef UNDEF_STYLE_STRUCT_INHERITED
#endif

#ifdef UNDEF_STYLE_STRUCT_RESET
#undef STYLE_STRUCT_RESET
#undef UNDEF_STYLE_STRUCT_RESET
#endif
