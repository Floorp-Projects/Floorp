#include "txIXPathContext.h"
#include "txAtoms.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"

/*
  Implementation of XSLT 1.0 extension function: system-property
*/

/**
 * Creates a new system-property function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
SystemPropertyFunctionCall::SystemPropertyFunctionCall(Node* aQNameResolveNode)
    : mQNameResolveNode(aQNameResolveNode)      
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param cs the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
ExprResult* SystemPropertyFunctionCall::evaluate(txIEvalContext* aContext)
{
    ExprResult* result = NULL;

    if (requireParams(1, 1, aContext)) {
        txListIterator iter(&params);
        Expr* param = (Expr*)iter.next();
        ExprResult* exprResult = param->evaluate(aContext);
        if (exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            txExpandedName qname;
            nsresult rv = qname.init(property, mQNameResolveNode, MB_TRUE);
            if (NS_SUCCEEDED(rv) &&
                qname.mNamespaceID == kNameSpaceID_XSLT) {
                if (qname.mLocalName == txXSLTAtoms::version) {
                    result = new NumberResult(1.0);
                }
                else if (qname.mLocalName == txXSLTAtoms::vendor) {
                    result = new StringResult("Transformiix");
                }
                else if (qname.mLocalName == txXSLTAtoms::vendorUrl) {
                    result = new StringResult("http://www.mozilla.org/projects/xslt/");
                }
            }
        }
        else {
            String err("Invalid argument passed to system-property(), expecting String");
            aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
            result = new StringResult(err);
        }
    }

    if (!result) {
        result = new StringResult("");
    }
    return result;
}

nsresult SystemPropertyFunctionCall::getNameAtom(txAtom** aAtom)
{
    *aAtom = txXSLTAtoms::systemProperty;
    TX_ADDREF_ATOM(*aAtom);
    return NS_OK;
}
