/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

package org.mozilla.dom.events;

import org.w3c.dom.events.KeyEvent;
import org.w3c.dom.views.AbstractView;
import org.mozilla.dom.events.UIEventImpl;

/**
 * The <code>KeyEvent</code> interface provides specific contextual  
 * information associated with Key events. 
 * @since DOM Level 2
 */
public class KeyEventImpl extends UIEventImpl implements KeyEvent {

    // instantiated from JNI only
    protected KeyEventImpl() {}

    public String toString() {
	return "<c=org.mozilla.dom.events.KeyUIEvent type=" + getType() + 
            " mods=(" + getCtrlKey() + "," + getMetaKey() + "," + getShiftKey() + "," + getAltKey() + ")" +
            " key=" + getKeyCode() + " char=" + getCharCode() +
	    " target=" + getTarget() +
            " phase=" + getEventPhase() +
            " p=" + Long.toHexString(p_nsIDOMEvent) + ">";
    }

  /**
   *  <code>ctrlKey</code> indicates whether the 'ctrl' key was depressed 
   * during the firing of the event. 
   */
  public native boolean            getCtrlKey();

  /**
   *  <code>shiftKey</code> indicates whether the 'shift' key was depressed 
   * during the firing of the event. 
   */
  public native boolean            getShiftKey();

  /**
   *  <code>altKey</code> indicates whether the 'alt' key was depressed during 
   * the firing of the event.  On some platforms this key may map to an 
   * alternative key name. 
   */
  public native boolean            getAltKey();

  /**
   *  <code>metaKey</code> indicates whether the 'meta' key was depressed 
   * during the firing of the event.  On some platforms this key may map to 
   * an alternative key name. 
   */
  public native boolean            getMetaKey();

  /**
   *  The value of <code>keyCode</code> holds the virtual key code value of 
   * the key which was depressed if the event is a key event.  Otherwise, the 
   * value is zero. 
   */
  public native int                getKeyCode();

  /**
   *  <code>charCode</code> holds the value of the Unicode character 
   * associated with the depressed key if the event is a key event.  
   * Otherwise, the value is zero. 
   */
  public native int                getCharCode();

  /**
   * 
   * @param typeArg Specifies the event type.
   * @param canBubbleArg Specifies whether or not the event can bubble.
   * @param cancelableArg Specifies whether or not the event's default  action 
   *   can be prevent.
   * @param ctrlKeyArg Specifies whether or not control key was depressed 
   *   during the  <code>Event</code>.
   * @param altKeyArg Specifies whether or not alt key was depressed during 
   *   the  <code>Event</code>.
   * @param shiftKeyArg Specifies whether or not shift key was depressed 
   *   during the  <code>Event</code>.
   * @param metaKeyArg Specifies whether or not meta key was depressed during 
   *   the  <code>Event</code>.
   * @param keyCodeArg Specifies the <code>Event</code>'s <code>keyCode</code>
   * @param charCodeArg Specifies the <code>Event</code>'s 
   *   <code>charCode</code>
   * @param viewArg Specifies the <code>Event</code>'s 
   *   <code>AbstractView</code>.
   */

  public void               initKeyEvent(String typeArg, 
                                         boolean canBubbleArg, 
                                         boolean cancelableArg, 
                                         boolean ctrlKeyArg, 
                                         boolean altKeyArg, 
                                         boolean shiftKeyArg, 
                                         boolean metaKeyArg, 
                                         int keyCodeArg, 
                                         int charCodeArg, 
                                         AbstractView viewArg) {
       throw new UnsupportedOperationException();
  }
}

