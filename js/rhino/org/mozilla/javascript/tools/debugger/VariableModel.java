/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino JavaScript Debugger code, released
 * November 21, 2000.
 *
 * The Initial Developer of the Original Code is See Beyond Corporation.

 * Portions created by See Beyond are
 * Copyright (C) 2000 See Beyond Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Christopher Oliver
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

package org.mozilla.javascript.tools.debugger;
import java.io.File;
import java.util.Date;
import org.mozilla.javascript.*;
import org.mozilla.javascript.tools.shell.Global;
import javax.swing.event.TableModelEvent;
import java.util.Hashtable;
import java.util.Enumeration;

public class VariableModel extends AbstractTreeTableModel 
                             implements TreeTableModel {

    // Names of the columns.
    //static protected String[]  cNames = {"Name", "Type", "Value"};
    static protected String[]  cNames = { " Name", " Value"};

    // Types of the columns.
    //static protected Class[]  cTypes = {TreeTableModel.class, String.class, String.class};
    static protected Class[]  cTypes = {TreeTableModel.class, String.class};

    public VariableModel(Scriptable scope) { 
	super(scope == null ? null : new VariableNode(scope, "this")); 
    }

    //
    // Some convenience methods. 
    //

    protected Object getObject(Object node) {
	VariableNode varNode = ((VariableNode)node); 
	if(varNode == null) return null;
	return varNode.getObject();       
    }

    protected Object[] getChildren(Object node) {
	VariableNode varNode = ((VariableNode)node); 
	return varNode.getChildren(); 
    }

    //
    // The TreeModel interface
    //

    public int getChildCount(Object node) { 
	Object[] children = getChildren(node); 
	return (children == null) ? 0 : children.length;
    }

    public Object getChild(Object node, int i) { 
	return getChildren(node)[i]; 
    }

    // The superclass's implementation would work, but this is more efficient. 
    public boolean isLeaf(Object node) { 
	if(node == null) return true;
	VariableNode varNode = (VariableNode)node;
	Object[] children = varNode.getChildren();
	if(children != null && children.length > 0) {
	    return false;
	}
	//Object value = getObject(node);
	//if(value != null && value instanceof Scriptable) {
	//if(value == Undefined.instance ||
	//value == ScriptableObject.NOT_FOUND) {
	//return true;
	//}
	//Scriptable scrip = (Scriptable)value;
	//Scriptable proto = scrip.getPrototype();
	//if(proto != null && 
	//proto != ScriptableObject.getObjectPrototype(scrip)) {
	//return false;
	//}
	//}
	//	if(value == null) return true;
	//	if (value == Undefined.instance)
	//	    return true;
	//	if (value == null)
	//	    return true;
	//	if (value instanceof String)
	//	    return true;
	//	if (value instanceof Number)
	//	    return true;
	//	if (value instanceof Boolean)
	//	    return true;
	//	if(value instanceof Scriptable) {
	//	    //Scriptable scrip = (Scriptable)value;
	    //if(scrip.has(0, scrip)) {
	    //return false;
	    //}
	    //if(ScriptableObject.getPropertyIds(scrip).length > 0) {
	    //return false;
	    //}
	    //return true;
	//}
	return true;
    }

    public boolean isCellEditable(Object node, int column) {
	return column == 0 || column == 2;
    }

    //
    //  The TreeTableNode interface. 
    //

    public int getColumnCount() {
	return cNames.length;
    }

    public String getColumnName(int column) {
	return cNames[column];
    }

    public Class getColumnClass(int column) {
	return cTypes[column];
    }
 
    public Object getValueAt(Object node, int column) {
	Object value = getObject(node); 
	try {
	    switch(column) {
	    case 0: // Name
		VariableNode varNode = (VariableNode)node;
		String name = "";
		if(varNode.name != null) {
		    return name + varNode.name;
		}
		return name + "[" + varNode.index + "]";
	    case -1: // Type
		if(value == Undefined.instance || value == ScriptableObject.NOT_FOUND) {
		    return "undefined";
		}
		if (value == null)
		    return "object";
		if (value instanceof Scriptable)
		    return (value instanceof Function) ? "function" : "object";
		if (value instanceof String)
		    return "string";
		if (value instanceof Number)
		    return "number";
		if (value instanceof Boolean)
		    return "boolean";
		return value.getClass().getName();
	    case 1: // value
		if(value == Undefined.instance || value == ScriptableObject.NOT_FOUND) {
		    return "undefined";
		}
		if(value instanceof Scriptable) {
		    try {
			Context cx = Context.enter();
			String result;
			try {
			    result = ScriptRuntime.toString(value);
			} catch(Exception exc) {
			    result = value.toString();
			} finally {
			    cx.exit();
			}
			StringBuffer buf = new StringBuffer();
			int len = result.length();
			for(int i = 0; i < len; i++) {
			    char ch = result.charAt(i);
			    if(Character.isISOControl(ch)) {
				ch = ' ';
			    } 
			    buf.append(ch);
			}
			return buf.toString();
		    } catch(Exception exc) {
			exc.printStackTrace();
		    }
		}
		if(value == null) {
		    return "null";
		}
		return value.toString();
	    }
	}
	catch  (Exception exc) { 
	    //exc.printStackTrace();
	}
	return null; 
    }

    public void setScope(Scriptable scope) {
	//System.out.println("setScope: " + scope);
	VariableNode rootVar = (VariableNode)root;
	rootVar.scope = scope;
	fireTreeNodesChanged(this,
			     new Object[]{root},
			     null, new Object[]{root});
    }

}


class VariableNode { 
    Scriptable scope;
    String name;
    int index;

    public VariableNode(Scriptable scope, String name) { 
	this.scope = scope;
	this.name = name;
    }

    public VariableNode(Scriptable scope, int index) { 
	this.scope = scope;
	this.name = null;
	this.index = index;
    }

    /**
     * Returns the the string to be used to display this leaf in the JTree.
     */
    public String toString() { 
	return (name != null ? name : "[" + index + "]");
    }

    public Object getObject() {
	try {
	    if(scope == null) return null;
	    if(name != null) {
		if(name.equals("this")) {
		    return scope;
		}
		Object result;
		if(name.equals("__proto__")) {
		    result = scope.getPrototype();
		} else if(name.equals("__parent__")) {
		    result = scope.getParentScope();
		} else {
		    result = scope.get(name, scope);
		}
		if(result == ScriptableObject.NOT_FOUND) {
		    result = Undefined.instance;
		}
		return result;
	    }
	    Object result = scope.get(index, scope);
	    if(result == ScriptableObject.NOT_FOUND) {
		result = Undefined.instance;
	    }
	    return result;
	} catch(Exception exc) {
	    return "undefined";
	}
    }

    Object[] children;

    /**
     * Loads the children, caching the results in the children ivar.
     */
    static final Object[] empty = new Object[0];
    static Scriptable builtin[];
    protected Object[] getChildren() {
	if(children != null) return children;
	try {
	    Object value = getObject();
	    if(value == null) return children = empty;
	    if(value == ScriptableObject.NOT_FOUND ||
	       value == Undefined.instance) {
		return children = empty;
	    }
	    if(value instanceof Scriptable) {
		Scriptable scrip = (Scriptable)value;
		Scriptable proto = scrip.getPrototype();
		Scriptable parent = scrip.getParentScope();
		if(scrip instanceof NativeCall) {
		    parent = proto = null;
		} else if(parent instanceof NativeCall) {
		    parent = null;
		}
		if(proto != null) {
		    if(builtin == null) {
			builtin = new Scriptable[6];
			builtin[0] =  
			    ScriptableObject.getObjectPrototype(scrip);
			builtin[1] = 
			    ScriptableObject.getFunctionPrototype(scrip);
			builtin[2] = 
			    ScriptableObject.getClassPrototype(scrip, 
							       "String");
			builtin[3] = 
			    ScriptableObject.getClassPrototype(scrip, 
							       "Boolean");
			builtin[4] = 
			    ScriptableObject.getClassPrototype(scrip, 
							       "Array");
			builtin[5] =
			    ScriptableObject.getClassPrototype(scrip, 
							       "Number");
		    }
		    for(int i = 0; i < builtin.length; i++) {
			if(proto == builtin[i]) {
			    proto = null;
			    break;
			}
		    }
		}
		if(scrip.has(0, scrip)) {
		    int len = 0;
		    try {
			Scriptable start = scrip;
			Scriptable obj = start;
			Object result = Undefined.instance;
			do {
			    if(obj.has("length", start)) {
				result = obj.get("length", start);
				if (result != Scriptable.NOT_FOUND)
				    break;
			    }
			    obj = obj.getPrototype();
			} while (obj != null);
			if(result instanceof Number) {
			    len = ((Number)result).intValue();
			}
		    } catch(Exception exc) {
		    }
		    if(parent != null) {
			len++;
		    }
		    if(proto != null) {
			len++;
		    }
		    children = new VariableNode[len];
		    int i = 0;
		    int j = 0;
		    if(proto != null) {
			children[i++] = new VariableNode(scrip, "__proto__");
			j++;
		    }
		    if(parent != null) {
			children[i++] = new VariableNode(scrip, "__parent__");
			j++;
		    }
		    for(; i < len; i++) {
			children[i] = new VariableNode(scrip, i-j);
		    }
		} else {
		    int len = 0;
		    Hashtable t = new Hashtable();
		    Object[] ids = scrip.getIds();
		    if(proto != null) t.put("__proto__", "__proto__");
		    if(parent != null) t.put("__parent__", "__parent__");
		    if(ids.length > 0) {
			for(int j = 0; j < ids.length; j++) {
			    t.put(ids[j], ids[j]);
			}
		    }
		    ids = new Object[t.size()];
		    Enumeration e = t.keys();
		    int j = 0;
		    while(e.hasMoreElements()) {
			ids[j++] = e.nextElement().toString();
		    }
		    if(ids != null && ids.length > 0) {
			java.util.Arrays.sort(ids, new java.util.Comparator() {
				public int compare(Object l, Object r) {
				    return l.toString().compareToIgnoreCase(r.toString());
				
				}
			    });
			len = ids.length;
		    }
		    children = new VariableNode[len]; 
		    for(int i = 0; i < len; i++) {
			Object id = ids[i];
				////System.out.println("id is " + id);
			children[i] = 
			    new VariableNode(scrip, id.toString());
		    }
		}
	    }
	} catch (Exception exc) {
	    exc.printStackTrace();
	}
	return children; 
    }
}

