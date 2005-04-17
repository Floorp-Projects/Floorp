/*
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
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#ifndef __LoadCompleteProgressListener_h
#define __LoadCompleteProgressListener_h

#include <nsIWebProgressListener.h>
#include <nsWeakReference.h>

#include "jni_util.h"

class NativeBrowserControl;

/*
 * Simple nsIWebProgressListener that offers a "loadComplete" property
 * that can be queried to determine if the load has completed.
 *
 */

class LoadCompleteProgressListener : public nsIWebProgressListener,
                      public nsSupportsWeakReference
{
public:
    LoadCompleteProgressListener();
    virtual ~LoadCompleteProgressListener();
    
    nsresult Init(NativeBrowserControl *aOwner);
    
    nsresult IsLoadComplete(PRBool *_retval);
    
    NS_DECL_ISUPPORTS
    
    NS_DECL_NSIWEBPROGRESSLISTENER
    
private:
    
    NativeBrowserControl *mOwner;
    
    PRBool mLoadComplete;
    
};

#endif /* __LoadCompleteProgressListener_h */
