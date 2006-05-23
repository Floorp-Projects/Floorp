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
 * The Original Code is Mozilla XForms support.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#ifndef nsXFormsSubmissionElement_h_
#define nsXFormsSubmissionElement_h_

#include "nsXFormsStubElement.h"
#include "nsIRequestObserver.h"
#include "nsIInputStream.h"
#include "nsCOMPtr.h"
#include "nsIModelElementPrivate.h"
#include "nsIXFormsSubmitElement.h"
#include "nsIXFormsSubmissionElement.h"
#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsHashSets.h"
#include "nsIDocument.h"


class nsIMultiplexInputStream;
class nsIDOMElement;
class nsIChannel;
class nsIURI;
class nsIFile;
class nsCString;
class nsString;

class SubmissionAttachmentArray;

/**
 * Implementation of the XForms \<submission\> element.
 *
 * @see http://www.w3.org/TR/xforms/slice3.html#structure-model-submission
 */
class nsXFormsSubmissionElement : public nsXFormsStubElement,
                                  public nsIRequestObserver,
                                  public nsIXFormsSubmissionElement,
                                  public nsIChannelEventSink,
                                  public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSIXFORMSSUBMISSIONELEMENT
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR

  nsXFormsSubmissionElement()
    : mElement(nsnull),
      mSubmissionActive(PR_FALSE),
      mIsReplaceInstance(PR_FALSE),
      mIsSOAPRequest(PR_FALSE),
      mFormat(0)
  {}

  // nsIXTFGenericElement overrides
  NS_IMETHOD OnDestroyed();
  NS_IMETHOD HandleDefault(nsIDOMEvent *aEvent, PRBool *aHandled);
  NS_IMETHOD OnCreated(nsIXTFGenericElementWrapper *aWrapper);

  NS_HIDDEN_(already_AddRefed<nsIModelElementPrivate>) GetModel();

  NS_HIDDEN_(nsresult) LoadReplaceInstance(nsIChannel *);
  NS_HIDDEN_(nsresult) LoadReplaceAll(nsIChannel *);
  NS_HIDDEN_(nsresult) Submit();
  NS_HIDDEN_(PRBool)   GetBooleanAttr(const nsAString &attrName,
                                      PRBool defaultVal = PR_FALSE);
  NS_HIDDEN_(void)     GetDefaultInstanceData(nsIDOMNode **result);
  NS_HIDDEN_(nsresult) GetBoundInstanceData(nsIDOMNode **result);
  NS_HIDDEN_(nsresult) GetSelectedInstanceElement(const nsAString &aInstance,
                                                  nsIModelElementPrivate *aModel,
                                                  nsIInstanceElementPrivate **result);
  NS_HIDDEN_(nsresult) SerializeData(nsIDOMDocument *data, nsCString &uri,
                                     nsIInputStream **, nsCString &contentType);
  NS_HIDDEN_(nsresult) SerializeDataXML(nsIDOMDocument *data, nsIInputStream **,
                                        nsCString &contentType);
  NS_HIDDEN_(nsresult) CreatePurgedDoc(nsIDOMNode *source,
                                       nsIDOMDocument **result);
  NS_HIDDEN_(nsresult) CopyChildren(nsIModelElementPrivate* model,
                                    nsIDOMNode *source, nsIDOMNode *dest,
                                    nsIDOMDocument *destDoc,
                                    const nsString &cdataElements,
                                    PRUint32 depth);
  NS_HIDDEN_(nsresult) SerializeDataURLEncoded(nsIDOMDocument *data,
                                               nsCString &uri,
                                               nsIInputStream **,
                                               nsCString &contentType);
  NS_HIDDEN_(void)     AppendURLEncodedData(nsIDOMNode *data,
                                            const nsCString &sep,
                                            nsCString &buf);
  NS_HIDDEN_(nsresult) SerializeDataMultipartRelated(nsIDOMDocument *data,
                                                     nsIInputStream **,
                                                     nsCString &contentType);
  NS_HIDDEN_(nsresult) SerializeDataMultipartFormData(nsIDOMDocument *data,
                                                      nsIInputStream **,
                                                      nsCString &contentType);
  NS_HIDDEN_(nsresult) AppendMultipartFormData(nsIDOMNode *data,
                                               const nsCString &boundary,
                                               nsCString &buf,
                                               nsIMultiplexInputStream *);
  NS_HIDDEN_(nsresult) AppendPostDataChunk(nsCString &postDataChunk,
                                           nsIMultiplexInputStream *multiStream);
  NS_HIDDEN_(nsresult) GetElementEncodingType(nsIDOMNode *data,
                                              PRUint32 *encType,
                                              nsIModelElementPrivate* aModel = nsnull);
  NS_HIDDEN_(nsresult) CreateFileStream(const nsString &absURI, nsIFile **file,
                                        nsIInputStream **stream);
  NS_HIDDEN_(nsresult) SendData(const nsCString &uri, nsIInputStream *stream,
                                const nsCString &contentType);

private:
  nsIDOMElement*                   mElement;
  PRPackedBool                     mSubmissionActive;
   // Valid when mSubmissionActive == PR_TRUE
  PRPackedBool                     mIsReplaceInstance;
  PRPackedBool                     mIsSOAPRequest;
  // Valid when mSubmissionActive == PR_TRUE
  PRUint32                         mFormat;
  nsCOMPtr<nsIXFormsSubmitElement> mActivator;

  // input end of pipe, which contains response data.
  nsCOMPtr<nsIInputStream>         mPipeIn;

  /**
   * @return true if aTestURI has the same origin as aBaseURI or if
   * there is no need for a same origin check.
   */
  PRBool CheckSameOrigin(nsIDocument *aBaseDocument, nsIURI *aTestURI);
  nsresult AddNameSpaces(nsIDOMElement* aTarget, nsIDOMNode* aSource,
                         nsStringHashSet* aPrefixHash);
  nsresult GetIncludeNSPrefixesAttr(nsStringHashSet** aHash);

  /**
   * Send xforms-submit-done/-error, depending on |aSucceeded|
   *
   * @param aSucceeded        Did submit succeed?
   *
   */
  void EndSubmit(PRBool aSucceeded);

  /**
   * Create submission document.
   *
   * This creates a new document, include namespaces, purges non-relevant
   * nodes, and checks simple type validity. If a check fails, no document
   * will be returned.
   *
   * @param aRoot             The source node
   * @param aReturnDoc        The resulting document
   */
  nsresult CreateSubmissionDoc(nsIDOMNode *aRoot,
                               nsIDOMDocument** aReturnDoc);

  /**
   * Run through document and create attachment ids for xsd:anyURI nodes. This
   * is used for "multipart/related".
   *
   * @param aModel            The model to use
   * @param aDoc              The document to run through
   * @param aAttachments      Array of files and attachment ids
   */
  nsresult CreateAttachments(nsIModelElementPrivate *aModel, nsIDOMNode *aDoc,
                             SubmissionAttachmentArray *aAttachments);
};

NS_HIDDEN_(nsresult)
NS_NewXFormsSubmissionElement(nsIXTFElement **aResult);

#endif
