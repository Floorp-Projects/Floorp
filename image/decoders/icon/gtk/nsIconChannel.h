/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_decoders_icon_gtk_nsIconChannel_h
#define mozilla_image_decoders_icon_gtk_nsIconChannel_h

#include "mozilla/Attributes.h"

#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsIIconURI.h"
#include "nsCOMPtr.h"

namespace mozilla::ipc {
class ByteBuf;
}

/// This class is the GTK implementation of nsIconChannel.  It asks
/// GTK for the icon, translates the pixel data in-memory into
/// nsIconDecoder format, and proxies the nsChannel interface to a new
/// channel returning that image.
class nsIconChannel final : public nsIChannel {
 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIREQUEST(mRealChannel->)
  NS_FORWARD_NSICHANNEL(mRealChannel->)

  nsIconChannel() {}

  static void Shutdown();

  /// Called by nsIconProtocolHandler after it creates this channel.
  /// Must be called before calling any other function on this object.
  /// If this method fails, no other function must be called on this object.
  nsresult Init(nsIURI* aURI);

  /// Obtains an icon, in nsIconDecoder format, as a ByteBuf instead
  /// of a channel.  For use with IPC.
  static nsresult GetIcon(nsIURI* aURI, mozilla::ipc::ByteBuf* aDataOut);

 private:
  ~nsIconChannel() {}
  /// The input stream channel which will yield the image.
  /// Will always be non-null after a successful Init.
  nsCOMPtr<nsIChannel> mRealChannel;

  static nsresult GetIconWithGIO(nsIMozIconURI* aIconURI,
                                 mozilla::ipc::ByteBuf* aDataOut);
};

#endif  // mozilla_image_decoders_icon_gtk_nsIconChannel_h
