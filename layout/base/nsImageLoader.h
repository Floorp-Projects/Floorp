/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "imgIDecoderObserver.h"

class nsIPresContext;
class nsIFrame;
class nsIURI;

#include "imgIRequest.h"
#include "nsCOMPtr.h"

class nsImageLoader : public imgIDecoderObserver
{
public:
  nsImageLoader();
  virtual ~nsImageLoader();

  NS_DECL_ISUPPORTS
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER

  void Init(nsIFrame *aFrame, nsIPresContext *aPresContext);
  nsresult Load(imgIRequest *aImage);

  void Destroy();

  nsIFrame *GetFrame() { return mFrame; }
  imgIRequest *GetRequest() { return mRequest; }

private:
  void RedrawDirtyFrame(const nsRect* aDamageRect);

private:
  nsIFrame *mFrame;
  nsIPresContext *mPresContext;
  nsCOMPtr<imgIRequest> mRequest;
};
