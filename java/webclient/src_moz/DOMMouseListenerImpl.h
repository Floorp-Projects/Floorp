/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 *
 */

#ifndef DOMMouseListenerImpl_h
#define DOMMouseListenerImpl_h

#include "nsISupports.h"
#include "nsISupportsUtils.h"
#include "nsString.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMNode.h"

#include "jni_util.h"

class nsIURI;

/**

 * This class is the shim between the mozilla listener event system for
 * mouse events and the java MouseListener interface.
 * For each of the Mouse* methods, we call the appropriate method in java.
 * See the implementation of MouseOver for an example.

 * For each mouseEvent, we create a Properties object containing
 * information about the event.  We use methods in dom_util to do this.

 */

class DOMMouseListenerImpl : public nsIDOMMouseListener {
  NS_DECL_ISUPPORTS
public: 

typedef enum {
  MOUSE_DOWN_EVENT_MASK = 0,
  MOUSE_UP_EVENT_MASK,
  MOUSE_CLICK_EVENT_MASK,
  MOUSE_DOUBLE_CLICK_EVENT_MASK,
  MOUSE_OVER_EVENT_MASK,
  MOUSE_OUT_EVENT_MASK,
  NUMBER_OF_MASK_NAMES
} EVENT_MASK_NAMES;


#ifdef XP_UNIX
static jlong maskValues [NUMBER_OF_MASK_NAMES];
#else
static jlong maskValues [DOMMouseListenerImpl::EVENT_MASK_NAMES::NUMBER_OF_MASK_NAMES];
#endif

static char *maskNames [];

    DOMMouseListenerImpl(JNIEnv *yourJNIEnv, 
                         WebShellInitContext *yourInitContext,
                         jobject yourTarget);
    
    DOMMouseListenerImpl();

    /* nsIDOMEventListener methods */
    nsresult HandleEvent(nsIDOMEvent* aEvent);


    /* nsIDOMMouseListener methods */

  nsresult MouseDown(nsIDOMEvent *aMouseEvent);

  nsresult MouseUp(nsIDOMEvent *aMouseEvent);

  nsresult MouseClick(nsIDOMEvent *aMouseEvent);

  nsresult MouseDblClick(nsIDOMEvent *aMouseEvent);

  nsresult MouseOver(nsIDOMEvent *aMouseEvent);

  nsresult MouseOut(nsIDOMEvent *aMouseEvent);

//
// Local methods
//

jobject JNICALL getPropertiesFromEvent(nsIDOMEvent *aMouseEvent);

static  nsresult JNICALL takeActionOnNode(nsCOMPtr<nsIDOMNode> curNode, 
                                          void *yourObject);

protected:

    JNIEnv *mJNIEnv;
    WebShellInitContext *mInitContext;
    jobject mTarget;

//
// The following arguments are used in the takeActionOnNode method.
//
    
/**

 * 0 is the leaf depth.  That's why we call it the inverse depth.

 */

    PRInt32 inverseDepth;

/**

 * The properties table, created during a mouseEvent handler

 */

    jobject properties;

/**

 * the nsIDOMEvent in the current event

 */

    nsCOMPtr<nsIDOMEvent> currentDOMEvent;

//
// End of ivars used in takeActionOnNode
//
  
};

#endif // DOMMouseListenerImpl_h
