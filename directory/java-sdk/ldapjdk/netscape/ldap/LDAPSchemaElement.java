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
 *
 * Abstract class representing an element (such as an object class
 * definition, an attribute type definition, or a matching rule
 * definition) in the schema. The specific types of elements are
 * represented by the <CODE>LDAPObjectClassSchema</CODE>,
 * <CODE>LDAPAttributeSchema</CODE>, and <CODE>LDAPMatchingRuleSchema</CODE>
 * subclasses.
 * <P>
 *
 * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
 * TARGET="_blank">RFC 2252, Lightweight Directory Access Protocol (v3):
 * Attribute Syntax Definitions</A> covers the types of information
 * that need to be specified in the definition of an object class,
 * attribute type, or matching rule.  All of these schema elements
 * can specify the following information:
 * <P>
 *
 * <UL>
 * <LI>a name identifying the element
 * <LI>an OID identifying the element
 * <LI>a description of the element
 * <LI>a qualifier "OBSOLETE"
 * </UL>
 * <P>
 *
 * In addition, there are optional standard qualifiers for attribute
 * types (see LDAPAttributeSchema), and implementation-specific
 * qualifiers may be added. Non-standard qualifiers must have names
 * starting with X-, e.g. "X-OWNER 'John Jacobson'". Optional and
 * non-standard qualifiers can be accessed with <CODE>getQualifier</CODE> and
 * <CODE>setQualifier</CODE>, and enumerated with
 * <CODE>getQualifierNames</CODE>.
 * <P>
 *
 * The <CODE>LDAPSchemaElement</CODE> class implements methods that
 * you can use with different types of schema elements (object class
 * definitions, attribute type definitions, and matching rule definitions).
 * You can do the following:
 * <UL>
 * <LI>get the name of a schema element
 * <LI>get the OID of a schema element
 * <LI>get the description of a schema element
 * <LI>add an element to the schema
 * <LI>remove an element from the schema
 * </UL>
 * <P>
 *
 * @see netscape.ldap.LDAPObjectClassSchema
 * @see netscape.ldap.LDAPAttributeSchema
 * @see netscape.ldap.LDAPMatchingRuleSchema
 * @version 1.0
 **/

public abstract class LDAPSchemaElement {
    /**
     * Construct a blank element.
     */
    protected LDAPSchemaElement() {
    }

    /**
     * Construct a definition explicitly.
     * @param name Name of element.
     * @param oid Dotted-string object identifier.
     * @param description Description of element.
     */
    protected LDAPSchemaElement( String name, String oid,
                                 String description ) {
        this.name = name;
        this.oid = oid;
        this.description = description;
    }

    /**
     * Gets the name of the object class, attribute type, or matching rule.
     * @return The name of the object class, attribute type, or
     * matching rule.
     */
    public String getName() {
        return name;
    }

    /**
     * Gets the object ID (OID) of the object class, attribute type,
     * or matching rule in dotted-string format (for example, "1.2.3.4").
     * @return The OID of the object class, attribute type,
     * or matching rule.
     */
    public String getOID() {
        return oid;
    }

    /**
     * Get the description of the object class, attribute type,
     * or matching rule.
     * @return The description of the object class, attribute type,
     * or matching rule.
     */
    public String getDescription() {
        return description;
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param atrr Attribute in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute attr,
                           String dn )
                           throws LDAPException {
        LDAPAttribute[] attrs = new LDAPAttribute[1];
        attrs[0] = attr;
        update( ld, op, attrs, dn );
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param attrs Attributes in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, LDAPAttribute[] attrs,
                           String dn )
                           throws LDAPException {
        LDAPModificationSet mods = new LDAPModificationSet();
        for( int i = 0; i < attrs.length; i++ ) {
            mods.add( op, attrs[i] );
        }
        String entryName = LDAPSchema.getSchemaDN( ld, dn );
        ld.modify( entryName, mods );
    }

    /**
     * Add, remove or modify the definition from a Directory.
     * @param ld An open connection to a Directory Server. Typically the
     * connection must have been authenticated to add a definition.
     * @param op Type of modification to make.
     * @param name Name of attribute in the schema entry to modify.
     * @exception LDAPException if the definition can't be added/removed.
     */
    protected void update( LDAPConnection ld, int op, String name,
                           String dn )
                           throws LDAPException {
        boolean quotingBug =
            !LDAPSchema.isAttributeSyntaxStandardsCompliant( ld );
        LDAPAttribute attr = new LDAPAttribute( name,
                                                getValue( quotingBug ) );
        update( ld, op, attr, dn );
    }

    /**
     * Adds the current object class, attribute type, or matching rule
     * definition to the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @param dn The entry at which to add the schema definition.
     * @exception LDAPException The specified definition cannot be
     * added to the schema.
     */
    public void add( LDAPConnection ld, String dn ) throws LDAPException {
        update( ld, LDAPModification.ADD, attrName, dn );
    }

    /**
     * Adds the current object class, attribute type, or matching rule
     * definition to the schema at the root DSE. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @exception LDAPException The specified definition cannot be
     * added to the schema.
     */
    public void add( LDAPConnection ld ) throws LDAPException {
        add( ld, "" );
    }

    /**
     * Replace a single value of the object class, attribute type,
     * or matching rule definition in the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @param newValue The new value
     * @param dn The entry at which to modify the schema definition.
     * @exception LDAPException The specified definition cannot be
     * modified.
     */
    public void modify( LDAPConnection ld, LDAPSchemaElement newValue,
                        String dn )
                        throws LDAPException {
        boolean quotingBug =
            !LDAPSchema.isAttributeSyntaxStandardsCompliant( ld );
        LDAPModificationSet mods = new LDAPModificationSet();
        mods.add( LDAPModification.DELETE,
                  new LDAPAttribute( attrName, getValue( quotingBug ) ) );
        mods.add( LDAPModification.ADD,
                  new LDAPAttribute( attrName,
                                     newValue.getValue( quotingBug ) ) );
        String entryName = LDAPSchema.getSchemaDN( ld, dn );
        ld.modify( entryName, mods );
    }

    /**
     * Replace a single value of the object class, attribute type,
     * or matching rule definition in the schema at the root DSE.
     * Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @param newValue The new value
     * @exception LDAPException The specified definition cannot be
     * modified.
     */
    public void modify( LDAPConnection ld, LDAPSchemaElement newValue )
                        throws LDAPException {
        modify( ld, newValue, "" );
    }

    /**
     * Removes the current object class, attribute type, or matching rule
     * definition from the schema. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @param dn The entry at which to remove the schema definition.
     * @exception LDAPException The specified definition cannot be
     * removed from the schema.
     */
    public void remove( LDAPConnection ld, String dn ) throws LDAPException {
        update( ld, LDAPModification.DELETE, attrName, dn );
    }

    /**
     * Removes the current object class, attribute type, or matching rule
     * definition from the schema at the root DSE. Typically, most servers
     * will require you to authenticate before allowing you to
     * edit the schema.
     * @param ld The <CODE>LDAPConnection</CODE> object representing
     * a connection to an LDAP server.
     * @exception LDAPException The specified definition cannot be
     * removed from the schema.
     */
    public void remove( LDAPConnection ld ) throws LDAPException {
        remove( ld, "" );
    }

    /**
     * Report if the element is marked as obsolete.
     * @return <CODE>true<CODE> if the element is defined as obsolete.
     */
    public boolean isObsolete() {
        return obsolete;
    }

    /**
     * Parse a raw schema value into OID, name, description, and
     * a Hashtable of other qualifiers and values
     *
     * @param raw A raw schema definition
     */
    protected void parseValue( String raw ) {
        if ( properties == null ) {
            properties = new Hashtable();
        }
        int l = raw.length();
        // Processing is faster in char array than in String
        char[] ch = new char[l];
        raw.getChars( 0, l, ch, 0 );
        // Trim leading and trailing space
        l--;
        while( ch[l] == ' ' ) {
            l--;
        }
        int start = 0;
        while( ch[start] == ' ' ) {
            start++;
        }
        // Skip past "( " and ")" to start of OID
        start += 2;
        // Find end of OID
        int ind = start + 1;
        while( ch[ind] != ' ' ) {
            ind++;
        }
        oid = new String( ch, start, ind - start );

        ind = ind + 1;
        String s;
        String val;
        while ( ind < l ) {
            // Skip past blanks to start of next token
            while( ch[ind] == ' ' ) {
                ind++;
            }
            // Find end of token
            int last = ind + 1;
            while( (last < l) && (ch[last] != ' ') )
                last++;
            if ( last < l ) {
                // Found a token
                s = new String( ch, ind, last-ind );
                ind = last;
				if ( novalsTable.containsKey( s ) ) {
					properties.put( s, "" );
					continue;
				}
            } else {
                // Reached end of string with no end of token
                s = "";
                ind = l;
                break;
            }

            // Find the start of the value of the token
            while( (ind < l) && (ch[ind] == ' ') ) {
                ind++;
            }
            last = ind + 1;
            if ( ind >= l ) {
                break;
            }

            boolean quoted = false;
            boolean list = false;
            if ( ch[ind] == '\'' ) {
                // The value is quoted
                quoted = true;
                ind++;
                while( (last < l) && (ch[last] != '\'') ) {
                    last++;
                }
			} else if ( ch[ind] == '(' ) {
                // The value is a list
                list = true;
                ind++;
                while( (last < l) && (ch[last] != ')') ) {
                    last++;
                }
            } else {
                // The value is not quoted
                while( (last < l) && (ch[last] != ' ') ) {
                    last++;
                }
            }
            if ( (ind < last) && (last < l) ) {
				if ( list ) {
					Vector v = new Vector();
                    val = new String( ch, ind+1, last-ind-1 );
                    StringTokenizer st = new StringTokenizer( val, " " );
                    while ( st.hasMoreTokens() ) {
                        String tok = st.nextToken();
                        if ( !tok.equals( "$" ) ) {
							// Remove quotes, if any
							if ( tok.charAt( 0 ) == '\'' ) {
								tok = tok.substring( 1, tok.length()-1 );
							}
                            v.addElement( tok );
                        }
                    }
					properties.put( s, v );
				} else {
					val = new String( ch, ind, last-ind );
					if ( s.equals( "NAME" ) ) {
						name = val;
					} else if ( s.equals( "DESC" ) ) {
						description = val;
					} else {
						properties.put( s, val );
					}
					if ( quoted ) {
						last++;
					}
                }
            }
            ind = last + 1;
        }
    }

    /**
     * Format a String in the format defined in X.501 (see
     * <A HREF="http://ds.internic.net/rfc/rfc2252.txt"
     * >RFC 2252, Lightweight Directory Access Protocol
     * (v3): Attribute Syntax Definitions</A>
     * for a description of this format).
     * This is the format that LDAP servers and clients use to exchange
     * schema information. For example, when
     * you search an LDAP server for its schema, the server returns an entry
     * with the attributes "objectclasses" and "attributetypes".  The
     * values of the "attributetypes" attribute are attribute type
     * descriptions in this format.
     * <P>
     * @param quotingBug <CODE>true</CODE> if single quotes are to be
     * supplied around the SYNTAX and SUP value
     * @return A formatted String for defining a schema element
     */
    public String getValue() {
        return getValue( false );
    }

    abstract String getValue( boolean quotingBug );

    /**
     * Prepare the initial common part of a schema element value in
     * RFC 2252 format for submitting to a server
     *
     * @return The OID, name, description, and possibly OBSOLETE
     * fields of a schema element definition
     */
	String getValuePrefix() {
        String s = "( " + oid + " NAME \'" + name + "\' DESC \'" +
            description + "\' ";
        if ( isObsolete() ) {
            s += OBSOLETE + ' ';
        }
        return s;
    }

    /**
     * Get qualifiers which may or may not be present
     *
     * @param names List of qualifiers to look up
     * @return String in RFC 2252 format containing any values
     * found, not terminated with ' '
     */
    protected String getOptionalValues( String[] names ) {
        String s = "";
        for( int i = 0; i < names.length; i++ ) {
            String[] vals = getQualifier( names[i] );
            if ( (vals != null) && (vals.length > 0) ) {
                s += names[i] + ' ' + vals[0];
            }
        }
        return s;
    }

    /**
     * Get any qualifiers marked as custom
     *
     * @return String in RFC 2252 format, without a terminating
     * ' '
     */
    protected String getCustomValues() {
        String s = "";
		Enumeration en = properties.keys();
		while( en.hasMoreElements() ) {
			String key = (String)en.nextElement();
			if ( !key.startsWith( "X-" ) ) {
                continue;
            }
			s += getValue( key, true );
		}
		return s;
	}

    /**
     * Get a qualifier's value or values, if present, and format
     * the String according to RFC 2252
     *
     * @return String in RFC 2252 format, without a terminating
     * ' '
     */
    String getValue( String key, boolean doQuote ) {
        String s = "";
        Object o = properties.get( key );
        if ( o == null ) {
            return s;
        }
        if ( o instanceof String ) {
            if ( ((String)o).length() > 0 ) {
                s += key + ' ';
                if ( doQuote ) {
                    s += '\'';
                }
                s += (String)o;
                if ( doQuote ) {
                    s += '\'';
                }
            }
        } else {
            s += key + " ( ";
            Vector v = (Vector)o;
            for( int i = 0; i < v.size(); i++ ) {
                if ( doQuote ) {
                    s += '\'';
                }
                s += (String)v.elementAt(i);
                if ( doQuote ) {
                    s += '\'';
                }
                s += ' ';
                if ( i < (v.size() - 1) ) {
                    s += "$ ";
                }
            }
            s += ')';
        }
		return s;
	}

    /**
     * Keep track of qualifiers which are not predefined.
     * @param name Name of qualifier
     * @param value Value of qualifier. "" for no value
     */
    public void setQualifier( String name, String value ) {
        if ( properties == null ) {
            properties = new Hashtable();
        }
        if ( value != null ) {
            properties.put( name, value );
        } else {
            properties.remove( name );
        }
    }

    /**
     * Keep track of qualifiers which are not predefined.
     * @param name Name of qualifier
     * @param values Array of values
     */
    public void setQualifier( String name, String[] values ) {
        if ( values == null ) {
            return;
        }
        if ( properties == null ) {
            properties = new Hashtable();
        }
        Vector v = new Vector();
        for( int i = 0; i < v.size(); i++ ) {
            v.addElement( values[i] );
        }
        properties.put( name, v );
    }

    /**
     * Get the value of a qualifier which is not predefined.
     * @param name Name of qualifier
     * @return Value or values of qualifier. <CODE>null</CODE> if not
     * present, a zero-length array if present but with no value
     */
    public String[] getQualifier( String name ) {
        if ( properties == null ) {
            return null;
        }
        Object o = properties.get( name );
        if ( o == null ) {
            return null;
        }
        if ( o instanceof Vector ) {
            Vector v = (Vector)o;
            String[] vals = new String[v.size()];
            v.copyInto( vals );
            return vals;
        }
        String s = (String)o;
        if ( s.length() < 1 ) {
            return new String[0];
        } else {
            return new String[] { s };
        }
    }

    /**
     * Get an enumeration of all qualifiers which are not predefined.
     * @return Enumeration of qualifiers.
     */
    public Enumeration getQualifierNames() {
        return properties.keys();
    }

    /**
     * Create a string for use in toString with any qualifiers of the element.
     *
     * @param ignore Any qualifiers to NOT include
     * @return A String with any known qualifiers
     */
    String getQualifierString( String[] ignore ) {
        Hashtable toIgnore = null;
        if ( ignore != null ) {
            toIgnore = new Hashtable();
            for( int i = 0; i < ignore.length; i++ ) {
                toIgnore.put( ignore[i], ignore[i] );
            }
        }
        String s = "";
        Enumeration en = getQualifierNames();
        while( en.hasMoreElements() ) {
            String qualifier = (String)en.nextElement();
            if ( (toIgnore != null) && toIgnore.containsKey( qualifier ) ) {
                continue;
            }
            s += "; " + qualifier;
            String[] vals = getQualifier( qualifier );
            if ( vals == null ) {
                s += ' ';
                continue;
            }
            s += ": ";
            for( int i = 0; i < vals.length; i++ ) {
                s += vals[i] + ' ';
            }
        }
        return s;
    }

    // Constants for known syntax types
    public static final int unknown = 0;
    public static final int cis = 1;
    public static final int binary = 2;
    public static final int telephone = 3;
    public static final int ces = 4;
    public static final int dn = 5;
    public static final int integer = 6;

    protected static final String cisString =
                                      "1.3.6.1.4.1.1466.115.121.1.15";
    protected static final String binaryString =
                                      "1.3.6.1.4.1.1466.115.121.1.5";
    protected static final String telephoneString =
                                      "1.3.6.1.4.1.1466.115.121.1.50";
    protected static final String cesString =
                                      "1.3.6.1.4.1.1466.115.121.1.26";
    protected static final String intString =
                                      "1.3.6.1.4.1.1466.115.121.1.27";
    protected static final String dnString =
                                      "1.3.6.1.4.1.1466.115.121.1.12";
	// Predefined qualifiers which apply to any schema element type
	public static final String OBSOLETE = "OBSOLETE";
	public static final String SUPERIOR = "SUP";

    // Properties which are common to all schema elements
	protected String oid = null;
    protected String name = "";
    protected String description = "";
    protected String attrName = null;
    protected boolean obsolete = false;
    protected String rawValue = null;
    // Additional qualifiers
    protected Hashtable properties = null;
    // Qualifiers known to not have values
	static protected Hashtable novalsTable = new Hashtable();
}
