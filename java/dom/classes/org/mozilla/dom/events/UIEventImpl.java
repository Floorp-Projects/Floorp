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

import org.w3c.dom.events.UIEvent;
import org.w3c.dom.views.AbstractView;
import org.mozilla.dom.events.EventImpl;

/**
 * The <code>UIEvent</code> interface provides specific contextual  
 * information associated with User Interface events.
 * @since DOM Level 2
 */
public class UIEventImpl extends EventImpl implements UIEvent {

    // instantiated from JNI only
    protected UIEventImpl() {}

    public String toString() {
        return "<c=org.mozilla.dom.events.UIEvent type=" + getType() + 
	        " target=" + getTarget() +
            " phase=" + getEventPhase() +
            " p=" + Long.toHexString(p_nsIDOMEvent) + ">";
    }


  /**
   * The <code>view</code> attribute identifies the <code>AbstractView</code> 
   * from which the event was generated.
   */
    public AbstractView getView() {
	return nativeGetView();
    }
  public native AbstractView nativeGetView();


  /**
   * Specifies some detail information about the <code>Event</code>, depending 
   * on the type of event.
   */
    public int getDetail() {
	return nativeGetDetail();
    }
  public native int nativeGetDetail();


  /**
   * 
   * @param typeArg Specifies the event type.
   * @param canBubbleArg Specifies whether or not the event can bubble.
   * @param cancelableArg Specifies whether or not the event's default  action 
   *   can be prevent.
   * @param viewArg Specifies the <code>Event</code>'s 
   *   <code>AbstractView</code>.
   * @param detailArg Specifies the <code>Event</code>'s detail.
   */
  public void initUIEvent(String typeArg, 
			  boolean canBubbleArg, 
			  boolean cancelableArg, 
			  AbstractView viewArg, 
			  int detailArg) {
      nativeInitUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 
			detailArg);
  }
  native void nativeInitUIEvent(String typeArg, 
			  boolean canBubbleArg, 
			  boolean cancelableArg, 
			  AbstractView viewArg, 
			  int detailArg);
    
}

