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

// mozXMLTermStream.h: declaration of mozXMLTermStream
// which implements the mozIXMLTermStream interface
// to display HTML/XML streams as documents

#include "nscore.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIGenericFactory.h"

#include "nsIDOMWindowInternal.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMElement.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDocument.h"

#include "nsIDocumentLoader.h"
#include "nsIDocumentLoaderFactory.h"

#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"

#include "nsIWebShell.h"
#include "nsIPresShell.h"
#include "nsIScriptContext.h"

#include "mozXMLT.h"
#include "mozIXMLTermStream.h"

#define NO_WORKAROUND

class mozXMLTermStream : public mozIXMLTermStream
{
  public:

  mozXMLTermStream();
  virtual ~mozXMLTermStream();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_MOZIXMLTERMSTREAM

 protected:

  /** Adjusts height of frame displaying stream to fit content
   * @param maxHeight maximum height of resized frame (pixels)
   *                  (zero value implies no maximum)
   */
  NS_IMETHOD SizeToContentHeight(PRInt32 maxHeight);

  /** UTF8 data buffer to hold to be read by rendering engine */
  nsCString mUTF8Buffer;

  /** offset at which to start reading the UTF8 data buffer */
  PRUint32 mUTF8Offset;

  /** maximum frame height for resizing */
  PRInt32 mMaxResizeHeight;

  /** DOM window in which to display stream */
  nsCOMPtr<nsIDOMWindowInternal> mDOMWindow;

  /** Frame element in which to display stream */
  nsCOMPtr<nsIDOMElement> mDOMIFrameElement;

#ifdef NO_WORKAROUND
  /** Context for stream display */
  nsCOMPtr<nsISupports> mContext;

  /** Load group for stream display */
  nsCOMPtr<nsILoadGroup> mLoadGroup;

  /** Channel for stream display */
  nsCOMPtr<nsIChannel> mChannel;

  /** Stream listener object */
  nsCOMPtr<nsIStreamListener> mStreamListener;
#else // !NO_WORKAROUND
  nsCOMPtr<nsIDOMHTMLDocument> mDOMHTMLDocument;
#endif // !NO_WORKAROUND
};

