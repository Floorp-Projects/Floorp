/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_FRAGMENTDIRECTIVE_H_
#define DOM_FRAGMENTDIRECTIVE_H_

#include "js/TypeDecls.h"
#include "mozilla/dom/BindingDeclarations.h"

#include "mozilla/dom/fragmentdirectives_ffi_generated.h"
#include "nsCycleCollectionParticipant.h"
#include "nsStringFwd.h"
#include "nsWrapperCache.h"

class nsINode;
class nsIURI;
class nsRange;
namespace mozilla::dom {
class Document;
class Text;

/**
 * @brief The `FragmentDirective` class is the C++ representation of the
 * `Document.fragmentDirective` webidl property.
 *
 * This class also serves as the main interface to interact with the fragment
 * directive from the C++ side. It allows to find text fragment ranges from a
 * given list of `TextDirective`s using
 * `FragmentDirective::FindTextFragmentsInDocument()`.
 * To avoid Text Directives being applied multiple times, this class implements
 * the `uninvoked directive` mechanism, which in the spec is defined to be part
 * of the `Document` [0].
 *
 * [0]
 * https://wicg.github.io/scroll-to-text-fragment/#document-uninvoked-directives
 */
class FragmentDirective final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(FragmentDirective)

 public:
  explicit FragmentDirective(Document* aDocument);
  FragmentDirective(Document* aDocument,
                    nsTArray<TextDirective>&& aTextDirectives)
      : mDocument(aDocument),
        mUninvokedTextDirectives(std::move(aTextDirectives)) {}

 protected:
  ~FragmentDirective() = default;

 public:
  Document* GetParentObject() const { return mDocument; };

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  /**
   * @brief Sets Text Directives as "uninvoked directive".
   */
  void SetTextDirectives(nsTArray<TextDirective>&& aTextDirectives) {
    mUninvokedTextDirectives = std::move(aTextDirectives);
  }

  /** Returns true if there are Text Directives that have not been applied to
   * the `Document`.
   */
  bool HasUninvokedDirectives() const {
    return !mUninvokedTextDirectives.IsEmpty();
  };

  /** Clears all uninvoked directives. */
  void ClearUninvokedDirectives() { mUninvokedTextDirectives.Clear(); }

  /** Searches for the current uninvoked text directives and creates a range for
   * each one that is found.
   *
   * When this method returns, the uninvoked directives for this document are
   * cleared.
   *
   * This method tries to follow the specification as close as possible in how
   * to find a matching range for a text directive. However, instead of using
   * collator-based search, a standard case-insensitive search is used
   * (`nsString::find()`).
   */
  nsTArray<RefPtr<nsRange>> FindTextFragmentsInDocument();

  /** Utility function which parses the fragment directive and removes it from
   * the hash of the given URI. This operation happens in-place.
   *
   * If aTextDirectives is nullptr, the parsed fragment directive is discarded.
   */
  static void ParseAndRemoveFragmentDirectiveFromFragment(
      nsCOMPtr<nsIURI>& aURI,
      nsTArray<TextDirective>* aTextDirectives = nullptr);

  /** Parses the fragment directive and removes it from the hash, given as
   * string. This operation happens in-place.
   *
   * This function is called internally by
   * `ParseAndRemoveFragmentDirectiveFromFragment()`.
   *
   * This function returns true if it modified `aFragment`.
   */
  static bool ParseAndRemoveFragmentDirectiveFromFragmentString(
      nsCString& aFragment, nsTArray<TextDirective>* aTextDirectives = nullptr);

 private:
  RefPtr<nsRange> FindRangeForTextDirective(
      const TextDirective& aTextDirective);
  RefPtr<nsRange> FindStringInRange(nsRange* aSearchRange,
                                    const nsAString& aQuery,
                                    bool aWordStartBounded,
                                    bool aWordEndBounded);

  RefPtr<Document> mDocument;
  nsTArray<TextDirective> mUninvokedTextDirectives;
};

}  // namespace mozilla::dom

#endif  // DOM_FRAGMENTDIRECTIVE_H_
