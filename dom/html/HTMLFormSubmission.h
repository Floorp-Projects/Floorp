/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFormSubmission_h
#define mozilla_dom_HTMLFormSubmission_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/HTMLDialogElement.h"
#include "nsCOMPtr.h"
#include "mozilla/Encoding.h"
#include "nsString.h"

class nsIURI;
class nsIInputStream;
class nsGenericHTMLElement;
class nsIMultiplexInputStream;

namespace mozilla::dom {

class Blob;
class DialogFormSubmission;
class Directory;
class Element;
class HTMLFormElement;

/**
 * Class for form submissions; encompasses the function to call to submit as
 * well as the form submission name/value pairs
 */
class HTMLFormSubmission {
 public:
  /**
   * Get a submission object based on attributes in the form (ENCTYPE and
   * METHOD)
   *
   * @param aForm the form to get a submission object based on
   * @param aSubmitter the submitter element (can be null)
   * @param aEncoding the submiter element's encoding
   * @param aFormSubmission the form submission object (out param)
   */
  static nsresult GetFromForm(HTMLFormElement* aForm,
                              nsGenericHTMLElement* aSubmitter,
                              NotNull<const Encoding*>& aEncoding,
                              HTMLFormSubmission** aFormSubmission);

  MOZ_COUNTED_DTOR_VIRTUAL(HTMLFormSubmission)

  /**
   * Submit a name/value pair
   *
   * @param aName the name of the parameter
   * @param aValue the value of the parameter
   */
  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) = 0;

  /**
   * Submit a name/blob pair
   *
   * @param aName the name of the parameter
   * @param aBlob the blob to submit. The file's name will be used if the Blob
   * is actually a File, otherwise 'blob' string is used instead. Must not be
   * null.
   */
  virtual nsresult AddNameBlobPair(const nsAString& aName, Blob* aBlob) = 0;

  /**
   * Submit a name/directory pair
   *
   * @param aName the name of the parameter
   * @param aBlob the directory to submit.
   */
  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) = 0;

  /**
   * Given a URI and the current submission, create the final URI and data
   * stream that will be submitted.  Subclasses *must* implement this.
   *
   * @param aURI the URI being submitted to [IN]
   * @param aPostDataStream a data stream for POST data [OUT]
   * @param aOutURI the resulting URI. May be the same as aURI [OUT]
   */
  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) = 0;

  /**
   * Get the charset that will be used for submission.
   */
  void GetCharset(nsACString& aCharset) { mEncoding->Name(aCharset); }

  /**
   * Get the action URI that will be used for submission.
   */
  nsIURI* GetActionURL() const { return mActionURL; }

  /**
   * Get the target that will be used for submission.
   */
  void GetTarget(nsAString& aTarget) { aTarget = mTarget; }

  /**
   * Return true if this form submission was user-initiated.
   */
  bool IsInitiatedFromUserInput() const { return mInitiatedFromUserInput; }

  virtual DialogFormSubmission* GetAsDialogSubmission() { return nullptr; }

 protected:
  /**
   * Can only be constructed by subclasses.
   *
   * @param aEncoding the character encoding of the form
   */
  HTMLFormSubmission(nsIURI* aActionURL, const nsAString& aTarget,
                     mozilla::NotNull<const mozilla::Encoding*> aEncoding);

  // The action url.
  nsCOMPtr<nsIURI> mActionURL;

  // The target.
  nsString mTarget;

  // The character encoding of this form submission
  mozilla::NotNull<const mozilla::Encoding*> mEncoding;

  // Keep track of whether this form submission was user-initiated or not
  bool mInitiatedFromUserInput;
};

class EncodingFormSubmission : public HTMLFormSubmission {
 public:
  EncodingFormSubmission(nsIURI* aActionURL, const nsAString& aTarget,
                         mozilla::NotNull<const mozilla::Encoding*> aEncoding,
                         Element* aSubmitter);

  virtual ~EncodingFormSubmission();

  // Indicates the type of newline normalization and escaping to perform in
  // `EncodeVal`, in addition to encoding the string into bytes.
  enum EncodeType {
    // Normalizes newlines to CRLF and then escapes for use in
    // `Content-Disposition`. (Useful for `multipart/form-data` entry names.)
    eNameEncode,
    // Escapes for use in `Content-Disposition`. (Useful for
    // `multipart/form-data` filenames.)
    eFilenameEncode,
    // Normalizes newlines to CRLF.
    eValueEncode,
  };

  /**
   * Encode a Unicode string to bytes, additionally performing escapes or
   * normalizations.
   * @param aStr the string to encode
   * @param aOut the encoded string [OUT]
   * @param aEncodeType The type of escapes or normalizations to perform on the
   *                    encoded string.
   * @throws an error if UnicodeToNewBytes fails
   */
  nsresult EncodeVal(const nsAString& aStr, nsCString& aOut,
                     EncodeType aEncodeType);
};

class DialogFormSubmission final : public HTMLFormSubmission {
 public:
  DialogFormSubmission(nsAString& aResult, NotNull<const Encoding*> aEncoding,
                       HTMLDialogElement* aDialogElement)
      : HTMLFormSubmission(nullptr, u""_ns, aEncoding),
        mDialogElement(aDialogElement),
        mReturnValue(aResult) {}
  nsresult AddNameValuePair(const nsAString& aName,
                            const nsAString& aValue) override {
    MOZ_CRASH("This method should not be called");
    return NS_OK;
  }

  nsresult AddNameBlobPair(const nsAString& aName, Blob* aBlob) override {
    MOZ_CRASH("This method should not be called");
    return NS_OK;
  }

  nsresult AddNameDirectoryPair(const nsAString& aName,
                                Directory* aDirectory) override {
    MOZ_CRASH("This method should not be called");
    return NS_OK;
  }

  nsresult GetEncodedSubmission(nsIURI* aURI, nsIInputStream** aPostDataStream,
                                nsCOMPtr<nsIURI>& aOutURI) override {
    MOZ_CRASH("This method should not be called");
    return NS_OK;
  }

  DialogFormSubmission* GetAsDialogSubmission() override { return this; }

  HTMLDialogElement* DialogElement() { return mDialogElement; }

  nsString& ReturnValue() { return mReturnValue; }

 private:
  const RefPtr<HTMLDialogElement> mDialogElement;
  nsString mReturnValue;
};

/**
 * Handle multipart/form-data encoding, which does files as well as normal
 * inputs.  This always does POST.
 */
class FSMultipartFormData : public EncodingFormSubmission {
 public:
  /**
   * @param aEncoding the character encoding of the form
   */
  FSMultipartFormData(nsIURI* aActionURL, const nsAString& aTarget,
                      mozilla::NotNull<const mozilla::Encoding*> aEncoding,
                      Element* aSubmitter);
  ~FSMultipartFormData();

  virtual nsresult AddNameValuePair(const nsAString& aName,
                                    const nsAString& aValue) override;

  virtual nsresult AddNameBlobPair(const nsAString& aName,
                                   Blob* aBlob) override;

  virtual nsresult AddNameDirectoryPair(const nsAString& aName,
                                        Directory* aDirectory) override;

  virtual nsresult GetEncodedSubmission(nsIURI* aURI,
                                        nsIInputStream** aPostDataStream,
                                        nsCOMPtr<nsIURI>& aOutURI) override;

  void GetContentType(nsACString& aContentType) {
    aContentType = "multipart/form-data; boundary="_ns + mBoundary;
  }

  nsIInputStream* GetSubmissionBody(uint64_t* aContentLength);

 protected:
  /**
   * Roll up the data we have so far and add it to the multiplexed data stream.
   */
  nsresult AddPostDataStream();

 private:
  void AddDataChunk(const nsACString& aName, const nsACString& aFilename,
                    const nsACString& aContentType,
                    nsIInputStream* aInputStream, uint64_t aInputStreamSize);
  /**
   * The post data stream as it is so far.  This is a collection of smaller
   * chunks--string streams and file streams interleaved to make one big POST
   * stream.
   */
  nsCOMPtr<nsIMultiplexInputStream> mPostData;

  /**
   * The same stream, but as an nsIInputStream.
   * Raw pointers because it is just QI of mInputStream.
   */
  nsIInputStream* mPostDataStream;

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

}  // namespace mozilla::dom

#endif /* mozilla_dom_HTMLFormSubmission_h */
