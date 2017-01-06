/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* a list of style struct's member variables which can be visited-dependent */

/* This file contains the list of all style structs and fields that can
 * be visited-dependent. Each entry is defined as a STYLE_STRUCT macro
 * with the following parameters:
 * - 'name_' the name of the style struct
 * - 'fields_' the list of member variables in the style struct that can
 *   be visited-dependent
 *
 * Users of this list should define a macro STYLE_STRUCT(name_, fields_)
 * before including this file.
 *
 * Note that, currently, there is a restriction that all fields in a
 * style struct must have the same type.
 */

STYLE_STRUCT(Color, (mColor))
STYLE_STRUCT(Background, (mBackgroundColor))
STYLE_STRUCT(Border, (mBorderTopColor,
                      mBorderRightColor,
                      mBorderBottomColor,
                      mBorderLeftColor))
STYLE_STRUCT(Outline, (mOutlineColor))
STYLE_STRUCT(Column, (mColumnRuleColor))
STYLE_STRUCT(Text, (mTextEmphasisColor,
                    mWebkitTextFillColor,
                    mWebkitTextStrokeColor))
STYLE_STRUCT(TextReset, (mTextDecorationColor))
STYLE_STRUCT(SVG, (mFill, mStroke))
