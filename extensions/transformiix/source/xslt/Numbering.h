/*
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
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * Numbering methods
**/

#ifndef MITREXSL_NUMBERING_H
#define MITREXSL_NUMBERING_H

#include "baseutils.h"
#include "TxString.h"
#include "ProcessorState.h"
#include "Expr.h"
#include "primitives.h"
#include "ExprResult.h"

class Numbering {

public:

    static void doNumbering
        (Element* xslNumber, String& dest, Node* context, ProcessorState* ps);

private:
    static int countPreceedingSiblings
        (PatternExpr* patternExpr, Node* context, ProcessorState* ps);

    static NodeSet* getAncestorsOrSelf
        ( PatternExpr* countExpr,
          PatternExpr* from,
          Node* context,
          ProcessorState* ps,
          MBool findNearest );
};
#endif

