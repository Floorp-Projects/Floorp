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

#ifndef __EmbedEventListener_h
#define __EmbedEventListener_h

#include <nsIDOMKeyListener.h>
#include <nsIDOMMouseListener.h>

#include "jni_util.h"

class NativeBrowserControl;


class EmbedEventListener : public nsIDOMKeyListener,
                           public nsIDOMMouseListener
{
 public:

  EmbedEventListener();
  virtual ~EmbedEventListener();

  nsresult Init(NativeBrowserControl *aOwner);

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener

  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMKeyListener
  
  NS_IMETHOD KeyDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aDOMEvent);

  // nsIDOMMouseListener

  NS_IMETHOD MouseDown(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aDOMEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aDOMEvent);

    nsresult SetEventRegistration(jobject eventRegistration);

 private:

    nsresult PopulatePropertiesFromEvent(nsIDOMEvent *aMouseEvent);
    nsresult addMouseEventDataToProperties(nsIDOMEvent *aMouseEvent);
    static  nsresult JNICALL takeActionOnNode(nsCOMPtr<nsIDOMNode> curNode,
					      void *yourObject);


    NativeBrowserControl *mOwner;

    jobject mEventRegistration;

    /**
     * The properties table, created during a mouseEvent handler
     */
    
    jobject mProperties;

//
// The following ivars are used in the takeActionOnNode method.
//
    
/**
 * 0 is the leaf depth.  That's why we call it the inverse depth.
 */

    PRInt32 mInverseDepth;

//
// End of ivars used in takeActionOnNode
//


};

#endif /* __EmbedEventListener_h */
