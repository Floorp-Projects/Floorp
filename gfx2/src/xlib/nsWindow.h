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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#ifndef nsWindow_h___
#define nsWindow_h___

#include "nsIWindow.h"
#include "nsPIWindowXlib.h"
#include "nsDrawable.h"

#include "nsIRegion.h"

#include "nsWeakReference.h"

#include "nsIGUIEventListener.h"

class nsWindow : public nsIWindow,
                 public nsPIWindowXlib,
                 public nsSupportsWeakReference,
                 public nsDrawable
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIWINDOW
  NS_DECL_NSPIWINDOWXLIB

  nsWindow();
  virtual ~nsWindow();

private:

protected:
  Window mWindow;
  nsWeakPtr mParent;

/* for exposes, invalidates, etc */
  nsCOMPtr<nsIRegion> mUpdateRegion;

/* state*/
  PRBool mMapped;

/* listeners */
  nsCOMPtr<nsIGUIEventListener> mEventListener;

};

#endif  // nsWindow_h___
