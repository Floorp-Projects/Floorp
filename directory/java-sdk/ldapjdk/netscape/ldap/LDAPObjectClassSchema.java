/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;

/**
 * The definition of an object class in the schema.
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an object class.
 * According to the RFC, the description of an object class can
 * include the following information:
 * <P>
 *
 * <UL>
 * <LI>an OID identifying the object class
 * <LI>a name identifying the object class
 * <LI>a description of the object class
 * <LI>the name of the parent object class
 * <LI>the list of attribute types that are required in this object class
 * <LI>the list of attribute types that are allowed (optional) in this
 * object class
 * </UL>
 * <P>
 *
 * When you construct an <CODE>LDAPObjectSchema</CODE> object, you can specify
 * these types of information as arguments to the constructor or in the
 * ObjectClassDescription format specified in RFC 2252.
 * When an LDAP client searches an LDAP server for the schema, the server
 * returns schema information as an object with attribute values in this
 * format.
 * <P>
 *
 * RFC 2252 also notes that you can specify whether or not an object class
 * is abstract, structural, or auxiliary in the object description.
 * Abstract object classes are used only to derive other object classes.
 * Entries cannot belong to an abstract object class. <CODE>top</CODE>
 * is an abstract object class. Entries must belong to a structural
 * object class, so most object classes are structural object classes.
 * Objects of the <CODE>LDAPObjectClassSchema</CODE> class are structural
 * object classes by default. Auxiliary object classes can be used to
 * add attributes to entries of different types. For example, an
 * auxiliary object class might be used to specify personal preference
 * attributes. An entry can not contain just that object class, but may
 * include it along with a structural object class, for example
 * inetOrgPerson.
 * If the definition of an object (in ObjectClassDescription format)
 * specifies the AUXILIARY keyword, an <CODE>LDAPObjectClassSchema</CODE>
 * object created from that description represents an auxiliary object class.
 * <P>
 *
 * You can get the name, OID, and description of this object class
 * definition by using the <CODE>getName</CODE>, <CODE>getOID</CODE>, and
 * <CODE>getDescription</CODE> methods inherited from the abstract class
 * <CODE>LDAPSchemaElement</CODE>. Optional and custom qualifiers are
 * accessed with <CODE>getQualifier</CODE> and <CODE>getQualifierNames</CODE>
 * from <CODE>LDAPSchemaElement</CODE>.
 
 * <P>
 *
 * To add or remove this object class definition from the
 * schema, use the <CODE>add</CODE> and <CODE>remove</CODE>
 * methods, which this class inherits from the <CODE>LDAPSchemaElement</CODE>
 * abstract class.
 * <P>
 * RFC 2252 defines ObjectClassDescription as follows:
 * <P>
 * <PRE>
 *    ObjectClassDescription = "(" whsp
 *        numericoid whsp      ; ObjectClass identifier
 *        [ "NAME" qdescrs ]
 *        [ "DESC" qdstring ]
 *        [ "OBSOLETE" whsp ]
 *        [ "SUP" oids ]       ; Superior ObjectClasses
 *        [ ( "ABSTRACT" / "STRUCTURAL" / "AUXILIARY" ) whsp ]
 *                             ; default structural
 *        [ "MUST" oids ]      ; AttributeTypes
 *        [ "MAY" oids ]       ; AttributeTypes
 *    whsp ")"
 * </PRE>
 *
 * @version 1.0
 * @see netscape.ldap.LDAPSchemaElement
 **/
public class LDAPObjectClassSchema extends LDAPSchemaElement {
    /**
     * Constructs an object class definition, using the specified
     * information.
     * @param name Name of the object class.
     * @param oid Object identifier (OID) of the object class
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of the object class.
     * @param superior Name of the parent object class
     * (the object class that the new object class inherits from).
     * @param required Array of the names of the attributes required
     * in this object class.
     * @param optional Array of the names of the optional attributes
     * allowed in this object class.
     */
    public LDAPObjectClassSchema( String name, String oid, String superior,
                                  String description,
                                  String[] required, String[] optional ) {
        super( name, oid, description );
        attrName = "objectclasses";
        this.superiors = new String[] { superior };
        setQualifier( SUPERIOR, superior );
        for( int i = 0; i < required.length; i++ ) {
            must.addElement( required[i] );
        }
        for( int i = 0; i < optional.length; i++ ) {
            may.addElement( optional[i] );
        }
    }

    /**
     * Constructs an object class definition, using the specified
     * information.
     * @param name Name of the object class.
     * @param oid Object identifier (OID) of the object class
     * in dotted-string format (for example, "1.2.3.4").
     * @param description Description of the object class.
     * @param superiors Name of parent object classes
     * (the object class that the new object class inherits from).
     * @param required Array of the names of the attributes required
     * in this object class.
     * @param optional Array of the names of the optional attributes
     * allowed in this object class.
     * @param type Either ABSTRACT, STRUCTURAL, or AUXILIARY
     */
    public LDAPObjectClassSchema( String name, String oid,
                                  String[] superiors,
                                  String description,
                                  String[] required, String[] optional,
                                  int type ) {
        this( name, oid, superiors[0], description, required, optional );
        this.superiors = superiors;
        setQualifier( SUPERIOR, this.superiors );
        this.type = type;
    }

    /**
     * Constructs an object class definition based on a description in
     * the ObjectClassDescription format.  For information on this format,
     * (see <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
     * Attribute Syntax Definitions</A>.  This is the format that LDAP servers
     * and clients use to exchange schema information.  (For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "objectclasses" attribute are object class descriptions
     * in this format.)
     * <P>
     *
     * @param raw Definition of the object in the ObjectClassDescription
     * format.
     */
    public LDAPObjectClassSchema( String raw ) {
        attrName = "objectclasses";
        parseValue( raw );
        String[] vals = getQualifier( SUPERIOR );
        if ( vals != null ) {
            superiors = vals;
            setQualifier( SUPERIOR, (String)null );
        }
        if ( properties.containsKey( "AUXILIARY" ) ) {
            type = AUXILIARY;
        } else if ( properties.containsKey( "ABSTRACT" ) ) {
            type = ABSTRACT;
        } else if ( properties.containsKey( "STRUCTURAL" ) ) {
            type = STRUCTURAL;
        }
        obsolete = properties.containsKey( OBSOLETE );
        Object o = properties.get( "MAY" );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                may = (Vector)o;
            } else {
                may.addElement( o );
            }
        }
        o = properties.get( "MUST" );
        if ( o != null ) {
            if ( o instanceof Vector ) {
                must = (Vector)o;
            } else {
                must.addElement( o );
            }
        }
    }

    /**
     * Get the name of the object class that this class inherits from.
     * @return The name of the object class that this class
     * inherits from.
     */
    public String getSuperior() {
        return superiors[0];
    }

    /**
     * Get the names of all object classes that this class inherits
     * from. Typically only one, but RFC 2252 allows multiple
     * inheritance.
     * @return The names of the object classes that this class
     * inherits from.
     */
    public String[] getSuperiors() {
        return superiors;
    }

    /**
     * Get an enumeration of the names of the required attribute for
     * this object class.
     * @return An enumeration of the names of the required attributes
     * for this object class.
     */
    public Enumeration getRequiredAttributes() {
        return must.elements();
    }

    /**
     * Get an enumeration of names of optional attributes allowed
     * in this object class.
     * @return An enumeration of the names of optional attributes
     * allowed in this object class.
     */
    public Enumeration getOptionalAttributes() {
        return may.elements();
    }

    /**
     * Get the type of the object class.
     * @return STRUCTURAL, ABSTRACT, or AUXILIARY
     */
    public int getType() {
        return type;
    }

    /**
     * Prepare a value in RFC 2252 format for submitting to a server
     *
     * @param quotingBug <CODE>true</CODE> if SUP and SYNTAX values are to
     * be quoted. That is to satisfy bugs in certain LDAP servers.
     * @return A String ready to be submitted to an LDAP server
     */
	String getValue( boolean quotingBug ) {
        String s = getValuePrefix();
        s += getValue( SUPERIOR, quotingBug );
        s += ' ';
        String val = getOptionalValues( NOVALS );
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += "MUST " + vectorToList( must );
        s += ' ';
        s += "MAY " + vectorToList( may );
        s += ' ';
        val = getCustomValues();
        if ( val.length() > 0 ) {
            s += val + ' ';
        }
        s += ')';
        return s;
    }

    /**
     * Get the definition of the object class in a user friendly format.
     * This is the format that the object class definition uses when
     * you print the object class or the schema.
     * @return Definition of the object class in a user friendly format.
     */
    public String toString() {
        String s = "Name: " + name + "; OID: " + oid +
            "; Superior: ";
        for( int i = 0; i < superiors.length; i++ ) {
            s += superiors[i];
            if ( i < (superiors.length-1) ) {
                s += ", ";
            }
        }
        s += "; Description: " + description + "; Required: ";
        int i = 0;
        Enumeration e = getRequiredAttributes();
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += ", ";
            i++;
            s += (String)e.nextElement();
        }
        s += "; Optional: ";
        e = getOptionalAttributes();
        i = 0;
        while( e.hasMoreElements() ) {
            if ( i > 0 )
                s += ", ";
            i++;
            s += (String)e.nextElement();
        }
        switch ( type ) {
        case STRUCTURAL:
            break;
        case ABSTRACT:
            s += "; ABSTRACT";
            break;
        case AUXILIARY:
            s += "; AUXILIARY";
            break;
        }
        if ( obsolete ) {
            s += "; OBSOLETE";
        }
        s += getQualifierString( IGNOREVALS );
        return s;
    }

    /**
     * Create a list within parentheses, with $ as delimiter
     *
     * @param vals Values for list
     * @return A String with a list of values
     */
    protected String vectorToList( Vector vals ) {
        String val = "( ";
        for( int i = 0; i < vals.size(); i++ ) {
            val += (String)vals.elementAt(i) + ' ';
            if ( i < (vals.size() - 1) ) {
                val += "$ ";
            }
        }
        val += ')';
        return val;
    }

    public static final int STRUCTURAL = 0;
    public static final int ABSTRACT = 1;
    public static final int AUXILIARY = 2;

    private String[] superiors = { "" };
    private Vector must = new Vector();
    private Vector may = new Vector();
    private int type = STRUCTURAL;

    // Qualifers known to not have values
	static String[] NOVALS = { "ABSTRACT", "STRUCTURAL",
							   "AUXILIARY" };
	static {
		for( int i = 0; i < NOVALS.length; i++ ) {
			novalsTable.put( NOVALS[i], NOVALS[i] );
		}
	}
	static String[] IGNOREVALS = { "ABSTRACT", "STRUCTURAL",
                                   "AUXILIARY", "MUST", "MAY" };
}
