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
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- original author.
 */

/**
 * NumberFunctionCall
 * A representation of the XPath Number funtions
 * @author <A HREF="mailto:ogerardin@vo.lu">Olivier Gerardin</A>
**/

#include "FunctionLib.h"

/**
 * Creates a default NumberFunctionCall. The number() function
 * is the default
**/
NumberFunctionCall::NumberFunctionCall() : FunctionCall(XPathNames::NUMBER_FN) {
    type = NUMBER;
} //-- NumberFunctionCall

/**
 * Creates a NumberFunctionCall of the given type
**/
NumberFunctionCall::NumberFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
    case ROUND :
      FunctionCall::setName(XPathNames::ROUND_FN);
      break;
    case CEILING :
      FunctionCall::setName(XPathNames::CEILING_FN);
      break;
    case FLOOR :
      FunctionCall::setName(XPathNames::FLOOR_FN);
      break;
    case NUMBER :
    default :
      FunctionCall::setName(XPathNames::NUMBER_FN);
      break;
    }
} //-- NumberFunctionCall

#ifdef WINDOWS
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
    NumberResult* result = new NumberResult();
    ListIterator* iter = params.iterator();
    int argc = params.getLength();
    Expr* param = 0;
    String err;

    switch ( type ) {
    case CEILING :
      if ( requireParams(1, 1, cs) ) {
	double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
	result->setValue(ceil(dbl));
      }
      else {
	result->setValue(0.0);
      }
      break;
      
    case FLOOR :
      if ( requireParams(1, 1, cs) ) {
	double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
	result->setValue(floor(dbl));
      }
      else {
	result->setValue(0.0);
      }
      break;
      
    case ROUND :
      if ( requireParams(1, 1, cs) ) {
	double dbl = evaluateToNumber((Expr*)iter->next(), context, cs);
	double res = rint(dbl);
	if ((dbl>0.0) && (res == dbl-0.5)) {
	  // fix for native round function from math library (rint()) which does not
	  // match the XPath spec for positive half values
	  result->setValue(res+1.0);
	}
	else {
	  result->setValue(res);
	}
	break;
      }
      else result->setValue(0.0);
      break;
      
    case NUMBER :
    default : //-- number( object? )
      if ( requireParams(0, 1, cs) ) {
	if (iter->hasNext()) {
	  param = (Expr*) iter->next();
	  ExprResult* exprResult = param->evaluate(context, cs);
	  result->setValue(exprResult->numberValue());
	  delete exprResult;
	}
	else {
	  String resultStr;
	  DOMString temp;
	  XMLDOMUtils::getNodeValue(context, &temp);
	  if ( cs->isStripSpaceAllowed(context) ) {
	    XMLUtils::stripSpace(temp, resultStr);
	  }
	  else {
	    resultStr.append(temp);
	  }
	  Double dbl(resultStr);
	  result->setValue(dbl.doubleValue());
	}
      }
      else {
	result = new NumberResult(0.0);
      }
      break;
    }
    delete iter;
    return result;
} //-- evaluate

