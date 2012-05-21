/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FoldConstants_h__
#define FoldConstants_h__

#include "jsprvtd.h"

namespace js {

bool
FoldConstants(JSContext *cx, ParseNode *pn, Parser *parser, bool inGenexpLambda = false,
              bool inCond = false);

} /* namespace js */

#endif /* FoldConstants_h__ */
