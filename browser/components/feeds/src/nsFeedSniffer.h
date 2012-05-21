/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIContentSniffer.h"
#include "nsIStreamListener.h"
#include "nsStringAPI.h"

class nsFeedSniffer : public nsIContentSniffer, nsIStreamListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTSNIFFER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER

  static NS_METHOD AppendSegmentToString(nsIInputStream* inputStream,
                                         void* closure,
                                         const char* rawSegment,
                                         PRUint32 toOffset,
                                         PRUint32 count,
                                         PRUint32* writeCount);

protected:
  nsresult ConvertEncodedData(nsIRequest* request, const PRUint8* data,
                              PRUint32 length);

private:
  nsCString mDecodedData;
};

