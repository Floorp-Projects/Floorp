/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Initial Developer of the Original Code is Jonas Sicking.
 * Portions created by Jonas Sicking are Copyright (C) 2001, Jonas Sicking.
 * All rights reserved.
 *
 * Contributor(s):
 * Jonas Sicking, sicking@bigfoot.com
 *   -- original author.
 */
 
#include "Expr.h"

Expr::~Expr() {
}

/*
 * Determines whether this Expr matches the given node within
 * the given context.
 */
MBool Expr::matches(Node* node, Node* context, ContextState* cs)
{
    NS_ASSERTION(0, "Expr::matches() called");
    return MB_FALSE;
}

/*
 * Returns the default priority of this Expr based on the given Node,
 * context Node, and ContextState.
 */
double Expr::getDefaultPriority(Node* node, Node* context, ContextState* cs)
{
    NS_ASSERTION(0, "Expr::matches() called");
    return 0;
}
