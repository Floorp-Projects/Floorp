/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTreeSanitizer_h_
#define nsTreeSanitizer_h_

#include "nsAtom.h"
#include "nsHashKeys.h"
#include "nsHashtablesFwd.h"
#include "nsIPrincipal.h"
#include "nsTArray.h"
#include "nsTHashSet.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SanitizerBinding.h"

class nsIContent;
class nsINode;

namespace mozilla {
class DeclarationBlock;
}

namespace mozilla::dom {
class DocumentFragment;
class Element;
}  // namespace mozilla::dom

/**
 * See the documentation of nsIParserUtils::sanitize for documentation
 * about the default behavior and the configuration options of this sanitizer.
 */
class nsTreeSanitizer {
 public:
  /**
   * The constructor.
   *
   * @param aFlags Flags from nsIParserUtils
   */
  explicit nsTreeSanitizer(uint32_t aFlags = 0);

  static void InitializeStatics();
  static void ReleaseStatics();

  /**
   * Sanitizes a disconnected DOM fragment freshly obtained from a parser.
   * The fragment must have just come from a parser so that it can't have
   * mutation event listeners set on it.
   */
  void Sanitize(mozilla::dom::DocumentFragment* aFragment);

  /**
   * Sanitizes a disconnected (not in a docshell) document freshly obtained
   * from a parser. The document must not be embedded in a docshell and must
   * not have had a chance to get mutation event listeners attached to it.
   * The root element must be <html>.
   */
  void Sanitize(mozilla::dom::Document* aDocument);

  /**
   * Provides additional options for usage from the Web Sanitizer API
   * which allows modifying the allow-list from above
   */
  void WithWebSanitizerOptions(const mozilla::dom::SanitizerConfig& aOptions);

 private:
  /**
   * Whether <style> and style="" are allowed.
   */
  bool mAllowStyles;

  /**
   * Whether comment nodes are allowed.
   */
  bool mAllowComments;

  /**
   * Whether HTML <font>, <center>, bgcolor="", etc., are dropped.
   */
  bool mDropNonCSSPresentation;

  /**
   * Whether to remove forms and form controls (excluding fieldset/legend).
   */
  bool mDropForms;

  /**
   * Whether only cid: embeds are allowed.
   */
  bool mCidEmbedsOnly;

  /**
   * Whether to drop <img>, <video>, <audio> and <svg>.
   */
  bool mDropMedia;

  /**
   * Whether we are sanitizing a full document (as opposed to a fragment).
   */
  bool mFullDocument;

  /**
   * Whether we should notify to the console for anything that's stripped.
   */
  bool mLogRemovals;

  /**
   * Whether we should remove CSS conditional rules, no other changes.
   */
  bool mOnlyConditionalCSS;

  /**
   * We have various tables of static atoms for elements and attributes.
   */
  class AtomsTable : public nsTHashSet<const nsStaticAtom*> {
   public:
    explicit AtomsTable(uint32_t aLength)
        : nsTHashSet<const nsStaticAtom*>(aLength) {}

    bool Contains(nsAtom* aAtom) {
      // Because this table only contains static atoms, if aAtom isn't
      // static we can immediately fail.
      return aAtom->IsStatic() && GetEntry(aAtom->AsStatic());
    }
  };
  // Use this table for user-defined lists
  class DynamicAtomsTable : public nsTHashSet<RefPtr<nsAtom>> {
   public:
    explicit DynamicAtomsTable(uint32_t aLength)
        : nsTHashSet<RefPtr<nsAtom>>(aLength) {}

    bool Contains(nsAtom* aAtom) { return GetEntry(aAtom); }
  };

  void SanitizeChildren(nsINode* aRoot);

  /**
   * Queries if an element must be replaced with its children.
   * @param aNamespace the namespace of the element the question is about
   * @param aLocal the local name of the element the question is about
   * @return true if the element must be replaced with its children and
   *         false if the element is to be kept
   */
  bool MustFlatten(int32_t aNamespace, nsAtom* aLocal);

  /**
   * Queries if an element including its children must be removed.
   * @param aNamespace the namespace of the element the question is about
   * @param aLocal the local name of the element the question is about
   * @param aElement the element node itself for inspecting attributes
   * @return true if the element and its children must be removed and
   *         false if the element is to be kept
   */
  bool MustPrune(int32_t aNamespace, nsAtom* aLocal,
                 mozilla::dom::Element* aElement);

  /**
   * Checks if a given local name (for an attribute) is on the given list
   * of URL attribute names.
   * @param aURLs the list of URL attribute names
   * @param aLocalName the name to search on the list
   * @return true if aLocalName is on the aURLs list and false otherwise
   */
  bool IsURL(const nsStaticAtom* const* aURLs, nsAtom* aLocalName);

  /**
   * Struct for what attributes and their values are allowed.
   */
  struct AllowedAttributes {
    // The whitelist of permitted local names to use.
    AtomsTable* mNames = nullptr;
    // The local names of URL-valued attributes for URL checking.
    const nsStaticAtom* const* mURLs = nullptr;
    // Whether XLink attributes are allowed.
    bool mXLink = false;
    // Whether the style attribute is allowed.
    bool mStyle = false;
    // Whether to leave the value of the src attribute unsanitized.
    bool mDangerousSrc = false;
  };

  /**
   * Removes dangerous attributes from the element. If the style attribute
   * is allowed, its value is sanitized. The values of URL attributes are
   * sanitized, except src isn't sanitized when it is allowed to remain
   * potentially dangerous.
   *
   * @param aElement the element whose attributes should be sanitized
   * @param aAllowed options for sanitizing attributes
   */
  void SanitizeAttributes(mozilla::dom::Element* aElement,
                          AllowedAttributes aAllowed);

  /**
   * Remove the named URL attribute from the element if the URL fails a
   * security check.
   *
   * @param aElement the element whose attribute to possibly modify
   * @param aNamespace the namespace of the URL attribute
   * @param aLocalName the local name of the URL attribute
   * @return true if the attribute was removed and false otherwise
   */
  bool SanitizeURL(mozilla::dom::Element* aElement, int32_t aNamespace,
                   nsAtom* aLocalName);

  /**
   * Checks a style rule for the presence of the 'binding' CSS property and
   * removes that property from the rule.
   *
   * @param aDeclaration The style declaration to check
   * @return true if the rule was modified and false otherwise
   */
  bool SanitizeStyleDeclaration(mozilla::DeclarationBlock* aDeclaration);

  /**
   * Parses a style sheet and reserializes it with the 'binding' property
   * removed if it was present.
   *
   * @param aOriginal the original style sheet source
   * @param aSanitized the reserialization without dangerous CSS.
   * @param aDocument the document the style sheet belongs to
   * @param aBaseURI the base URI to use
   */
  void SanitizeStyleSheet(const nsAString& aOriginal, nsAString& aSanitized,
                          mozilla::dom::Document* aDocument, nsIURI* aBaseURI);

  /**
   * Removes all attributes from an element node.
   */
  void RemoveAllAttributes(mozilla::dom::Element* aElement);

  /**
   * Removes all attributes from the descendants of an element but not from
   * the element itself.
   */
  void RemoveAllAttributesFromDescendants(mozilla::dom::Element* aElement);

  /**
   * Log a Console Service message to indicate we removed something.
   * If you pass an element and/or attribute, their information will
   * be appended to the message.
   *
   * @param aMessage   the basic message to log.
   * @param aDocument  the base document we're modifying
   *                   (used for the error message)
   * @param aElement   optional, the element being removed or modified.
   * @param aAttribute optional, the attribute being removed or modified.
   */
  void LogMessage(const char* aMessage, mozilla::dom::Document* aDoc,
                  mozilla::dom::Element* aElement = nullptr,
                  nsAtom* aAttr = nullptr);

  /**
   * The whitelist of HTML elements.
   */
  static AtomsTable* sElementsHTML;

  /**
   * The whitelist of non-presentational HTML attributes.
   */
  static AtomsTable* sAttributesHTML;

  /**
   * The whitelist of presentational HTML attributes.
   */
  static AtomsTable* sPresAttributesHTML;

  /**
   * The whitelist of SVG elements.
   */
  static AtomsTable* sElementsSVG;

  /**
   * The whitelist of SVG attributes.
   */
  static AtomsTable* sAttributesSVG;

  /**
   * The whitelist of SVG elements.
   */
  static AtomsTable* sElementsMathML;

  /**
   * The whitelist of MathML attributes.
   */
  static AtomsTable* sAttributesMathML;

  /**
   * Reusable null principal for URL checks.
   */
  static nsIPrincipal* sNullPrincipal;

  // Short-hand to determine whether this is a customized Sanitizer.
  bool mIsCustomized = false;

  // An allow-list of elements to keep.
  mozilla::UniquePtr<DynamicAtomsTable> mAllowedElements;

  // A deny-list of elements to block.
  mozilla::UniquePtr<DynamicAtomsTable> mBlockedElements;

  // An allow-list of attributes to keep.
  mozilla::UniquePtr<
      nsTHashMap<RefPtr<nsAtom>, mozilla::UniquePtr<DynamicAtomsTable>>>
      mAllowedAttributes;

  // A deny-list of attributes to drop.
  mozilla::UniquePtr<
      nsTHashMap<RefPtr<nsAtom>, mozilla::UniquePtr<DynamicAtomsTable>>>
      mDroppedAttributes;
};

#endif  // nsTreeSanitizer_h_
