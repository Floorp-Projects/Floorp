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
 * The Original Code is HTML/SVG/MathML sanitizer code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henri Sivonen <hsivonen@iki.fi>
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

#ifndef nsTreeSanitizer_h_
#define nsTreeSanitizer_h_

#include "nsIContent.h"
#include "mozilla/css/StyleRule.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"

class NS_STACK_CLASS nsTreeSanitizer {

  public:

    /**
     * The constructor.
     *
     * @param aAllowStyles Whether to allow <style> and style=""
     * @param aAllowComments Whether to allow comment nodes
     */
    nsTreeSanitizer(bool aAllowStyles, bool aAllowComments);

    static void InitializeStatics();
    static void ReleaseStatics();

    /**
     * Sanitizes a disconnected DOM fragment freshly obtained from a parser.
     * The argument must be of type nsINode::eDOCUMENT_FRAGMENT and,
     * consequently, must not be in the document. Furthermore, the fragment
     * must have just come from a parser so that it can't have mutation
     * event listeners set on it.
     */
    void Sanitize(nsIContent* aFragment);

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
     * Queries if an element must be replaced with its children.
     * @param aNamespace the namespace of the element the question is about
     * @param aLocal the local name of the element the question is about
     * @return true if the element must be replaced with its children and
     *         false if the element is to be kept
     */
    bool MustFlatten(PRInt32 aNamespace, nsIAtom* aLocal);

    /**
     * Queries if an element including its children must be removed.
     * @param aNamespace the namespace of the element the question is about
     * @param aLocal the local name of the element the question is about
     * @param aElement the element node itself for inspecting attributes
     * @return true if the element and its children must be removed and
     *         false if the element is to be kept
     */
    bool MustPrune(PRInt32 aNamespace,
                     nsIAtom* aLocal,
                     mozilla::dom::Element* aElement);

    /**
     * Checks if a given local name (for an attribute) is on the given list
     * of URL attribute names.
     * @param aURLs the list of URL attribute names
     * @param aLocalName the name to search on the list
     * @return true if aLocalName is on the aURLs list and false otherwise
     */
    bool IsURL(nsIAtom*** aURLs, nsIAtom* aLocalName);

    /**
     * Removes dangerous attributes from the element. If the style attribute
     * is allowed, its value is sanitized. The values of URL attributes are
     * sanitized, except src isn't sanitized when it is allowed to remain
     * potentially dangerous.
     *
     * @param aElement the element whose attributes should be sanitized
     * @param aAllowed the whitelist of permitted local names to use
     * @param aURLs the local names of URL-valued attributes
     * @param aAllowXLink whether XLink attributes are allowed
     * @param aAllowStyle whether the style attribute is allowed
     * @param aAllowDangerousSrc whether to leave the value of the src
     *                           attribute unsanitized
     */
    void SanitizeAttributes(mozilla::dom::Element* aElement,
                            nsTHashtable<nsISupportsHashKey>* aAllowed,
                            nsIAtom*** aURLs,
                            bool aAllowXLink,
                            bool aAllowStyle,
                            bool aAllowDangerousSrc);

    /**
     * Remove the named URL attribute from the element if the URL fails a
     * security check.
     *
     * @param aElement the element whose attribute to possibly modify
     * @param aNamespace the namespace of the URL attribute
     * @param aLocalName the local name of the URL attribute
     * @return true if the attribute was removed and false otherwise
     */
    bool SanitizeURL(mozilla::dom::Element* aElement,
                       PRInt32 aNamespace,
                       nsIAtom* aLocalName);

    /**
     * Checks a style rule for the presence of the 'binding' CSS property and
     * removes that property from the rule and reserializes in case the
     * property was found.
     *
     * @param aRule The style rule to check
     * @param aRuleText the serialized mutated rule if the method returns true
     * @return true if the rule was modified and false otherwise
     */
    bool SanitizeStyleRule(mozilla::css::StyleRule* aRule,
                             nsAutoString &aRuleText);

    /**
     * Parses a style sheet and reserializes it with the 'binding' property
     * removed if it was present.
     *
     * @param aOrigin the original style sheet source
     * @param aSanitized the reserialization without 'binding'; only valid if
     *                   this method return true
     * @param aDocument the document the style sheet belongs to
     * @param aBaseURI the base URI to use
     * @return true if the 'binding' property was encountered and false
     *              otherwise
     */
    bool SanitizeStyleSheet(const nsAString& aOriginal,
                              nsAString& aSanitized,
                              nsIDocument* aDocument,
                              nsIURI* aBaseURI);

    /**
     * The whitelist of HTML elements.
     */
    static nsTHashtable<nsISupportsHashKey>* sElementsHTML;

    /**
     * The whitelist of HTML attributes.
     */
    static nsTHashtable<nsISupportsHashKey>* sAttributesHTML;

    /**
     * The whitelist of SVG elements.
     */
    static nsTHashtable<nsISupportsHashKey>* sElementsSVG;

    /**
     * The whitelist of SVG attributes.
     */
    static nsTHashtable<nsISupportsHashKey>* sAttributesSVG;

    /**
     * The whitelist of SVG elements.
     */
    static nsTHashtable<nsISupportsHashKey>* sElementsMathML;

    /**
     * The whitelist of MathML attributes.
     */
    static nsTHashtable<nsISupportsHashKey>* sAttributesMathML;

    /**
     * Reusable null principal for URL checks.
     */
    static nsIPrincipal* sNullPrincipal;
};

#endif // nsTreeSanitizer_h_
