/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFormSubmission_h
#define mozilla_dom_HTMLFormSubmission_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "mozilla/Encoding.h"
#include "nsString.h"

class nsIURI;
class nsIInputStream;
class nsGenericHTMLElement;
class nsIMultiplexInputStream;

namespace mozilla {
namespace dom {

class Blob;
class Directory;

/**
 * Class for form submissions; encompasses the function to call to submit as
 * well as the form submission name/value pairs
 */
class HTMLFormSubmission
{
public:
  /**
   * Get a submission object based on attributes in the form (ENCTYPE and
   * METHOD)
   *
   * @param aForm the form to get a submission object based on
   * @param aOriginatingElement the originating element (can be null)
   * @param aFormSubmission the form submission object (out param)
   */
  static nsresult
  GetFromForm(nsGenericHTMLElement* aForm,
              nsGenericHTMLElement* aOriginatingElement,
              HTMLFormSubmission** aFormSubmission);

  virtual ~HTMLFormSubmission()
  {
    MOZ_COUNT_DTOR(HTMLFormSubmission);
  }

  /**
   * Submit a name/value pair
   *
   * @param aName the name of the parameter
   * @param aValue the value of the parameter
   */
  virtual nsresult
  AddNameValuePair(const nsAString& aName, const nsAString& aValue) = 0;

  /**
   * Submit a name/blob pair
   *
   * @param aName the name of the parameter
   * @param aBlob the blob to submit. The file's name will be used if the Blob
   * is actually a File, otherwise 'blob' string is used instead if the aBlob is
   * not null.
   */
  virtual nsresult
  AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob) = 0;

  /**
   * Submit a name/directory pair
   *
   * @param aName the name of the parameter
   * @param aBlob the directory to submit.
   */
  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) = 0;

  /**
   * Reports whether the instance supports AddIsindex().
   *
   * @return true if supported.
   */
  virtual bool SupportsIsindexSubmission()
  {
    return false;
  }

  /**
   * Adds an isindex value to the submission.
   *
   * @param aValue the field value
   */
  virtual nsresult AddIsindex(const nsAString& aValue)
  {
    NS_NOTREACHED("AddIsindex called when not supported");
    return NS_ERROR_UNEXPECTED;
  }

  /**
   * Given a URI and the current submission, create the final URI and data
   * stream that will be submitted.  Subclasses *must* implement this.
   *
   * @param aURI the URI being submitted to [INOUT]
   * @param aPostDataStream a data stream for POST data [OUT]
   */
  virtual nsresult
  GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream) = 0;

  /**
   * Get the charset that will be used for submission.
   */
  void GetCharset(nsACString& aCharset) { mEncoding->Name(aCharset); }

  nsIContent* GetOriginatingElement() const
  {
    return mOriginatingElement.get();
  }

protected:
  /**
   * Can only be constructed by subclasses.
   *
   * @param aEncoding the character encoding of the form
   * @param aOriginatingElement the originating element (can be null)
   */
  HTMLFormSubmission(mozilla::NotNull<const mozilla::Encoding*> aEncoding,
                     nsIContent* aOriginatingElement)
    : mEncoding(aEncoding)
    , mOriginatingElement(aOriginatingElement)
  {
    MOZ_COUNT_CTOR(HTMLFormSubmission);
  }

  // The character encoding of this form submission
  mozilla::NotNull<const mozilla::Encoding*> mEncoding;

  // Originating element.
  nsCOMPtr<nsIContent> mOriginatingElement;
};

class EncodingFormSubmission : public HTMLFormSubmission
{
public:
  EncodingFormSubmission(mozilla::NotNull<const mozilla::Encoding*> aEncoding,
                         nsIContent* aOriginatingElement);

  virtual ~EncodingFormSubmission();

  /**
   * Encode a Unicode string to bytes using the encoder (or just copy the input
   * if there is no encoder).
   * @param aStr the string to encode
   * @param aResult the encoded string [OUT]
   * @param aHeaderEncode If true, turns all linebreaks into spaces and escapes
   *                      all quotes
   * @throws an error if UnicodeToNewBytes fails
   */
  nsresult EncodeVal(const nsAString& aStr, nsCString& aResult,
                     bool aHeaderEncode);
};

/**
 * Handle multipart/form-data encoding, which does files as well as normal
 * inputs.  This always does POST.
 */
class FSMultipartFormData : public EncodingFormSubmission
{
public:
  /**
   * @param aEncoding the character encoding of the form
   */
  FSMultipartFormData(mozilla::NotNull<const mozilla::Encoding*> aEncoding,
                      nsIContent* aOriginatingElement);
  ~FSMultipartFormData();
 
  virtual nsresult
  AddNameValuePair(const nsAString& aName, const nsAString& aValue) override;

  virtual nsresult
  AddNameBlobOrNullPair(const nsAString& aName, Blob* aBlob) override;

  virtual nsresult
  AddNameDirectoryPair(const nsAString& aName, Directory* aDirectory) override;

  virtual nsresult
  GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream) override;

  void GetContentType(nsACString& aContentType)
  {
    aContentType =
      NS_LITERAL_CSTRING("multipart/form-data; boundary=") + mBoundary;
  }

  nsIInputStream* GetSubmissionBody(uint64_t* aContentLength);

protected:

  /**
   * Roll up the data we have so far and add it to the multiplexed data stream.
   */
  nsresult AddPostDataStream();

private:
  void AddDataChunk(const nsACString& aName,
                    const nsACString& aFilename,
                    const nsACString& aContentType,
                    nsIInputStream* aInputStream,
                    uint64_t aInputStreamSize);
  /**
   * The post data stream as it is so far.  This is a collection of smaller
   * chunks--string streams and file streams interleaved to make one big POST
   * stream.
   */
  nsCOMPtr<nsIMultiplexInputStream> mPostData;

  /**
   * The same stream, but as an nsIInputStream.
   */
  nsCOMPtr<nsIInputStream> mPostDataStream;

  /**
   * The current string chunk.  When a file is hit, the string chunk gets
   * wrapped up into an input stream and put into mPostDataStream so that the
   * file input stream can then be appended and everything is in the right
   * order.  Then the string chunk gets appended to again as we process more
   * name/value pairs.
   */
  nsCString mPostDataChunk;

  /**
   * The boundary string to use after each "part" (the boundary that marks the
   * end of a value).  This is computed randomly and is different for each
   * submission.
   */
  nsCString mBoundary;

  /**
   * The total length in bytes of the streams that make up mPostDataStream
   */
  uint64_t mTotalLength;
};

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_HTMLFormSubmission_h */
