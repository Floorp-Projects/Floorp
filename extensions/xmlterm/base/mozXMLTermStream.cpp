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
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// mozXMLTermStream.cpp: implementation of mozIXMLTermStream
// to display HTML/XML streams as documents

#include "nscore.h"
#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "plstr.h"

#include "nsMemory.h"

#include "nsIServiceManager.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"

#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDeviceContext.h"
#include "nsIFrame.h"

#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"

#include "nsICategoryManager.h"
#include "nsXPCOMCID.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"
#include "mozXMLTermStream.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);

/////////////////////////////////////////////////////////////////////////
// mozXMLTermStream implementation
/////////////////////////////////////////////////////////////////////////

NS_IMPL_THREADSAFE_ISUPPORTS2(mozXMLTermStream, 
                              mozIXMLTermStream,
                              nsIInputStream)

mozXMLTermStream::mozXMLTermStream() :
  mUTF8Buffer(""),
  mUTF8Offset(0),
  mMaxResizeHeight(0),
  mDOMWindow( nsnull ),
#ifdef NO_WORKAROUND
  mDOMIFrameElement( nsnull ),
  mContext( nsnull ),
  mLoadGroup( nsnull ),
  mChannel( nsnull ),
  mStreamListener( nsnull )
#else // !NO_WORKAROUND
  mDOMHTMLDocument( nsnull )
#endif // !NO_WORKAROUND
{
}


mozXMLTermStream::~mozXMLTermStream()
{
}

// mozIXMLTermStream interface

/** Open stream in specified frame, or in current frame if frameName is null
 * @param aDOMWindow parent window
 * @param frameName name of child frame in which to display stream, or null
 *                  to display in parent window
 * @param contentURL URL of stream content
 * @param contentType MIME type of stream content
 * @param maxResizeHeight maximum resize height (0=> do not resize)
 * @return NS_OK on success
 */
NS_IMETHODIMP mozXMLTermStream::Open(nsIDOMWindowInternal* aDOMWindow,
                                     const char* frameName,
                                     const char* contentURL,
                                     const char* contentType,
                                     PRInt32 maxResizeHeight)
{
  nsresult result;

  XMLT_LOG(mozXMLTermStream::Open,20,("contentURL=%s, contentType=%s\n",
                                      contentURL, contentType));

  mMaxResizeHeight = maxResizeHeight;

  if (frameName && *frameName) {
    // Open stream in named subframe of current frame
    XMLT_LOG(mozXMLTermStream::Open,22,("frameName=%s\n", frameName));

    nsAutoString innerFrameName; innerFrameName.AssignASCII(frameName);

    // Get DOM IFRAME element
    nsCOMPtr<nsIDOMDocument> domDoc;
    result = aDOMWindow->GetDocument(getter_AddRefs(domDoc));
    if (NS_FAILED(result) || !domDoc)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMHTMLDocument> domHTMLDoc = do_QueryInterface(domDoc);
    if (!domHTMLDoc)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNodeList> nodeList;
    result = domHTMLDoc->GetElementsByName(innerFrameName,
                                           getter_AddRefs(nodeList));
    if (NS_FAILED(result) || !nodeList)
      return NS_ERROR_FAILURE;

    PRUint32 count;
    nodeList->GetLength(&count);
    PR_ASSERT(count==1);

    nsCOMPtr<nsIDOMNode> domNode;
    result = nodeList->Item(0, getter_AddRefs(domNode));
    if (NS_FAILED(result) || !domNode)
      return NS_ERROR_FAILURE;

    mDOMIFrameElement = do_QueryInterface(domNode);
    if (!mDOMIFrameElement)
      return NS_ERROR_FAILURE;

    // Ensure that it is indeed an IFRAME element
    nsAutoString tagName;
    result = mDOMIFrameElement->GetTagName(tagName);
    if (NS_FAILED(result))
      return NS_ERROR_FAILURE;

    if (!tagName.LowerCaseEqualsLiteral("iframe"))
      return NS_ERROR_FAILURE;

    if (mMaxResizeHeight > 0) {
      // Set initial IFRAME size to be as wide as the window, but very short
      nsAutoString attWidth(NS_LITERAL_STRING("width"));
      nsAutoString valWidth(NS_LITERAL_STRING("100%"));
      mDOMIFrameElement->SetAttribute(attWidth,valWidth);

      nsAutoString attHeight(NS_LITERAL_STRING("height"));
      nsAutoString valHeight(NS_LITERAL_STRING("10"));
      mDOMIFrameElement->SetAttribute(attHeight,valHeight);
    }

    // Get inner DOM window by looking up the frames list
    nsCOMPtr<nsIDOMWindowInternal> innerDOMWindow;
    result = mozXMLTermUtils::GetInnerDOMWindow(aDOMWindow, innerFrameName,
                                getter_AddRefs(innerDOMWindow));
    if (NS_FAILED(result) || !innerDOMWindow)
      return NS_ERROR_FAILURE;

    mDOMWindow = innerDOMWindow;

  } else {
    // Open stream in current frame
    mDOMIFrameElement = nsnull;
    mDOMWindow = aDOMWindow;
  }

  // Get docshell for DOM window
  nsCOMPtr<nsIDocShell> docShell;
  result = mozXMLTermUtils::ConvertDOMWindowToDocShell(mDOMWindow,
                                                    getter_AddRefs(docShell));
  if (NS_FAILED(result) || !docShell)
    return NS_ERROR_FAILURE;

#ifdef NO_WORKAROUND
  XMLT_WARNING("mozXMLTermStream::Open, NO_WORKAROUND, url=%s\n", contentURL);

  nsCOMPtr<nsIInputStream> inputStream = this;

  // Create a simple URI
  nsCOMPtr<nsIURI> uri = do_CreateInstance(kSimpleURICID, &result);
  if (NS_FAILED(result))
    return result;

  result = uri->SetSpec(nsDependentCString(contentURL));
  if (NS_FAILED(result))
    return result;


  // Create a new load group
  result = NS_NewLoadGroup(getter_AddRefs(mLoadGroup), nsnull);
  if (NS_FAILED(result))
    return result;

  // Create an input stream channel
  result = NS_NewInputStreamChannel(getter_AddRefs(mChannel),
                                    uri,
                                    inputStream,
                                    nsDependentCString(contentType),
                                    EmptyCString());
  if (NS_FAILED(result))
    return result;

  // Set channel's load group
  result = mChannel->SetLoadGroup(mLoadGroup);
  if (NS_FAILED(result))
    return result;

  // Create document loader for specified command and content type
  nsCOMPtr<nsICategoryManager> catMan(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &result));
  if (NS_FAILED(result))
    return result;

  nsXPIDLCString contractID;
  result = catMan->GetCategoryEntry("Gecko-Content-Viewers", contentType,
                                    getter_Copies(contractID));
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIDocumentLoaderFactory> docLoaderFactory;
  docLoaderFactory = do_GetService(contractID.get(), &result);
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIContentViewerContainer> contViewContainer =
                                             do_QueryInterface(docShell);
  nsCOMPtr<nsIContentViewer> contentViewer;
  result = docLoaderFactory->CreateInstance("view",
                                            mChannel,
                                            mLoadGroup,
                                            contentType,
                                            contViewContainer,
                                            nsnull,
                                            getter_AddRefs(mStreamListener),
                                            getter_AddRefs(contentViewer) );
  if (NS_FAILED(result))
    return result;

  // Set container for content view
  result = contentViewer->SetContainer(contViewContainer);
  if (NS_FAILED(result))
    return result;

  // Embed contentViewer in containing docShell
  result = contViewContainer->Embed(contentViewer, "view", nsnull);
  if (NS_FAILED(result))
    return result;

  // Start request
  result = mStreamListener->OnStartRequest(mChannel, mContext);
  if (NS_FAILED(result))
    return result;

#else // !NO_WORKAROUND
  XMLT_WARNING("mozXMLTermStream::Open, WORKAROUND\n");

  nsCOMPtr<nsIDOMDocument> innerDOMDoc;
  result = mDOMWindow->GetDocument(getter_AddRefs(innerDOMDoc));
  XMLT_WARNING("mozXMLTermStream::Open,check1, 0x%x, 0x%x\n",
         result, (int) innerDOMDoc.get());
  if (NS_FAILED(result) || !innerDOMDoc)
    return NS_ERROR_FAILURE;

  mDOMHTMLDocument = do_QueryInterface(innerDOMDoc);
  XMLT_WARNING("mozXMLTermStream::Open,check2, 0x%x\n", result);
  if (!mDOMHTMLDocument)
    return NS_ERROR_FAILURE;

  result = mDOMHTMLDocument->Open();
  XMLT_WARNING("mozXMLTermStream::Open,check3, 0x%x\n", result);
  if (NS_FAILED(result))
    return result;
#endif // !NO_WORKAROUND

  XMLT_LOG(mozXMLTermStream::Open,21,("returning\n"));

  return NS_OK;
}


// nsIInputStream interface
NS_IMETHODIMP mozXMLTermStream::Close(void)
{
  nsresult result;

  XMLT_LOG(mozXMLTermStream::Close,20,("\n"));

  mUTF8Buffer = "";
  mUTF8Offset = 0;

#ifdef NO_WORKAROUND
  PRUint32 sourceOffset = 0;
  PRUint32 count = 0;
  result = mStreamListener->OnDataAvailable(mChannel, mContext,
                                            this, sourceOffset, count);
  if (NS_FAILED(result))
    return result;

  nsresult status = NS_OK;
  nsAutoString errorMsg;
  result = mStreamListener->OnStopRequest(mChannel, mContext, status);
  if (NS_FAILED(result))
    return result;

  mContext = nsnull;
  mLoadGroup = nsnull;
  mChannel = nsnull;
  mStreamListener = nsnull;

#else // !NO_WORKAROUND
  result = mDOMHTMLDocument->Close();
  if (NS_FAILED(result))
    return result;
#endif // !NO_WORKAROUND

  if (mMaxResizeHeight && mDOMIFrameElement) {
    // Size frame to content
    result = SizeToContentHeight(mMaxResizeHeight);
  }
  mMaxResizeHeight = 0;

  // Release interfaces etc
  mDOMWindow = nsnull;
  mDOMIFrameElement = nsnull;

  return NS_OK;
}


/** Adjusts height of frame displaying stream to fit content
 * @param maxHeight maximum height of resized frame (pixels)
 *                  (zero value implies no maximum)
 */
NS_IMETHODIMP mozXMLTermStream::SizeToContentHeight(PRInt32 maxHeight)
{
  nsresult result;

  // Get docshell
  nsCOMPtr<nsIDocShell> docShell;
  result = mozXMLTermUtils::ConvertDOMWindowToDocShell(mDOMWindow,
                                                   getter_AddRefs(docShell));
  if (NS_FAILED(result) || !docShell)
    return NS_ERROR_FAILURE;

  // Get pres context
  nsCOMPtr<nsPresContext> presContext;
  result = docShell->GetPresContext(getter_AddRefs(presContext));
  if (NS_FAILED(result) || !presContext)
    return NS_ERROR_FAILURE;

  // Get scrollable view
  nsIScrollableView* scrollableView;
  result = mozXMLTermUtils::GetPresContextScrollableView(presContext,
                                                         &scrollableView);
  if (NS_FAILED(result) || !scrollableView)
    return NS_ERROR_FAILURE;

  // Get device context
  nsCOMPtr<nsIDeviceContext> deviceContext;
  result = mozXMLTermUtils::GetPresContextDeviceContext(presContext,
                                              getter_AddRefs(deviceContext));
  if (NS_FAILED(result) || !deviceContext)
    return NS_ERROR_FAILURE;

  // Determine twips to pixels conversion factor
  float pixelScale;
  pixelScale = presContext->TwipsToPixels();

  // Get scrollbar dimensions in pixels
  float sbWidth, sbHeight;
  deviceContext->GetScrollBarDimensions(sbWidth, sbHeight);
  PRInt32 scrollBarWidth = PRInt32(sbWidth*pixelScale);
  PRInt32 scrollBarHeight = PRInt32(sbHeight*pixelScale);

  // Determine docshell size in pixels
  nsRect shellArea = presContext->GetVisibleArea();

  PRInt32 shellWidth = PRInt32((float)shellArea.width * pixelScale);
  PRInt32 shellHeight = PRInt32((float)shellArea.height * pixelScale);

  // Determine page size in pixels
  nscoord contX, contY;
  scrollableView->GetContainerSize(&contX, &contY);

  PRInt32 pageWidth, pageHeight;
  pageWidth = PRInt32((float)contX*pixelScale);
  pageHeight = PRInt32((float)contY*pixelScale);

  XMLT_WARNING("mozXMLTermStream::SizeToContentHeight: scrollbar %d, %d\n",
         scrollBarWidth, scrollBarHeight);

  XMLT_WARNING("mozXMLTermStream::SizeToContentHeight: presShell %d, %d\n",
         shellWidth, shellHeight);

  XMLT_WARNING("mozXMLTermStream::SizeToContentHeight: page %d, %d, %e\n",
         pageWidth, pageHeight, pixelScale);

  if ((pageHeight > shellHeight) || (pageWidth > shellWidth)) {
    // Page larger than docshell
    nsAutoString attHeight(NS_LITERAL_STRING("height"));
    nsAutoString attWidth(NS_LITERAL_STRING("width"));
    nsAutoString attValue; attValue.SetLength(0);

    PRInt32 newPageHeight = pageHeight;
    PRInt32 excessWidth = (pageWidth+scrollBarWidth - shellWidth);

    XMLT_WARNING("mozXMLTermStream::SizeToContentHeight: excessWidth %d\n",
           excessWidth);

    if (excessWidth > 0) {
      // Widen IFRAME beyond page width by scrollbar width
      attValue.SetLength(0);
      attValue.AppendInt(shellWidth+scrollBarWidth);
      mDOMIFrameElement->SetAttribute(attWidth,attValue);

      // Recompute page dimensions
      scrollableView->GetContainerSize(&contX, &contY);
      pageWidth = PRInt32((float)contX*pixelScale);
      pageHeight = PRInt32((float)contY*pixelScale);

      newPageHeight = pageHeight;
      if (excessWidth > scrollBarWidth)
        newPageHeight += scrollBarHeight;

      XMLT_WARNING("mozXMLTermStream::SizeToContentHeight: page2 %d, %d, %d\n",
             pageWidth, pageHeight, newPageHeight);

      // Reset IFRAME width
      attValue.SetLength(0);
      attValue.AppendInt(shellWidth);
      mDOMIFrameElement->SetAttribute(attWidth,attValue);
    }

    // Resize IFRAME height to match page height (subject to a maximum)
    if (newPageHeight > maxHeight) newPageHeight = maxHeight;
    attValue.SetLength(0);
    attValue.AppendInt(newPageHeight);
    mDOMIFrameElement->SetAttribute(attHeight,attValue);
  }

  return NS_OK;
}


// nsIInputStream interface
NS_IMETHODIMP mozXMLTermStream::Available(PRUint32 *_retval)
{
  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  *_retval = mUTF8Buffer.Length() - mUTF8Offset;

  XMLT_LOG(mozXMLTermStream::Available,60,("retval=%d\n", *_retval));

  return NS_OK;
}


NS_IMETHODIMP mozXMLTermStream::Read(char* buf, PRUint32 count,
                                     PRUint32* _retval)
{
  XMLT_LOG(mozXMLTermStream::Read,60,("count=%d\n", count));

  if (!_retval)
    return NS_ERROR_NULL_POINTER;

  PR_ASSERT(mUTF8Buffer.Length() >= mUTF8Offset);

  PRUint32 remCount = mUTF8Buffer.Length() - mUTF8Offset;

  if (remCount == 0) {
    // Empty buffer
    *_retval = 0;
    return NS_OK;
  }

  if (count >= remCount) {
    // Return entire remaining buffer
    *_retval = remCount;

  } else {
    // Return only portion of buffer
    *_retval = count;
  }

  // Copy portion of string
  PL_strncpyz(buf, mUTF8Buffer.get() + mUTF8Offset, *_retval);

  mUTF8Offset += *_retval;

  XMLT_LOG(mozXMLTermStream::Read,61,("*retval=%d\n", *_retval));

  return NS_OK;
}


/** Write Unicode string to stream (blocks until write is completed)
 * @param buf string to write
 * @return NS_OK on success
 */
NS_IMETHODIMP mozXMLTermStream::Write(const PRUnichar* buf)
{
  nsresult result;

  XMLT_LOG(mozXMLTermStream::Write,50,("\n"));

  if (!buf)
    return NS_ERROR_FAILURE;

  nsAutoString strBuf ( buf );

  // Convert Unicode string to UTF8 and store in buffer
  char* utf8Str = ToNewUTF8String(strBuf);
  mUTF8Buffer = utf8Str;
  nsMemory::Free(utf8Str);

  mUTF8Offset = 0;

#ifdef NO_WORKAROUND
  PRUint32 sourceOffset = 0;

  while (mUTF8Offset < mUTF8Buffer.Length()) {
    PRUint32 remCount = mUTF8Buffer.Length() - mUTF8Offset;
    result = mStreamListener->OnDataAvailable(mChannel, mContext,
                                              this, sourceOffset, remCount);
    if (NS_FAILED(result))
      return result;
  }

#else // !NO_WORKAROUND
  result = mDOMHTMLDocument->Write(strBuf);
  if (NS_FAILED(result))
    return result;
#endif // !NO_WORKAROUND

  XMLT_WARNING("mozXMLTermStream::Write: str=%s\n", mUTF8Buffer.get());

  XMLT_LOG(mozXMLTermStream::Write,51,
           ("returning mUTF8Offset=%u\n", mUTF8Offset));

  return NS_OK;
}

NS_IMETHODIMP
mozXMLTermStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    NS_NOTREACHED("ReadSegments");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
mozXMLTermStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_TRUE;
    return NS_OK;
}
