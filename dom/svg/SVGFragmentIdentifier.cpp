/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGFragmentIdentifier.h"

#include "mozilla/dom/SVGSVGElement.h"
#include "mozilla/dom/SVGViewElement.h"
#include "mozilla/SVGOuterSVGFrame.h"
#include "nsCharSeparatedTokenizer.h"
#include "SVGAnimatedTransformList.h"

namespace mozilla {

using namespace dom;

static bool IsMatchingParameter(const nsAString& aString,
                                const nsAString& aParameterName) {
  // The first two tests ensure aString.Length() > aParameterName.Length()
  // so it's then safe to do the third test
  return StringBeginsWith(aString, aParameterName) && aString.Last() == ')' &&
         aString.CharAt(aParameterName.Length()) == '(';
}

// Handles setting/clearing the root's mSVGView pointer.
class MOZ_RAII AutoSVGViewHandler {
 public:
  explicit AutoSVGViewHandler(SVGSVGElement* aRoot)
      : mRoot(aRoot), mValid(false) {
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
      mRoot->mSVGView = std::move(mSVGView);
    }
    mRoot->InvalidateTransformNotifyFrame();
    if (nsIFrame* f = mRoot->GetPrimaryFrame()) {
      if (SVGOuterSVGFrame* osf = do_QueryFrame(f)) {
        osf->MaybeSendIntrinsicSizeAndRatioToEmbedder();
      }
    }
  }

  void CreateSVGView() {
    MOZ_ASSERT(!mSVGView, "CreateSVGView should not be called multiple times");
    mSVGView = MakeUnique<SVGView>();
  }

  bool ProcessAttr(const nsAString& aToken, const nsAString& aParams) {
    MOZ_ASSERT(mSVGView, "CreateSVGView should have been called");

    // SVGViewAttributes may occur in any order, but each type may only occur
    // at most one time in a correctly formed SVGViewSpec.
    // If we encounter any attribute more than once or get any syntax errors
    // we're going to return false and cancel any changes.

    if (IsMatchingParameter(aToken, u"viewBox"_ns)) {
      if (mSVGView->mViewBox.IsExplicitlySet() ||
          NS_FAILED(
              mSVGView->mViewBox.SetBaseValueString(aParams, mRoot, false))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, u"preserveAspectRatio"_ns)) {
      if (mSVGView->mPreserveAspectRatio.IsExplicitlySet() ||
          NS_FAILED(mSVGView->mPreserveAspectRatio.SetBaseValueString(
              aParams, mRoot, false))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, u"transform"_ns)) {
      if (mSVGView->mTransforms) {
        return false;
      }
      mSVGView->mTransforms = MakeUnique<SVGAnimatedTransformList>();
      if (NS_FAILED(
              mSVGView->mTransforms->SetBaseValueString(aParams, mRoot))) {
        return false;
      }
    } else if (IsMatchingParameter(aToken, u"zoomAndPan"_ns)) {
      if (mSVGView->mZoomAndPan.IsExplicitlySet()) {
        return false;
      }
      nsAtom* valAtom = NS_GetStaticAtom(aParams);
      if (!valAtom || !mSVGView->mZoomAndPan.SetBaseValueAtom(valAtom, mRoot)) {
        return false;
      }
    } else {
      return false;
    }
    return true;
  }

  void SetValid() { mValid = true; }

 private:
  SVGSVGElement* mRoot;
  UniquePtr<SVGView> mSVGView;
  bool mValid;
  bool mWasOverridden;
};

bool SVGFragmentIdentifier::ProcessSVGViewSpec(const nsAString& aViewSpec,
                                               SVGSVGElement* aRoot) {
  AutoSVGViewHandler viewHandler(aRoot);

  if (!IsMatchingParameter(aViewSpec, u"svgView"_ns)) {
    return false;
  }

  // Each token is a SVGViewAttribute
  int32_t bracketPos = aViewSpec.FindChar('(');
  uint32_t lengthOfViewSpec = aViewSpec.Length() - bracketPos - 2;
  nsCharSeparatedTokenizerTemplate<NS_TokenizerIgnoreNothing> tokenizer(
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

    const nsAString& params =
        Substring(token, bracketPos + 1, token.Length() - bracketPos - 2);

    if (!viewHandler.ProcessAttr(token, params)) {
      return false;
    }

  } while (tokenizer.hasMoreTokens());

  viewHandler.SetValid();
  return true;
}

bool SVGFragmentIdentifier::ProcessFragmentIdentifier(
    Document* aDocument, const nsAString& aAnchorName) {
  MOZ_ASSERT(aDocument->GetRootElement()->IsSVGElement(nsGkAtoms::svg),
             "expecting an SVG root element");

  auto* rootElement = SVGSVGElement::FromNode(aDocument->GetRootElement());

  const auto* viewElement =
      SVGViewElement::FromNodeOrNull(aDocument->GetElementById(aAnchorName));

  if (viewElement) {
    if (!rootElement->mCurrentViewID) {
      rootElement->mCurrentViewID = MakeUnique<nsString>();
    }
    *rootElement->mCurrentViewID = aAnchorName;
    rootElement->mSVGView = nullptr;
    rootElement->InvalidateTransformNotifyFrame();
    if (nsIFrame* f = rootElement->GetPrimaryFrame()) {
      if (SVGOuterSVGFrame* osf = do_QueryFrame(f)) {
        osf->MaybeSendIntrinsicSizeAndRatioToEmbedder();
      }
    }
    // not an svgView()-style fragment identifier, return false so the caller
    // continues processing to match any :target pseudo elements
    return false;
  }

  return ProcessSVGViewSpec(aAnchorName, rootElement);
}

}  // namespace mozilla
