/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIDocumentEncoder_h__
#define nsIDocumentEncoder_h__

#include "nsISupports.h"
#include "nsString.h"

class nsIDocumentEncoder;
class nsIDocument;
class nsIDOMRange;
class nsISelection;
class nsIOutputStream;
class nsISupportsArray;
class nsIDOMNode;


#define NS_IDOCUMENT_ENCODER_IID                     \
{ /* a6cf9103-15b3-11d2-932e-00805f8add32 */         \
    0xa6cf9103,                                      \
    0x15b3,                                          \
    0x11d2,                                          \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} \
  }

#define NS_TEXT_ENCODER_CID                          \
{ /* e7ba1480-1dea-11d3-830f-00104bed045e */         \
    0xe7ba1480,                                      \
    0x1dea,                                          \
    0x11d3,                                          \
    {0x83, 0x0f, 0x00, 0x10, 0x4b, 0xed, 0x04, 0x5e} \
}

#define NS_DOC_ENCODER_CONTRACTID_BASE "@mozilla.org/layout/documentEncoder;1?type="

// {7f915b01-98fc-11d4-8eb0-a803f80ff1bc}
#define NS_HTMLCOPY_TEXT_ENCODER_CID                      \
{ 0x7f915b01, 0x98fc, 0x11d4, { 0x8e, 0xb0, 0xa8, 0x03, 0xf8, 0x0f, 0xf1, 0xbc } }

// {0BC1FAC0-B710-11d4-959F-0020183BF181}
#define NS_IDOCUMENTENCODERNODEFIXUP_IID                     \
{ 0xbc1fac0, 0xb710, 0x11d4, { 0x95, 0x9f, 0x0, 0x20, 0x18, 0x3b, 0xf1, 0x81 } }
  
#define NS_HTMLCOPY_ENCODER_CONTRACTID "@mozilla.org/layout/htmlCopyEncoder;1"

class nsIDocumentEncoderNodeFixup : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTENCODERNODEFIXUP_IID)

  /**
   * Create a fixed up version of a node. This method is called before
   * each node in a document is about to be persisted. The implementor
   * may return a new node with fixed up attributes or nsnull. 
   */
  NS_IMETHOD FixupNode(nsIDOMNode *aNode, nsIDOMNode **aOutNode) = 0;
};

class nsIDocumentEncoder : public nsISupports
{
public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENT_ENCODER_IID)

  /**
   * Output methods flag bits.
   *
   * There are a frightening number of these,
   * because everyone wants something a little bit different!
   *
   * These should move to an idl file so that Javascript can
   * have access to the symbols, not just the constants.
   */
  enum {
    // Output only the selection (as opposed to the whole document).
    OutputSelectionOnly = 1,

    // Plaintext output: Convert html to plaintext that looks like the html.
    // Implies wrap (except inside <pre>), since html wraps.
    // HTML output: always do prettyprinting, ignoring existing formatting.
    // (Probably not well tested for HTML output.)
    OutputFormatted     = 2,

    // OutputRaw is used by copying text from widgets
    OutputRaw           = 4,

    // No html head tags
    OutputBodyOnly      = 8,

    // Wrap even if we're not doing formatted output (e.g. for text fields)
    OutputPreformatted  = 16,

    // Output as though the content is preformatted
    // (e.g. maybe it's wrapped in a MOZ_PRE or MOZ_PRE_WRAP style tag)
    OutputWrap          = 32,

    // Output for format flowed (RFC 2646). This is used when converting
    // to text for mail sending. This differs just slightly
    // but in an important way from normal formatted, and that is that
    // lines are space stuffed. This can't (correctly) be done later.
    OutputFormatFlowed  = 64,

    // Convert links, image src, and script src to absolute URLs when possible
    OutputAbsoluteLinks = 128,

    // Encode entities when outputting to a string.
    // E.g. If set, we'll output &nbsp; if clear, we'll output 0xa0.
    OutputEncodeEntities = 256,

    // LineBreak processing: we can do either platform line breaks,
    // CR, LF, or CRLF.  If neither of these flags is set, then we
    // will use platform line breaks.
    OutputCRLineBreak = 512,
    OutputLFLineBreak = 1024,

    // Output the content of noscript elements (only for serializing
    // to plaintext).
    OutputNoScriptContent = 2048
  };

  /**
   *  Initialize with a pointer to the document and the mime type.
   */
  NS_IMETHOD Init(nsIDocument* aDocument, const nsAReadableString& aMimeType,
                  PRUint32 flags) = 0;

  /**
   *  If the selection is set to a non-null value, then the
   *  selection is used for encoding, otherwise the entire
   *  document is encoded.
   */
  NS_IMETHOD SetSelection(nsISelection* aSelection) = 0;

  /**
   *  If the range is set to a non-null value, then the
   *  range is used for encoding, otherwise the entire
   *  document or selection is encoded.
   */
  NS_IMETHOD SetRange(nsIDOMRange* aRange) = 0;

  /**
   *  Documents typically have an intrinsic character set.
   *  If no intrinsic value is found, the platform character set
   *  is used.
   *  aCharset overrides the both the intrinsic or platform
   *  character set when encoding the document.
   *
   *  Possible result codes: NS_ERROR_NO_CHARSET_CONVERTER
   */
  NS_IMETHOD SetCharset(const nsAReadableString& aCharset) = 0;

  /**
   *  Set a wrap column.  This may have no effect in some types of encoders.
   */
  NS_IMETHOD SetWrapColumn(PRUint32 aWC) = 0;

  /**
   *  Get the mime type preferred by the encoder.  This piece of api was
   *  added because the copy encoder may need to switch mime types on you
   *  if you ask it to copy html that really represents plaintext content.
   *  Call this AFTER Init() and SetSelection() have both been called.
   */
  NS_IMETHOD GetMimeType(nsAWritableString& aMimeType) = 0;
  
  /**
   *  The document is encoded, the result is sent to the 
   *  to nsIOutputStream.
   * 
   *  Possible result codes are passing along whatever stream errors
   *  might have been encountered.
   */
  NS_IMETHOD EncodeToStream(nsIOutputStream* aStream) = 0;
  NS_IMETHOD EncodeToString(nsAWritableString& aOutputString) = 0;

  /**
   *  The document is encoded, the result is sent to the 
   *  to aEncodedString.  Parent heirarchy information is encoded
   *  to aContextString.  Extra context info is encoded in aInfoString.
   * 
   */
  NS_IMETHOD EncodeToStringWithContext(nsAWritableString& aEncodedString, 
                                       nsAWritableString& aContextString, 
                                       nsAWritableString& aInfoString) = 0;

  /**
   * Set the fixup object associated with node persistence.
   */
  NS_IMETHOD SetNodeFixup(nsIDocumentEncoderNodeFixup *aFixup) = 0;
};

#endif /* nsIDocumentEncoder_h__ */

