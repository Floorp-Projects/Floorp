#include "XSLTFunctions.h"
#include "XMLUtils.h"
#include "Names.h"

const String XSL_VERSION_PROPERTY    = "version";
const String XSL_VENDOR_PROPERTY     = "vendor";
const String XSL_VENDOR_URL_PROPERTY = "vendor-url";

/*
  Implementation of XSLT 1.0 extension function: system-property
*/

/**
 * Creates a new system-property function call
**/
SystemPropertyFunctionCall::SystemPropertyFunctionCall() :
        FunctionCall(SYSTEM_PROPERTY_FN)
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
ExprResult* SystemPropertyFunctionCall::evaluate(Node* context, ContextState* cs) {
    ExprResult* result = NULL;

    if ( requireParams(1,1,cs) ) {
        ListIterator* iter = params.iterator();
        Expr* param = (Expr*) iter->next();
        delete iter;
        ExprResult* exprResult = param->evaluate(context, cs);
        if (exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            if (XMLUtils::isValidQName(property)) {
                String propertyNsURI, prefix;
                XMLUtils::getLocalPart(property, prefix);
                cs->getNameSpaceURIFromPrefix(prefix, propertyNsURI);
                if (propertyNsURI.isEqual(XSLT_NS)) {
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
            result = new StringResult(err);
        }
    }

    if (result) 
        return result;
    else
        return new StringResult("");
}

