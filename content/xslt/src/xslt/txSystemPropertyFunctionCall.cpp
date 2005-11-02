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
nsresult
SystemPropertyFunctionCall::evaluate(txIEvalContext* aContext,
                                     txAExprResult** aResult)
{
    *aResult = nsnull;

    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    txListIterator iter(&params);
    Expr* param = (Expr*)iter.next();
    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = param->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString property;
    exprResult->stringValue(property);
    txExpandedName qname;
    rv = qname.init(property, mMappings, MB_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (qname.mNamespaceID == kNameSpaceID_XSLT) {
        if (qname.mLocalName == txXSLTAtoms::version) {
            return aContext->recycler()->getNumberResult(1.0, aResult);
        }
        if (qname.mLocalName == txXSLTAtoms::vendor) {
            return aContext->recycler()->getStringResult(
                  NS_LITERAL_STRING("Transformiix"), aResult);
        }
        if (qname.mLocalName == txXSLTAtoms::vendorUrl) {
            return aContext->recycler()->getStringResult(
                  NS_LITERAL_STRING("http://www.mozilla.org/projects/xslt/"),
                  aResult);
        }
    }
    aContext->recycler()->getEmptyStringResult(aResult);

    return NS_OK;

}

#ifdef TX_TO_STRING
nsresult
SystemPropertyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::systemProperty;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
