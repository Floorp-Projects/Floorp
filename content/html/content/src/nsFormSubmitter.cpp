/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFormSubmitter.h"

#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIForm.h"
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLDocument.h"
#include "nsIFormControl.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsDOMError.h"
#include "nsHTMLValue.h"
#include "nsGenericElement.h"

// JBK added for submit move from content frame
#include "nsIFileSpec.h"
#include "nsIFormProcessor.h"
static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID);
#include "nsIURI.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#include "nsIObserverService.h"
#include "nsIFormSubmitObserver.h"
#include "nsIDOMWindowInternal.h"
#include "nsIUnicodeEncoder.h"
#include "nsIPref.h"
#include "nsSpecialSystemDirectory.h"
#include "nsLinebreakConverter.h"
#include "nsIPlatformCharset.h"
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);
#include "nsICharsetConverterManager.h"
static NS_DEFINE_CID(kCharsetConverterManagerCID,
                     NS_ICHARSETCONVERTERMANAGER_CID);
#include "xp_path.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsICharsetAlias.h"
#include "nsEscape.h"
#include "nsISimpleEnumerator.h"
#include "nsUnicharUtils.h"
#include "nsICategoryManager.h"

#include "nsIDOMNode.h"
#include "nsRange.h"

//BIDI
#ifdef IBMBIDI
#include "nsIUBidiUtils.h"
static NS_DEFINE_CID(kUBidiUtilCID, NS_UNICHARBIDIUTIL_CID);
#endif
//end
#define CONTENT_DISP "Content-Disposition: form-data; name=\""
#define FILENAME "\"; filename=\""
#define CONTENT_TYPE "Content-Type: "
#define CONTENT_TRANSFER "Content-Transfer-Encoding: "
#define BINARY_CONTENT "binary"
#define BUFSIZE 1024
#define MULTIPART "multipart/form-data"
#define SEP "--"

// End added JBK

// JBK moved from nsFormFrame - bug 34297
// submission

PRBool nsFormSubmitter::gFirstFormSubmitted = PR_FALSE;

// static
nsresult
nsFormSubmitter::CompareNodes(nsIDOMNode* a, nsIDOMNode* b, PRInt32* retval)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNode> aParent;
  PRInt32 aIndex;
  rv = a->GetParentNode(getter_AddRefs(aParent));
  if (NS_FAILED(rv)) {
    return rv;
  }
  {
    // To get the index, we must turn them both into contents
    // and do IndexOf().  Ick.
    nsCOMPtr<nsIContent> aParentContent(do_QueryInterface(aParent));
    nsCOMPtr<nsIContent> aContent(do_QueryInterface(a));
    rv = aParentContent->IndexOf(aContent, aIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  nsCOMPtr<nsIDOMNode> bParent;
  PRInt32 bIndex;
  rv = b->GetParentNode(getter_AddRefs(bParent));
  if (NS_FAILED(rv)) {
    return rv;
  }
  {
    // To get the index, we must turn them both into contents
    // and do IndexOf().  Ick.
    nsCOMPtr<nsIContent> bParentContent(do_QueryInterface(bParent));
    nsCOMPtr<nsIContent> bContent(do_QueryInterface(b));
    rv = bParentContent->IndexOf(bContent, bIndex);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  *retval = ComparePoints(aParent, aIndex, bParent, bIndex);
  return NS_OK;
}

// static
nsresult
nsFormSubmitter::OnSubmit(nsIForm* form,
                          nsIPresContext* aPresContext,
                          nsIContent* aSubmitElement)
{
  PRUint8 ctrlsModAtSubmit=0;
  PRUint8 textDirAtSubmit=0;
#ifdef IBMBIDI
//ahmed
  PRUint32 bidiOptions;
  aPresContext->GetBidi(&bidiOptions);
  ctrlsModAtSubmit = GET_BIDI_OPTION_CONTROLSTEXTMODE(bidiOptions);
  textDirAtSubmit  = GET_BIDI_OPTION_DIRECTION(bidiOptions);
//ahmed end
#endif

  // If the submitElement is input type=image, find out where in the control
  // array it should have gone so we can submit it there.
  PRInt32 submitPosition = -1;
  {
    nsCOMPtr<nsIFormControl> submitControl(do_QueryInterface(aSubmitElement));
    if (submitControl) {
      PRInt32 type;
      submitControl->GetType(&type);
      if (type == NS_FORM_INPUT_IMAGE) {
        nsCOMPtr<nsIDOMNode> submitNode(do_QueryInterface(aSubmitElement));
        if (submitNode) {
          // Loop through the control array and see where the image should go
          PRUint32 numElements;
          PRUint32 elementX;
          form->GetElementCount(&numElements);
          for (elementX = 0; elementX < numElements; elementX++) {
            nsCOMPtr<nsIFormControl> curControl;
            form->GetElementAt(elementX, getter_AddRefs(curControl));
            nsCOMPtr<nsIDOMNode> curNode(do_QueryInterface(curControl));
            if (curNode) {
              PRInt32 comparison;
              nsresult rv = CompareNodes(submitNode, curNode, &comparison);
              if (NS_SUCCEEDED(rv) && comparison < 0) {
                submitPosition = elementX;
                break;
              }
            }
          }

          // If it was larger than everything, we put it at the end
          if (submitPosition == -1) {
            submitPosition = numElements;
          }
        }
      }
    }
  }

   // Get a service to process the value part of the form data
   // If one doesn't exist, that fine. It's not required.
  nsresult rv = NS_OK;
  nsCOMPtr<nsIFormProcessor> formProcessor =
           do_GetService(kFormProcessorCID, &rv);

  PRInt32 method, enctype;
  FullyGetMethod(form, &method);
  FullyGetEnctype(form, &enctype);

  PRBool isURLEncoded = (NS_FORM_ENCTYPE_MULTIPART != enctype);

  // for enctype=multipart/form-data, force it to be post
  // if method is "" (not specified) use "get" as default
  PRBool isPost = (NS_FORM_METHOD_POST == method) || !isURLEncoded;

  nsString data; // this could be more efficient, by allocating a larger buffer
  nsIFileSpec* multipartDataFile = nsnull;
  if (isURLEncoded) {
    rv = ProcessAsURLEncoded(form, aPresContext, formProcessor,
                                 isPost, data,
                                 aSubmitElement, submitPosition,
                                 ctrlsModAtSubmit, textDirAtSubmit);
  }
  else {
    rv = ProcessAsMultipart(form, aPresContext, formProcessor,
                                multipartDataFile,
                                aSubmitElement, submitPosition,
                                ctrlsModAtSubmit, textDirAtSubmit);
  }

  // Don't bother submitting form if we failed to generate a valid submission
  if (NS_FAILED(rv)) {
    NS_IF_RELEASE(multipartDataFile);
    return rv;
  }

  // make the url string
  nsCOMPtr<nsILinkHandler> handler;
  if (NS_OK == aPresContext->GetLinkHandler(getter_AddRefs(handler))) {
    nsAutoString href;
    nsCOMPtr<nsIDOMHTMLFormElement> formDOMElement = do_QueryInterface(form);
    if (formDOMElement) {
      formDOMElement->GetAction(href);
    }

    // Get the document.
    // We'll need it now to form the URL we're submitting to.
    // We'll also need it later to get the DOM window when notifying form submit
    // observers (bug 33203)
    nsCOMPtr<nsGenericElement> formElement = do_QueryInterface(form);
    if (!formElement) return NS_OK; // same as !document
    nsCOMPtr<nsIDocument> document;
    formElement->GetDocument(*getter_AddRefs(document));
    if (!document) return NS_OK; // No doc means don't submit, see Bug 28988

    // Resolve url to an absolute url
    nsCOMPtr<nsIURI> docURL;
    document->GetBaseURL(*getter_AddRefs(docURL));
    NS_ASSERTION(docURL, "No Base URL found in Form Submit!\n");
    if (!docURL) return NS_OK; // No base URL -> exit early, see Bug 30721

      // If an action is not specified and we are inside
      // a HTML document then reload the URL. This makes us
      // compatible with 4.x browsers.
      // If we are in some other type of document such as XML or
      // XUL, do nothing. This prevents undesirable reloading of
      // a document inside XUL.

    if (href.IsEmpty()) {
      nsCOMPtr<nsIHTMLDocument> htmlDoc;
      if (!NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIHTMLDocument),
                                             getter_AddRefs(htmlDoc)))) {
        // Must be a XML, XUL or other non-HTML document type
        // so do nothing.
        return NS_OK;
      }

      // Necko's MakeAbsoluteURI doesn't reuse the baseURL's rel path if it is
      // passed a zero length rel path.
      nsXPIDLCString relPath;
      docURL->GetSpec(getter_Copies(relPath));
      NS_ASSERTION(relPath, "Rel path couldn't be formed in form submit!\n");
      if (relPath) {
        href.AppendWithConversion(relPath);

      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      // Get security manager, check to see if access to action URI is allowed.
      nsCOMPtr<nsIScriptSecurityManager> securityManager =
               do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
      nsCOMPtr<nsIURI> actionURL;
      if (NS_FAILED(rv)) return rv;

      rv = NS_NewURI(getter_AddRefs(actionURL), href, docURL);
      if (NS_SUCCEEDED(rv)) {
        rv = securityManager->CheckLoadURI(docURL, actionURL,
                                    nsIScriptSecurityManager::STANDARD);
        if (NS_FAILED(rv)) return rv;
      }

      nsXPIDLCString scheme;
      PRBool isMailto = PR_FALSE;
      if (actionURL
          && NS_FAILED(rv = actionURL->SchemeIs("mailto", &isMailto)))
        return rv;
      if (isMailto) {
        PRBool enabled;
        rv = securityManager->IsCapabilityEnabled("UniversalSendMail",
                                                      &enabled);
        if (NS_FAILED(rv)) {
          return rv;
        }
        if (!enabled) {
          // Form submit to a mailto: URI requires UniversalSendMail privilege
          return NS_ERROR_DOM_SECURITY_ERR;
        }
      }
    }

    nsAutoString target;
    if (formDOMElement) {
      formDOMElement->GetTarget(target);
    }

    // Add the URI encoded form values to the URI
    // Get the scheme of the URI.
    nsCOMPtr<nsIURI> actionURL;
    nsXPIDLCString scheme;
    rv = NS_NewURI(getter_AddRefs(actionURL), href, docURL);
    if (NS_SUCCEEDED(rv)) {
      rv = actionURL->GetScheme(getter_Copies(scheme));
    }
    NS_ConvertASCIItoUCS2 theScheme(scheme);
    // Append the URI encoded variable/value pairs for GET's
    if (!isPost) {
      // Not for JS URIs, see bug 26917
      if (!theScheme.EqualsIgnoreCase("javascript")) {

        // Bug 42616: Trim off named anchor and save it to add later
        PRInt32 namedAnchorPos = href.FindChar('#', PR_FALSE, 0);
        nsAutoString namedAnchor;
        if (kNotFound != namedAnchorPos) {
          href.Right(namedAnchor, (href.Length() - namedAnchorPos));
          href.Truncate(namedAnchorPos);
        }

        // Chop off old query string (bug 25330, 57333)
        // Only do this for GET not POST (bug 41585)
        PRInt32 queryStart = href.FindChar('?');
        if (kNotFound != queryStart) {
          href.Truncate(queryStart);
        }

        href.Append(PRUnichar('?'));
        href.Append(data);

        // Bug 42616: Add named anchor to end after query string
        if (namedAnchor.Length()) {
          href.Append(namedAnchor);
        }
      }
    }

    nsAutoString absURLSpec;
    rv = NS_MakeAbsoluteURI(absURLSpec, href, docURL);
    if (NS_FAILED(rv)) return rv;

    // If this is the first form, bring alive the first form submit
    // category observers
    if (!gFirstFormSubmitted) {
      gFirstFormSubmitted = PR_TRUE;
      NS_CreateServicesFromCategory(NS_FIRST_FORMSUBMIT_CATEGORY,
                                    nsnull,
                                    NS_FIRST_FORMSUBMIT_CATEGORY);
    }

    // Notify observers that the form is being submitted.
    rv = NS_OK;
    nsCOMPtr<nsIObserverService> service =
             do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> theEnum;
    rv = service->EnumerateObservers(NS_FORMSUBMIT_SUBJECT,
                                     getter_AddRefs(theEnum));
    if (NS_SUCCEEDED(rv) && theEnum) {
      nsCOMPtr<nsISupports> inst;
      // XXX What is cancelSubmit doing anyway?
      PRBool cancelSubmit = PR_FALSE;

      nsCOMPtr<nsIScriptGlobalObject> globalObject;
      document->GetScriptGlobalObject(getter_AddRefs(globalObject));
      nsCOMPtr<nsIDOMWindowInternal> window = do_QueryInterface(globalObject);

      PRBool loop = PR_TRUE;
      while (NS_SUCCEEDED(theEnum->HasMoreElements(&loop)) && loop) {
        theEnum->GetNext(getter_AddRefs(inst));

        nsCOMPtr<nsIFormSubmitObserver> formSubmitObserver(
                        do_QueryInterface(inst, &rv));
        if (formSubmitObserver) {
          nsCOMPtr<nsGenericElement> formElement = do_QueryInterface(form);
          if (formElement) {
            nsresult notifyStatus = formSubmitObserver->Notify(formElement,
                                                               window,
                                                               actionURL,
                                                               &cancelSubmit);
            if (NS_FAILED(notifyStatus)) { // assert/warn if we get here?
              return notifyStatus;
            }
          }
        }
        if (cancelSubmit) {
          return NS_OK;
        }
      }
    }

    // Now pass on absolute url to the click handler
    nsCOMPtr<nsIInputStream> postDataStream;
    if (isPost) {
      if (isURLEncoded) {
        nsCAutoString postBuffer;
        postBuffer.AssignWithConversion(data);
        NS_NewPostDataStream(getter_AddRefs(postDataStream), !isURLEncoded,
                             postBuffer.get(), 0);
      } else {
        // Cut-and-paste of NS_NewPostDataStream
        nsCOMPtr<nsIIOService> serv(do_GetService(kIOServiceCID));
        if (serv && multipartDataFile) {
          nsCOMPtr<nsIInputStream> rawStream;
          multipartDataFile->GetInputStream(getter_AddRefs(rawStream));
          if (rawStream) {
              NS_NewBufferedInputStream(getter_AddRefs(postDataStream),
                                        rawStream, 8192);
          }
        }
      }
    }
    if (handler) {
#if defined(DEBUG_rods) || defined(DEBUG_pollmann)
      {
        printf("******\n");
        char * str = ToNewCString(data);
        printf("postBuffer[%s]\n", str);
        Recycle(str);

        str = ToNewCString(absURLSpec);
        printf("absURLSpec[%s]\n", str);
        Recycle(str);

        str = ToNewCString(target);
        printf("target    [%s]\n", str);
        Recycle(str);
        printf("******\n");
      }
#endif
      nsCOMPtr<nsGenericElement> formElement = do_QueryInterface(form);
      handler->OnLinkClick(formElement, eLinkVerb_Replace,
                           absURLSpec.get(),
                           target.get(), postDataStream);
    }
// We need to delete the data file somewhere...
//    if (!isURLEncoded) {
//      nsFileSpec mdf = nsnull;
//      rv = multipartDataFile->GetFileSpec(&mdf);
//      if (NS_SUCCEEDED(rv) && mdf) {
//        mdf.Delete(PR_FALSE);
//      }
//    }
  }
  return rv;
}

// JBK moved from nsFormFrame - bug 34297
// Process form stuff without worrying about FILE elements
#define CRLF "\015\012"
// static
nsresult
nsFormSubmitter::ProcessAsURLEncoded(nsIForm* form,
                                     nsIPresContext* aPresContext,
                                     nsIFormProcessor* aFormProcessor,
                                     PRBool isPost,
                                     nsAString& aData,
                                     nsIContent* aSubmitElement,
                                     PRInt32 aSubmitPosition,
                                     PRUint8 aCtrlsModAtSubmit,
                                     PRUint8 aTextDirAtSubmit)
{
  nsresult rv = NS_OK;
  nsString buf;
  PRBool firstTime = PR_TRUE;

  nsAutoString charset;
  GetSubmitCharset(form, charset, aPresContext, aCtrlsModAtSubmit);

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  GetEncoder(form, aPresContext, getter_AddRefs(encoder),
             aCtrlsModAtSubmit, charset);
  // Non-fatal error if fail, so encoder could be NULL

  // collect and encode the data from the children controls
  // JBK walk the elements[] array instead of form frame controls - bug 34297
  PRUint32 numElements;
  PRUint32 elementX;
  form->GetElementCount(&numElements);
  PRBool mustSubmitElement = (aSubmitPosition > -1);
  for (elementX = 0; elementX < numElements || mustSubmitElement; elementX++) {
    nsCOMPtr<nsIFormControl> controlNode;
    if ((aSubmitPosition == (PRInt32)elementX || elementX >= numElements)
       && mustSubmitElement) {
      controlNode = do_QueryInterface(aSubmitElement);
      elementX--;
      mustSubmitElement = PR_FALSE;
    } else {
      form->GetElementAt(elementX, getter_AddRefs(controlNode));
    }

    if (controlNode) {
      PRBool successful;
      rv = controlNode->IsSuccessful(aSubmitElement, &successful);
      NS_ENSURE_SUCCESS(rv, rv);

      if (successful) {
        PRInt32 numValues = 0;
        PRInt32 maxNumValues;
        controlNode->GetMaxNumValues(&maxNumValues);
        if (0 >= maxNumValues) {
          continue;
        }
        nsString* names = new nsString[maxNumValues];
        if (!names) {
          rv = NS_ERROR_OUT_OF_MEMORY;
        } else {
          nsString* values = new nsString[maxNumValues];
          if (!values) {
            rv = NS_ERROR_OUT_OF_MEMORY;
          } else {
            rv = controlNode->GetNamesValues(maxNumValues, numValues,
                                             values, names);
            if (NS_SUCCEEDED(rv)) {
              for (int valueX = 0; valueX < numValues; valueX++) {
                if (PR_TRUE == firstTime) {
                  firstTime = PR_FALSE;
                } else {
                  buf.AppendWithConversion("&");
                }
                nsString* convName = URLEncode(names[valueX],
                                               encoder,
                                               aCtrlsModAtSubmit,
                                               aTextDirAtSubmit,
                                               charset);
                buf += *convName;
                delete convName;
                buf.AppendWithConversion("=");
                nsAutoString newValue;
                newValue.Append(values[valueX]);
                if (aFormProcessor) {
                  // No need for ProcessValue
                  nsCOMPtr<nsIDOMHTMLElement> htmlElement(
                                              do_QueryInterface(controlNode));
                  rv = aFormProcessor->ProcessValue(htmlElement,
                                                    names[valueX],
                                                    newValue);
                  NS_ASSERTION(NS_SUCCEEDED(rv),
                               "unable to Notify form process observer");
                }
                nsString* convValue = URLEncode(newValue,
                                                encoder,
                                                aCtrlsModAtSubmit,
                                                aTextDirAtSubmit,
                                                charset);
                buf += *convValue;
                delete convValue;
              }
            }
            delete [] values;
          }
          delete [] names;
        }
      }
    }
  }
  aData.SetLength(0);
  if (isPost) {
    char size[16];
    sprintf(size, "%d", buf.Length());
    aData = NS_LITERAL_STRING(
              "Content-type: application/x-www-form-urlencoded");
#ifdef SPECIFY_CHARSET_IN_CONTENT_TYPE
    aData += "; charset=";
    aData += charset;
#endif
    aData.Append(NS_LITERAL_STRING(CRLF));
    aData.Append(NS_LITERAL_STRING("Content-Length: "));
    aData.Append(NS_ConvertASCIItoUCS2(size));
    aData.Append(NS_LITERAL_STRING(CRLF));
    aData.Append(NS_LITERAL_STRING(CRLF));
  }
  aData += buf;
  return rv;
}

// static
nsresult
nsFormSubmitter::ProcessAsMultipart(nsIForm* form,
                                    nsIPresContext* aPresContext,
                                    nsIFormProcessor* aFormProcessor,
                                    nsIFileSpec*& aMultipartDataFile,
                                    nsIContent* aSubmitElement,
                                    PRInt32 aSubmitPosition,
                                    PRUint8 aCtrlsModAtSubmit,
                                    PRUint8 aTextDirAtSubmit)
{
  PRBool compatibleSubmit = PR_TRUE;
  nsCOMPtr<nsIPref> prefService(do_GetService(NS_PREF_CONTRACTID));
  if (prefService)
    prefService->GetBoolPref("browser.forms.submit.backwards_compatible",
                             &compatibleSubmit);

  char buffer[BUFSIZE];

  // Create a temporary file to write the form post data to
  nsSpecialSystemDirectory tempDir(
                             nsSpecialSystemDirectory::OS_TemporaryDirectory);
  tempDir += "formpost";
  tempDir.MakeUnique();
  nsIFileSpec* postDataFile = nsnull;
  nsresult rv = NS_NewFileSpecWithSpec(tempDir, &postDataFile);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Post data file couldn't be created!");
  if (NS_FAILED(rv)) return rv;

  // write the content-type, boundary to the tmp file
  char boundary[80];
  sprintf(boundary, "---------------------------%d%d%d",
          rand(), rand(), rand());
  sprintf(buffer, "Content-type: %s; boundary=%s" CRLF, MULTIPART, boundary);
  PRInt32 wantbytes = 0, gotbytes = 0;
  rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
  if (NS_FAILED(rv) || (wantbytes != gotbytes)) return rv;

  nsAutoString charset;
  GetSubmitCharset(form, charset, aPresContext, aCtrlsModAtSubmit);

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  // Non-fatal error
  if (NS_FAILED(GetEncoder(form, aPresContext, getter_AddRefs(encoder),
                          aCtrlsModAtSubmit, charset)))
     encoder = nsnull;

  nsCOMPtr<nsIUnicodeEncoder> platformencoder;
  // Non-fatal error
  if (NS_FAILED(GetPlatformEncoder(getter_AddRefs(platformencoder))))
     platformencoder = nsnull;


  PRInt32 boundaryLen = PL_strlen(boundary);
  PRInt32 contDispLen = PL_strlen(CONTENT_DISP);
  PRInt32 crlfLen     = PL_strlen(CRLF);
  PRInt32 sepLen      = PL_strlen(SEP);

  // compute the content length
  /////////////////////////////

  PRInt32 contentLen = 0;

  // JBK step over elements[] instead of form control frames - bug 34297
  PRUint32 numElements;
  form->GetElementCount(&numElements);
  PRBool mustSubmitElement = (aSubmitPosition > -1);
  for (PRUint32 elementX = 0;
       elementX < numElements || mustSubmitElement;
       elementX++) {
    nsCOMPtr<nsIFormControl> controlNode;
    if ((aSubmitPosition == (PRInt32)elementX || elementX >= numElements)
       && mustSubmitElement) {
      controlNode = do_QueryInterface(aSubmitElement);
      elementX--;
      mustSubmitElement = PR_FALSE;
    } else {
      form->GetElementAt(elementX, getter_AddRefs(controlNode));
    }

    if (controlNode) {
      PRInt32 type;
      PRBool successful;
      controlNode->GetType(&type);
      controlNode->IsSuccessful(aSubmitElement, &successful);
      if (successful) {
        PRInt32 numValues = 0;
        PRInt32 maxNumValues;
        controlNode->GetMaxNumValues(&maxNumValues);
        if (maxNumValues <= 0) {
          continue;
        }
        nsString* names  = new nsString[maxNumValues];
        nsString* values = new nsString[maxNumValues];
        if (NS_FAILED(controlNode->GetNamesValues(maxNumValues, numValues,
                                                  values, names))) {
          continue;
        }
        for (int valueX = 0; valueX < numValues; valueX++) {
          char* name = nsnull;
          char* value = nsnull;
          char* fname = nsnull; // basename (path removed)

          nsString valueStr = values[valueX];
          if (aFormProcessor) {
            // No need for ProcessValue
            nsCOMPtr<nsIDOMHTMLElement> htmlElement(
                                          do_QueryInterface(controlNode));
            rv = aFormProcessor->ProcessValue(htmlElement,
                                              names[valueX], valueStr);
            NS_ASSERTION(NS_SUCCEEDED(rv),
                         "unable to Notify form process observer");
          }
          if (encoder) {
              name  = UnicodeToNewBytes(names[valueX].get(),
                                        names[valueX].Length(),
                                        encoder,
                                        aCtrlsModAtSubmit,
                                        aTextDirAtSubmit,
                                        charset);
          }

          //use the platformencoder only for values containing file names
          PRUint32 fileNameStart = 0;
          if (NS_FORM_INPUT_FILE == type) {
            fileNameStart = GetFileNameWithinPath(valueStr);
            if (platformencoder) {
              value  = UnicodeToNewBytes(valueStr.get(),
                                         valueStr.Length(),
                                         platformencoder,
                                         aCtrlsModAtSubmit,
                                         aTextDirAtSubmit,
                                         charset);

              // filename with the leading dirs stripped
              fname = UnicodeToNewBytes(valueStr.get() + fileNameStart,
                                        valueStr.Length() - fileNameStart,
                                        platformencoder,
                                        aCtrlsModAtSubmit,
                                        aTextDirAtSubmit,
                                        charset);
            }
          } else {
            if (encoder) {
              value  = UnicodeToNewBytes(valueStr.get(),
                                         valueStr.Length(),
                                         encoder,
                                         aCtrlsModAtSubmit,
                                         aTextDirAtSubmit,
                                         charset);
            }
          }

          if (!name)
            name  = ToNewCString(names[valueX]);
          if (!value)
            value = ToNewCString(valueStr);

          if (names[valueX].IsEmpty()) {
            continue;
          }

          // convert value to CRLF line breaks
          char* newValue = nsLinebreakConverter::ConvertLineBreaks(value,
                           nsLinebreakConverter::eLinebreakAny,
                           nsLinebreakConverter::eLinebreakNet);
          if (value)
            nsMemory::Free(value);
          value = newValue;

          // Add boundary line
          contentLen += sepLen + boundaryLen + crlfLen;

          // File inputs should include Content-Transfer-Encoding
          if (NS_FORM_INPUT_FILE == type && !compatibleSubmit) {
            contentLen += PL_strlen(CONTENT_TRANSFER);
            // XXX is there any way to tell when "8bit" or "7bit" etc may be
            // more appropriate than always using "binary"?
            contentLen += PL_strlen(BINARY_CONTENT);
            contentLen += crlfLen;
          }
          // End Content-Transfer-Encoding line

          // Add Content-Disp line
          contentLen += contDispLen;
          contentLen += PL_strlen(name);

          // File inputs also list filename on Content-Disp line
          if (NS_FORM_INPUT_FILE == type) {
            contentLen += PL_strlen(FILENAME);
            contentLen += PL_strlen(fname);
          }
          // End Content-Disp Line (quote plus CRLF)
          contentLen += 1 + crlfLen;  // ending name quote plus CRLF

          // File inputs add Content-Type line
          if (NS_FORM_INPUT_FILE == type) {
            char* contentType = nsnull;
            rv = GetContentType(value, &contentType);
            if (NS_FAILED(rv)) break; // Need to free up anything here?
            contentLen += PL_strlen(CONTENT_TYPE);
            contentLen += PL_strlen(contentType) + crlfLen;
            nsCRT::free(contentType);
          }

          // Blank line before value
          contentLen += crlfLen;

          // File inputs add file contents next
          if (NS_FORM_INPUT_FILE == type &&
              PL_strlen(value)) { // Don't bother if no file specified
            do {
              // Because we have a native path to the file we can't use
              // PR_GetFileInfo on the Mac as it expects a Unix style path.
              // Instead we'll use our spiffy new nsILocalFile
              nsILocalFile* tempFile = nsnull;
              rv = NS_NewLocalFile(value, PR_TRUE, &tempFile);
              NS_ASSERTION(tempFile,
                           "Couldn't create nsIFileSpec to get file size!");
              if (NS_FAILED(rv) || !tempFile)
                break; // NS_ERROR_OUT_OF_MEMORY
              PRUint32 fileSize32 = 0;
              PRInt64  fileSize = LL_Zero();
              rv = tempFile->GetFileSize(&fileSize);
              if (NS_FAILED(rv)) {
                NS_RELEASE(tempFile);
                break;
              }
              LL_L2UI(fileSize32, fileSize);
              contentLen += fileSize32;
              NS_RELEASE(tempFile);
            } while (PR_FALSE);

            // Add CRLF after file
            contentLen += crlfLen;
          } else {
            // Non-file inputs add value line
            contentLen += PL_strlen(value) + crlfLen;
          }
          if (name)
            nsMemory::Free(name);
          if (value)
            nsMemory::Free(value);
          if (fname)
            nsMemory::Free(fname);
        }
        delete [] names;
        delete [] values;
      }
    }

    aMultipartDataFile = postDataFile;
  }

  // Add the post file boundary line
  contentLen += sepLen + boundaryLen + sepLen + crlfLen;

  // write the content
  ////////////////////

  sprintf(buffer, "Content-Length: %d" CRLF CRLF, contentLen);
  rv = postDataFile->Write(buffer, wantbytes = PL_strlen(buffer), &gotbytes);
  if (NS_SUCCEEDED(rv) && (wantbytes == gotbytes)) {

    // write the content passing through all of the form controls a 2nd time
    PRBool mustSubmitElement = (aSubmitPosition > -1);
    for (PRUint32 elementX = 0;
         elementX < numElements || mustSubmitElement;
         elementX++) {
      nsCOMPtr<nsIFormControl> controlNode;
      if ((aSubmitPosition == (PRInt32)elementX || elementX >= numElements)
         && mustSubmitElement) {
        controlNode = do_QueryInterface(aSubmitElement);
        elementX--;
        mustSubmitElement = PR_FALSE;
      } else {
        form->GetElementAt(elementX, getter_AddRefs(controlNode));
      }

      if (controlNode) {
        PRInt32 type;
        PRBool successful;
        controlNode->GetType(&type);
        controlNode->IsSuccessful(aSubmitElement, &successful);
        if (successful) {
          PRInt32 numValues = 0;
          PRInt32 maxNumValues;
          controlNode->GetMaxNumValues(&maxNumValues);
          if (maxNumValues <= 0) {
            continue;
          }
          nsString* names  = new nsString[maxNumValues];
          nsString* values = new nsString[maxNumValues];
          if (NS_FAILED(controlNode->GetNamesValues(maxNumValues, numValues,
                                                    values, names))) {
            continue;
          }
          for (int valueX = 0; valueX < numValues; valueX++) {
            char* name = nsnull;
            char* value = nsnull;
            char* fname = nsnull; // basename (path removed)

            nsString valueStr = values[valueX];

            if (encoder) {
              name  = UnicodeToNewBytes(names[valueX].get(),
                                        names[valueX].Length(),
                                        encoder,
                                        aCtrlsModAtSubmit,
                                        aTextDirAtSubmit,
                                        charset);
            }

            //use the platformencoder only for values containing file names
            PRUint32 fileNameStart = 0;
            if (NS_FORM_INPUT_FILE == type) {
              fileNameStart = GetFileNameWithinPath(valueStr);
              if (platformencoder) {
                value = UnicodeToNewBytes(valueStr.get(),
                                          valueStr.Length(),
                                          platformencoder,
                                          aCtrlsModAtSubmit,
                                          aTextDirAtSubmit,
                                          charset);

                // filename with the leading dirs stripped
                fname = UnicodeToNewBytes(valueStr.get() + fileNameStart,
                                          valueStr.Length() - fileNameStart,
                                          platformencoder,
                                          aCtrlsModAtSubmit,
                                          aTextDirAtSubmit,
                                          charset);
              }
            } else {
              if (encoder) {
                  value = UnicodeToNewBytes(valueStr.get(),
                                            valueStr.Length(),
                                            encoder,
                                            aCtrlsModAtSubmit,
                                            aTextDirAtSubmit,
                                            charset);
              }
            }

            if (!name)
              name  = ToNewCString(names[valueX]);
            if (!value)
              value = ToNewCString(valueStr);

            if (names[valueX].IsEmpty()) {
              continue;
            }

            // convert value to CRLF line breaks
            char* newValue = nsLinebreakConverter::ConvertLineBreaks(value,
                             nsLinebreakConverter::eLinebreakAny,
                             nsLinebreakConverter::eLinebreakNet);
            if (value)
              nsMemory::Free(value);
            value = newValue;

            // Print boundary line
            sprintf(buffer, SEP "%s" CRLF, boundary);
            wantbytes = PL_strlen(buffer);
            rv = postDataFile->Write(buffer, wantbytes, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs should include Content-Transfer-Encoding to prep
            // server side MIME decoders
            if (NS_FORM_INPUT_FILE == type && !compatibleSubmit) {
              wantbytes = PL_strlen(CONTENT_TRANSFER);
              rv = postDataFile->Write(CONTENT_TRANSFER, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

              // XXX is there any way to tell when "8bit" or "7bit" etc may be
              // more appropriate than always using "binary"?

              wantbytes = PL_strlen(BINARY_CONTENT);
              rv = postDataFile->Write(BINARY_CONTENT, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

              wantbytes = PL_strlen(CRLF);
              rv = postDataFile->Write(CRLF, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // Print Content-Disp line
            wantbytes = contDispLen;
            rv = postDataFile->Write(CONTENT_DISP, wantbytes, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            wantbytes = PL_strlen(name);
            rv = postDataFile->Write(name, wantbytes, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs also list filename on Content-Disp line
            if (NS_FORM_INPUT_FILE == type) {
              wantbytes = PL_strlen(FILENAME);
              rv = postDataFile->Write(FILENAME, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              wantbytes = PL_strlen(fname);
              rv = postDataFile->Write(fname, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // End Content Disp
            wantbytes = PL_strlen("\"" CRLF);
            rv = postDataFile->Write("\"" CRLF , wantbytes, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs write Content-Type line
            if (NS_FORM_INPUT_FILE == type) {
              char* contentType = nsnull;
              rv = GetContentType(value, &contentType);
              if (NS_FAILED(rv)) break;
              wantbytes = PL_strlen(CONTENT_TYPE);
              rv = postDataFile->Write(CONTENT_TYPE, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              wantbytes = PL_strlen(contentType);
              rv = postDataFile->Write(contentType, wantbytes, &gotbytes);
              nsCRT::free(contentType);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              wantbytes = PL_strlen(CRLF);
              rv = postDataFile->Write(CRLF, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              // end content-type header
            }

            // Blank line before value
            wantbytes = PL_strlen(CRLF);
            rv = postDataFile->Write(CRLF, wantbytes, &gotbytes);
            if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;

            // File inputs print file contents next
            if (NS_FORM_INPUT_FILE == type) {
              nsIFileSpec* contentFile = nsnull;
              rv = NS_NewFileSpec(&contentFile);
              NS_ASSERTION(contentFile,
                           "Post content file couldn't be created!");
              // NS_ERROR_OUT_OF_MEMORY
              if (NS_FAILED(rv) || !contentFile) break;
              rv = contentFile->SetNativePath(value);
              NS_ASSERTION(contentFile,
                           "Post content file path couldn't be set!");
              if (NS_FAILED(rv)) {
                NS_RELEASE(contentFile);
                break;
              }
              // Print file contents
              PRInt32 size = 1;
              while (1) {
                char* readbuffer = nsnull;
                rv = contentFile->Read(&readbuffer, BUFSIZE, &size);
                if (NS_FAILED(rv) || 0 >= size) break;
                wantbytes = size;
                rv = postDataFile->Write(readbuffer, wantbytes, &gotbytes);
                if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              }
              NS_RELEASE(contentFile);
              // Print CRLF after file
              wantbytes = PL_strlen(CRLF);
              rv = postDataFile->Write(CRLF, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }

            // Non-file inputs print value line
            else {
              wantbytes = PL_strlen(value);
              rv = postDataFile->Write(value, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
              wantbytes = PL_strlen(CRLF);
              rv = postDataFile->Write(CRLF, wantbytes, &gotbytes);
              if (NS_FAILED(rv) || (wantbytes != gotbytes)) break;
            }
            if (name)
              nsMemory::Free(name);
            if (value)
              nsMemory::Free(value);
            if (fname)
              nsMemory::Free(fname);
          }
          delete [] names;
          delete [] values;
        }
      }
    }
  }

  if (NS_SUCCEEDED(rv)) {
    sprintf(buffer, SEP "%s" SEP CRLF, boundary);
    wantbytes = PL_strlen(buffer);
    rv = postDataFile->Write(buffer, wantbytes, &gotbytes);
    if (NS_SUCCEEDED(rv) && (wantbytes == gotbytes)) {
      rv = postDataFile->CloseStream();
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv),
               "Generating the form post temp file failed.\n");
  return rv;
}



// JBK moved from nsFormFrame - bug 34297
// static
void
nsFormSubmitter::GetSubmitCharset(nsIForm* form,
                                  nsAString& oCharset,
                                  nsIPresContext* aPresContext,
                                  PRUint8 aCtrlsModAtSubmit)
{
  oCharset = NS_LITERAL_STRING("UTF-8"); // default to utf-8
  // XXX We may want to get it from the HTML 4 Accept-Charset attribute first
  // see 17.3 The FORM element in HTML 4 for details
  nsresult rv = NS_OK;
  nsAutoString acceptCharsetValue;
  nsCOMPtr<nsIHTMLContent> formContent = do_QueryInterface(form);
  nsHTMLValue value;
  rv = formContent->GetHTMLAttribute(nsHTMLAtoms::acceptcharset, value);
  if (NS_CONTENT_ATTR_HAS_VALUE == rv) {
    if (eHTMLUnit_String == value.GetUnit()) {
      value.GetStringValue(acceptCharsetValue);
    }
  }

#ifdef DEBUG_ftang
  printf("accept-charset = %s\n", acceptCharsetValue.ToNewUTF8String());
#endif
  PRInt32 l = acceptCharsetValue.Length();
  if (l > 0 ) {
    PRInt32 offset=0;
    PRInt32 spPos=0;
    // get charset from charsets one by one
    nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
    if (NS_SUCCEEDED(rv) && calias) {
      do {
        spPos = acceptCharsetValue.FindChar(PRUnichar(' '),PR_TRUE, offset);
        PRInt32 cnt = ((-1==spPos)?(l-offset):(spPos-offset));
        if (cnt > 0) {
          nsAutoString charset;
          acceptCharsetValue.Mid(charset, offset, cnt);
#ifdef DEBUG_ftang
          printf("charset[i] = %s\n",charset.ToNewUTF8String());
#endif
          if (NS_SUCCEEDED(calias->GetPreferred(charset, oCharset)))
            return;
        }
        offset = spPos + 1;
      } while (spPos != -1);
    }
  }
  // if there are no accept-charset or all the charset are not supported
  // Get the charset from document
  nsCOMPtr<nsGenericElement> formElement = do_QueryInterface(form);
  if (formElement) {
    nsIDocument* doc = nsnull;
    formElement->GetDocument(doc);
    if ( doc ) {
      rv = doc->GetDocumentCharacterSet(oCharset);
      NS_RELEASE(doc);
    }
  }

#ifdef IBMBIDI
  if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL
     && Compare(oCharset, NS_LITERAL_STRING("windows-1256"),
                 nsCaseInsensitiveStringComparator()) == 0)  {
//Mohamed
    oCharset = NS_LITERAL_STRING("IBM864");
  }
  else if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_LOGICAL
          && Compare(oCharset, NS_LITERAL_STRING("IBM864"),
                     nsCaseInsensitiveStringComparator()) == 0) {
    oCharset = NS_LITERAL_STRING("IBM864i");
  }
  else if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL
          && Compare(oCharset, NS_LITERAL_STRING("ISO-8859-6"),
                     nsCaseInsensitiveStringComparator()) == 0) {
    oCharset = NS_LITERAL_STRING("IBM864");
  }
  else if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL
          && Compare(oCharset, NS_LITERAL_STRING("UTF-8"),
                     nsCaseInsensitiveStringComparator()) == 0) {
    oCharset = NS_LITERAL_STRING("IBM864");
  }

#endif
}

// JBK moved from nsFormFrame - bug 34297
// static
nsresult
nsFormSubmitter::GetEncoder(nsIForm* form,
                            nsIPresContext* aPresContext,
                            nsIUnicodeEncoder** encoder,
                            PRUint8 aCtrlsModAtSubmit,
                            const nsAString& aCharset)
{
  *encoder = nsnull;
  nsresult rv = NS_OK;
#ifdef DEBUG_ftang
  printf("charset=%s\n", ToNewCString(charset));
#endif

  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);
  if (NS_SUCCEEDED(rv) && ccm) {
     nsString charset(aCharset);
     if (charset.Equals(NS_LITERAL_STRING("ISO-8859-1")))
       charset.Assign(NS_LITERAL_STRING("windows-1252"));
     rv = ccm->GetUnicodeEncoder(&charset, encoder);
     nsServiceManager::ReleaseService( kCharsetConverterManagerCID, ccm);
     if (encoder) {
       rv = NS_ERROR_FAILURE;
     }
     if (NS_SUCCEEDED(rv)) {
       rv = (*encoder)->SetOutputErrorBehavior(
                          nsIUnicodeEncoder::kOnError_Replace,
                          nsnull,
                          (PRUnichar)'?');
     }
  }
  return NS_OK;
}

// XXX i18n helper routines
// static
nsString*
nsFormSubmitter::URLEncode(const nsAString& aString,
                           nsIUnicodeEncoder* encoder,
                           PRUint8 aCtrlsModAtSubmit,
                           PRUint8 aTextDirAtSubmit,
                           const nsAString& aCharset)
{
  char* inBuf = nsnull;
  if (encoder) {
    // XXX This is inefficient.  When nsAString gets a get() equivalent,
    // use that.
    PRUnichar* strUnicode = ToNewUnicode(aString);
    inBuf  = UnicodeToNewBytes(strUnicode,
                               aString.Length(),
                               encoder,
                               aCtrlsModAtSubmit,
                               aTextDirAtSubmit,
                               aCharset);
    delete strUnicode;
  }

  if (!inBuf)
    inBuf  = ToNewCString(aString);

  // convert to CRLF breaks
  char* convertedBuf = nsLinebreakConverter::ConvertLineBreaks(inBuf,
                           nsLinebreakConverter::eLinebreakAny,
                           nsLinebreakConverter::eLinebreakNet);
  delete [] inBuf;

  char* outBuf = nsEscape(convertedBuf, url_XPAlphas);
  nsString* result = new nsString;
  result->AssignWithConversion(outBuf);
  nsCRT::free(outBuf);
  nsMemory::Free(convertedBuf);
  return result;
}

// XXX i18n helper routines
// static
char*
nsFormSubmitter::UnicodeToNewBytes(const PRUnichar* aSrc,
                                   PRUint32 aLen,
                                   nsIUnicodeEncoder* encoder,
                                   PRUint8 aCtrlsModAtSubmit,
                                   PRUint8 aTextDirAtSubmit,
                                   const nsAString& aCharset)
{
#ifdef IBMBIDI
  //ahmed 15-1
  nsAutoString temp;
  nsCOMPtr<nsIUBidiUtils> bidiUtils(
                  do_GetService("@mozilla.org/intl/unicharbidiutil;1"));
  nsAutoString newBuffer;
  //This condition handle the RTL,LTR for a logical file
  if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL
     && Compare(aCharset, NS_LITERAL_STRING("windows-1256"),
                nsCaseInsensitiveStringComparator()) == 0 ) {
    bidiUtils->Conv_06_FE_WithReverse(nsString(aSrc),
                                      newBuffer,
                                      aTextDirAtSubmit);
    aSrc = (PRUnichar *)newBuffer.get();
    aLen=newBuffer.Length();
  }
  else if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_LOGICAL
          && Compare(aCharset, NS_LITERAL_STRING("IBM864"),
                     nsCaseInsensitiveStringComparator()) == 0) {
    //For 864 file, When it is logical, if LTR then only convert
    //If RTL will mak a reverse for the buffer
    bidiUtils->Conv_FE_06(nsString(aSrc), newBuffer);
    aSrc = (PRUnichar *)newBuffer.get();
    temp = newBuffer;
    aLen=newBuffer.Length();
    if (aTextDirAtSubmit == 2) { //RTL
    //Now we need to reverse the Buffer, it is by searching the buffer
      PRUint32 loop = aLen;
      PRUint32 z;
      for (z=0; z<=aLen; z++) {
        temp.SetCharAt((PRUnichar)aSrc[loop], z);
        loop--;
      }
    }
    aSrc = (PRUnichar *)temp.get();
  }
  else if (aCtrlsModAtSubmit==IBMBIDI_CONTROLSTEXTMODE_VISUAL
          && Compare(aCharset, NS_LITERAL_STRING("IBM864"),
                     nsCaseInsensitiveStringComparator()) == 0
                  && aTextDirAtSubmit == IBMBIDI_TEXTDIRECTION_RTL) {

    bidiUtils->Conv_FE_06(nsString(aSrc), newBuffer);
    aSrc = (PRUnichar *)newBuffer.get();
    temp = newBuffer;
    aLen=newBuffer.Length();
    //Now we need to reverse the Buffer, it is by searching the buffer
    PRUint32 loop = aLen;
    PRUint32 z;
    for (z=0; z<=aLen; z++) {
      temp.SetCharAt((PRUnichar)aSrc[loop], z);
      loop--;
    }
    aSrc = (PRUnichar *)temp.get();
  }
#endif
   char* res = nsnull;
   nsresult rv = encoder->Reset();
   if (NS_SUCCEEDED(rv)) {
      PRInt32 maxByteLen = 0;
      rv = encoder->GetMaxLength(aSrc, (PRInt32) aLen, &maxByteLen);
      if (NS_SUCCEEDED(rv)) {
          res = new char[maxByteLen+1];
          if (res) {
             PRInt32 reslen = maxByteLen;
             PRInt32 reslen2 ;
             PRInt32 srclen = aLen;
             encoder->Convert(aSrc, &srclen, res, &reslen);
             reslen2 = maxByteLen-reslen;
             encoder->Finish(res+reslen, &reslen2);
             res[reslen+reslen2] = '\0';
          }
      }

   }
   return res;
}


// static
nsresult
nsFormSubmitter::GetPlatformEncoder(nsIUnicodeEncoder** encoder)
{
  *encoder = nsnull;
  nsAutoString localeCharset;
  nsresult rv = NS_OK;

  // Get Charset, get the encoder.
  nsICharsetConverterManager * ccm = nsnull;
  rv = nsServiceManager::GetService(kCharsetConverterManagerCID ,
                                    NS_GET_IID(nsICharsetConverterManager),
                                    (nsISupports**)&ccm);

  if (NS_SUCCEEDED(rv) && ccm) {

     nsCOMPtr<nsIPlatformCharset> platformCharset(
         do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv));

     if (NS_SUCCEEDED(rv)) {
       rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName,
                                        localeCharset);
     }

     if (NS_FAILED(rv)) {
       NS_ASSERTION(0, "error getting locale charset, using ISO-8859-1");
       localeCharset.AssignWithConversion("ISO-8859-1");
       rv = NS_OK;
     }

     // get unicode converter mgr
     //nsCOMPtr<nsICharsetConverterManager> ccm =
     //         do_GetService(kCharsetConverterManagerCID, &rv);

     if (NS_SUCCEEDED(rv)) {
       rv = ccm->GetUnicodeEncoder(&localeCharset, encoder);
     }
   }

  return NS_OK;
}


// return the filename without the leading directories (Unix basename)
// static
PRUint32
nsFormSubmitter::GetFileNameWithinPath(nsString& aPathName)
{
  // We need to operator on Unicode strings and not on nsCStrings
  // because Shift_JIS and Big5 encoded filenames can have
  // embedded directory separators in them.
#ifdef XP_MAC
  // On a Mac the only invalid character in a file name is a :
  // so we have to avoid the test for '\'. We can't use
  // PR_DIRECTORY_SEPARATOR_STR (even though ':' is a dir sep for MacOS)
  // because this is set to '/' for reasons unknown to this coder.
  PRInt32 fileNameStart = aPathName.RFind(":");
#else
  PRInt32 fileNameStart = aPathName.RFind(PR_DIRECTORY_SEPARATOR_STR);
#endif
  // if no directory separator is found (-1), return the whole
  // string, otherwise return the basename only
  return (PRUint32) (fileNameStart + 1);
}


// static
nsresult
nsFormSubmitter::GetContentType(char* aPathName, char** aContentType)
{
  nsresult rv = NS_OK;
  NS_ASSERTION(aContentType, "null pointer");

  if (aPathName && *aPathName) {
    // Get file extension and mimetype from that.g936
    char* fileExt = aPathName + nsCRT::strlen(aPathName);
    while (fileExt > aPathName) {
      if (*(--fileExt) == '.') {
        break;
      }
    }

    if (*fileExt == '.' && *(fileExt + 1)) {
      nsCOMPtr<nsIMIMEService> MIMEService =
        do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
      if (NS_FAILED(rv))
        return rv;

      rv = MIMEService->GetTypeFromExtension(fileExt + 1, aContentType);

      if (NS_SUCCEEDED(rv)) {
        return NS_OK;
      }
    }
  }
  *aContentType = nsCRT::strdup("application/octet-stream");
  if (!*aContentType) return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

// static
nsresult
nsFormSubmitter::GetEnumAttr(nsIForm* form, nsIAtom* atom, PRInt32* aValue)
{
  nsCOMPtr<nsIHTMLContent> content = do_QueryInterface(form);
  if (content) {
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
                    content->GetHTMLAttribute(atom, value)) {
      if (eHTMLUnit_Enumerated == value.GetUnit()) {
        (*aValue) = value.GetIntValue();
      }
    }
  }
  return NS_OK;
}

// JBK moved from nsFormFrame - bug 34297
// Get the Method, with proper defaults (for submit/reset)
// static
nsresult
nsFormSubmitter::FullyGetMethod(nsIForm* form, PRInt32* aMethod)
{
  (*aMethod) = NS_FORM_METHOD_GET;
  return GetEnumAttr(form, nsHTMLAtoms::method, aMethod);
}

// JBK moved from nsFormFrame - bug 34297
// Get the Enctype, with proper defaults (for submit/reset)
// static
nsresult
nsFormSubmitter::FullyGetEnctype(nsIForm* form, PRInt32* aEnctype)
{
  (*aEnctype) = NS_FORM_ENCTYPE_URLENCODED;
  return GetEnumAttr(form, nsHTMLAtoms::enctype, aEnctype);
}
