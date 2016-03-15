/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationUtils.h"

#include "nsCSSParser.h" // For nsCSSParser
#include "nsDebug.h"
#include "nsIAtom.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsGlobalWindow.h"
#include "nsString.h"
#include "mozilla/Attributes.h"
#include "mozilla/ComputedTimingFunction.h" // ComputedTimingFunction
#include "xpcpublic.h" // For xpc::NativeGlobal

namespace mozilla {

/* static */ void
AnimationUtils::LogAsyncAnimationFailure(nsCString& aMessage,
                                         const nsIContent* aContent)
{
  if (aContent) {
    aMessage.AppendLiteral(" [");
    aMessage.Append(nsAtomCString(aContent->NodeInfo()->NameAtom()));

    nsIAtom* id = aContent->GetID();
    if (id) {
      aMessage.AppendLiteral(" with id '");
      aMessage.Append(nsAtomCString(aContent->GetID()));
      aMessage.Append('\'');
    }
    aMessage.Append(']');
  }
  aMessage.Append('\n');
  printf_stderr("%s", aMessage.get());
}

/* static */ Maybe<ComputedTimingFunction>
AnimationUtils::ParseEasing(const nsAString& aEasing,
                            nsIDocument* aDocument)
{
  MOZ_ASSERT(aDocument);

  nsCSSValue value;
  nsCSSParser parser;
  parser.ParseLonghandProperty(eCSSProperty_animation_timing_function,
                               aEasing,
                               aDocument->GetDocumentURI(),
                               aDocument->GetDocumentURI(),
                               aDocument->NodePrincipal(),
                               value);

  switch (value.GetUnit()) {
    case eCSSUnit_List: {
      const nsCSSValueList* list = value.GetListValue();
      if (list->mNext) {
        // don't support a list of timing functions
        break;
      }
      switch (list->mValue.GetUnit()) {
        case eCSSUnit_Enumerated:
          // Return Nothing() if "linear" is passed in.
          if (list->mValue.GetIntValue() ==
              NS_STYLE_TRANSITION_TIMING_FUNCTION_LINEAR) {
            return Nothing();
          }
          MOZ_FALLTHROUGH;
        case eCSSUnit_Cubic_Bezier:
        case eCSSUnit_Steps: {
          nsTimingFunction timingFunction;
          nsRuleNode::ComputeTimingFunction(list->mValue, timingFunction);
          ComputedTimingFunction computedTimingFunction;
          computedTimingFunction.Init(timingFunction);
          return Some(computedTimingFunction);
        }
        default:
          MOZ_ASSERT_UNREACHABLE("unexpected animation-timing-function list "
                                 "item unit");
        break;
      }
      break;
    }
    case eCSSUnit_Null:
    case eCSSUnit_Inherit:
    case eCSSUnit_Initial:
    case eCSSUnit_Unset:
    case eCSSUnit_TokenStream:
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected animation-timing-function unit");
      break;
  }
  return Nothing();
}

/* static */ nsIDocument*
AnimationUtils::GetCurrentRealmDocument(JSContext* aCx)
{
  nsGlobalWindow* win = xpc::CurrentWindowOrNull(aCx);
  if (!win) {
    return nullptr;
  }
  return win->GetDoc();
}

} // namespace mozilla
