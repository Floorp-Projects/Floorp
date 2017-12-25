/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prprf.h"

#include "nsIDOMNodeList.h"
#include "nsUnicharUtils.h"

#include "nsArrayUtils.h"
#include "nsVariant.h"
#include "nsAppDirectoryServiceDefs.h"

#include "nsIURI.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsGkAtoms.h"
#include "nsContentUtils.h"

#include "nsXULTemplateBuilder.h"
#include "nsXULTemplateResultStorage.h"
#include "nsXULContentUtils.h"
#include "nsXULSortService.h"

#include "mozIStorageService.h"
#include "nsIChannel.h"
#include "nsIDocument.h"
#include "nsNetUtil.h"
#include "nsTemplateMatch.h"

//----------------------------------------------------------------------
//
// nsXULTemplateResultSetStorage
//

NS_IMPL_ISUPPORTS(nsXULTemplateResultSetStorage, nsISimpleEnumerator)


nsXULTemplateResultSetStorage::nsXULTemplateResultSetStorage(mozIStorageStatement* aStatement)
        : mStatement(aStatement)
{
    uint32_t count;
    nsresult rv = aStatement->GetColumnCount(&count);
    if (NS_FAILED(rv)) {
        mStatement = nullptr;
        return;
    }
    for (uint32_t c = 0; c < count; c++) {
        nsAutoCString name;
        rv = aStatement->GetColumnName(c, name);
        if (NS_SUCCEEDED(rv)) {
            RefPtr<nsAtom> columnName = NS_Atomize(NS_LITERAL_CSTRING("?") + name);
            mColumnNames.AppendElement(columnName);
        }
    }
}

NS_IMETHODIMP
nsXULTemplateResultSetStorage::HasMoreElements(bool *aResult)
{
    if (!mStatement) {
        *aResult = false;
        return NS_OK;
    }

    nsresult rv = mStatement->ExecuteStep(aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    // Because the nsXULTemplateResultSetStorage is owned by many nsXULTemplateResultStorage objects,
    // it could live longer than it needed to get results.
    // So we destroy the statement to free resources when all results are fetched
    if (!*aResult) {
        mStatement = nullptr;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateResultSetStorage::GetNext(nsISupports **aResult)
{
    nsXULTemplateResultStorage* result =
        new nsXULTemplateResultStorage(this);
    *aResult = result;
    NS_ADDREF(result);
    return NS_OK;
}


int32_t
nsXULTemplateResultSetStorage::GetColumnIndex(nsAtom* aColumnName)
{
    int32_t count = mColumnNames.Length();
    for (int32_t c = 0; c < count; c++) {
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

    int32_t count = mColumnNames.Length();

    for (int32_t c = 0; c < count; c++) {
        RefPtr<nsVariant> value = new nsVariant();

        int32_t type;
        mStatement->GetTypeOfIndex(c, &type);

        if (type == mStatement->VALUE_TYPE_INTEGER) {
            int64_t val = mStatement->AsInt64(c);
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

NS_IMPL_ISUPPORTS(nsXULTemplateQueryProcessorStorage,
                  nsIXULTemplateQueryProcessor)


nsXULTemplateQueryProcessorStorage::nsXULTemplateQueryProcessorStorage()
    : mGenerationStarted(false)
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
    *aReturn = nullptr;
    *aShouldDelayBuilding = false;

    if (!aIsTrusted) {
        return NS_OK;
    }

    uint32_t length;
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
    nsAutoCString scheme;
    rv = uri->GetScheme(scheme);
    NS_ENSURE_SUCCESS(rv, rv);

    if (scheme.EqualsLiteral("profile")) {

        nsAutoCString path;
        rv = uri->GetPathQueryRef(path);
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
        nsCOMPtr<nsINode> node = do_QueryInterface(aRootNode);

        // The following channel is never openend, so it does not matter what
        // securityFlags we pass; let's follow the principle of least privilege.
        rv = NS_NewChannel(getter_AddRefs(channel),
                           uri,
                           node,
                           nsILoadInfo::SEC_REQUIRE_SAME_ORIGIN_DATA_IS_BLOCKED,
                           nsIContentPolicy::TYPE_OTHER);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<nsIFileChannel> fileChannel = do_QueryInterface(channel, &rv);
        if (NS_FAILED(rv)) { // if it fails, not a file url
            nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_BAD_URI);
            return rv;
        }

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

    connection.forget(aReturn);
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
    mGenerationStarted = false;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::CompileQuery(nsIXULTemplateBuilder* aBuilder,
                                                 nsIDOMNode* aQueryNode,
                                                 nsAtom* aRefVariable,
                                                 nsAtom* aMemberVariable,
                                                 nsISupports** aReturn)
{
    nsCOMPtr<nsIDOMNodeList> childNodes;
    aQueryNode->GetChildNodes(getter_AddRefs(childNodes));

    uint32_t length;
    childNodes->GetLength(&length);

    nsCOMPtr<mozIStorageStatement> statement;
    nsCOMPtr<nsIContent> queryContent = do_QueryInterface(aQueryNode);
    nsAutoString sqlQuery;

    // Let's get all text nodes (which should be the query)
    if (!nsContentUtils::GetNodeTextContent(queryContent, false, sqlQuery, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    nsresult rv = mStorageConnection->CreateStatement(NS_ConvertUTF16toUTF8(sqlQuery),
                                                              getter_AddRefs(statement));
    if (NS_FAILED(rv)) {
        nsXULContentUtils::LogTemplateError(ERROR_TEMPLATE_STORAGE_BAD_QUERY);
        return rv;
    }

    uint32_t parameterCount = 0;
    for (nsIContent* child = queryContent->GetFirstChild();
         child;
         child = child->GetNextSibling()) {

        if (child->NodeInfo()->Equals(nsGkAtoms::param, kNameSpaceID_XUL)) {
            nsAutoString value;
            if (!nsContentUtils::GetNodeTextContent(child, false, value, fallible)) {
              return NS_ERROR_OUT_OF_MEMORY;
            }

            uint32_t index = parameterCount;
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
                  &nsGkAtoms::null, &nsGkAtoms::double_, &nsGkAtoms::string, nullptr };

            int32_t typeError = 1;
            int32_t typeValue = child->FindAttrValueIn(kNameSpaceID_None, nsGkAtoms::type,
                                                       sTypeValues, eCaseMatters);
            rv = NS_ERROR_ILLEGAL_VALUE;
            int32_t valInt32 = 0;
            int64_t valInt64 = 0;
            double valFloat = 0;

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
    mGenerationStarted = true;

    nsCOMPtr<mozIStorageStatement> statement = do_QueryInterface(aQuery);
    if (!statement)
        return NS_ERROR_FAILURE;

    nsXULTemplateResultSetStorage* results =
        new nsXULTemplateResultSetStorage(statement);
    *aResults = results;
    NS_ADDREF(*aResults);

    return NS_OK;
}

NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::AddBinding(nsIDOMNode* aRuleNode,
                                               nsAtom* aVar,
                                               nsAtom* aRef,
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
        new nsXULTemplateResultStorage(nullptr);
    *aRef = result;
    NS_ADDREF(*aRef);
    return NS_OK;
}


NS_IMETHODIMP
nsXULTemplateQueryProcessorStorage::CompareResults(nsIXULTemplateResult* aLeft,
                                                   nsIXULTemplateResult* aRight,
                                                   nsAtom* aVar,
                                                   uint32_t aSortHints,
                                                   int32_t* aResult)
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
            uint16_t vtypeL, vtypeR;
            vLeftValue->GetDataType(&vtypeL);
            vRightValue->GetDataType(&vtypeR);

            if (vtypeL == vtypeR) {
                if (vtypeL == nsIDataType::VTYPE_INT64) {
                    int64_t leftValue, rightValue;
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
