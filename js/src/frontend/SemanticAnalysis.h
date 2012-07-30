/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SemanticAnalysis_h__
#define SemanticAnalysis_h__

namespace js {

class StackFrame;

namespace frontend {

struct Parser;

/*
 * Called between parsing a top-level function/statement and emitting it.
 * Currently, all this function does is set the "has extensible parents" bit.
 */
bool
AnalyzeFunctions(Parser *parser);

} /* namespace frontend */
} /* namespace js */

#endif /* SemanticAnalysis_h__ */
