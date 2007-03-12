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
import org.w3c.dom.views.AbstractView;
import org.w3c.dom.events.MouseEvent;
import org.w3c.dom.events.EventTarget;

/**
 * The <code>MouseEvent</code> interface provides specific contextual  
 * information associated with Mouse events.
 * <p>The <code>detail</code> attribute inherited from <code>UIEvent</code> 
 * indicates the number of times a mouse button has been pressed and released 
 * over the same screen location during a user action.  The attribute value 
 * is 1 when the user begins this action and increments by 1 for each full 
 * sequence of pressing and releasing. If the user moves the mouse between 
 * the mousedown and mouseup the value will be set to 0, indicating that no 
 * click is occurring.
 * @since DOM Level 2
 */
public class MouseEventImpl extends UIEventImpl implements MouseEvent {

    // instantiated from JNI only
    private MouseEventImpl() {}

    public String toString() {
	return "<c=org.mozilla.dom.events.mouseUIEvent type=" + getType() + 
            " screen=(" + getScreenX() + "," + getScreenY() + ")" +
            " client=(" + getClientX() + "," + getClientY() + ")" +
            " mods=(" + getCtrlKey() + "," + getMetaKey() + "," + getShiftKey() + "," + getAltKey() + ")" +
            " button=" + getButton() + 
	    " target=" + getTarget() +
            " phase=" + getEventPhase() +
            " p=" + Long.toHexString(p_nsIDOMEvent) + ">";
    }

    /**
     * <code>screenX</code> indicates the horizontal coordinate at which the 
     * event occurred in relative to the origin of the screen coordinate system.
     */
    public int getScreenX() {
	return nativeGetScreenX();
    }
    native int nativeGetScreenX();

    
    /**
     * <code>screenY</code> indicates the vertical coordinate at which the event 
     * occurred relative to the origin of the screen coordinate system.
     */
    public int getScreenY() {
	return nativeGetScreenY();
    }
    native int nativeGetScreenY();

    
    /**
     * <code>clientX</code> indicates the horizontal coordinate at which the 
     * event occurred relative to the DOM implementation's client area.
     */
    public int getClientX() {
	return nativeGetClientX();
    }
    native int nativeGetClientX();

    
    /**
     * <code>clientY</code> indicates the vertical coordinate at which the event 
     * occurred relative to the DOM implementation's client area.
     */
    public int getClientY() {
	return nativeGetClientY();
    }
    native int nativeGetClientY();

    
    /**
     * <code>ctrlKey</code> indicates whether the 'ctrl' key was depressed 
     * during the firing of the event.
     */
    public boolean getCtrlKey() {
	return nativeGetCtrlKey();
    }
    native boolean nativeGetCtrlKey();

    
    /**
     * <code>shiftKey</code> indicates whether the 'shift' key was depressed 
     * during the firing of the event.
     */
    public boolean getShiftKey() {
	return nativeGetShiftKey();
    }
    native boolean nativeGetShiftKey();

    
    /**
     * <code>altKey</code> indicates whether the 'alt' key was depressed during 
     * the firing of the event.  On some platforms this key may map to an 
     * alternative key name.
     */
    public boolean getAltKey() {
	return nativeGetAltKey();
    }
    native boolean nativeGetAltKey();

    
    /**
     * <code>metaKey</code> indicates whether the 'meta' key was depressed 
     * during the firing of the event.  On some platforms this key may map to 
     * an alternative key name.
     */
    public boolean getMetaKey() {
	return nativeGetMetaKey();
    }
    native boolean nativeGetMetaKey();

    
    /**
     * During mouse events caused by the depression or release of a mouse 
     * button, <code>button</code> is used to indicate which mouse button 
     * changed state.
     */
    public short getButton() {
	return nativeGetButton();
    }
    native short nativeGetButton();

    
    /**
     * <code>relatedNode</code> is used to identify a secondary node related to 
     * a UI event.
     */
    public EventTarget getRelatedTarget() {
	return nativeGetRelatedTarget();
    }
    native EventTarget nativeGetRelatedTarget();

    
    /**
     * 
     * @param typeArg Specifies the event type.
     * @param canBubbleArg Specifies whether or not the event can bubble.
     * @param cancelableArg Specifies whether or not the event's default  action 
     *   can be prevent.
     * @param viewArg Specifies the <code>Event</code>'s 
     *   <code>AbstractView</code>.
     * @param detailArg Specifies the <code>Event</code>'s mouse click count.
     * @param screenXArg Specifies the <code>Event</code>'s screen x coordinate
     * @param screenYArg Specifies the <code>Event</code>'s screen y coordinate
     * @param clientXArg Specifies the <code>Event</code>'s client x coordinate
     * @param clientYArg Specifies the <code>Event</code>'s client y coordinate
     * @param ctrlKeyArg Specifies whether or not control key was depressed 
     *   during the <code>Event</code>.
     * @param altKeyArg Specifies whether or not alt key was depressed during 
     *   the  <code>Event</code>.
     * @param shiftKeyArg Specifies whether or not shift key was depressed 
     *   during the <code>Event</code>.
     * @param metaKeyArg Specifies whether or not meta key was depressed during 
     *   the  <code>Event</code>.
     * @param buttonArg Specifies the <code>Event</code>'s mouse button.
     * @param relatedTargetArg Specifies the <code>Event</code>'s related Node.
     */
    public void initMouseEvent(String typeArg, 
			       boolean canBubbleArg, 
			       boolean cancelableArg, 
			       AbstractView viewArg, 
			       int detailArg, 
			       int screenXArg, 
			       int screenYArg, 
			       int clientXArg, 
			       int clientYArg, 
			       boolean ctrlKeyArg, 
			       boolean altKeyArg, 
			       boolean shiftKeyArg, 
			       boolean metaKeyArg, 
			       short buttonArg, 
			       EventTarget relatedTargetArg) {
	nativeInitMouseEvent(typeArg, canBubbleArg, cancelableArg, viewArg,
			     detailArg, screenXArg, screenYArg, clientXArg,
			     clientYArg, ctrlKeyArg, altKeyArg, shiftKeyArg,
			     metaKeyArg, buttonArg, relatedTargetArg);
    }
    native void nativeInitMouseEvent(String typeArg, 
				     boolean canBubbleArg, 
				     boolean cancelableArg, 
				     AbstractView viewArg, 
				     int detailArg, 
				     int screenXArg, 
				     int screenYArg, 
				     int clientXArg, 
				     int clientYArg, 
				     boolean ctrlKeyArg, 
				     boolean altKeyArg, 
				     boolean shiftKeyArg, 
				     boolean metaKeyArg, 
				     short buttonArg, 
				     EventTarget relatedTargetArg);
}

