#include "txIXPathContext.h"
#include "txAtoms.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"
#include "ExprResult.h"
#include "txNamespaceMap.h"

/*
  Implementation of XSLT 1.0 extension function: system-property
*/

/**
 * Creates a new system-property function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
SystemPropertyFunctionCall::SystemPropertyFunctionCall(txNamespaceMap* aMappings)
    : mMappings(aMappings)      
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
    ExprResult* result = nsnull;

    if (requireParams(1, 1, aContext)) {
        txListIterator iter(&params);
        Expr* param = (Expr*)iter.next();
        ExprResult* exprResult = param->evaluate(aContext);
        if (exprResult->getResultType() == ExprResult::STRING) {
            nsAutoString property;
            exprResult->stringValue(property);
            txExpandedName qname;
            nsresult rv = qname.init(property, mMappings, MB_TRUE);
            if (NS_SUCCEEDED(rv) &&
                qname.mNamespaceID == kNameSpaceID_XSLT) {
                if (qname.mLocalName == txXSLTAtoms::version) {
                    result = new NumberResult(1.0);
                }
                else if (qname.mLocalName == txXSLTAtoms::vendor) {
                    result = new StringResult(NS_LITERAL_STRING("Transformiix"));
                }
                else if (qname.mLocalName == txXSLTAtoms::vendorUrl) {
                    result = new StringResult(NS_LITERAL_STRING("http://www.mozilla.org/projects/xslt/"));
                }
            }
        }
        else {
            NS_NAMED_LITERAL_STRING(err, "Invalid argument passed to system-property(), expecting String");
            aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
            result = new StringResult(err);
        }
    }

    if (!result) {
        result = new StringResult();
    }
    return result;
}

nsresult SystemPropertyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::systemProperty;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
