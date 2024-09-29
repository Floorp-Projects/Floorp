/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/EnumeratedRange.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/MacroForEach.h"

#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "txExecutionState.h"
#include "txExpr.h"
#include "txIXPathContext.h"
#include "txIEXSLTFunctions.h"
#include "txNodeSet.h"
#include "txOutputFormat.h"
#include "txRtfHandler.h"
#include "txXPathTreeWalker.h"
#include "nsImportModule.h"
#include "nsPrintfCString.h"
#include "nsComponentManagerUtils.h"
#include "nsContentCreatorFunctions.h"
#include "nsIContent.h"
#include "txMozillaXMLOutput.h"
#include "nsTextNode.h"
#include "mozilla/dom/DocumentFragmentBinding.h"
#include "prtime.h"
#include "xpcprivate.h"

using namespace mozilla;
using namespace mozilla::dom;

class txStylesheetCompilerState;

// ------------------------------------------------------------------
// Utility functions
// ------------------------------------------------------------------

static Document* getSourceDocument(txIEvalContext* aContext) {
  txExecutionState* es =
      static_cast<txExecutionState*>(aContext->getPrivateContext());
  if (!es) {
    NS_ERROR("Need txExecutionState!");

    return nullptr;
  }

  const txXPathNode& document = es->getSourceDocument();
  return txXPathNativeNode::getDocument(document);
}

static nsresult convertRtfToNode(txIEvalContext* aContext,
                                 txResultTreeFragment* aRtf) {
  Document* doc = getSourceDocument(aContext);
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<DocumentFragment> domFragment =
      new (doc->NodeInfoManager()) DocumentFragment(doc->NodeInfoManager());

  txOutputFormat format;
  txMozillaXMLOutput mozHandler(&format, domFragment, true);

  nsresult rv = aRtf->flushToHandler(&mozHandler);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mozHandler.closePrevious(true);
  NS_ENSURE_SUCCESS(rv, rv);

  // The txResultTreeFragment will own this.
  const txXPathNode* node =
      txXPathNativeNode::createXPathNode(domFragment, true);
  NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);

  aRtf->setNode(node);

  return NS_OK;
}

static nsresult createTextNode(txIEvalContext* aContext, nsString& aValue,
                               txXPathNode** aResult) {
  Document* doc = getSourceDocument(aContext);
  if (!doc) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<nsTextNode> text =
      new (doc->NodeInfoManager()) nsTextNode(doc->NodeInfoManager());

  nsresult rv = text->SetText(aValue, false);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = txXPathNativeNode::createXPathNode(text, true);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

static nsresult createAndAddToResult(nsAtom* aName, const nsAString& aValue,
                                     txNodeSet* aResultSet,
                                     DocumentFragment* aResultHolder) {
  Document* doc = aResultHolder->OwnerDoc();
  nsCOMPtr<Element> elem =
      doc->CreateElem(nsDependentAtomString(aName), nullptr, kNameSpaceID_None);
  NS_ENSURE_TRUE(elem, NS_ERROR_NULL_POINTER);

  RefPtr<nsTextNode> text =
      new (doc->NodeInfoManager()) nsTextNode(doc->NodeInfoManager());

  nsresult rv = text->SetText(aValue, false);
  NS_ENSURE_SUCCESS(rv, rv);

  ErrorResult error;
  elem->AppendChildTo(text, false, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  aResultHolder->AppendChildTo(elem, false, error);
  if (error.Failed()) {
    return error.StealNSResult();
  }

  UniquePtr<txXPathNode> xpathNode(
      txXPathNativeNode::createXPathNode(elem, true));
  NS_ENSURE_TRUE(xpathNode, NS_ERROR_OUT_OF_MEMORY);

  aResultSet->append(*xpathNode);

  return NS_OK;
}

// Need to update this array if types are added to the ResultType enum in
// txAExprResult.
static const char* const sTypes[] = {"node-set", "boolean", "number", "string",
                                     "RTF"};

// ------------------------------------------------------------------
// Function implementations
// ------------------------------------------------------------------

enum class txEXSLTType {
  // http://exslt.org/common
  NODE_SET,
  OBJECT_TYPE,

  // http://exslt.org/dates-and-times
  DATE_TIME,

  // http://exslt.org/math
  MAX,
  MIN,
  HIGHEST,
  LOWEST,

  // http://exslt.org/regular-expressions
  MATCH,
  REPLACE,
  TEST,

  // http://exslt.org/sets
  DIFFERENCE_,  // not DIFFERENCE to avoid a conflict with a winuser.h macro
  DISTINCT,
  HAS_SAME_NODE,
  INTERSECTION,
  LEADING,
  TRAILING,

  // http://exslt.org/strings
  CONCAT,
  SPLIT,
  TOKENIZE,

  _LIMIT,
};

struct txEXSLTFunctionDescriptor {
  int8_t mMinParams;
  int8_t mMaxParams;
  Expr::ResultType mReturnType;
  nsStaticAtom* mName;
  bool (*mCreator)(txEXSLTType, FunctionCall**);
  int32_t mNamespaceID;
};

static EnumeratedArray<txEXSLTType, txEXSLTFunctionDescriptor,
                       size_t(txEXSLTType::_LIMIT)>
    descriptTable;

class txEXSLTFunctionCall : public FunctionCall {
 public:
  explicit txEXSLTFunctionCall(txEXSLTType aType) : mType(aType) {}

  static bool Create(txEXSLTType aType, FunctionCall** aFunction) {
    *aFunction = new txEXSLTFunctionCall(aType);
    return true;
  }

  TX_DECL_FUNCTION

 private:
  txEXSLTType mType;
};

class txEXSLTRegExFunctionCall : public FunctionCall {
 public:
  explicit txEXSLTRegExFunctionCall(txEXSLTType aType) : mType(aType) {}

  static bool Create(txEXSLTType aType, FunctionCall** aFunction) {
    *aFunction = new txEXSLTRegExFunctionCall(aType);
    return true;
  }

  TX_DECL_FUNCTION

 private:
  txEXSLTType mType;
};

nsresult txEXSLTFunctionCall::evaluate(txIEvalContext* aContext,
                                       txAExprResult** aResult) {
  *aResult = nullptr;
  if (!requireParams(descriptTable[mType].mMinParams,
                     descriptTable[mType].mMaxParams, aContext)) {
    return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
  }

  nsresult rv = NS_OK;
  switch (mType) {
    case txEXSLTType::NODE_SET: {
      RefPtr<txAExprResult> exprResult;
      rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult));
      NS_ENSURE_SUCCESS(rv, rv);

      if (exprResult->getResultType() == txAExprResult::NODESET) {
        exprResult.forget(aResult);
      } else {
        RefPtr<txNodeSet> resultSet;
        rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
        NS_ENSURE_SUCCESS(rv, rv);

        if (exprResult->getResultType() ==
            txAExprResult::RESULT_TREE_FRAGMENT) {
          txResultTreeFragment* rtf =
              static_cast<txResultTreeFragment*>(exprResult.get());

          const txXPathNode* node = rtf->getNode();
          if (!node) {
            rv = convertRtfToNode(aContext, rtf);
            NS_ENSURE_SUCCESS(rv, rv);

            node = rtf->getNode();
          }

          resultSet->append(*node);
        } else {
          nsAutoString value;
          exprResult->stringValue(value);

          UniquePtr<txXPathNode> node;
          rv = createTextNode(aContext, value, getter_Transfers(node));
          NS_ENSURE_SUCCESS(rv, rv);

          resultSet->append(*node);
        }

        NS_ADDREF(*aResult = resultSet);
      }

      return NS_OK;
    }
    case txEXSLTType::OBJECT_TYPE: {
      RefPtr<txAExprResult> exprResult;
      nsresult rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<StringResult> strRes;
      rv = aContext->recycler()->getStringResult(getter_AddRefs(strRes));
      NS_ENSURE_SUCCESS(rv, rv);

      AppendASCIItoUTF16(MakeStringSpan(sTypes[exprResult->getResultType()]),
                         strRes->mValue);

      NS_ADDREF(*aResult = strRes);

      return NS_OK;
    }
    case txEXSLTType::DIFFERENCE_:
    case txEXSLTType::INTERSECTION: {
      RefPtr<txNodeSet> nodes1;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes1));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<txNodeSet> nodes2;
      rv = evaluateToNodeSet(mParams[1], aContext, getter_AddRefs(nodes2));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      bool insertOnFound = mType == txEXSLTType::INTERSECTION;

      int32_t searchPos = 0;
      int32_t i, len = nodes1->size();
      for (i = 0; i < len; ++i) {
        const txXPathNode& node = nodes1->get(i);
        int32_t foundPos = nodes2->indexOf(node, searchPos);
        if (foundPos >= 0) {
          searchPos = foundPos + 1;
        }

        if ((foundPos >= 0) == insertOnFound) {
          rv = resultSet->append(node);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ADDREF(*aResult = resultSet);

      return NS_OK;
    }
    case txEXSLTType::DISTINCT: {
      RefPtr<txNodeSet> nodes;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      nsTHashSet<nsString> hash;

      int32_t i, len = nodes->size();
      for (i = 0; i < len; ++i) {
        nsAutoString str;
        const txXPathNode& node = nodes->get(i);
        txXPathNodeUtils::appendNodeValue(node, str);
        if (hash.EnsureInserted(str)) {
          rv = resultSet->append(node);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ADDREF(*aResult = resultSet);

      return NS_OK;
    }
    case txEXSLTType::HAS_SAME_NODE: {
      RefPtr<txNodeSet> nodes1;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes1));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<txNodeSet> nodes2;
      rv = evaluateToNodeSet(mParams[1], aContext, getter_AddRefs(nodes2));
      NS_ENSURE_SUCCESS(rv, rv);

      bool found = false;
      int32_t i, len = nodes1->size();
      for (i = 0; i < len; ++i) {
        if (nodes2->contains(nodes1->get(i))) {
          found = true;
          break;
        }
      }

      aContext->recycler()->getBoolResult(found, aResult);

      return NS_OK;
    }
    case txEXSLTType::LEADING:
    case txEXSLTType::TRAILING: {
      RefPtr<txNodeSet> nodes1;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes1));
      NS_ENSURE_SUCCESS(rv, rv);

      RefPtr<txNodeSet> nodes2;
      rv = evaluateToNodeSet(mParams[1], aContext, getter_AddRefs(nodes2));
      NS_ENSURE_SUCCESS(rv, rv);

      if (nodes2->isEmpty()) {
        *aResult = nodes1;
        NS_ADDREF(*aResult);

        return NS_OK;
      }

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      int32_t end = nodes1->indexOf(nodes2->get(0));
      if (end >= 0) {
        int32_t i = 0;
        if (mType == txEXSLTType::TRAILING) {
          i = end + 1;
          end = nodes1->size();
        }
        for (; i < end; ++i) {
          rv = resultSet->append(nodes1->get(i));
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ADDREF(*aResult = resultSet);

      return NS_OK;
    }
    case txEXSLTType::CONCAT: {
      RefPtr<txNodeSet> nodes;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString str;
      int32_t i, len = nodes->size();
      for (i = 0; i < len; ++i) {
        txXPathNodeUtils::appendNodeValue(nodes->get(i), str);
      }

      return aContext->recycler()->getStringResult(str, aResult);
    }
    case txEXSLTType::SPLIT:
    case txEXSLTType::TOKENIZE: {
      // Evaluate parameters
      nsAutoString string;
      rv = mParams[0]->evaluateToString(aContext, string);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString pattern;
      if (mParams.Length() == 2) {
        rv = mParams[1]->evaluateToString(aContext, pattern);
        NS_ENSURE_SUCCESS(rv, rv);
      } else if (mType == txEXSLTType::SPLIT) {
        pattern.Assign(' ');
      } else {
        pattern.AssignLiteral("\t\r\n ");
      }

      // Set up holders for the result
      Document* sourceDoc = getSourceDocument(aContext);
      NS_ENSURE_STATE(sourceDoc);

      RefPtr<DocumentFragment> docFrag = new (sourceDoc->NodeInfoManager())
          DocumentFragment(sourceDoc->NodeInfoManager());

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t tailIndex;

      // Start splitting
      if (pattern.IsEmpty()) {
        nsString::const_char_iterator start = string.BeginReading();
        nsString::const_char_iterator end = string.EndReading();
        for (; start < end; ++start) {
          rv = createAndAddToResult(nsGkAtoms::token,
                                    Substring(start, start + 1), resultSet,
                                    docFrag);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        tailIndex = string.Length();
      } else if (mType == txEXSLTType::SPLIT) {
        nsAString::const_iterator strStart, strEnd;
        string.BeginReading(strStart);
        string.EndReading(strEnd);
        nsAString::const_iterator start = strStart, end = strEnd;

        while (FindInReadable(pattern, start, end)) {
          if (start != strStart) {
            rv = createAndAddToResult(nsGkAtoms::token,
                                      Substring(strStart, start), resultSet,
                                      docFrag);
            NS_ENSURE_SUCCESS(rv, rv);
          }
          strStart = start = end;
          end = strEnd;
        }

        tailIndex = strStart.get() - string.get();
      } else {
        int32_t found, start = 0;
        while ((found = string.FindCharInSet(pattern, start)) != kNotFound) {
          if (found != start) {
            rv = createAndAddToResult(nsGkAtoms::token,
                                      Substring(string, start, found - start),
                                      resultSet, docFrag);
            NS_ENSURE_SUCCESS(rv, rv);
          }
          start = found + 1;
        }

        tailIndex = start;
      }

      // Add tail if needed
      if (tailIndex != (uint32_t)string.Length()) {
        rv = createAndAddToResult(
            nsGkAtoms::token, Substring(string, tailIndex), resultSet, docFrag);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      NS_ADDREF(*aResult = resultSet);

      return NS_OK;
    }
    case txEXSLTType::MAX:
    case txEXSLTType::MIN: {
      RefPtr<txNodeSet> nodes;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);

      if (nodes->isEmpty()) {
        return aContext->recycler()->getNumberResult(UnspecifiedNaN<double>(),
                                                     aResult);
      }

      bool findMax = mType == txEXSLTType::MAX;

      double res = findMax ? mozilla::NegativeInfinity<double>()
                           : mozilla::PositiveInfinity<double>();
      int32_t i, len = nodes->size();
      for (i = 0; i < len; ++i) {
        nsAutoString str;
        txXPathNodeUtils::appendNodeValue(nodes->get(i), str);
        double val = txDouble::toDouble(str);
        if (std::isnan(val)) {
          res = UnspecifiedNaN<double>();
          break;
        }

        if (findMax ? (val > res) : (val < res)) {
          res = val;
        }
      }

      return aContext->recycler()->getNumberResult(res, aResult);
    }
    case txEXSLTType::HIGHEST:
    case txEXSLTType::LOWEST: {
      RefPtr<txNodeSet> nodes;
      rv = evaluateToNodeSet(mParams[0], aContext, getter_AddRefs(nodes));
      NS_ENSURE_SUCCESS(rv, rv);

      if (nodes->isEmpty()) {
        NS_ADDREF(*aResult = nodes);

        return NS_OK;
      }

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      bool findMax = mType == txEXSLTType::HIGHEST;
      double res = findMax ? mozilla::NegativeInfinity<double>()
                           : mozilla::PositiveInfinity<double>();
      int32_t i, len = nodes->size();
      for (i = 0; i < len; ++i) {
        nsAutoString str;
        const txXPathNode& node = nodes->get(i);
        txXPathNodeUtils::appendNodeValue(node, str);
        double val = txDouble::toDouble(str);
        if (std::isnan(val)) {
          resultSet->clear();
          break;
        }
        if (findMax ? (val > res) : (val < res)) {
          resultSet->clear();
          res = val;
        }

        if (res == val) {
          rv = resultSet->append(node);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ADDREF(*aResult = resultSet);

      return NS_OK;
    }
    case txEXSLTType::DATE_TIME: {
      // http://exslt.org/date/functions/date-time/

      PRExplodedTime prtime;
      Document* sourceDoc = getSourceDocument(aContext);
      NS_ENSURE_STATE(sourceDoc);

      PRTimeParamFn timezone =
          sourceDoc->ShouldResistFingerprinting(RFPTarget::JSDateTimeUTC)
              ? PR_GMTParameters
              : PR_LocalTimeParameters;

      PRTime time =
          sourceDoc->ShouldResistFingerprinting(RFPTarget::ReduceTimerPrecision)
              ? (PRTime)nsRFPService::ReduceTimePrecisionAsSecs(
                    (double)PR_Now() / PR_USEC_PER_SEC, 0,
                    RTPCallerType::ResistFingerprinting) *
                    PR_USEC_PER_SEC
              : PR_Now();
      PR_ExplodeTime(time, timezone, &prtime);

      int32_t offset =
          (prtime.tm_params.tp_gmt_offset + prtime.tm_params.tp_dst_offset) /
          60;

      bool isneg = offset < 0;
      if (isneg) offset = -offset;

      StringResult* strRes;
      rv = aContext->recycler()->getStringResult(&strRes);
      NS_ENSURE_SUCCESS(rv, rv);

      // format: YYYY-MM-DDTTHH:MM:SS.sss+00:00
      CopyASCIItoUTF16(
          nsPrintfCString("%04hd-%02" PRId32 "-%02" PRId32 "T%02" PRId32
                          ":%02" PRId32 ":%02" PRId32 ".%03" PRId32
                          "%c%02" PRId32 ":%02" PRId32,
                          prtime.tm_year, prtime.tm_month + 1, prtime.tm_mday,
                          prtime.tm_hour, prtime.tm_min, prtime.tm_sec,
                          prtime.tm_usec / 10000, isneg ? '-' : '+',
                          offset / 60, offset % 60),
          strRes->mValue);

      *aResult = strRes;

      return NS_OK;
    }
    default: {
      aContext->receiveError(u"Internal error"_ns, NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Missing return?");
  return NS_ERROR_UNEXPECTED;
}

Expr::ResultType txEXSLTFunctionCall::getReturnType() {
  return descriptTable[mType].mReturnType;
}

bool txEXSLTFunctionCall::isSensitiveTo(ContextSensitivity aContext) {
  if (mType == txEXSLTType::NODE_SET || mType == txEXSLTType::SPLIT ||
      mType == txEXSLTType::TOKENIZE || mType == txEXSLTType::DATE_TIME) {
    return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
  }
  return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void txEXSLTFunctionCall::appendName(nsAString& aDest) {
  aDest.Append(descriptTable[mType].mName->GetUTF16String());
}
#endif

nsresult txEXSLTRegExFunctionCall::evaluate(txIEvalContext* aContext,
                                            txAExprResult** aResult) {
  *aResult = nullptr;
  if (!requireParams(descriptTable[mType].mMinParams,
                     descriptTable[mType].mMaxParams, aContext)) {
    return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
  }

  nsAutoString string;
  nsresult rv = mParams[0]->evaluateToString(aContext, string);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString regex;
  rv = mParams[1]->evaluateToString(aContext, regex);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString flags;
  if (mParams.Length() >= 3) {
    rv = mParams[2]->evaluateToString(aContext, flags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<txIEXSLTFunctions> funcs =
      do_ImportESModule("resource://gre/modules/txEXSLTRegExFunctions.sys.mjs");
  MOZ_ALWAYS_TRUE(funcs);

  switch (mType) {
    case txEXSLTType::MATCH: {
      nsCOMPtr<Document> sourceDoc = getSourceDocument(aContext);
      NS_ENSURE_STATE(sourceDoc);

      RefPtr<DocumentFragment> docFrag;
      rv = funcs->Match(string, regex, flags, sourceDoc,
                        getter_AddRefs(docFrag));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_STATE(docFrag);

      RefPtr<txNodeSet> resultSet;
      rv = aContext->recycler()->getNodeSet(getter_AddRefs(resultSet));
      NS_ENSURE_SUCCESS(rv, rv);

      UniquePtr<txXPathNode> node;
      for (nsIContent* result = docFrag->GetFirstChild(); result;
           result = result->GetNextSibling()) {
        node = WrapUnique(txXPathNativeNode::createXPathNode(result, true));
        rv = resultSet->add(*node);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      resultSet.forget(aResult);

      return NS_OK;
    }
    case txEXSLTType::REPLACE: {
      nsAutoString replace;
      rv = mParams[3]->evaluateToString(aContext, replace);
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString result;
      rv = funcs->Replace(string, regex, flags, replace, result);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = aContext->recycler()->getStringResult(result, aResult);
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
    case txEXSLTType::TEST: {
      bool result;
      rv = funcs->Test(string, regex, flags, &result);
      NS_ENSURE_SUCCESS(rv, rv);

      aContext->recycler()->getBoolResult(result, aResult);

      return NS_OK;
    }
    default: {
      aContext->receiveError(u"Internal error"_ns, NS_ERROR_UNEXPECTED);
      return NS_ERROR_UNEXPECTED;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Missing return?");
  return NS_ERROR_UNEXPECTED;
}

Expr::ResultType txEXSLTRegExFunctionCall::getReturnType() {
  return descriptTable[mType].mReturnType;
}

bool txEXSLTRegExFunctionCall::isSensitiveTo(ContextSensitivity aContext) {
  if (mType == txEXSLTType::MATCH) {
    return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
  }
  return argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
void txEXSLTRegExFunctionCall::appendName(nsAString& aDest) {
  aDest.Append(descriptTable[mType].mName->GetUTF16String());
}
#endif

extern nsresult TX_ConstructEXSLTFunction(nsAtom* aName, int32_t aNamespaceID,
                                          txStylesheetCompilerState* aState,
                                          FunctionCall** aResult) {
  for (auto i : MakeEnumeratedRange(txEXSLTType::_LIMIT)) {
    const txEXSLTFunctionDescriptor& desc = descriptTable[i];
    if (aName == desc.mName && aNamespaceID == desc.mNamespaceID) {
      return desc.mCreator(i, aResult) ? NS_OK : NS_ERROR_FAILURE;
    }
  }

  return NS_ERROR_XPATH_UNKNOWN_FUNCTION;
}

extern bool TX_InitEXSLTFunction() {
#define EXSLT_FUNCS(NS, CLASS, ...)                                         \
  {                                                                         \
    int32_t nsid = txNamespaceManager::getNamespaceID(nsLiteralString(NS)); \
    if (nsid == kNameSpaceID_Unknown) {                                     \
      return false;                                                         \
    }                                                                       \
    MOZ_FOR_EACH(EXSLT_FUNC, (nsid, CLASS, ), (__VA_ARGS__))                \
  }

#define EXSLT_FUNC(NS, CLASS, ...)                      \
  descriptTable[txEXSLTType::MOZ_ARG_1 __VA_ARGS__] = { \
      MOZ_ARGS_AFTER_1 __VA_ARGS__, CLASS::Create, NS};

  EXSLT_FUNCS(u"http://exslt.org/common", txEXSLTFunctionCall,
              (NODE_SET, 1, 1, Expr::NODESET_RESULT, nsGkAtoms::nodeSet),
              (OBJECT_TYPE, 1, 1, Expr::STRING_RESULT, nsGkAtoms::objectType))

  EXSLT_FUNCS(u"http://exslt.org/dates-and-times", txEXSLTFunctionCall,
              (DATE_TIME, 0, 0, Expr::STRING_RESULT, nsGkAtoms::dateTime))

  EXSLT_FUNCS(u"http://exslt.org/math", txEXSLTFunctionCall,
              (MAX, 1, 1, Expr::NUMBER_RESULT, nsGkAtoms::max),
              (MIN, 1, 1, Expr::NUMBER_RESULT, nsGkAtoms::min),
              (HIGHEST, 1, 1, Expr::NODESET_RESULT, nsGkAtoms::highest),
              (LOWEST, 1, 1, Expr::NODESET_RESULT, nsGkAtoms::lowest))

  EXSLT_FUNCS(u"http://exslt.org/regular-expressions", txEXSLTRegExFunctionCall,
              (MATCH, 2, 3, Expr::NODESET_RESULT, nsGkAtoms::match),
              (REPLACE, 4, 4, Expr::STRING_RESULT, nsGkAtoms::replace),
              (TEST, 2, 3, Expr::BOOLEAN_RESULT, nsGkAtoms::test))

  EXSLT_FUNCS(
      u"http://exslt.org/sets", txEXSLTFunctionCall,
      (DIFFERENCE_, 2, 2, Expr::NODESET_RESULT, nsGkAtoms::difference),
      (DISTINCT, 1, 1, Expr::NODESET_RESULT, nsGkAtoms::distinct),
      (HAS_SAME_NODE, 2, 2, Expr::BOOLEAN_RESULT, nsGkAtoms::hasSameNode),
      (INTERSECTION, 2, 2, Expr::NODESET_RESULT, nsGkAtoms::intersection),
      (LEADING, 2, 2, Expr::NODESET_RESULT, nsGkAtoms::leading),
      (TRAILING, 2, 2, Expr::NODESET_RESULT, nsGkAtoms::trailing))

  EXSLT_FUNCS(u"http://exslt.org/strings", txEXSLTFunctionCall,
              (CONCAT, 1, 1, Expr::STRING_RESULT, nsGkAtoms::concat),
              (SPLIT, 1, 2, Expr::STRING_RESULT, nsGkAtoms::split),
              (TOKENIZE, 1, 2, Expr::STRING_RESULT, nsGkAtoms::tokenize))

#undef EXSLT_FUNCS
#undef EXSLT_FUNC
#undef EXSLT_FUNC_HELPER
#undef EXSLT_FUNC_HELPER2

  return true;
}
