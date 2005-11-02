#include "XSLTFunctions.h"
#include "XMLUtils.h"
#include "Names.h"
#include "txIXPathContext.h"

const String XSL_VERSION_PROPERTY    = "version";
const String XSL_VENDOR_PROPERTY     = "vendor";
const String XSL_VENDOR_URL_PROPERTY = "vendor-url";

/*
  Implementation of XSLT 1.0 extension function: system-property
*/

/**
 * Creates a new system-property function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
SystemPropertyFunctionCall::SystemPropertyFunctionCall(Element* aNode) :
    mStylesheetNode(aNode), FunctionCall(SYSTEM_PROPERTY_FN)
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
            if (XMLUtils::isValidQName(property)) {
                String prefix;
                PRInt32 namespaceID = kNameSpaceID_None;
                XMLUtils::getPrefix(property, prefix);
                if (!prefix.isEmpty()) {
                    txAtom* prefixAtom = TX_GET_ATOM(prefix);
                    namespaceID =
                        mStylesheetNode->lookupNamespaceID(prefixAtom);
                    TX_IF_RELEASE_ATOM(prefixAtom);
                }
                if (namespaceID == kNameSpaceID_XSLT) {
                    String localName;
                    XMLUtils::getLocalPart(property, localName);
                    if (localName.isEqual(XSL_VERSION_PROPERTY))
                        result = new NumberResult(1.0);
                    else if (localName.isEqual(XSL_VENDOR_PROPERTY))
                        result = new StringResult("Transformiix");
                    else if (localName.isEqual(XSL_VENDOR_URL_PROPERTY))
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

    if (result) 
        return result;
    else
        return new StringResult("");
}

