/* -*- Mode: java; tab-width: 4; indent-tabs-mode: 1; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Helper class to add/remove listeners from Object array */

package org.mozilla.javascript;

/**
 * Utility class to manage listeners array.
 * A possible usage would be:
<pre>
    private Object[] listeners;
    ...
    void addListener(ListenerType listener) {
        synchronized (this) {
            listeners = ListenerArray.add(listeners, listener);
        }
    }

    void removeListener(ListenerType listener) {
        synchronized (this) {
            listeners = ListenerArray.remove(listeners, listener);
        }
    }
</pre>
  * Here is a thread safe while synchronization free example of event firing
<pre>
    void fireEvent(EventType event) {
        Object[] array = listeners;
        if (array != null) {
            for (int i = array.length; i-- != 0;) {
                ((ListenerType)array[i]).onEvent(event);
            }
        }

    }
</pre>

 * or if listeners of different types can present in listeners array:
<pre>
    void fireEvent(EventType event) {
        Object[] array = listeners;
        if (array != null) {
            for (int i = array.length; i-- != 0;) {
                Object obj = array[i];
                if (obj instanceof ListenerType) {
                    ((ListenerType)obj).onEvent(event);
                }
            }
        }

    }
</pre>
 */
public class ListenerArray {

    /** Return newly allocated array that contains listener and all elements
     ** from data array.
     ** Note: listener is added to resulting array even if it is already
     ** present in data */
    public static Object[] add(Object[] data, Object listener) {
        if (data == null) {
            data = new Object[1];
        }
        else {
            int N = data.length;
            Object[] tmp = new Object[N + 1];
            System.arraycopy(data, 0, tmp, 1, N);
            data = tmp;
        }
        data[0] = listener;
        return data;
    }

    /** Return a copy of data array with the first occurrence of listener
     ** removed.
     ** If listener is not present in data, simply return data.
     ** Note: return <code>null</code> if listener is the single element
     ** of data. */
    public static Object[] remove(Object[] data, Object listener) {
        if (data != null) {
            int N = data.length;
            for (int i = 0; i != N; ++i) {
                if (data[i] == listener) {
                    if (N == 1) { data = null; }
                    else {
                        Object[] tmp = new Object[N - 1];
                        System.arraycopy(data, 0, tmp, 0, i);
                        System.arraycopy(data, i + 1, tmp, i, N - 1 - i);
                        data = tmp;
                    }
                    break;
                }
            }
        }
        return data;
    }

}
