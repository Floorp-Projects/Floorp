/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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
 * Norris Boyd
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

package org.mozilla.javascript;

/**
 * This class implements the "arguments" object.
 *
 * See ECMA 10.1.8
 *
 * @see org.mozilla.javascript.NativeCall
 * @author Norris Boyd
 */
class Arguments extends IdScriptable {

    public Arguments(NativeCall activation)
    {
        this.activation = activation;

        Scriptable parent = activation.getParentScope();
        setParentScope(parent);
        setPrototype(ScriptableObject.getObjectPrototype(parent));

        args = activation.getOriginalArguments();
        lengthObj = new Integer(args.length);

        NativeFunction f = activation.getFunctionObject();
        calleeObj = f;

        if (f.version <= Context.VERSION_1_3
            && f.version != Context.VERSION_DEFAULT)
        {
            callerObj = null;
        } else {
            callerObj = NOT_FOUND;
        }
    }

    public String getClassName()
    {
        return "Arguments";
    }

    public boolean has(int index, Scriptable start)
    {
        if (0 <= index && index < args.length) {
            if (args[index] != NOT_FOUND) {
                return true;
            }
        }
        return super.has(index, start);
    }

    public Object get(int index, Scriptable start)
    {
        if (0 <= index && index < args.length) {
            Object value = args[index];
            if (value != NOT_FOUND) {
                if (sharedWithActivation(index)) {
                    NativeFunction f = activation.getFunctionObject();
                    String argName = f.argNames[index];
                    value = activation.get(argName, activation);
                    if (value == NOT_FOUND) Kit.codeBug();
                }
                return value;
            }
        }
        return super.get(index, start);
    }

    private boolean sharedWithActivation(int index)
    {
        NativeFunction f = activation.getFunctionObject();
        int definedCount = f.argCount;
        if (index < definedCount) {
            // Check if argument is not hidden by later argument with the same
            // name as hidden arguments are not shared with activation
            if (index < definedCount - 1) {
                String argName = f.argNames[index];
                for (int i = index + 1; i < definedCount; i++) {
                    if (argName.equals(f.argNames[i])) {
                        return false;
                    }
                }
            }
            return true;
        }
        return false;
    }

    public void put(int index, Scriptable start, Object value)
    {
        if (0 <= index && index < args.length) {
            if (args[index] != NOT_FOUND) {
                if (sharedWithActivation(index)) {
                    NativeFunction f = activation.getFunctionObject();
                    String argName = f.argNames[index];
                    activation.put(argName, activation, value);
                    return;
                }
                synchronized (this) {
                    if (args[index] != NOT_FOUND) {
                        if (args == activation.getOriginalArguments()) {
                            args = (Object[])args.clone();
                        }
                        args[index] = value;
                        return;
                    }
                }
            }
        }
        super.put(index, start, value);
    }

    public void delete(int index)
    {
        if (0 <= index && index < args.length) {
            synchronized (this) {
                if (args[index] != NOT_FOUND) {
                    if (args == activation.getOriginalArguments()) {
                        args = (Object[])args.clone();
                    }
                    args[index] = NOT_FOUND;
                    return;
                }
            }
        }
        super.delete(index);
    }

// #string_id_map#

    private static final int
        Id_callee           = 1,
        Id_length           = 2,
        Id_caller           = 3,

        MAX_INSTANCE_ID     = 3;

    {
        setMaxInstanceId(0, MAX_INSTANCE_ID);
    }

    protected int findInstanceIdInfo(String s)
    {
        int id;
// #generated# Last update: 2002-04-09 20:46:33 CEST
        L0: { id = 0; String X = null; int c;
            if (s.length()==6) {
                c=s.charAt(5);
                if (c=='e') { X="callee";id=Id_callee; }
                else if (c=='h') { X="length";id=Id_length; }
                else if (c=='r') { X="caller";id=Id_caller; }
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#

        if (id == 0) return super.findInstanceIdInfo(s);

        int attr;
        switch (id) {
          case Id_callee:
          case Id_caller:
          case Id_length:
            attr = DONTENUM;
            break;
          default: throw new IllegalStateException();
        }
        return instanceIdInfo(attr, id);
    }

// #/string_id_map#

    protected String getInstanceIdName(int id)
    {
        switch (id) {
            case Id_callee: return "callee";
            case Id_length: return "length";
            case Id_caller: return "caller";
        }
        return null;
    }

    protected Object getInstanceIdValue(int id)
    {
        switch (id) {
            case Id_callee: return calleeObj;
            case Id_length: return lengthObj;
            case Id_caller: {
                Object value = callerObj;
                if (value == UniqueTag.NULL_VALUE) { value = null; }
                else if (value == null) {
                    NativeCall caller = activation.parentActivationCall;
                    if (caller == null) {
                        value = null;
                    } else {
                        value = caller.get("arguments", caller);
                    }
                }
                return value;
            }
        }
        return super.getInstanceIdValue(id);
    }

    protected void setInstanceIdValue(int id, Object value)
    {
        switch (id) {
            case Id_callee: calleeObj = value; return;
            case Id_length: lengthObj = value; return;
            case Id_caller:
                callerObj = (value != null) ? value : UniqueTag.NULL_VALUE;
                return;
        }
        super.setInstanceIdValue(id, value);
    }

    Object[] getIds(boolean getAll)
    {
        Object[] ids = super.getIds(getAll);
        if (getAll && args.length != 0) {
            boolean[] present = null;
            int extraCount = args.length;
            for (int i = 0; i != ids.length; ++i) {
                Object id = ids[i];
                if (id instanceof Integer) {
                    int index = ((Integer)id).intValue();
                    if (0 <= index && index < args.length) {
                        if (present == null) {
                            present = new boolean[args.length];
                        }
                        if (!present[index]) {
                            present[index] = true;
                            extraCount--;
                        }
                    }
                }
            }
            if (extraCount != 0) {
                Object[] tmp = new Object[extraCount + ids.length];
                System.arraycopy(ids, 0, tmp, extraCount, ids.length);
                ids = tmp;
                int offset = 0;
                for (int i = 0; i != args.length; ++i) {
                    if (present == null || !present[i]) {
                        ids[offset] = new Integer(i);
                        ++offset;
                    }
                }
                if (offset != extraCount) Kit.codeBug();
            }
        }
        return ids;
    }

// Fields to hold caller, callee and length properties,
// where NOT_FOUND value tags deleted properties.
// In addition if callerObj == NULL_VALUE, it tags null for scripts, as
// initial callerObj == null means access to caller arguments available
// only in JS <= 1.3 scripts
    private Object callerObj;
    private Object calleeObj;
    private Object lengthObj;

    private NativeCall activation;

// Initially args holds activation.getOriginalArgs(), but any modification
// of its elements triggers creation of a copy. If its element holds NOT_FOUND,
// it indicates deleted index, in which case super class is queried.
    private Object[] args;
}
