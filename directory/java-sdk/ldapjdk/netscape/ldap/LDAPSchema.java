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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
package netscape.ldap;

import java.util.*;

/**
 * This object represents the schema of an LDAP v3 server.
 * You can use the <CODE>fetchSchema</CODE> method to retrieve
 * the schema used by a server. (The server must support LDAP v3
 * and the capability to retrieve the schema over the LDAP protocol.)
 * <P>
 *
 * After you retrieve the schema, you can use this object to get
 * the object class, attribute type, and matching rule descriptions
 * in the schema.  You can also add your own object classes,
 * attribute types, and matching rules to the schema.
 * <P>
 *
 * To remove any object classes, attribute types, and matching rules
 * that you added, call the <CODE>remove</CODE> methods of the
 * <CODE>LDAPObjectClassSchema</CODE>, <CODE>LDAPAttributeSchema</CODE>,
 * and <CODE>LDAPMatchingRuleSchema</CODE> classes.  (This method is
 * inherited from the <CODE>LDAPSchemaElement</CODE> class.)
 * <P>
 *
 * The following class is an example of an LDAP client that can
 * fetch the schema, get and print object class descriptions and
 * attribute type descriptions, and add object classes and attribute
 * types to the schema over the LDAP protocol.
 * <P>
 *
 * <PRE>
 * import netscape.ldap.*;
 * public class TestSchema {
 *     public static void main( String[] args ) {
 *         String HOSTNAME = "ldap.netscape.com";
 *         int PORT_NUMBER = DEFAULT_PORT;
 *         String ROOT_DN = "cn=Directory Manager";
 *         String ROOT_PASSWORD = "23skidoo";
 *
 *         LDAPConnection ld = new LDAPConnection();
 *
 *         // Construct a new <CODE>LDAPSchema</CODE> object to get the schema.
 *         LDAPSchema dirSchema = new LDAPSchema();
 *
 *         try {
 *             // Connect to the server.
 *             ld.connect( HOSTNAME, PORT_NUMBER );
 *
 *             // Get the schema from the directory.
 *             dirSchema.fetchSchema( ld );
 *
 *             // Get and print the inetOrgPerson object class description.
 *             LDAPObjectClassSchema objClass = dirSchema.getObjectClass(
 *                 "inetOrgPerson" );
 *             if ( objClass != null ) {
 *                 System.out.println("inetOrgPerson := "+objClass.toString());
 *             }
 *
 *             // Get and print the definition of the userPassword attribute.
 *             LDAPAttributeSchema attrType = dirSchema.getAttribute(
 *                 "userpassword" );
 *             if ( attrType != null ) {
 *                 System.out.println("userPassword := "+attrType.toString());
 *             }
 *
 *             // Add a new object class.
 *             String[] requiredAttrs = {"cn", "mail"};
 *             String[] optionalAttrs = {"sn", "phoneNumber"};
 *             LDAPObjectClassSchema newObjClass = new LDAPObjectClassSchema(
 *                 "newInetOrgPerson", "1.2.3.4.5.6.7", "top", "Experiment",
 *                 requiredAttrs, optionalAttrs );
 *
 *             // Authenticate as root DN to get permissions to edit the schema.
 *             ld.authenticate( ROOT_DN, ROOT_PASSWORD );
 *
 *             // Add the new object class to the schema.
 *             newObjClass.add( ld );
 *
 *             // Create a new attribute type "hairColor".
 *             LDAPAttributeSchema newAttrType = new LDAPAttributeSchema(
 *                 "hairColor", "1.2.3.4.5.4.3.2.1", "Blonde, red, etc",
 *                 LDAPAttributeSchema.cis, false );
 *
 *             // Add the new attribute type to the schema.
 *             newAttrType.add( ld );
 *
 *             // Fetch the schema again to verify that changes were made.
 *             dirSchema.fetchSchema( ld );
 *
 *             // Get and print the new attribute type.
 *             newAttrType = dirSchema.getAttribute( "hairColor" );
 *             if ( newAttrType != null )
 *                 System.out.println("hairColor := "+newAttrType.toString());
 *
 *             // Get and print the new object class.
 *             newObjClass = dirSchema.getObjectClass( "newInetOrgPerson" );
 *             if ( newObjClass != null )
 *                 System.out.println("newInetOrgPerson := "+newObjClass.toString());
 *
 *             ld.disconnect();
 *
 *         } catch ( Exception e ) {
 *             System.err.println( e.toString() );
 *             System.exit( 1 );
 *         }
 *
 *         System.exit( 0 );
 *     }
 * }
 * </PRE>
 *
 * If you are using the Netscape Directory Server 3.0, you can also
 * verify that the class and attribute type have been added through
 * the directory server manager (go to Schema | Edit or View Attributes
 * or Schema | Edit or View Object Classes).
 * <P>
 *
 * To remove the classes and attribute types added by the example,
 * see the examples under the <CODE>LDAPSchemaElement</CODE> class.
 * <P>
 *
 * @see netscape.ldap.LDAPAttributeSchema
 * @see netscape.ldap.LDAPObjectClassSchema
 * @see netscape.ldap.LDAPMatchingRuleSchema
 * @see netscape.ldap.LDAPSchemaElement
 * @version 1.0
 * @author Rob Weltman
 **/
public class LDAPSchema {
    /**
     * Constructs a new <CODE>LDAPSchema</CODE> object.
     * Once you construct the object, you can get
     * the schema by calling <CODE>fetchSchema</CODE>.
     * <P>
     *
     * You can also print out the schema by using the
     * <CODE>main</CODE> method. For example, you can enter
     * the following command:
     * <PRE>
     * java netscape.ldap.LDAPSchema myhost.mydomain.com 389
     * </PRE>
     *
     * Note that you need to call <CODE>fetchSchema</CODE>
     * to get the schema from the server.  Constructing the
     * object does not fetch the schema.
     * <P>
     *
     * @see netscape.ldap.LDAPSchema#fetchSchema
     * @see netscape.ldap.LDAPSchema#main
     */
    public LDAPSchema() {
    }

    /**
     * Adds an object class schema definition to the current schema.
     * You can also add object class schema definitions by calling the
     * <CODE>add</CODE> method of your newly constructed
     * <CODE>LDAPObjectClassSchema</CODE> object.
     * <P>
     *
     * To remove an object class schema definition that you have added,
     * call the <CODE>getObjectClass</CODE> method to get the
     * <CODE>LDAPObjectClassSchema</CODE> object representing your
     * object class and call the <CODE>remove</CODE> method.
     * <P>
     *
     * <B>NOTE: </B>For information on the <CODE>add</CODE> and
     * <CODE>remove</CODE> methods of <CODE>LDAPObjectClassSchema</CODE>,
     * see the documentation for <CODE>LDAPSchemaElement</CODE>.
     * (These methods are inherited from <CODE>LDAPSchemaElement</CODE>.)
     * <P>
     *
     * @param objectSchema <CODE>LDAPObjectClassSchema</CODE> object
     * representing the object class schema definition that you want
     * to add.
     * @see netscape.ldap.LDAPObjectClassSchema
     * @see netscape.ldap.LDAPSchemaElement#add
     * @see netscape.ldap.LDAPSchemaElement#remove
     */
    public void addObjectClass( LDAPObjectClassSchema objectSchema ) {
        objectClasses.put( objectSchema.getName().toLowerCase(),
                           objectSchema );
    }

    /**
     * Add an attribute type schema definition to the current schema.
     * You can also add attribute type schema definitions by calling the
     * <CODE>add</CODE> method of your newly constructed
     * <CODE>LDAPAttributeSchema</CODE> object.
     * <P>
     *
     * To remove an attribute type schema definition that you have added,
     * call the <CODE>getAttribute</CODE> method to get the
     * <CODE>LDAPAttributeSchema</CODE> object representing your
     * attribute type and call the <CODE>remove</CODE> method.
     * <P>
     *
     * <B>NOTE: </B>For information on the <CODE>add</CODE> and
     * <CODE>remove</CODE> methods of <CODE>LDAPAttributeSchema</CODE>,
     * see the documentation for <CODE>LDAPSchemaElement</CODE>.
     * (These methods are inherited from <CODE>LDAPSchemaElement</CODE>.)
     * <P>
     *
     * @param attrSchema <CODE>LDAPAttributeSchema</CODE> object
     * representing the attribute type schema definition that you
     * want to add.
     * @see netscape.ldap.LDAPAttributeSchema
     * @see netscape.ldap.LDAPSchemaElement#add
     * @see netscape.ldap.LDAPSchemaElement#remove
     */
    public void addAttribute( LDAPAttributeSchema attrSchema ) {
        attributes.put( attrSchema.getName().toLowerCase(), attrSchema );
    }

    /**
     * Add a matching rule schema definition to the current schema.
     * You can also add matching rule schema definitions by calling the
     * <CODE>add</CODE> method of your newly constructed
     * <CODE>LDAPMatchingRuleSchema</CODE> object.
     * <P>
     *
     * To remove an attribute type schema definition that you have added,
     * call the <CODE>getMatchingRule</CODE> method to get the
     * <CODE>LDAPMatchingRuleSchema</CODE> object representing your
     * matching rule and call the <CODE>remove</CODE> method.
     * <P>
     *
     * <B>NOTE: </B>For information on the <CODE>add</CODE> and
     * <CODE>remove</CODE> methods of <CODE>LDAPMatchingRuleSchema</CODE>,
     * see the documentation for <CODE>LDAPSchemaElement</CODE>.
     * (These methods are inherited from <CODE>LDAPSchemaElement</CODE>.)
     * <P>
     *
     * @param matchSchema <CODE>LDAPMatchingRuleSchema</CODE> object
     * representing the matching rule schema definition that you want
     * to add.
     * @see netscape.ldap.LDAPMatchingRuleSchema
     * @see netscape.ldap.LDAPSchemaElement#add
     * @see netscape.ldap.LDAPSchemaElement#remove
     */
    public void addMatchingRule( LDAPMatchingRuleSchema matchSchema ) {
        matchingRules.put( matchSchema.getName().toLowerCase(), matchSchema );
    }

    /**
     * Gets an enumeration ofthe object class definitions in this schema.
     * @return An enumeration ofobject class definitions.
     */
    public Enumeration getObjectClasses() {
        return objectClasses.elements();
    }

    /**
     * Gets an enumeration ofthe attribute type definitions in this schema.
     * @return An enumeration ofattribute type definitions.
     */
    public Enumeration getAttributes() {
        return attributes.elements();
    }

    /**
     * Gets an enumeration ofthe matching rule definitions in this schema.
     * @return An enumeration ofmatching rule definitions.
     */
    public Enumeration getMatchingRules() {
        return matchingRules.elements();
    }

    /**
     * Gets the definition of the object class with the specified name.
     * @param name Name of the object class that you want to find.
     * @return An <CODE>LDAPObjectClassSchema</CODE> object representing
     * the object class definition, or <CODE>null</CODE> if not found.
     */
    public LDAPObjectClassSchema getObjectClass( String name ) {
        return (LDAPObjectClassSchema)objectClasses.get( name.toLowerCase() );
    }

    /**
     * Gets the definition of the attribute type with the specified name.
     * @param name Name of the attribute type that you want to find.
     * @return An <CODE>LDAPAttributeSchema</CODE> object representing
     * the attribute type definition, or <CODE>null</CODE> if not found.
     */
    public LDAPAttributeSchema getAttribute( String name ) {
        return (LDAPAttributeSchema)attributes.get( name.toLowerCase() );
    }

    /**
     * Gets the definition of a matching rule with the specified name.
     * @param name Name of the matching rule that you want to find.
     * @return An <CODE>LDAPMatchingRuleSchema</CODE> object representing
     * the matching rule definition, or <CODE>null</CODE> if not found.
     */
    public LDAPMatchingRuleSchema getMatchingRule( String name ) {
        return (LDAPMatchingRuleSchema)matchingRules.get( name.toLowerCase() );
    }

    /**
     * Get an enumeration of the names of the object classes in this schema.
     * @return An enumeration of object class names (all lower-case).
     */
    public Enumeration getObjectClassNames() {
        return objectClasses.keys();
    }

    /**
     * Get an enumeration of the names of the attribute types in this schema.
     * @return An enumeration of attribute names (all lower-case).
     */
    public Enumeration getAttributeNames() {
        return attributes.keys();
    }

    /**
     * Get an enumeration of the names of the matching rules in this schema.
     * @return An enumeration of matching rule names (all lower-case).
     */
    public Enumeration getMatchingRuleNames() {
        return matchingRules.keys();
    }

    /**
     * Retrieve the entire schema from a Directory Server.
     * @param ld An active connection to a Directory Server.
     * @exception LDAPException on failure.
     */
    public void fetchSchema( LDAPConnection ld ) throws LDAPException {
        if ( (ld == null) || !ld.isConnected() ) {
            throw new LDAPException( "No connection", LDAPException.OTHER );
        }
        LDAPEntry entry = ld.read( "" );
        if ( entry == null )
            throw new LDAPException( "", LDAPException.NO_SUCH_OBJECT );
        LDAPAttribute attr = entry.getAttribute( "subschemasubentry" );
        entryName = "cn=schema";
        Enumeration en;
        if ( attr != null ) {
            en = attr.getStringValues();
            if ( en.hasMoreElements() )
                entryName = (String)en.nextElement();
        }
        /* Get the entire schema definition entry */
        entry = readSchema( ld, entryName );
        /* Get all object class definitions */
        attr = entry.getAttribute( "objectclasses" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPObjectClassSchema sch =
                    new LDAPObjectClassSchema( (String)en.nextElement() );
                addObjectClass( sch );
            }
        }

        /* Get all attribute definitions */
        attr = entry.getAttribute( "attributetypes" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPAttributeSchema sch =
                    new LDAPAttributeSchema( (String)en.nextElement() );
                addAttribute( sch );
            }
        }

        /* Matching rules are tricky, because we have to match up a
           rule with its use. First get all the uses. */
        Hashtable h = new Hashtable();
        attr = entry.getAttribute( "matchingruleuse" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                String use = (String)en.nextElement();
                LDAPMatchingRuleSchema sch =
                    new LDAPMatchingRuleSchema( null, use );
                h.put( sch.getOID(), use );
            }
        }
        /* Now get the rules, and assign uses to them */
        attr = entry.getAttribute( "matchingrules" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                String raw = (String)en.nextElement();
                LDAPMatchingRuleSchema sch =
                    new LDAPMatchingRuleSchema( raw, null );
                String use = (String)h.get( sch.getOID() );
                if ( use != null )
                    sch = new LDAPMatchingRuleSchema( raw, use );
                addMatchingRule( sch );
            }
        }
    }

    /**
     * Displays the schema (including the descriptions of its object
     * classes, attribute types, and matching rules) in an easily
     * readable format (not the same as the format expected by
     * an LDAP server).
     * @return A string containing the schema in printable format.
     */
    public String toString() {
        String s = "Object classes: ";
        Enumeration en = getObjectClasses();
        while( en.hasMoreElements() ) {
            s += (String)en.nextElement();
            s += ' ';
        }
        s += "Attributes: ";
        en = getAttributes();
        while( en.hasMoreElements() ) {
            s += (String)en.nextElement();
            s += ' ';
        }
        s += "Matching rules: ";
        en = getMatchingRules();
        while( en.hasMoreElements() ) {
            s += (String)en.nextElement();
            s += ' ';
        }
        return s;
    }

    private LDAPEntry readSchema( LDAPConnection ld, String dn ) throws LDAPException {
        LDAPSearchResults results = ld.search (dn, ld.SCOPE_BASE,
                                               "objectclass=subschema",
                                               null, false);
        return results.next ();
    }

    /**
     * Helper for "main" to print out schema elements.
     * @param en Enumeration of schema elements.
     */
    private static void printEnum( Enumeration en ) {
        while( en.hasMoreElements() ) {
            LDAPSchemaElement s = (LDAPSchemaElement)en.nextElement();
            System.out.println( "  " + s );
        }
    }

    /**
     * Fetch the schema from the LDAP server at the specified
     * host and port, and print out the schema (including descriptions
     * of its object classes, attribute types, and matching rules).
     * The schema is printed in an easily readable format (not the
     * same as the format expected by an LDAP server).  For example,
     * you can enter the following command to print the schema:
     * <PRE>
     * java netscape.ldap.LDAPSchema myhost.mydomain.com 389
     * </PRE>
     *
     * @param args The host name and the port number of the LDAP server
     * (for example, <CODE>netscape.ldap.LDAPSchema directory.netscape.com
     * 389</CODE>).
     */
    public static void main( String[] args ) {
        if ( args.length < 2 ) {
            System.err.println( "Usage: netscape.ldap.LDAPSchema HOST PORT" );
            System.exit(1 );
        }
        int port = Integer.parseInt( args[1] );
        LDAPConnection ld = new LDAPConnection();
        try {
            ld.connect( args[0], port );
            LDAPSchema schema = new LDAPSchema();
            schema.fetchSchema( ld );
            ld.disconnect();
            System.out.println( "Object classes: " );
            printEnum( schema.getObjectClasses() );
            System.out.println( "\nAttributes: " );
            printEnum( schema.getAttributes() );
            System.out.println( "\nMatching rules: " );
            printEnum( schema.getMatchingRules() );
            System.exit( 0 );
        } catch ( LDAPException e ) {
            System.err.println( e );
        }
    }

    private Hashtable objectClasses = new Hashtable();
    private Hashtable attributes = new Hashtable();
    private Hashtable matchingRules = new Hashtable();
    String entryName = null;
}
