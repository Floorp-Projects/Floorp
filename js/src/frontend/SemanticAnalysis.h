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
 * For each function in the compilation unit given by sc and functionList,
 * decide whether the function is a full closure or a null closure and set
 * JSFunction flags accordingly.
 */
bool
AnalyzeFunctions(Parser *parser, StackFrame *callerFrame);

} /* namespace frontend */
} /* namespace js */

#endif /* SemanticAnalysis_h__ */
