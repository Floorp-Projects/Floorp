package com.compilercompany.es3c.v1;
import java.util.Vector;
import java.util.Enumeration;

/**
 * A reference value.
 */

public class ReferenceValue extends Value {

    private static final boolean debug = true;

    Scope  scope;
    Vector namespaces;
    Value  qualifier;
    String name;

    public String toString() { 
        return "reference(scope=" + scope + ", namespace=" + namespaces + ", name=" + name  + ")"; 
    }

    public ReferenceValue( Scope scope, Vector namespaces, String name ) {

        if( debug ) {
            Debugger.trace( "creating reference scope = " + scope + 
                            " name = " + name + " used_namespaces = " + namespaces );
        }

        this.scope      = scope;
        this.namespaces = namespaces;
        this.qualifier  = null;
        this.name       = name;
    }

    public ReferenceValue( Scope scope, Value qualifier, String name ) {

        if( debug ) {
            Debugger.trace( "creating reference scope = " + scope + 
                            " name = " + name + " namespaces = " + namespaces );
        }

        this.scope      = scope;
        this.namespaces = null;
        this.qualifier  = qualifier;
        this.name       = name;
    }

    String getName() {
        return name;
    }

    Slot getSlotInScope(Context context, Scope scope, String name) throws Exception {
        Slot slot = null;
        if( debug ) {
            Debugger.trace("entering getSlotInScope() with namespaces=" + namespaces + 
                                ", name= " + name + ", slot=" + slot );
        }
        if( namespaces==null || namespaces.size()==0 ) {
            slot = scope.get(qualifier,name);
        } else {
            Enumeration e = namespaces.elements();
            while(e.hasMoreElements()) {
                Value namespace = (Value) e.nextElement();
                slot = scope.get(namespace,name);
                if( debug ) {
                    Debugger.trace("looking for name = " + name + " in namespace=" + namespace + ", slot=" + slot );
                }
                if( slot!=null ) {
                    break;
                }
            }

			if( slot==null ) {
                slot = scope.get(qualifier/*=null*/,name);
			}
        }
        if( debug ) {
            Debugger.trace("leaving getSlotInScope() with slot="+slot);
        }
        return slot;
    }

    public Slot getSlot(Context context) throws Exception {

        if( debug ) {
            Debugger.trace("ReferenceValue.getSlot() with namespaces = " + namespaces + ", name = " + name );
        }

        Slot slot = null;

        // lookup the name in the specific scope.


        if( scope!=null ) {

            slot = getSlotInScope(context,scope,name);

            // ACTION: Look also in the prototype(s).

            // ISSUE: What about reference to static vars though instances?

        } else {

            Scope scope = context.getLocal();

            while(scope!=null) {
                slot = getSlotInScope(context,scope,name);

                if(slot!=null) {
                    break;
                }

                // ACTION: Look also in the prototype(s).

                scope = context.nextScope(scope);
            }
        }

        return slot;
    }

    public Value getValue(Context context) throws Exception {

        if( debug ) {
            Debugger.trace("ReferenceValue.getValue() with namespaces = " + namespaces + ", name = " + name );
        }

        Value value = UndefinedValue.undefinedValue;
        Slot  slot  = getSlot(context);
        
        if( slot != null ) {

            Block d = slot.block;
            Block r = context.getBlock();

            // d == null means that the definition is a native
            // an so dominates all user code.

            if( d == null || Flow.dom(context,d,r) ) {
                value = slot.value;
            } else {
                // d does not dominate r, so we can't tell at compile-time
                // what the value of the current reference is.
                value = UndefinedValue.undefinedValue;
            }
        }

        return value;
    }

    public Value getType(Context context) throws Exception {

        if( debug ) {
            Debugger.trace("ReferenceValue.getType() with namespaces = " + namespaces + ", name = " + name );
        }

        Value type = ObjectType.type;
        Slot slot = getSlot(context);
        if( slot != null ) {
            type = slot.type;
        } 
        return type;
    }

    Value getAttrs(Context context) throws Exception {

        if( debug ) {
            Debugger.trace("ReferenceValue.getAttrs() with namespaces = " + namespaces + ", name = " + name );
        }

        Value attrs = null;
        Slot  slot  = getSlot(context);
        
        if( slot != null ) {
            attrs = slot.attrs;
        } 

        return attrs;
    }

}

/*
 * The end.
 */
