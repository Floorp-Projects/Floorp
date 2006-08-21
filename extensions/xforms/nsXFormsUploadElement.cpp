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
 *   Darin Fisher <darin@meer.net>
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

#include "nsIXFormsUploadElement.h"
#include "nsIXFormsUploadUIElement.h"
#include "nsXFormsUtils.h"
#include "nsIContent.h"
#include "nsNetUtil.h"
#include "nsXFormsDelegateStub.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "plbase64.h"
#include "nsIFilePicker.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMWindowInternal.h"
#include "nsIStringBundle.h"
#include "nsAutoBuffer.h"
#include "nsIEventStateManager.h"
#include "prmem.h"
#include "nsISchema.h"

#define NS_HTMLFORM_BUNDLE_URL \
          "chrome://global/locale/layout/HtmlForm.properties"

/**
 * Implementation of the \<upload\> element.
 */
class nsXFormsUploadElement : public nsXFormsDelegateStub,
                              public nsIXFormsUploadElement
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIXFORMSUPLOADELEMENT

  NS_IMETHOD Refresh();

private:
  /**
   * Returns the nsISchemaBuiltinType of the node to which this element is
   * bound.
   */
  PRUint16 GetBoundType();

  /**
   * Sets file path/contents into instance data.  If aFile is nsnull,
   * this clears the data.
   */
  nsresult SetFile(nsILocalFile *aFile);

  /**
   * Sets "filename" & "mediatype" in the instance data, using the given file.
   * If aFile == nsnull, then this function clears the values in the
   * instance data.
   */
  nsresult HandleChildElements(nsILocalFile *aFile, PRBool *aChanged);

  /**
   * Read the contents of the file and encode in Base64 or Hex.  |aResult| must
   * be freed by nsMemory::Free().
   */
  nsresult EncodeFileContents(nsIFile *aFile, PRUint16 aType,
                              PRUnichar **aResult);

  void BinaryToHex(const char *aBuffer, PRUint32 aCount,
                   PRUnichar **aHexString);
};

NS_IMPL_ISUPPORTS_INHERITED1(nsXFormsUploadElement,
                             nsXFormsDelegateStub,
                             nsIXFormsUploadElement)

NS_IMETHODIMP
nsXFormsUploadElement::Refresh()
{
  // This is called when the 'bind', 'ref' or 'model' attribute has changed.
  // So we need to make sure that the element is still bound to a node of
  // type 'anyURI', 'base64Binary', or 'hexBinary'.

  nsresult rv = nsXFormsDelegateStub::Refresh();
  if (NS_FAILED(rv) || rv == NS_OK_XFORMS_NOREFRESH)
    return rv;

  if (!mBoundNode)
    return NS_OK;

  // If it is not bound to 'anyURI', 'base64Binary', 'hexBinary', or an
  // extension or derivation of one of these three types, then put an error in
  // the console.  CSS and XBL will make sure that the control won't appear in
  // the form.
  PRUint16 boundType = GetBoundType();
  if (boundType == nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY ||
      boundType == nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY ||
      boundType == nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI) {
    return NS_OK;
  }

  nsXFormsUtils::ReportError(NS_LITERAL_STRING("uploadBoundTypeError"),
                             mElement);
  return NS_OK;
}

PRUint16
nsXFormsUploadElement::GetBoundType()
{
  if (!mModel)
    return 0;

  // get type of bound instance data
  nsCOMPtr<nsISchemaType> schemaType;
  nsresult rv = mModel->GetTypeForControl(this, getter_AddRefs(schemaType));
  if (NS_FAILED(rv)) {
    return 0;
  }

  PRUint16 builtinType = 0;
  rv = mModel->GetRootBuiltinType(schemaType, &builtinType);
  if (NS_FAILED(rv) || !builtinType) {
    return 0;
  }

  return builtinType;
}

static void
ReleaseObject(void    *aObject,
              nsIAtom *aPropertyName,
              void    *aPropertyValue,
              void    *aData)
{
  NS_STATIC_CAST(nsISupports *, aPropertyValue)->Release();
}

NS_IMETHODIMP
nsXFormsUploadElement::PickFile()
{
  if (!mElement)
    return NS_OK;

  nsresult rv;

  // get localized file picker title text
  nsCOMPtr<nsIStringBundleService> bundleService =
            do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIStringBundle> bundle;
  rv = bundleService->CreateBundle(NS_HTMLFORM_BUNDLE_URL,
                                   getter_AddRefs(bundle));
  NS_ENSURE_SUCCESS(rv, rv);
  nsXPIDLString filepickerTitle;
  rv = bundle->GetStringFromName(NS_LITERAL_STRING("FileUpload").get(),
                                 getter_Copies(filepickerTitle));
  if (NS_FAILED(rv)) {
    // fall back to English text
    filepickerTitle.AssignASCII("File Upload");
  }

  // get nsIDOMWindowInternal
  nsCOMPtr<nsIDOMDocument> doc;
  nsCOMPtr<nsIDOMWindowInternal> internal;
  mElement->GetOwnerDocument(getter_AddRefs(doc));
  rv = nsXFormsUtils::GetWindowFromDocument(doc, getter_AddRefs(internal));
  NS_ENSURE_STATE(internal);

  // init filepicker
  nsCOMPtr<nsIFilePicker> filePicker =
            do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  rv = filePicker->Init(internal, filepickerTitle, nsIFilePicker::modeOpen);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set the file picker filters based on the mediatype attribute.
  nsAutoString mediaType;
  mElement->GetAttribute(NS_LITERAL_STRING("mediatype"), mediaType);

  if (!mediaType.IsEmpty()) {
    // The mediatype attribute contains a space delimited list of mime types.
    nsresult rv;
    nsCOMPtr<nsIMIMEService> mimeService =
      do_GetService("@mozilla.org/mime;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAString::const_iterator start, end, iter;
    mediaType.BeginReading(start);
    mediaType.BeginReading(iter);
    mediaType.EndReading(end);

    nsAutoString fileFilter;
    nsAutoString mimeType;
    while (iter != end) {
      if (FindCharInReadable(' ', iter, end)) {
         mimeType = Substring(start, iter);
         // Skip the space.
         ++iter;
         // Save the starting position for the next mime type (if any).
         start = iter;
      } else {
        // Only 1 media type or we've reached the end of the list.
        mimeType = Substring(start, end);
      }

      // Map the mime type to a file extension and add the extension to the file
      // type filter for the file dialog.
      nsCAutoString fileExtension;
      rv = mimeService->GetPrimaryExtension(NS_ConvertUTF16toUTF8(mimeType),
                                            EmptyCString(), fileExtension);
      if (NS_SUCCEEDED(rv) && !fileExtension.IsEmpty()) {
        fileFilter.AppendLiteral("*.");
        fileFilter.Append(NS_ConvertUTF8toUTF16(fileExtension));
        if (iter != end) {
          fileFilter.AppendLiteral(";");
        }
      }
    }

    // Append the file extension filter.
    filePicker->AppendFilter(fileFilter, fileFilter);
  }

  // Always add 'All Files'
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  // open dialog
  PRInt16 mode;
  rv = filePicker->Show(&mode);
  NS_ENSURE_SUCCESS(rv, rv);
  if (mode == nsIFilePicker::returnCancel)
    return NS_OK;

  // file was selected
  nsCOMPtr<nsILocalFile> localFile;
  rv = filePicker->GetFile(getter_AddRefs(localFile));
  if (localFile) {
    // set path value in \<upload\>'s text field.
    nsCOMPtr<nsIXFormsUploadUIElement> uiUpload = do_QueryInterface(mElement);
    if (uiUpload) {
      nsCAutoString spec;
      NS_GetURLSpecFromFile(localFile, spec);
      uiUpload->SetFieldText(NS_ConvertUTF8toUTF16(spec));
    }

    // set file into instance data
    return SetFile(localFile);
  }

  return rv;
}

NS_IMETHODIMP
nsXFormsUploadElement::ClearFile()
{
  // clear path value in \<upload\>'s text field.
  nsCOMPtr<nsIXFormsUploadUIElement> uiUpload = do_QueryInterface(mElement);
  if (uiUpload) {
    uiUpload->SetFieldText(EmptyString());
  }

  // clear file from instance data
  return SetFile(nsnull);
}

nsresult
nsXFormsUploadElement::SetFile(nsILocalFile *aFile)
{
  if (!mBoundNode || !mModel)
    return NS_OK;

  nsresult rv;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mBoundNode);
  NS_ENSURE_STATE(content);

  PRBool dataChanged = PR_FALSE;
  if (!aFile) {
    // clear instance data
    content->DeleteProperty(nsXFormsAtoms::uploadFileProperty);
    rv = mModel->SetNodeValue(mBoundNode, EmptyString(), PR_FALSE,
                              &dataChanged);
  } else {
    // set file into instance data

    PRUint16 type = GetBoundType();
    if (type == nsISchemaBuiltinType::BUILTIN_TYPE_ANYURI) {
      // set fully qualified path as value in instance data node
      nsCAutoString spec;
      NS_GetURLSpecFromFile(aFile, spec);
      rv = mModel->SetNodeValue(mBoundNode, NS_ConvertUTF8toUTF16(spec),
                                PR_FALSE, &dataChanged);
    } else if (type == nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY ||
               type == nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY) {
      // encode file contents in base64/hex and set into instance data node
      PRUnichar *fileData;
      rv = EncodeFileContents(aFile, type, &fileData);
      if (NS_SUCCEEDED(rv)) {
        rv = mModel->SetNodeValue(mBoundNode, nsDependentString(fileData),
                                  PR_FALSE, &dataChanged);
        nsMemory::Free(fileData);
      }
    } else {
      rv = NS_ERROR_FAILURE;
    }

    // XXX need to handle derived types

    // Set nsIFile object as property on instance data node, so submission
    // code can use it.
    if (NS_SUCCEEDED(rv)) {
      nsIFile *fileCopy = nsnull;
      rv = aFile->Clone(&fileCopy);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = content->SetProperty(nsXFormsAtoms::uploadFileProperty, fileCopy,
                                ReleaseObject);
    }
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Handle <filename> and <mediatype> children
  PRBool childrenChanged;
  rv = HandleChildElements(aFile, &childrenChanged);
  NS_ENSURE_SUCCESS(rv, rv);

  if (dataChanged || childrenChanged) {
    rv = mModel->RequestRecalculate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mModel->RequestRevalidate();
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mModel->RequestRefresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsXFormsUploadElement::HandleChildElements(nsILocalFile *aFile,
                                           PRBool *aChanged)
{
  NS_ENSURE_ARG_POINTER(aChanged);
  NS_ENSURE_STATE(mModel);

  *aChanged = PR_FALSE;

  // return immediately if we have no children
  PRBool hasNodes;
  mElement->HasChildNodes(&hasNodes);
  if (!hasNodes)
    return NS_OK;

  nsresult rv = NS_OK;

  // look for the \<filename\> & \<mediatype\> elements in the
  // \<upload\> children
  nsCOMPtr<nsIDOMNode> filenameNode, mediatypeNode;
  nsCOMPtr<nsIDOMNode> child, temp;
  mElement->GetFirstChild(getter_AddRefs(child));
  while (child && (!filenameNode || !mediatypeNode)) {
    if (!filenameNode &&
        nsXFormsUtils::IsXFormsElement(child,
                                       NS_LITERAL_STRING("filename")))
    {
      filenameNode = child;
    }

    if (!mediatypeNode &&
        nsXFormsUtils::IsXFormsElement(child,
                                       NS_LITERAL_STRING("mediatype")))
    {
      mediatypeNode = child;
    }

    temp.swap(child);
    temp->GetNextSibling(getter_AddRefs(child));
  }

  // handle "filename"
  PRBool filenameChanged = PR_FALSE;
  if (filenameNode) {
    nsCOMPtr<nsIDOMElement> filenameElem = do_QueryInterface(filenameNode);
    if (aFile) {
      nsAutoString filename;
      rv = aFile->GetLeafName(filename);
      if (!filename.IsEmpty()) {
        rv = nsXFormsUtils::SetSingleNodeBindingValue(filenameElem, filename,
                                                      &filenameChanged);
      }
    } else {
      rv = nsXFormsUtils::SetSingleNodeBindingValue(filenameElem, EmptyString(),
                                                    &filenameChanged);
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // handle "mediatype"
  PRBool mediatypechanged = PR_FALSE;
  if (mediatypeNode) {
    nsCOMPtr<nsIDOMElement> mediatypeElem = do_QueryInterface(mediatypeNode);
    if (aFile) {
      nsCOMPtr<nsIMIMEService> mimeService =
                do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        nsCAutoString contentType;
        rv = mimeService->GetTypeFromFile(aFile, contentType);
        if (NS_FAILED(rv)) {
          contentType.AssignLiteral("application/octet-stream");
        }
        rv = nsXFormsUtils::SetSingleNodeBindingValue(mediatypeElem,
                        NS_ConvertUTF8toUTF16(contentType), &mediatypechanged);
      }
    } else {
      rv = nsXFormsUtils::SetSingleNodeBindingValue(mediatypeElem,
                        EmptyString(), &mediatypechanged);
    }
  }

  *aChanged = filenameChanged || mediatypechanged;
  return rv;
}

typedef nsAutoBuffer<char, 256> nsAutoCharBuffer;

static void
ReportEncodingMemoryError(nsIDOMElement* aElement, nsIFile *aFile,
                          PRUint32 aFailedSize)
{
  nsAutoString filename;
  if (NS_FAILED(aFile->GetLeafName(filename))) {
    return;
  }

  nsAutoString size;
  size.AppendInt((PRInt64)aFailedSize);
  const PRUnichar *strings[] = { filename.get(), size.get() };
  nsXFormsUtils::ReportError(NS_LITERAL_STRING("encodingMemoryError"),
                             strings, 2, aElement, aElement);
}

nsresult
nsXFormsUploadElement::EncodeFileContents(nsIFile *aFile, PRUint16 aType,
                                          PRUnichar **aResult)
{
  nsresult rv;

  // get file contents
  nsCOMPtr<nsIInputStream> fileStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), aFile,
                                  PR_RDONLY, -1 /* no mode bits */,
                                  nsIFileInputStream::CLOSE_ON_EOF);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 size;
  rv = fileStream->Available(&size);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCharBuffer fileData;
  if (!fileData.EnsureElemCapacity(size + 1)) {
    ReportEncodingMemoryError(mElement, aFile, size + 1);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 bytesRead;
  rv = fileStream->Read(fileData.get(), size, &bytesRead);
  NS_ASSERTION(NS_SUCCEEDED(rv) && bytesRead == size,
               "fileStream->Read failed");

  if (NS_SUCCEEDED(rv)) {
    if (aType == nsISchemaBuiltinType::BUILTIN_TYPE_BASE64BINARY) {
      // encode file contents
      *aResult = nsnull;
      char *buffer = PL_Base64Encode(fileData.get(), bytesRead, nsnull);
      if (buffer) {
        *aResult = ToNewUnicode(nsDependentCString(buffer));
        PR_Free(buffer);
      }
      if (!*aResult) {
        PRUint32 failedSize = buffer ? strlen(buffer) * sizeof(PRUnichar)
                                     : ((bytesRead + 2) / 3) * 4 + 1;
        ReportEncodingMemoryError(mElement, aFile, failedSize);
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    } else if (aType == nsISchemaBuiltinType::BUILTIN_TYPE_HEXBINARY) {
      // create buffer for hex encoded data
      PRUint32 length = bytesRead * 2 + 1;
      PRUnichar *fileDataHex =
        NS_STATIC_CAST(PRUnichar*, nsMemory::Alloc(length * sizeof(PRUnichar)));
      if (!fileDataHex) {
        ReportEncodingMemoryError(mElement, aFile, length * sizeof(PRUnichar));
        rv = NS_ERROR_OUT_OF_MEMORY;
      } else {
        // encode file contents
        BinaryToHex(fileData.get(), bytesRead, &fileDataHex);
        fileDataHex[bytesRead * 2] = 0;
        *aResult = fileDataHex;
      }
    } else {
      NS_ERROR("Unknown encoding type for <upload> element");
      rv = NS_ERROR_INVALID_ARG;
    }
  }

  return rv;
}

static inline PRUnichar
ToHexChar(PRInt16 aValue)
{
  if (aValue < 10)
    return (PRUnichar) aValue + '0';
  else
    return (PRUnichar) aValue - 10 + 'A';
}

void
nsXFormsUploadElement::BinaryToHex(const char *aBuffer, PRUint32 aCount,
                                   PRUnichar **aHexString)
{
  for (PRUint32 index = 0; index < aCount; index++) {
    (*aHexString)[index * 2] = ToHexChar((aBuffer[index] >> 4) & 0xf);
    (*aHexString)[index * 2 + 1] = ToHexChar(aBuffer[index] & 0xf);
  }
}


NS_HIDDEN_(nsresult)
NS_NewXFormsUploadElement(nsIXTFElement **aResult)
{
  *aResult = new nsXFormsUploadElement();
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);
  return NS_OK;
}
