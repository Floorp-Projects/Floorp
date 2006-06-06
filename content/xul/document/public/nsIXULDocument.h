/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*

  An XUL-specific extension to nsIDocument. Includes methods for
  setting the root resource of the document content model, a factory
  method for constructing the children of a node, etc.

  XXX This should really be called nsIXULDocument.

 */

#ifndef nsIXULDocument_h___
#define nsIXULDocument_h___

#include "nsISupports.h"
#include "nsString.h"

class nsForwardReference;
class nsIAtom;
class nsIDOMElement;
class nsIPrincipal;
class nsIRDFResource;
class nsISupportsArray;
class nsIXULPrototypeDocument;
class nsIXULTemplateBuilder;
class nsIURI;
class nsIContent;
class nsIRDFDataSource;

// {7f9c0158-1da3-4279-9ee5-fa7931b94db1}
#define NS_IXULDOCUMENT_IID \
{ 0x7f9c0158, 0x1da3, 0x4279, \
 { 0x9e, 0xe5, 0xfa, 0x79, 0x31, 0xb9, 0x4d, 0xb1 } }

/**
 * XUL extensions to nsIDocument
 */

class nsIXULDocument : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULDOCUMENT_IID)

  // The resource-to-element map is a one-to-many mapping of RDF
  // resources to content elements.

  /**
   * Add an entry to the ID-to-element map.
   */
  NS_IMETHOD AddElementForID(const nsAString& aID, nsIContent* aElement) = 0;

  /**
   * Remove an entry from the ID-to-element map.
   */
  NS_IMETHOD RemoveElementForID(const nsAString& aID, nsIContent* aElement) = 0;

  /**
   * Get the elements for a particular resource in the resource-to-element
   * map. The nsISupportsArray will be truncated and filled in with
   * nsIContent pointers.
   */
  NS_IMETHOD GetElementsForID(const nsAString& aID, nsISupportsArray* aElements) = 0;

  /**
   * Add a "forward declaration" of a XUL observer. Such declarations
   * will be resolved when document loading completes.
   */
  NS_IMETHOD AddForwardReference(nsForwardReference* aForwardReference) = 0;

  /**
   * Resolve the all of the document's forward references.
   */
  NS_IMETHOD ResolveForwardReferences() = 0;

  /**
   * Set the master prototype.
   */
  NS_IMETHOD SetMasterPrototype(nsIXULPrototypeDocument* aDocument) = 0;

  /**
   * Get the master prototype.
   */
  NS_IMETHOD GetMasterPrototype(nsIXULPrototypeDocument** aPrototypeDocument) = 0;

  /**
   * Set the current prototype
   */
  NS_IMETHOD SetCurrentPrototype(nsIXULPrototypeDocument* aDocument) = 0;

  /**
   * Notify the XUL document that a subtree has been added
   */
  NS_IMETHOD AddSubtreeToDocument(nsIContent* aElement) = 0;

  /**
   * Notify the XUL document that a subtree has been removed
   */
  NS_IMETHOD RemoveSubtreeFromDocument(nsIContent* aElement) = 0;

  /**
   * Attach a XUL template builder to the specified content node.
   * @param aBuilder the template builder to attach, or null if
   *   the builder is to be removed.
   */
  NS_IMETHOD SetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder* aBuilder) = 0;

  /**
   * Retrieve the XUL template builder that's attached to a content
   * node.
   */
  NS_IMETHOD GetTemplateBuilderFor(nsIContent* aContent, nsIXULTemplateBuilder** aResult) = 0;

  /**
   * Callback notifying this document when its XUL prototype document load
   * completes.  The prototype load was initiated by another document load
   * request than the one whose document is being notified here.
   */
  NS_IMETHOD OnPrototypeLoadDone() = 0;

  /**
   * Callback notifying when a document could not be parsed properly.
   */
  virtual PRBool OnDocumentParserError() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULDocument, NS_IXULDOCUMENT_IID)

// factory functions
nsresult NS_NewXULDocument(nsIXULDocument** result);

#endif // nsIXULDocument_h___
