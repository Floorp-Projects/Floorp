/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIXULDocument_h___
#define nsIXULDocument_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsCOMArray.h"

class nsIXULTemplateBuilder;
class nsIContent;
class nsIScriptGlobalObjectOwner;


// 3e872e97-b678-418e-a7e3-41b8305d4e75
#define NS_IXULDOCUMENT_IID \
{ 0x3e872e97, 0xb678, 0x418e, \
  { 0xa7, 0xe3, 0x41, 0xb8, 0x30, 0x5d, 0x4e, 0x75 } }


/*
 * An XUL-specific extension to nsIDocument. Includes methods for
 * setting the root resource of the document content model, a factory
 * method for constructing the children of a node, etc.
 */
class nsIXULDocument : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXULDOCUMENT_IID)

  /**
   * Get the elements for a particular resource --- all elements whose 'id'
   * or 'ref' is aID. The nsCOMArray will be truncated and filled in with
   * nsIContent pointers.
   */
  virtual void GetElementsForID(const nsAString& aID, nsCOMArray<nsIContent>& aElements) = 0;

  /**
   * Get the nsIScriptGlobalObjectOwner for this document.
   */
  NS_IMETHOD GetScriptGlobalObjectOwner(nsIScriptGlobalObjectOwner** aGlobalOwner) = 0;

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
   * This is invoked whenever the prototype for this document is loaded
   * and should be walked, regardless of whether the XUL cache is
   * disabled, whether the protototype was loaded, whether the
   * prototype was loaded from the cache or created by parsing the
   * actual XUL source, etc.
   *
   * @param aResumeWalk whether this should also call ResumeWalk().
   * Sometimes the caller of OnPrototypeLoadDone resumes the walk itself
   */
  NS_IMETHOD OnPrototypeLoadDone(bool aResumeWalk) = 0;

  /**
   * Callback notifying when a document could not be parsed properly.
   */
  virtual bool OnDocumentParserError() = 0;

  /**
   * Reset the document direction so that it is recomputed.
   */
  virtual void ResetDocumentDirection() = 0;

  virtual void ResetDocumentLWTheme() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXULDocument, NS_IXULDOCUMENT_IID)

// factory functions
nsresult NS_NewXULDocument(nsIXULDocument** result);

#endif // nsIXULDocument_h___
