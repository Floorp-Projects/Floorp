/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txIXPathContext.h"
#include "nsGkAtoms.h"
#include "txError.h"
#include "txXMLUtils.h"
#include "txXSLTFunctions.h"
#include "txNamespaceMap.h"

nsresult
txXSLTEnvironmentFunctionCall::evaluate(txIEvalContext* aContext,
                                        txAExprResult** aResult)
{
    *aResult = nullptr;

    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    nsAutoString property;
    nsresult rv = mParams[0]->evaluateToString(aContext, property);
    NS_ENSURE_SUCCESS(rv, rv);

    txExpandedName qname;
    rv = qname.init(property, mMappings, mType != FUNCTION_AVAILABLE);
    NS_ENSURE_SUCCESS(rv, rv);

    switch (mType) {
        case SYSTEM_PROPERTY:
        {
            if (qname.mNamespaceID == kNameSpaceID_XSLT) {
                if (qname.mLocalName == nsGkAtoms::version) {
                    return aContext->recycler()->getNumberResult(1.0, aResult);
                }
                if (qname.mLocalName == nsGkAtoms::vendor) {
                    return aContext->recycler()->getStringResult(
                          NS_LITERAL_STRING("Transformiix"), aResult);
                }
                if (qname.mLocalName == nsGkAtoms::vendorUrl) {
                    return aContext->recycler()->getStringResult(
                          NS_LITERAL_STRING("http://www.mozilla.org/projects/xslt/"),
                          aResult);
                }
            }
            aContext->recycler()->getEmptyStringResult(aResult);
            break;
        }
        case ELEMENT_AVAILABLE:
        {
            bool val = qname.mNamespaceID == kNameSpaceID_XSLT &&
                         (qname.mLocalName == nsGkAtoms::applyImports ||
                          qname.mLocalName == nsGkAtoms::applyTemplates ||
                          qname.mLocalName == nsGkAtoms::attribute ||
                          qname.mLocalName == nsGkAtoms::attributeSet ||
                          qname.mLocalName == nsGkAtoms::callTemplate ||
                          qname.mLocalName == nsGkAtoms::choose ||
                          qname.mLocalName == nsGkAtoms::comment ||
                          qname.mLocalName == nsGkAtoms::copy ||
                          qname.mLocalName == nsGkAtoms::copyOf ||
                          qname.mLocalName == nsGkAtoms::decimalFormat ||
                          qname.mLocalName == nsGkAtoms::element ||
                          qname.mLocalName == nsGkAtoms::fallback ||
                          qname.mLocalName == nsGkAtoms::forEach ||
                          qname.mLocalName == nsGkAtoms::_if ||
                          qname.mLocalName == nsGkAtoms::import ||
                          qname.mLocalName == nsGkAtoms::include ||
                          qname.mLocalName == nsGkAtoms::key ||
                          qname.mLocalName == nsGkAtoms::message ||
                          //qname.mLocalName == nsGkAtoms::namespaceAlias ||
                          qname.mLocalName == nsGkAtoms::number ||
                          qname.mLocalName == nsGkAtoms::otherwise ||
                          qname.mLocalName == nsGkAtoms::output ||
                          qname.mLocalName == nsGkAtoms::param ||
                          qname.mLocalName == nsGkAtoms::preserveSpace ||
                          qname.mLocalName == nsGkAtoms::processingInstruction ||
                          qname.mLocalName == nsGkAtoms::sort ||
                          qname.mLocalName == nsGkAtoms::stripSpace ||
                          qname.mLocalName == nsGkAtoms::stylesheet ||
                          qname.mLocalName == nsGkAtoms::_template ||
                          qname.mLocalName == nsGkAtoms::text ||
                          qname.mLocalName == nsGkAtoms::transform ||
                          qname.mLocalName == nsGkAtoms::valueOf ||
                          qname.mLocalName == nsGkAtoms::variable ||
                          qname.mLocalName == nsGkAtoms::when ||
                          qname.mLocalName == nsGkAtoms::withParam);

            aContext->recycler()->getBoolResult(val, aResult);
            break;
        }
        case FUNCTION_AVAILABLE:
        {
            extern bool TX_XSLTFunctionAvailable(nsIAtom* aName,
                                                   PRInt32 aNameSpaceID);

            txCoreFunctionCall::eType type;
            bool val = (qname.mNamespaceID == kNameSpaceID_None &&
                          txCoreFunctionCall::getTypeFromAtom(qname.mLocalName,
                                                              type)) ||
                         TX_XSLTFunctionAvailable(qname.mLocalName,
                                                  qname.mNamespaceID);

            aContext->recycler()->getBoolResult(val, aResult);
            break;
        }
    }

    return NS_OK;
}

Expr::ResultType
txXSLTEnvironmentFunctionCall::getReturnType()
{
    return mType == SYSTEM_PROPERTY ? (STRING_RESULT | NUMBER_RESULT) :
                                      BOOLEAN_RESULT;
}

bool
txXSLTEnvironmentFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
txXSLTEnvironmentFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = mType == SYSTEM_PROPERTY ? nsGkAtoms::systemProperty :
             mType == ELEMENT_AVAILABLE ? nsGkAtoms::elementAvailable :
             nsGkAtoms::functionAvailable;
    NS_ADDREF(*aAtom);

    return NS_OK;
}
#endif
