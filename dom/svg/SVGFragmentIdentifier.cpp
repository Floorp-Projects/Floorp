/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGFragmentIdentifier.h"

#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGViewElement.h"
#include "nsContentUtils.h" // for nsCharSeparatedTokenizerTemplate
#include "nsSVGAnimatedTransformList.h"
#include "nsCharSeparatedTokenizer.h"

namespace mozilla {

using namespace dom;

static bool
IsMatchingParameter(const nsAString& aString, const nsAString& aParameterName)
{
  // The first two tests ensure aString.Length() > aParameterName.Length()
  // so it's then safe to do the third test
  return StringBeginsWith(aString, aParameterName) &&
         aString.Last() == ')' &&
         aString.CharAt(aParameterName.Length()) == '(';
}

inline bool
IgnoreWhitespace(char16_t aChar)
{
  return false;
}

static SVGViewElement*
GetViewElement(nsIDocument* aDocument, const nsAString& aId)
{
  Element* element = aDocument->GetElementById(aId);
  return (element && element->IsSVGElement(nsGkAtoms::view)) ?
            static_cast<SVGViewElement*>(element) : nullptr;
}

// Handles setting/clearing the root's mSVGView pointer.
class MOZ_RAII AutoSVGViewHandler
{
public:
  explicit AutoSVGViewHandler(SVGSVGElement* aRoot
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    : mRoot(aRoot), mValid(false) {
    MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    mWasOverridden = mRoot->UseCurrentView();
    mRoot->mSVGView = nullptr;
    mRoot->mCurrentViewID = nullptr;
  }

  ~AutoSVGViewHandler() {
    if (!mWasOverridden && !mValid) {
      // we weren't overridden before and we aren't
      // overridden now so nothing has changed.
      return;
    }
    if (mValid) {
      mRoot->mSVGView = mSVGView;
    }
    mRoot->InvalidateTransformNotifyFrame();
  }

  void CreateSVGView() {
    MOZ_ASSERT(!mSVGView, "CreateSVGView should not be called multiple times");
    mSVGView = new SVGView();
  }

  bool ProcessAttr(const nsAString& aToken, const nsAString &aParams) {

    MOZ_ASSERT(mSVGView, "CreateSVGView should have been called");

    // SVGViewAttributes may occur in any order, but each type may only occur
    // at most one time in a correctly formed SVGViewSpec.
    // If we encounter any attribute more than once or get any syntax errors
    // we're going to return false and cancel any changes.

    if (IsMatchingParameter(aToken, NS_LITERAL_STRING("viewBox"))) {
      if (mSVGView->mViewBox.IsExplicitlySet() ||
          NS_FAILED(mSVGView->mViewBox.SetBaseValueString(
                      aParams, mRoot, false))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, NS_LITERAL_STRING("preserveAspectRatio"))) {
      if (mSVGView->mPreserveAspectRatio.IsExplicitlySet() ||
          NS_FAILED(mSVGView->mPreserveAspectRatio.SetBaseValueString(
                      aParams, mRoot, false))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, NS_LITERAL_STRING("transform"))) {
      if (mSVGView->mTransforms) {
        return false;
      }
      mSVGView->mTransforms = new nsSVGAnimatedTransformList();
      if (NS_FAILED(mSVGView->mTransforms->SetBaseValueString(aParams))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, NS_LITERAL_STRING("zoomAndPan"))) {
      if (mSVGView->mZoomAndPan.IsExplicitlySet()) {
        return false;
      }
      nsIAtom* valAtom = NS_GetStaticAtom(aParams);
      if (!valAtom ||
          NS_FAILED(mSVGView->mZoomAndPan.SetBaseValueAtom(
                      valAtom, mRoot))) {
        return false;
      }
    } else {
      // We don't support viewTarget currently
      return false;
    }
    return true;
  }

  void SetValid() {
    mValid = true;
  }

private:
  SVGSVGElement*     mRoot;
  nsAutoPtr<SVGView> mSVGView;
  bool               mValid;
  bool               mWasOverridden;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

bool
SVGFragmentIdentifier::ProcessSVGViewSpec(const nsAString& aViewSpec,
                                          SVGSVGElement* aRoot)
{
  AutoSVGViewHandler viewHandler(aRoot);

  if (!IsMatchingParameter(aViewSpec, NS_LITERAL_STRING("svgView"))) {
    return false;
  }

  // Each token is a SVGViewAttribute
  int32_t bracketPos = aViewSpec.FindChar('(');
  uint32_t lengthOfViewSpec = aViewSpec.Length() - bracketPos - 2;
  nsCharSeparatedTokenizerTemplate<IgnoreWhitespace> tokenizer(
    Substring(aViewSpec, bracketPos + 1, lengthOfViewSpec), ';');

  if (!tokenizer.hasMoreTokens()) {
    return false;
  }
  viewHandler.CreateSVGView();

  do {

    nsAutoString token(tokenizer.nextToken());

    bracketPos = token.FindChar('(');
    if (bracketPos < 1 || token.Last() != ')') {
      // invalid SVGViewAttribute syntax
      return false;
    }

    const nsAString &params =
      Substring(token, bracketPos + 1, token.Length() - bracketPos - 2);

    if (!viewHandler.ProcessAttr(token, params)) {
      return false;
    }

  } while (tokenizer.hasMoreTokens());

  viewHandler.SetValid();
  return true;
}

bool
SVGFragmentIdentifier::ProcessFragmentIdentifier(nsIDocument* aDocument,
                                                 const nsAString& aAnchorName)
{
  MOZ_ASSERT(aDocument->GetRootElement()->IsSVGElement(nsGkAtoms::svg),
             "expecting an SVG root element");

  SVGSVGElement* rootElement =
    static_cast<SVGSVGElement*>(aDocument->GetRootElement());

  const SVGViewElement* viewElement = GetViewElement(aDocument, aAnchorName);

  if (viewElement) {
    if (!rootElement->mCurrentViewID) {
      rootElement->mCurrentViewID = new nsString();
    }
    *rootElement->mCurrentViewID = aAnchorName;
    rootElement->mSVGView = nullptr;
    rootElement->InvalidateTransformNotifyFrame();
    // not an svgView()-style fragment identifier, return false so the caller
    // continues processing to match any :target pseudo elements
    return false;
  }

  return ProcessSVGViewSpec(aAnchorName, rootElement);
}

} // namespace mozilla
