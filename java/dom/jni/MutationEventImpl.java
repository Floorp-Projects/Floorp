/* 
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
*/

package org.mozilla.dom.events;

import org.w3c.dom.Node;
import org.w3c.dom.events.MutationEvent;
import org.mozilla.dom.events.EventImpl;

public class MutationEventImpl extends EventImpl implements MutationEvent  {
  /**
   *  <code>relatedNode</code> is used to identify a secondary node related to 
   * a mutation event. For example, if a mutation event is dispatched to a 
   * node indicating that its parent has changed, the <code>relatedNode</code>
   *  is the changed parent.  If an event is instead dispatch to a subtree 
   * indicating a node was changed within it, the <code>relatedNode</code> is 
   * the changed node. 
   */
  public Node               getRelatedNode() {
    throw new UnsupportedOperationException();
  }
  
  /**
   *  <code>prevValue</code> indicates the previous value of text nodes and 
   * attributes in attrModified and charDataModified events. 
   */
  public String             getPrevValue() {
    throw new UnsupportedOperationException();
  }
  
  /**
   *  <code>newValue</code> indicates the new value of text nodes and 
   * attributes in attrModified and charDataModified events. 
   */
  public String             getNewValue() {
    throw new UnsupportedOperationException();
  }
  
  /**
   *  <code>attrName</code> indicates the changed attr in the attrModified 
   * event. 
   */
  public String             getAttrName() {
    throw new UnsupportedOperationException();
  }
  
  /**
   * 
   * @param typeArg Specifies the event type.
   * @param canBubbleArg Specifies whether or not the event can bubble.
   * @param cancelableArg Specifies whether or not the event's default  action 
   *   can be prevent.
   * @param relatedNodeArg Specifies the <code>Event</code>'s related Node
   * @param prevValueArg Specifies the <code>Event</code>'s 
   *   <code>prevValue</code> property
   * @param newValueArg Specifies the <code>Event</code>'s 
   *   <code>newValue</code> property
   * @param attrNameArg Specifies the <code>Event</code>'s 
   *   <code>attrName</code> property
   */
  public void               initMutationEvent(String typeArg, 
                                              boolean canBubbleArg, 
                                              boolean cancelableArg, 
                                              Node relatedNodeArg, 
                                              String prevValueArg, 
                                              String newValueArg, 
                                              String attrNameArg) {
    throw new UnsupportedOperationException();
  }
}

