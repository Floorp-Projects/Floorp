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
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- original author.
 *
 * Nisheeth Ranjan, nisheeth@netscape.com
 *   -- implemented rint function, which was not available on Windows.
 *
 */

/*
  NumberFunctionCall
  A representation of the XPath Number funtions
*/

#include "FunctionLib.h"
#include "XMLUtils.h"
#include "XMLDOMUtils.h"
#include <math.h>

/**
 * Creates a NumberFunctionCall of the given type
**/
NumberFunctionCall::NumberFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
    case ROUND :
        name = XPathNames::ROUND_FN;
        break;
    case CEILING :
        name = XPathNames::CEILING_FN;
        break;
    case FLOOR :
        name = XPathNames::FLOOR_FN;
        break;
    case SUM :
        name = XPathNames::SUM_FN;
        break;
    case NUMBER :
    default :
        name = XPathNames::NUMBER_FN;
        break;
    }
} //-- NumberFunctionCall

#if !defined(HAVE_RINT)
static double rint(double r)
{
  double integerPart = 0;
  double fraction = 0;
  fraction = modf(r, &integerPart);
  if (fraction >= 0.5)
      integerPart++;

  return integerPart;
}
#endif

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* NumberFunctionCall::evaluate(Node* context, ContextState* cs) {
    NumberResult* result = 0;
    ListIterator* iter = params.iterator();
    Expr* param = 0;
    String err;

    switch ( type ) {
    case CEILING :
        if ( requireParams(1, 1, cs) ) {
            double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
            result = new NumberResult(ceil(dbl));
        }
        else {
            result = new NumberResult(0.0);
        }
        break;

    case FLOOR :
        if ( requireParams(1, 1, cs) ) {
            double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
            result = new NumberResult(floor(dbl));
        }
        else {
            result = new NumberResult(0.0);
        }
        break;
      
    case ROUND :
        if ( requireParams(1, 1, cs) ) {
            double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
            double res = rint(dbl);
            if ((dbl>0.0) && (res == dbl-0.5)) {
                // fix for native round function from math library (rint()) which does not
                // match the XPath spec for positive half values
                result = new NumberResult(res+1.0);
            }
            else
                result = new NumberResult(res);
            break;
        }
        else result = new NumberResult(0.0);
        break;
      
    case SUM :
        double numResult;
        numResult = 0 ;
        if ( requireParams(1, 1, cs) ) {
            param = (Expr*)iter->next();
            ExprResult* exprResult = param->evaluate(context, cs);
            if ( exprResult->getResultType() == ExprResult::NODESET ) {
                NodeSet *lNList = (NodeSet *)exprResult;
                NodeSet tmp;
                for (int i=0; i<lNList->size(); i++){
                    tmp.add(0,lNList->get(i));
                    numResult += tmp.numberValue();
                };
            };
            delete exprResult;
            exprResult=0;
        };
        result = new NumberResult(numResult);
        break;
      
    case NUMBER :
    default : //-- number( object? )
        if ( requireParams(0, 1, cs) ) {
            if (iter->hasNext()) {
                param = (Expr*) iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result = new NumberResult(exprResult->numberValue());
                delete exprResult;
            }
            else {
                String resultStr;
                XMLDOMUtils::getNodeValue(context, &resultStr);
                if ( cs->isStripSpaceAllowed(context) &&
                     XMLUtils::shouldStripTextnode(resultStr)) {
                    result = new NumberResult(Double::NaN);
                }
                else {
                    Double dbl(resultStr);
                    result = new NumberResult(dbl.doubleValue());
                }
            }
        }
        else {
            result = new NumberResult(Double::NaN);
        }
        break;
    }
    delete iter;
    return result;
} //-- evaluate

