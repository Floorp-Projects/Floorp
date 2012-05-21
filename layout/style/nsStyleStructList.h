/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#define STYLE_STRUCT_INHERITED(name, checkdata_cb, ctor_args) \
  STYLE_STRUCT(name, checkdata_cb, ctor_args)
#define UNDEF_STYLE_STRUCT_INHERITED
#endif

#ifndef STYLE_STRUCT_RESET
#define STYLE_STRUCT_RESET(name, checkdata_cb, ctor_args) \
  STYLE_STRUCT(name, checkdata_cb, ctor_args)
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
STYLE_STRUCT_INHERITED(Font, CheckFontCallback, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Color, CheckColorCallback, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 2) {)
STYLE_STRUCT_INHERITED(List, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Text, CheckTextCallback, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 6) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 4) {)
STYLE_STRUCT_INHERITED(Visibility, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Quotes, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 6) {)
STYLE_STRUCT_INHERITED(UserInterface, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(TableBorder, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 16) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 12) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 10) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 8) {)
STYLE_STRUCT_INHERITED(SVG, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Background, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 10) {)
STYLE_STRUCT_RESET(Position, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(TextReset, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 14) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 12) {)
STYLE_STRUCT_RESET(Display, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Content, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 14) {)
STYLE_STRUCT_RESET(UIReset, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Table, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 20) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 18) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 16) {)
STYLE_STRUCT_RESET(Margin, nsnull, ())
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(Padding, nsnull, ())
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 18) {)
STYLE_STRUCT_RESET(Border, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(Outline, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 22) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST == 20) {)
STYLE_STRUCT_RESET(XUL, nsnull, ())
  STYLE_STRUCT_TEST_CODE(  } else {)
STYLE_STRUCT_RESET(SVGReset,nsnull, ())
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else {)
  STYLE_STRUCT_TEST_CODE(  NS_ASSERTION(STYLE_STRUCT_TEST == 22, "out of range");)
STYLE_STRUCT_RESET(Column, nsnull, (SSARG_PRESCONTEXT))
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
