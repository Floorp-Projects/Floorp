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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Laurent Jouanneau <laurent.jouanneau@disruptive-innovations.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "prprf.h"

#include "nsIDOMNodeList.h"
#include "nsUnicharUtils.h"

#include "nsArrayUtils.h"
#include "nsIVariant.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIURI.h"
#include "nsIIOService.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"

#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateResultStorage.h"
#include "nsXULContentUtils.h"
#include "nsXULSortService.h"

#include "mozIStorageService.h"

//----------------------------------------------------------------------
//
// nsXULTemplateResultSetStorage
//

NS_IMPL_ISUPPORTS1(nsXULTemplateResultSetStorage, nsISimpleEnumerator)


nsXULTemplateResultSetStorage::nsXULTemplateResultSetStorage(mozIStorageStatement* aStatement)
        : mStatement(aStatement)
{
    PRUint32 count;
    nsresult rv = aStatement->GetColumnCount(&count);
    if (NS_FAILED(rv)) {
        mStatement = nsnull;
        return;
    }
    for (PRUint32 c = 0; c < count; c++) {
        nsCAutoString name;
        rv = aStatement->GetColumnName(c, name);
        if (NS_SUCCEEDED(rv)) {
            nsCOMPtr<nsIAtom> columnName = do_GetAtom(NS_LITERAL_CSTRING("?") + name);
            mColumnNames.AppendObject(columnName);
        }
    }
}

NS_IMETHODIMP
nsXULTemplateResultSetStorage::HasMoreElements(bool *aResult)
{
    if (!mStatement) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    nsresult rv = mStatement->ExecuteStep(aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    // Because the nsXULTemplateResultSetStorage is owned by many nsXULTemplateResultStorage objects,
    // it could live longer than it needed to get results.
    // So we destroy the statement to free resources when all results are fetched
    if (!*aResult) {
        mStatement = nsnull;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultSetStorage::GetNext(nsISupports **aResult)
{
    nsXULTemplateResultStorage* result =
        new nsXULTemplateResultStorage(this);

    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = result;
    NS_ADDREF(result);
    return NS_OK;
}


PRInt32
nsXULTemplateResultSetStorage::GetColumnIndex(nsIAtom* aColumnName)
{
    PRInt32 count = mColumnNames.Count();
    for (PRInt32 c = 0; c < count; c++) {
        if (mColumnNames[c] == aColumnName)
            return c;
    }

    return -1;
}

void
nsXULTemplateResultSetStorage::FillColumnValues(nsCOMArray<nsIVariant>& aArray)
{
    if (!mStatement)
        return;

    PRInt32 count = mColumnNames.Count();

    for (PRInt32 c = 0; c < count; c++) {
        nsCOMPtr<nsIWritableVariant> value = do_CreateInstance("@mozilla.org/variant;1");

        PRInt32 type;
        mStatement->GetTypeOfIndex(c, &type);

        if (type == mStatement->VALUE_TYPE_INTEGER) {
            PRInt64 val = mStatement->AsInt64(c);
            value->SetAsInt64(val);
        }
        else if (type == mStatement->VALUE_TYPE_FLOAT) {
            double val = mStatement->AsDouble(c);
            value->SetAsDouble(val);
        }
        else {
            nsAutoString val;
            nsresult rv = mStatement->GetString(c, val);
            if (NS_FAILED(rv))
                value->SetAsAString(EmptyString());
            else
                value->SetAsAString(val);
        }
        aArray.AppendObject(value);
    }
}



//----------------------------------------------------------------------
//
// nsXULTemplateQueryProcessorStorage
//

NS_IMPL_ISUPPORTS1(nsXULTemplateQueryProcessorStorage,
                   nsIXULTemplateQueryProcessor)


nsXULTemplateQueryProcessorStorage::nsXULTemplateQueryProcessorStorage() 
    : mGenerationStarted(PR_FALSE)
{
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::GetDatasource(nsIArray* aDataSources,
                                                  nsIDOMNode* aRootNode,
                                                  bool aIsTrusted,
                                                  nsIXULTemplateBuilder* aBuilder,
                                                  bool* aShouldDelayBuilding,
                                                  nsISupports** aReturn)
{
    *aReturn = nsnull;
    *aShouldDelayBuilding = PR_FALSE;

    if (!aIsTrusted) {
        return NS_OK;
    }

    PRUint32 length;
    nsresult rv = aDataSources->GetLength(&length);
    NS_ENSURE_SUCCESS(rv, rv);

    if (length == 0) {
        return NS_OK;
    }

    // We get only the first uri. This query processor supports
    // only one database at a time.
    nsCOMPtr<nsIURI> uri;
    uri = do_QueryElementAt(aDataSources, 0);

    if (!uri) {
        // No uri in the list of datasources
        return NS_OK;
    }

    nsCOMPtr<mozIStorageService> storage =
        do_GetService("@mozilla.org/storage/service;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIFile> databaseFile;
    nsCAutoString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    if (scheme.EqualsLiteral("profile")) {

        nsCAutoString path;
        rv = uri->GetPath(path);
        NS_ENSURE_SUCCESS(rv, rv);

        if (path.IsEmpty()) {
            return NS_ERROR_FAILURE;
        }

        rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                             getter_AddRefs(databaseFile));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = databaseFile->AppendNative(path);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        nsCOMPtr<nsIChannel> channel;
        nsCOMPtr<nsIIOService> ioservice =
            do_GetService("@mozilla.org/network/io-service;1", &rv);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = ioservice->NewChannelFromURI(uri, getter_AddRefs(channel));
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel, &rv);
        if (NS_FAILED(rv)) { // if it fails, not a file url
            nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_BAD_URI);
            return rv;
        }

        nsCOMPtr<nsIFile> file;
        rv = fileChannel->GetFile(getter_AddRefs(databaseFile));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // ok now we have an URI of a sqlite file
    nsCOMPtr<mozIStorageConnection> connection;
    rv = storage->OpenDatabase(databaseFile, getter_AddRefs(connection));
    if (NS_FAILED(rv)) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_CANNOT_OPEN_DATABASE);
        return rv;
    }

    NS_ADDREF(*aReturn = connection);
    return NS_OK;
}



NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::InitializeForBuilding(nsISupports* aDatasource,
                                                          nsIXULTemplateBuilder* aBuilder,
                                                          nsIDOMNode* aRootNode)
{
    NS_ENSURE_STATE(!mGenerationStarted);

    mStorageConnection = do_QueryInterface(aDatasource);
    if (!mStorageConnection)
        return NS_ERROR_INVALID_ARG;

    bool ready;
    mStorageConnection->GetConnectionReady(&ready);
    if (!ready)
      return NS_ERROR_UNEXPECTED;

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::Done()
{
    mGenerationStarted = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::CompileQuery(nsIXULTemplateBuilder* aBuilder,
                                                 nsIDOMNode* aQueryNode,
                                                 nsIAtom* aRefVariable,
                                                 nsIAtom* aMemberVariable,
                                                 nsISupports** aReturn)
{
    nsCOMPtr<nsIDOMNodeList> childNodes;
    aQueryNode->GetChildNodes(getter_AddRefs(childNodes));

    PRUint32 length;
    childNodes->GetLength(&length);

    nsCOMPtr<mozIStorageStatement> statement;
    nsCOMPtr<nsIContent> queryContent = do_QueryInterface(aQueryNode);
    nsAutoString sqlQuery;

    // Let's get all text nodes (which should be the query) 
    nsContentUtils::GetNodeTextContent(queryContent, PR_FALSE, sqlQuery);

    nsresult rv = mStorageConnection->CreateStatement(NS_ConvertUTF16toUTF8(sqlQuery),
                                                              getter_AddRefs(statement));
    if (NS_FAILED(rv)) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_BAD_QUERY);
        return rv;
    }

    PRUint32 parameterCount = 0;
    for (nsIContent* child = queryContent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        if (child->NodeInfo()->Equals(nsGkAtoms::param, kNameSpaceID_XUL)) {
            nsAutoString value;
            nsContentUtils::GetNodeTextContent(child, PR_FALSE, value);

            PRUint32 index = parameterCount;
            nsAutoString name, indexValue;

            if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
                rv = statement->GetParameterIndex(NS_ConvertUTF16toUTF8(name),
                                                  &index);
                if (NS_FAILED(rv)) {
                    nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_UNKNOWN_QUERY_PARAMETER);
                    return rv;
                }
                parameterCount++;
            }
            else if (child->GetAttr(kNameSpaceID_None, nsGkAtoms::index, indexValue)) {
                PR_sscanf(NS_ConvertUTF16toUTF8(indexValue).get(),"%d",&index);
                if (index > 0)
                    index--;
            }
            else {
                parameterCount++;
            }

            static nsIContent::AttrValuesArray sTypeValues[] =
                { &nsGkAtoms::int32, &nsGkAtoms::integer, &nsGkAtoms::int64,
                  &nsGkAtoms::null, &nsGkAtoms::double_, &nsGkAtoms::string, nsnull };

            PRInt32 typeError = 1;
            PRInt32 typeValue = child->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                                       sTypeValues, eCaseMatters);
            rv = NS_ERROR_ILLEGAL_VALUE;
            PRInt32 valInt32 = 0;
            PRInt64 valInt64 = 0;
            PRFloat64 valFloat = 0;

            switch (typeValue) {
              case 0:
              case 1:
                typeError = PR_sscanf(NS_ConvertUTF16toUTF8(value).get(),"%d",&valInt32);
                if (typeError > 0)
                    rv = statement->BindInt32ByIndex(index, valInt32);
                break;
              case 2:
                typeError = PR_sscanf(NS_ConvertUTF16toUTF8(value).get(),"%lld",&valInt64);
                if (typeError > 0)
                    rv = statement->BindInt64ByIndex(index, valInt64);
                break;
              case 3:
                rv = statement->BindNullByIndex(index);
                break;
              case 4:
                typeError = PR_sscanf(NS_ConvertUTF16toUTF8(value).get(),"%lf",&valFloat);
                if (typeError > 0)
                    rv = statement->BindDoubleByIndex(index, valFloat);
                break;
              case 5:
              case nsIContent::ATTR_MISSING:
                rv = statement->BindStringByIndex(index, value);
                break;
              default:
                typeError = 0;
            }

            if (typeError <= 0) {
                nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_WRONG_TYPE_QUERY_PARAMETER);
                return rv;
            }

            if (NS_FAILED(rv)) {
                nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_QUERY_PARAMETER_NOT_BOUND);
                return rv;
            }
        }
    }

    *aReturn = statement;
    NS_IF_ADDREF(*aReturn);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::GenerateResults(nsISupports* aDatasource,
                                                    nsIXULTemplateResult* aRef,
                                                    nsISupports* aQuery,
                                                    nsISimpleEnumerator** aResults)
{
    mGenerationStarted = PR_TRUE;

    nsCOMPtr<mozIStorageStatement> statement = do_QueryInterface(aQuery);
    if (!statement)
        return NS_ERROR_FAILURE;

    nsXULTemplateResultSetStorage* results =
        new nsXULTemplateResultSetStorage(statement);

    if (!results)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResults = results;
    NS_ADDREF(*aResults);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::AddBinding(nsIDOMNode* aRuleNode,
                                               nsIAtom* aVar,
                                               nsIAtom* aRef,
                                               const nsAString& aExpr)
{
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::TranslateRef(nsISupports* aDatasource,
                                                 const nsAString& aRefString,
                                                 nsIXULTemplateResult** aRef)
{
    nsXULTemplateResultStorage* result =
        new nsXULTemplateResultStorage(nsnull);
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    *aRef = result;
    NS_ADDREF(*aRef);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::CompareResults(nsIXULTemplateResult* aLeft,
                                                   nsIXULTemplateResult* aRight,
                                                   nsIAtom* aVar,
                                                   PRUint32 aSortHints,
                                                   PRInt32* aResult)
{
    *aResult = 0;
    if (!aVar)
      return NS_OK;

    // We're going to see if values are integers or float, to perform
    // a suitable comparison
    nsCOMPtr<nsISupports> leftValue, rightValue;
    if (aLeft)
      aLeft->GetBindingObjectFor(aVar, getter_AddRefs(leftValue));
    if (aRight)
      aRight->GetBindingObjectFor(aVar, getter_AddRefs(rightValue));

    if (leftValue && rightValue) {
        nsCOMPtr<nsIVariant> vLeftValue = do_QueryInterface(leftValue);
        nsCOMPtr<nsIVariant> vRightValue = do_QueryInterface(rightValue);

        if (vLeftValue && vRightValue) {
            nsresult rv1, rv2;
            PRUint16 vtypeL, vtypeR;
            vLeftValue->GetDataType(&vtypeL);
            vRightValue->GetDataType(&vtypeR);

            if (vtypeL == vtypeR) {
                if (vtypeL == nsIDataType::VTYPE_INT64) {
                    PRInt64 leftValue, rightValue;
                    rv1 = vLeftValue->GetAsInt64(&leftValue);
                    rv2 = vRightValue->GetAsInt64(&rightValue);
                    if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
                        if (leftValue > rightValue)
                            *aResult = 1;
                        else if (leftValue < rightValue)
                            *aResult = -1;
                        return NS_OK;
                    }
                }
                else if (vtypeL == nsIDataType::VTYPE_DOUBLE) {
                    double leftValue, rightValue;
                    rv1 = vLeftValue->GetAsDouble(&leftValue);
                    rv2 = vRightValue->GetAsDouble(&rightValue);
                    if (NS_SUCCEEDED(rv1) && NS_SUCCEEDED(rv2)) {
                        if (leftValue > rightValue)
                            *aResult = 1;
                        else if (leftValue < rightValue)
                            *aResult = -1;
                        return NS_OK;
                    }
                }
            }
        }
    }

    // Values are not integers or floats, so we just compare them as simple strings
    nsAutoString leftVal;
    if (aLeft)
        aLeft->GetBindingFor(aVar, leftVal);

    nsAutoString rightVal;
    if (aRight)
        aRight->GetBindingFor(aVar, rightVal);

    *aResult = XULSortServiceImpl::CompareValues(leftVal, rightVal, aSortHints);
    return NS_OK;
}
