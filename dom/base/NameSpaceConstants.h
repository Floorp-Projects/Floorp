/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NameSpaceConstants_h__
#define mozilla_dom_NameSpaceConstants_h__

#define kNameSpaceID_Unknown -1
// 0 is special at C++, so use a static const int32_t for
// kNameSpaceID_None to keep if from being cast to pointers
// Note that the XBL cache assumes (and asserts) that it can treat a
// single-byte value higher than kNameSpaceID_LastBuiltin specially. 
static const int32_t kNameSpaceID_None = 0;
#define kNameSpaceID_XMLNS    1 // not really a namespace, but it needs to play the game
#define kNameSpaceID_XML      2
#define kNameSpaceID_XHTML    3
#define kNameSpaceID_XLink    4
#define kNameSpaceID_XSLT     5
#define kNameSpaceID_XBL      6
#define kNameSpaceID_MathML   7
#define kNameSpaceID_RDF      8
#define kNameSpaceID_XUL      9
#define kNameSpaceID_SVG      10
#define kNameSpaceID_disabled_MathML      11
#define kNameSpaceID_disabled_SVG         12
#define kNameSpaceID_LastBuiltin          12 // last 'built-in' namespace

#endif // mozilla_dom_NameSpaceConstants_h__
