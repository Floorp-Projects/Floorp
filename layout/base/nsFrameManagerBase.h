/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Brian Ryner <bryner@brianryner.com>
 *
 * This Original Code has been modified by IBM Corporation. Modifications made
 * by IBM described herein are Copyright (c) International Business Machines
 * Corporation, 2000. Modifications to Mozilla code or documentation identified
 * per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

#ifndef _nsFrameManagerBase_h_
#define _nsFrameManagerBase_h_

#include "pldhash.h"

class nsIPresShell;
class nsStyleSet;
class nsIContent;
class nsPlaceholderFrame;
class nsIFrame;
class nsStyleContext;
class nsIAtom;
class nsStyleChangeList;
class nsILayoutHistoryState;

struct CantRenderReplacedElementEvent;

class nsFrameManagerBase
{
public:
  struct PropertyList;

protected:
  class UndisplayedMap;

  // weak link, because the pres shell owns us
  nsIPresShell*                   mPresShell;
  // the pres shell owns the style set
  nsStyleSet*                     mStyleSet;
  nsIFrame*                       mRootFrame;
  PLDHashTable                    mPrimaryFrameMap;
  PLDHashTable                    mPlaceholderMap;
  UndisplayedMap*                 mUndisplayedMap;
  CantRenderReplacedElementEvent* mPostedEvents;
  PropertyList*                   mPropertyList;
  PRBool                          mIsDestroyingFrames;
};

#endif
