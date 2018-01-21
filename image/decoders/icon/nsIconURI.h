/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_icon_nsIconURI_h
#define mozilla_image_decoders_icon_nsIconURI_h

#include "nsIIconURI.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIIPCSerializableURI.h"
#include "nsINestedURI.h"
#include "nsIURIMutator.h"


class nsMozIconURI final
  : public nsIMozIconURI
  , public nsIIPCSerializableURI
  , public nsINestedURI
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURI
  NS_DECL_NSIMOZICONURI
  NS_DECL_NSIIPCSERIALIZABLEURI
  NS_DECL_NSINESTEDURI

  // nsMozIconURI
  nsMozIconURI();

protected:
  virtual ~nsMozIconURI();
  nsCOMPtr<nsIURL> mIconURL; // a URL that we want the icon for
  uint32_t mSize; // the # of pixels in a row that we want for this image.
                  // Typically 16, 32, 128, etc.
  nsCString mContentType; // optional field explicitly specifying the content
                          // type
  nsCString mFileName; // for if we don't have an actual file path, we're just
                       // given a filename with an extension
  nsCString mStockIcon;
  int32_t mIconSize;   // -1 if not specified, otherwise index into
                       // kSizeStrings
  int32_t mIconState;  // -1 if not specified, otherwise index into
                       // kStateStrings

public:
  class Mutator
      : public nsIURIMutator
      , public BaseURIMutator<nsMozIconURI>
  {
    NS_DECL_ISUPPORTS
    NS_FORWARD_SAFE_NSIURISETTERS_RET(mURI)

    NS_IMETHOD Deserialize(const mozilla::ipc::URIParams& aParams) override
    {
      return InitFromIPCParams(aParams);
    }

    NS_IMETHOD Read(nsIObjectInputStream* aStream) override
    {
      return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Finalize(nsIURI** aURI) override
    {
      mURI.forget(aURI);
      return NS_OK;
    }

    NS_IMETHOD SetSpec(const nsACString & aSpec, nsIURIMutator** aMutator) override
    {
      if (aMutator) {
        nsCOMPtr<nsIURIMutator> mutator = this;
        mutator.forget(aMutator);
      }
      return InitFromSpec(aSpec);
    }

    explicit Mutator() { }
  private:
    virtual ~Mutator() { }

    friend class nsMozIconURI;
  };
};

#endif // mozilla_image_decoders_icon_nsIconURI_h
