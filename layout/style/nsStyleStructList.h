/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// IWYU pragma: private, include "nsStyleStructFwd.h"

/*
 * list of structs that contain the data provided by nsStyleContext, the
 * internal API for computed style data for an element
 */

/*
 * This file is intended to be used by different parts of the code, with
 * the STYLE_STRUCT macro (or the STYLE_STRUCT_INHERITED and
 * STYLE_STRUCT_RESET pair of macros) defined in different ways.
 */

#ifndef STYLE_STRUCT_INHERITED
#define STYLE_STRUCT_INHERITED(name, checkdata_cb) \
  STYLE_STRUCT(name, checkdata_cb)
#define UNDEF_STYLE_STRUCT_INHERITED
#endif

#ifndef STYLE_STRUCT_RESET
#define STYLE_STRUCT_RESET(name, checkdata_cb) \
  STYLE_STRUCT(name, checkdata_cb)
#define UNDEF_STYLE_STRUCT_RESET
#endif

#ifdef STYLE_STRUCT_TEST
#define STYLE_STRUCT_TEST_CODE(c) c
#else
#define STYLE_STRUCT_TEST_CODE(c)
#endif

// The inherited structs must be listed before the Reset structs.
// nsStyleStructID assumes this is the case, and callers other than
// nsStyleStructFwd.h that want the structs in id-order just define
// STYLE_STRUCT rather than including the file twice.

  STYLE_STRUCT_TEST_CODE(if (STYLE_STRUCT_TEST < 8) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 4) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 2) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 0) {)
STYLE_STRUCT_INHERITED(Font, CheckFontCallback)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Color, CheckColorCallback)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 2) {)
STYLE_STRUCT_INHERITED(List, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Text, CheckTextCallback)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 6) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 4) {)
STYLE_STRUCT_INHERITED(Visibility, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Quotes, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 6) {)
STYLE_STRUCT_INHERITED(UserInterface, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(TableBorder, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 16) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 12) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 10) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 8) {)
STYLE_STRUCT_INHERITED(SVG, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Background, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 10) {)
STYLE_STRUCT_RESET(Position, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(TextReset, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 14) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 12) {)
STYLE_STRUCT_RESET(Display, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Content, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 14) {)
STYLE_STRUCT_RESET(UIReset, nullptr)
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Table, nullptr)
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 20) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 18) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 16) {)
STYLE_STRUCT_RESET(Margin, nullptr)
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(Padding, nullptr)
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 18) {)
STYLE_STRUCT_RESET(Border, nullptr)
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(Outline, nullptr)
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 22) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST == 20) {)
STYLE_STRUCT_RESET(XUL, nullptr)
  STYLE_STRUCT_TEST_CODE(  } else {)
STYLE_STRUCT_RESET(SVGReset, nullptr)
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else {)
  STYLE_STRUCT_TEST_CODE(  NS_ASSERTION(STYLE_STRUCT_TEST == 22, "out of range");)
STYLE_STRUCT_RESET(Column, nullptr)
  STYLE_STRUCT_TEST_CODE(})
      
#ifdef UNDEF_STYLE_STRUCT_INHERITED
#undef STYLE_STRUCT_INHERITED
#undef UNDEF_STYLE_STRUCT_INHERITED
#endif

#ifdef UNDEF_STYLE_STRUCT_RESET
#undef STYLE_STRUCT_RESET
#undef UNDEF_STYLE_STRUCT_RESET
#endif

#undef STYLE_STRUCT_TEST_CODE
