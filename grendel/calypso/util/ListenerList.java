/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil -*-
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
 * The Original Code is the Grendel mail/news client.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1997 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

package calypso.util;

import java.util.EventListener;
import java.util.Vector;
import java.util.Enumeration;


/**
 * ListenersList is a thread-safe, reentrant class which
 * contains a list of EventListeners and enforce some
 * policy on event dispatching. <p>

 * This class guarantees that events delivering is correctly
 * accomplished even if listeners are removed from or added to the list during
 * event dispatching. Added listeners, though, will not receive the current event. <p>

 * Event listeners are returned <b>LIFO</b>. <p>

 * User of this class must enclose event dispatching between
 * startDelivering()/stopDelivering() calls and pass the state object
 * to the nextListener call. <p>
 * <pre>
 *    ListenerListState state = eventListeners.startDelivering();
 *
 *    SomeEventListener el = (SomeEventListener)eventListeners.nextListener(state);
 *    while (null != el) {
 *      el.someEvent();
 *      el = (SomeEventListener)eventListeners.nextListener(state);
 *    }
 *
 *    eventListeners.stopDelivering(state);
 * </pre>
 */
public class ListenerList
{
  private Vector fListeners; // list of listeners
  private Vector fEnumerators; // list of enumerators for this list of listeners

  /**
   * Construct an array of listeners with the specifed size. <p>
   * The array size is doubled every time an element is added
   * to a full array.
   *
   * @param aSize the required size
   */
  public ListenerList(int aSize)
  {
    fListeners = new Vector(aSize);
  }

  /**
   * Construct an array of listeners with the specifed size. <p>
   * The array size is doubled every time an element is added
   * to a full array.
   *
   */
  public ListenerList()
  {
    fListeners = new Vector(1);
  }

  /**
   * Set a new event listener for the class of events
   * this ListenersList is mantaining.
   *
   * @param aListener a listener for the specific set of events
   */
  public void addListener(EventListener aListener)
  {
    fListeners.addElement(aListener);
  }

  /**
   * Remove a listener from the list of listeners this
   * ListenersList is mantaining.<p>
   * Existing and valid state object are updated to reflect
   * this change.
   *
   * @param aListener a listener for the specific set of events
   */
  public void removeListener(EventListener aListener)
  {
    synchronized(fListeners) {
      // find and remove the element
      int index = fListeners.indexOf(aListener);
      if (index != -1) {
        try {
          fListeners.removeElementAt(index);
        } catch (ArrayIndexOutOfBoundsException e) {
          Assert.Assertion(false); // no way!!
        }

        // go through the list of live state objects and update the
        // index if necessary
        if (fEnumerators != null) {
          for (int i =0; i < fEnumerators.size(); i++) {
            ListenerListState state = (ListenerListState)fEnumerators.elementAt(i);
            if (index < state.fIndex) {
              state.fIndex--;
            }
          }
        }

      }
    }
  }

  /**
   * A user of this class is starting event delivery. <p>
   * The state object represents the state of the list for
   * that user and has to be passed in every nextListener call.
   *
   * @param aListener a listener for the specific set of events
   * @return ListenerListState the state of the list for the caller
   */
  public ListenerListState startDelivering()
  {
    synchronized(fListeners) {
      ListenerListState state = new ListenerListState(fListeners);
      if (null == fEnumerators) {
        fEnumerators = new Vector(1);
      }
      fEnumerators.addElement(state);
      return state;
    }
  }

  /**
   * A user completed or aborted event delivering. <p>
   *
   * @return aState the state of the list for the caller
   */
  public void stopDelivering(ListenerListState aState)
  {
    synchronized(fListeners) {
      fEnumerators.removeElement(aState);
    }
  }

  /**
   * Return the next EventListener in the array. <p>
   * Listeners are returned with a last-in/first-out (LIFO) policy.
   *
   * @return aState the state of the list for the caller
   */
  public EventListener nextListener(ListenerListState aState)
  {
    synchronized(fListeners) {
      EventListener listener = null;

      if (--aState.fIndex >= 0) {
        try {
          listener = (EventListener)fListeners.elementAt(aState.fIndex);
        } catch (ArrayIndexOutOfBoundsException e) {
          Assert.Assertion(false);
          aState.fIndex = -1; // invalid state
        }
      }

      return listener;
    }
  }

}

