/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is collection of utilities useful for Rhino code.
 *
 * The Initial Developer of the Original Code is
 * RUnit Software AS.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov, igor@fastmail.fm
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript;

import java.io.IOException;
import java.io.Reader;
import java.lang.reflect.Method;
import java.util.Hashtable;

/**
 * Collection of utilities
 */

public class Kit
{
    /**
     * Reflection of Throwable.initCause(Throwable) from JDK 1.4
     * or nul if it is not available.
     */
    private static Method Throwable_initCause = null;

    static {
        // Are we running on a JDK 1.4 or later system?
        try {
            Class ThrowableClass = Kit.classOrNull("java.lang.Throwable");
            Class[] signature = { ThrowableClass };
            Throwable_initCause
                = ThrowableClass.getMethod("initCause", signature);
        } catch (Exception ex) {
            // Assume any exceptions means the method does not exist.
        }
    }

    public static Class classOrNull(String className)
    {
        try {
            return Class.forName(className);
        } catch  (ClassNotFoundException ex) {
        } catch  (SecurityException ex) {
        } catch  (LinkageError ex) {
        } catch (IllegalArgumentException e) {
            // Can be thrown if name has characters that a class name
            // can not contain
        }
        return null;
    }

    public static Class classOrNull(ClassLoader loader, String className)
    {
        try {
            return loader.loadClass(className);
        } catch (ClassNotFoundException ex) {
        } catch (SecurityException ex) {
        } catch  (LinkageError ex) {
        } catch (IllegalArgumentException e) {
            // Can be thrown if name has characters that a class name
            // can not contain
        }
        return null;
    }

    static Object newInstanceOrNull(Class cl)
    {
        try {
            return cl.newInstance();
        } catch (SecurityException x) {
        } catch  (LinkageError ex) {
        } catch (InstantiationException x) {
        } catch (IllegalAccessException x) {
        }
        return null;
    }

    /**
     * If initCause methods exists in Throwable, call
     * <tt>ex.initCause(cause)</tt> or otherwise do nothing.
     * @return The <tt>ex</tt> argument.
     */
    public static Throwable initCause(Throwable ex, Throwable cause)
    {
        if (Throwable_initCause != null) {
            Object[] args = { cause };
            try {
                Throwable_initCause.invoke(ex, args);
            } catch (Exception e) {
                // Ignore any exceptions
            }
        }
        return ex;
    }

    /**
     * Split string into array of strings using semicolon as string terminator
     * (; after the last string is required).
     */
    public static String[] semicolonSplit(String s)
    {
        int count = 0;
        for (int cursor = 0; ;) {
            int next = s.indexOf(';', cursor) + 1;
            if (next <= 0) {
                // check for missing ;
                if (cursor + 1 < s.length())
                    throw new IllegalArgumentException();
                break;
            }
            ++count;
            cursor = next + 1;
        }
        String[] array = new String[count];
        count = 0;
        for (int cursor = 0; ;) {
            int next = s.indexOf(';', cursor);
            if (next < 0) { break; }
            array[count] = s.substring(cursor, next);
            ++count;
            cursor = next + 1;
        }
        return array;
    }

    /**
     * Add <i>listener</i> to <i>bag</i> of listeners.
     * The function does not modify <i>bag</i> and return a new collection
     * containing <i>listener</i> and all listeners from <i>bag</i>.
     * Bag without listeners always represented as the null value.
     * <p>
     * Usage example:
     * <pre>
     *     private volatile Object changeListeners;
     *
     *     public void addMyListener(PropertyChangeListener l)
     *     {
     *         synchronized (this) {
     *             changeListeners = Kit.addListener(changeListeners, l);
     *         }
     *     }
     *
     *     public void removeTextListener(PropertyChangeListener l)
     *     {
     *         synchronized (this) {
     *             changeListeners = Kit.removeListener(changeListeners, l);
     *         }
     *     }
     *
     *     public void fireChangeEvent(Object oldValue, Object newValue)
     *     {
     *     // Get immune local copy
     *         Object listeners = changeListeners;
     *         if (listeners != null) {
     *             PropertyChangeEvent e = new PropertyChangeEvent(
     *                 this, "someProperty" oldValue, newValue);
     *             for (int i = 0; ; ++i) {
     *                 Object l = Kit.getListener(listeners, i);
     *                 if (l == null)
     *                     break;
     *                 ((PropertyChangeListener)l).propertyChange(e);
     *             }
     *         }
     *     }
     * </pre>
     *
     * @param listener Listener to add to <i>bag</i>
     * @param bag Current collection of listeners.
     * @return A new bag containing all listeners from <i>bag</i> and
     *          <i>listener</i>.
     * @see #removeListener(Object bag, Object listener)
     * @see #getListener(Object bag, int index)
     */
    public static Object addListener(Object bag, Object listener)
    {
        if (listener == null) throw new IllegalArgumentException();
        if (listener instanceof Object[]) throw new IllegalArgumentException();

        if (bag == null) {
            bag = listener;
        } else if (!(bag instanceof Object[])) {
            bag = new Object[] { bag, listener };
        } else {
            Object[] array = (Object[])bag;
            int L = array.length;
            // bag has at least 2 elements if it is array
            if (L < 2) throw new IllegalArgumentException();
            Object[] tmp = new Object[L + 1];
            System.arraycopy(array, 0, tmp, 0, L);
            tmp[L] = listener;
            bag = tmp;
        }

        return bag;
    }

    /**
     * Remove <i>listener</i> from <i>bag</i> of listeners.
     * The function does not modify <i>bag</i> and return a new collection
     * containing all listeners from <i>bag</i> except <i>listener</i>.
     * If <i>bag</i> does not contain <i>listener</i>, the function returns
     * <i>bag</i>.
     * <p>
     * For usage example, see {@link addListener(Object bag, Object listener)}.
     *
     * @param listener Listener to remove from <i>bag</i>
     * @param bag Current collection of listeners.
     * @return A new bag containing all listeners from <i>bag</i> except
     *          <i>listener</i>.
     * @see #addListener(Object bag, Object listener)
     * @see #getListener(Object bag, int index)
     */
    public static Object removeListener(Object bag, Object listener)
    {
        if (listener == null) throw new IllegalArgumentException();
        if (listener instanceof Object[]) throw new IllegalArgumentException();

        if (bag == listener) {
            bag = null;
        } else if (bag instanceof Object[]) {
            Object[] array = (Object[])bag;
            int L = array.length;
            // bag has at least 2 elements if it is array
            if (L < 2) throw new IllegalArgumentException();
            if (L == 2) {
                if (array[1] == listener) {
                    bag = array[0];
                } else if (array[0] == listener) {
                    bag = array[1];
                }
            } else {
                int i = L;
                do {
                    --i;
                    if (array[i] == listener) {
                        Object[] tmp = new Object[L - 1];
                        System.arraycopy(array, 0, tmp, 0, i);
                        System.arraycopy(array, i + 1, tmp, i, L - (i + 1));
                        bag = tmp;
                        break;
                    }
                } while (i != 0);
            }
        }

        return bag;
    }

    /**
     * Get listener at <i>index</i> position in <i>bag</i> or null if
     * <i>index</i> equals to number of listeners in <i>bag</i>.
     * <p>
     * For usage example, see {@link addListener(Object bag, Object listener)}.
     *
     * @param bag Current collection of listeners.
     * @param index Index of the listener to access.
     * @return Listener at the given index or null.
     * @see #addListener(Object bag, Object listener)
     * @see #removeListener(Object bag, Object listener)
     */
    public static Object getListener(Object bag, int index)
    {
        if (index == 0) {
            if (bag == null)
                return null;
            if (!(bag instanceof Object[]))
                return bag;
            Object[] array = (Object[])bag;
            // bag has at least 2 elements if it is array
            if (array.length < 2) throw new IllegalArgumentException();
            return array[0];
        } else if (index == 1) {
            if (!(bag instanceof Object[])) {
                if (bag == null) throw new IllegalArgumentException();
                return null;
            }
            Object[] array = (Object[])bag;
            // the array access will check for index on its own
            return array[1];
        } else {
            // bag has to array
            Object[] array = (Object[])bag;
            int L = array.length;
            if (L < 2) throw new IllegalArgumentException();
            if (index == L)
                return null;
            return array[index];
        }
    }

    static Object initHash(Hashtable h, Object key, Object initialValue)
    {
        synchronized (h) {
            Object current = h.get(key);
            if (current == null) {
                h.put(key, initialValue);
            } else {
                initialValue = current;
            }
        }
        return initialValue;
    }

    private final static class ComplexKey
    {
        private Object key1;
        private Object key2;
        private int hash;

        ComplexKey(Object key1, Object key2)
        {
            this.key1 = key1;
            this.key2 = key2;
        }

        public boolean equals(Object anotherObj)
        {
            if (!(anotherObj instanceof ComplexKey))
                return false;
            ComplexKey another = (ComplexKey)anotherObj;
            return key1.equals(another.key1) && key2.equals(another.key2);
        }

        public int hashCode()
        {
            if (hash == 0) {
                hash = key1.hashCode() ^ key2.hashCode();
            }
            return hash;
        }
    }

    public static Object makeHashKeyFromPair(Object key1, Object key2)
    {
        if (key1 == null) throw new IllegalArgumentException();
        if (key2 == null) throw new IllegalArgumentException();
        return new ComplexKey(key1, key2);
    }

    public static String readReader(Reader r)
        throws IOException
    {
        char[] buffer = new char[512];
        int cursor = 0;
        for (;;) {
            int n = r.read(buffer, cursor, buffer.length - cursor);
            if (n < 0) { break; }
            cursor += n;
            if (cursor == buffer.length) {
                char[] tmp = new char[buffer.length * 2];
                System.arraycopy(buffer, 0, tmp, 0, cursor);
                buffer = tmp;
            }
        }
        return new String(buffer, 0, cursor);
    }

    /**
     * Throws RuntimeException to indicate failed assertion.
     * The function never returns and its return type is RuntimeException
     * only to be able to write <tt>throw Kit.codeBug()</tt> if plain
     * <tt>Kit.codeBug()</tt> triggers unreachable code error.
     */
    public static RuntimeException codeBug()
        throws RuntimeException
    {
        throw new IllegalStateException("FAILED ASSERTION");
    }

    /**
     * Throws RuntimeException to indicate that x does not have a valid JS type.
     *
     * The function never returns and its return type is RuntimeException
     * to allow to indicate unrechable code via
     * <tt>throw Kit.badTypeJS(x)</tt>.
     */
    public static RuntimeException badTypeJS(Object x)
        throws RuntimeException
    {
        throw new IllegalArgumentException("Type "+x.getClass().getName()+" of "+x+" is not valid JS type");
    }
}

