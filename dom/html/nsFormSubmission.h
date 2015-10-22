/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIFormSubmission_h___
#define nsIFormSubmission_h___

#include "mozilla/Attributes.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsNCRFallbackEncoderWrapper.h"

class nsIURI;
class nsIInputStream;
class nsGenericHTMLElement;
class nsIMultiplexInputStream;

namespace mozilla {
namespace dom {
class File;
} // namespace dom
} // namespace mozilla

/**
 * Class for form submissions; encompasses the function to call to submit as
 * well as the form submission name/value pairs
 */
class nsFormSubmission
{
public:
  virtual ~nsFormSubmission()
  {
    MOZ_COUNT_DTOR(nsFormSubmission);
  }

  /**
   * Submit a name/value pair
   *
   * @param aName the name of the parameter
   * @param aValue the value of the parameter
   */
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) = 0;

  /**
   * Submit a name/file pair
   *
   * @param aName the name of the parameter
   * @param aFile the file to submit. The file's name will be used
   */
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   mozilla::dom::File* aFile) = 0;

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
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream) = 0;

  /**
   * Get the charset that will be used for submission.
   */
  void GetCharset(nsACString& aCharset)
  {
    aCharset = mCharset;
  }

  nsIContent* GetOriginatingElement() const
  {
    return mOriginatingElement.get();
  }

protected:
  /**
   * Can only be constructed by subclasses.
   *
   * @param aCharset the charset of the form as a string
   * @param aOriginatingElement the originating element (can be null)
   */
  nsFormSubmission(const nsACString& aCharset, nsIContent* aOriginatingElement)
    : mCharset(aCharset)
    , mOriginatingElement(aOriginatingElement)
  {
    MOZ_COUNT_CTOR(nsFormSubmission);
  }

  // The name of the encoder charset
  nsCString mCharset;

  // Originating element.
  nsCOMPtr<nsIContent> mOriginatingElement;
};

class nsEncodingFormSubmission : public nsFormSubmission
{
public:
  nsEncodingFormSubmission(const nsACString& aCharset,
                           nsIContent* aOriginatingElement);

  virtual ~nsEncodingFormSubmission();

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

private:
  // The encoder that will encode Unicode names and values
  nsNCRFallbackEncoderWrapper mEncoder;
};

/**
 * Handle multipart/form-data encoding, which does files as well as normal
 * inputs.  This always does POST.
 */
class nsFSMultipartFormData : public nsEncodingFormSubmission
{
public:
  /**
   * @param aCharset the charset of the form as a string
   */
  nsFSMultipartFormData(const nsACString& aCharset,
                        nsIContent* aOriginatingElement);
  ~nsFSMultipartFormData();
 
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override;
  virtual nsresult AddNameFilePair(const nsAString& aName,
                                   mozilla::dom::File* aFile) override;
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream) override;

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
  /**
   * The post data stream as it is so far.  This is a collection of smaller
   * chunks--string streams and file streams interleaved to make one big POST
   * stream.
   */
  nsCOMPtr<nsIMultiplexInputStream> mPostDataStream;

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

/**
 * Get a submission object based on attributes in the form (ENCTYPE and METHOD)
 *
 * @param aForm the form to get a submission object based on
 * @param aOriginatingElement the originating element (can be null)
 * @param aFormSubmission the form submission object (out param)
 */
nsresult GetSubmissionFromForm(nsGenericHTMLElement* aForm,
                               nsGenericHTMLElement* aOriginatingElement,
                               nsFormSubmission** aFormSubmission);

#endif /* nsIFormSubmission_h___ */
