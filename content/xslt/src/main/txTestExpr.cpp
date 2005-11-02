/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPCOM.h"
#include "txStandaloneXSLTProcessor.h"
#include "nsString.h"
#include "ExprParser.h"
#include "txIXPathContext.h"

/**
 * A ExprParser test exe
 */

static const char* kTokens[] = {"(", "concat", "(", "foo", ",", "'", "bar",
                                "'",")", "//", ".", "[", "preceding-sibling",
                                "::", "bar", "]", "/", "*", "[", "23", "]",
                                "|", "node", "(", ")", ")", "<", "3"};
static const PRUint8 kCount = sizeof(kTokens)/sizeof(char*);

class ParseContextImpl : public txIParseContext
{
public:
    nsresult
    resolveNamespacePrefix(nsIAtom* aPrefix, PRInt32& aID)
    {
        return NS_ERROR_FAILURE;
    }
    nsresult
    resolveFunctionCall(nsIAtom* aName, PRInt32 aID, FunctionCall*& aFunction)
    {
        return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
    }
    PRBool
    caseInsensitiveNameTests()
    {
        return PR_FALSE;
    }
    void
    SetErrorOffset(PRUint32 aOffset)
    {
        mOff = aOffset;
    }
    PRUint32 mOff;
};

static void doTest(const nsASingleFragmentString& aExpr)
{
    ParseContextImpl ct;
    nsAutoPtr<Expr> expression;
    cout << NS_LossyConvertUTF16toASCII(aExpr).get() << endl;
    ct.mOff = 0;
    nsresult rv = txExprParser::createExpr(aExpr, &ct,
                                           getter_Transfers(expression));

    cout << "createExpr returned " << ios::hex << rv  << ios::dec;
    cout << " at " << ct.mOff << endl;
    if (NS_FAILED(rv)) {
        NS_LossyConvertUTF16toASCII cstring(aExpr);
        cout << NS_LossyConvertUTF16toASCII(StringHead(aExpr, ct.mOff)).get();
        cout << " ^ ";
        cout << NS_LossyConvertUTF16toASCII(StringTail(aExpr, aExpr.Length()-ct.mOff)).get();
        cout << endl << endl;
    }
#ifdef TX_TO_STRING
    else {
        nsAutoString expr;
        expression->toString(expr);
        cout << "parsed expression: ";
        cout << NS_LossyConvertUTF16toASCII(expr).get() << endl << endl;
    }
#endif
}

int main(int argc, char** argv)
{
    using namespace std;
    nsresult rv = NS_InitXPCOM2(nsnull, nsnull, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!txXSLTProcessor::init())
        return 1;

    nsAutoString exprOrig, expr;
    nsStringArray exprHead, exprTail;
    PRUint8 i, dropStart, dropEnd;
    exprHead.AppendString(NS_ConvertASCIItoUTF16(kTokens[0]));
    exprTail.AppendString(NS_ConvertASCIItoUTF16(kTokens[kCount - 1]));
    for (i = 2; i < kCount; ++i) {
        exprHead.AppendString(*exprHead[i - 2] +
                              NS_ConvertASCIItoUTF16(kTokens[i - 1]));
        exprTail.AppendString(NS_ConvertASCIItoUTF16(kTokens[kCount - i]) +
                              *exprTail[i - 2]);
    }
    exprOrig = NS_ConvertASCIItoUTF16(kTokens[0]) + *exprTail[kCount - 2];
    cout << NS_LossyConvertUTF16toASCII(exprOrig).get() << endl << endl;
    for (dropStart = 0; dropStart < kCount - 2; ++dropStart) {
        doTest(*exprTail[kCount - 2 - dropStart]);
        for (dropEnd = kCount - 3 - dropStart; dropEnd > 0; --dropEnd) {
            expr = *exprHead[dropStart] + *exprTail[dropEnd];
            doTest(expr);
        }
        doTest(*exprHead[dropStart]);
    }
    doTest(*exprHead[kCount - 2]);

    txXSLTProcessor::shutdown();
    rv = NS_ShutdownXPCOM(nsnull);
    NS_ENSURE_SUCCESS(rv, rv);
    return 0;
}
