/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@dbaron.org> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

  STYLE_STRUCT_TEST_CODE(if (STYLE_STRUCT_TEST < 8) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 4) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 2) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 0) {)
STYLE_STRUCT_INHERITED(Font, CheckFontCallback, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Color, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 2) {)
STYLE_STRUCT_RESET(Background, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(List, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 6) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 4) {)
STYLE_STRUCT_RESET(Position, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(Text, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 6) {)
STYLE_STRUCT_RESET(TextReset, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Display, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 16) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 12) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 10) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 8) {)
STYLE_STRUCT_INHERITED(Visibility, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Content, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 10) {)
STYLE_STRUCT_INHERITED(Quotes, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_INHERITED(UserInterface, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST < 14) {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 12) {)
STYLE_STRUCT_RESET(UIReset, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Table, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    } else {)
  STYLE_STRUCT_TEST_CODE(      if (STYLE_STRUCT_TEST == 14) {)
STYLE_STRUCT_INHERITED(TableBorder, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(      } else {)
STYLE_STRUCT_RESET(Margin, nsnull, ())
  STYLE_STRUCT_TEST_CODE(      })
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 20) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST < 18) {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 16) {)
STYLE_STRUCT_RESET(Padding, nsnull, ())
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(Border, nsnull, ())
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  } else {)
  STYLE_STRUCT_TEST_CODE(    if (STYLE_STRUCT_TEST == 18) {)
STYLE_STRUCT_RESET(Outline, nsnull, (SSARG_PRESCONTEXT))
  STYLE_STRUCT_TEST_CODE(    } else {)
STYLE_STRUCT_RESET(XUL, nsnull, ())
  STYLE_STRUCT_TEST_CODE(    })
  STYLE_STRUCT_TEST_CODE(  })
#ifndef MOZ_SVG
  STYLE_STRUCT_TEST_CODE(} else {)
  STYLE_STRUCT_TEST_CODE(  NS_ASSERTION(STYLE_STRUCT_TEST == 20, "out of range");)
#else
  STYLE_STRUCT_TEST_CODE(} else if (STYLE_STRUCT_TEST < 22) {)
  STYLE_STRUCT_TEST_CODE(  if (STYLE_STRUCT_TEST == 20) {)
STYLE_STRUCT_INHERITED(SVG, nsnull, ())
  STYLE_STRUCT_TEST_CODE(  } else {)
STYLE_STRUCT_RESET(SVGReset,nsnull, ())
  STYLE_STRUCT_TEST_CODE(  })
  STYLE_STRUCT_TEST_CODE(} else {)
  STYLE_STRUCT_TEST_CODE(  NS_ASSERTION(STYLE_STRUCT_TEST == 22, "out of range");)
#endif
  STYLE_STRUCT_RESET(Column, nsnull, ())
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
