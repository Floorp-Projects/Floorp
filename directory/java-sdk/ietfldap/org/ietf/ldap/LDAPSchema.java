/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
package org.ietf.ldap;

import java.io.Serializable;
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
 * import org.ietf.ldap.*;
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
 *                 System.out.println("userPassword := " + attrType.toString());
 *             }
 *
 *             // Create a new object class definition.
 *             String[] requiredAttrs = {"cn", "mail"};
 *             String[] optionalAttrs = {"sn", "phoneNumber"};
 *             LDAPObjectClassSchema newObjClass =
 *                     new LDAPObjectClassSchema( "newInetOrgPerson",
 *                                                "1.2.3.4.5.6.7",
 *                                                "top",
 *                                                "Experiment",
 *                                                requiredAttrs,
 *                                                optionalAttrs );
 *
 *             // Authenticate as root DN to get permissions to edit the schema.
 *             ld.authenticate( ROOT_DN, ROOT_PASSWORD );
 *
 *             // Add the new object class to the schema.
 *             newObjClass.add( ld );
 *
 *             // Create a new attribute type "hairColor".
 *             LDAPAttributeSchema newAttrType =
 *                     new LDAPAttributeSchema( "hairColor",
 *                                              "1.2.3.4.5.4.3.2.1",
 *                                              "Blonde, red, etc",
 *                                              LDAPAttributeSchema.cis,
 *                                              false );
 *             // Add a custom qualifier
 *             newObjClass.setQualifier( "X-OWNER", "John Jacobson" );
 *
 *             // Add the new attribute type to the schema.
 *             newAttrType.add( ld );
 *
 *             // Fetch the schema again to verify that changes were made.
 *             dirSchema.fetchSchema( ld );
 *
 *             // Get and print the new attribute type.
 *             newAttrType = dirSchema.getAttribute( "hairColor" );
 *             if ( newAttrType != null ) {
 *                 System.out.println("hairColor := " + newAttrType.toString());
 *             }
 *
 *             // Get and print the new object class.
 *             newObjClass = dirSchema.getObjectClass( "newInetOrgPerson" );
 *             if ( newObjClass != null ) {
 *                 System.out.println("newInetOrgPerson := " +newObjClass.toString());
 *             }
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
 * @see org.ietf.ldap.LDAPAttributeSchema
 * @see org.ietf.ldap.LDAPObjectClassSchema
 * @see org.ietf.ldap.LDAPMatchingRuleSchema
 * @see org.ietf.ldap.LDAPSchemaElement
 * @version 1.0
 * @author Rob Weltman
 **/
public class LDAPSchema implements Serializable {

    static final long serialVersionUID = -3911737419783579398L;

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
     * java org.ietf.ldap.LDAPSchema myhost.mydomain.com 389
     * </PRE>
     *
     * Note that you need to call <CODE>fetchSchema</CODE>
     * to get the schema from the server.  Constructing the
     * object does not fetch the schema.
     * <P>
     *
     * @see org.ietf.ldap.LDAPSchema#fetchSchema
     * @see org.ietf.ldap.LDAPSchema#main
     */
    public LDAPSchema() {
    }

    public LDAPSchema( LDAPEntry entry ) {
        initialize( entry );
    }

    /**
     * Adds a schema element definition to the schema object. If the schema 
     * object already contains an element with the same class and OID as el, 
     * IllegalArgumentException is thrown. Changes are applied to a directory
     * by calling saveSchema.
     *
     * @param el A non-null schema definition
     */
    public void add( LDAPSchemaElement el ) {
        add( el, true );
    }

    /**
     * Adds a schema element definition to the schema object. If the schema 
     * object already contains an element with the same class and OID as el, 
     * IllegalArgumentException is thrown. Changes are applied to a directory
     * by calling saveSchema.
     *
     * @param el A non-null schema definition
     * @param isChange <CODE>true</CODE> if called by a client, meaning that
     * a directory is to be changed and not just this object
     */
    private void add( LDAPSchemaElement el,
                      boolean isChange ) {
        String ID = el.getID();
        if ( ID != null ) {
            LDAPSchemaElement oldEl = (LDAPSchemaElement)_IDToElement.get( ID );
            if ( (oldEl != null) && namesTheSame( oldEl, el ) ) {
                throw new IllegalArgumentException( "Schema element already " +
                                                    "defined for " + ID +
                                                    "; old: " +
                                                    oldEl +
                                                    " ; new: " + el );
            }
            _IDToElement.put( ID, el );
        }
        if ( isChange ) {
            _changeList.add( getModificationForElement( el,
                                                        LDAPModification.ADD,
                                                        null ) );
        }
        String name;
        try {
            name = (el.getNames().length > 0) ?
                el.getNames()[0].toLowerCase() : "";
        } catch ( Exception e ) {
            System.err.println( el );
            e.printStackTrace();
            return;
        }
        if ( el instanceof LDAPAttributeSchema ) {
            _attributes.put( name, el );
        } else if ( el instanceof LDAPObjectClassSchema ) {
            _objectClasses.put( name, el );
        } else if ( el instanceof LDAPMatchingRuleSchema ) {
            _matchingRules.put( name, el );
        } else if ( el instanceof LDAPMatchingRuleUseSchema ) {
            _matchingRuleUses.put( name, el );
        } else if ( el instanceof LDAPNameFormSchema ) {
            _nameForms.put( name, el );
        } else if ( el instanceof LDAPSyntaxSchema ) {
            if ( name.length() < 1 ) {
                name = el.getID();
            }
            _syntaxes.put( name, el );
        } else if ( el instanceof LDAPDITContentRuleSchema ) {
            _contentRules.put( name, el );
        } else if ( el instanceof LDAPDITStructureRuleSchema ) {
            _structureRulesByName.put( name, el );
            _structureRulesById.put( new Integer(
                ((LDAPDITStructureRuleSchema)el).getRuleID() ), el );
        }
    }

    /**
     * Retrieve the entire schema from the root of a Directory Server
     *
     * @param ld an active connection to a Directory Server
     * @exception LDAPException on failure.
     */
    public void fetchSchema( LDAPConnection ld ) throws LDAPException {
        fetchSchema( ld, "" );
    }

    /**
     * Retrieve the schema for a specific entry
     *
     * @param ld an active connection to a Directory Server
     * @param dn the entry for which to fetch schema
     * @exception LDAPException on failure.
     */
    public void fetchSchema( LDAPConnection ld, String dn )
        throws LDAPException {
        clear();
        /* Find the subschemasubentry value for this DN */
        String entryName = getSchemaDN( ld, dn );

        /* Get the entire schema definition entry */
        LDAPEntry entry = readSchema( ld, entryName );
        initialize( entry );
    }

    /**
     * Get an enumeration of the names of the attribute types in this schema.
     * @return an enumeration of attribute names (all lower-case).
     */
    public Enumeration getAttributeNames() {
        return _attributes.keys();
    }

    /**
     * Gets the definition of the attribute type with the specified name.
     * @param name name of the attribute type to find
     * @return an <CODE>LDAPAttributeSchema</CODE> object representing
     * the attribute type definition, or <CODE>null</CODE> if not found.
     */
    public LDAPAttributeSchema getAttributeSchema( String name ) {
        return (LDAPAttributeSchema)_attributes.get( name.toLowerCase() );
    }

    /**
     * Gets an enumeration of the attribute type definitions in this schema.
     * @return an enumeration of attribute type definitions.
     */
    public Enumeration getAttributeSchemas() {
        return _attributes.elements();
    }

    /**
     * Get an enumeration of the names of the content rules in this schema.
     * @return an enumeration of names of the content rule objects
     */
    public Enumeration getDITContentRuleNames() {
        return _contentRules.keys();
    }

    /**
     * Gets the definition of a content rule with the specified name.
     * @param name name of the rule to find
     * @return an <CODE>LDAPDITContentRuleSchema</CODE> object representing
     * the rule, or <CODE>null</CODE> if not found.
     */
    public LDAPDITContentRuleSchema getDITContentRuleSchema( String name ) {
        return (LDAPDITContentRuleSchema)_contentRules.get(
            name.toLowerCase() );
    }

    /**
     * Get an enumeration of the content rules in this schema.
     * @return an enumeration of content rule objects
     */
    public Enumeration getDITContentRuleSchemas() {
        return _contentRules.elements();
    }

    /**
     * Get an enumeration of the names of the structure rules in this schema.
     * @return an enumeration of names of the structure rule objects
     */
    public Enumeration getDITStructureRuleNames() {
        return _structureRulesByName.keys();
    }

    /**
     * Gets the definition of a structure rule with the specified name.
     * @param name name of the rule to find
     * @return an <CODE>LDAPDITStructureRuleSchema</CODE> object representing
     * the rule, or <CODE>null</CODE> if not found.
     */
    public LDAPDITStructureRuleSchema getDITStructureRuleSchema( String name ) {
        return (LDAPDITStructureRuleSchema)_structureRulesByName.get(
            name.toLowerCase() );
    }

    /**
     * Gets the definition of a structure rule with the specified name.
     * @param ID ID of the rule to find
     * @return an <CODE>LDAPDITStructureRuleSchema</CODE> object representing
     * the rule, or <CODE>null</CODE> if not found.
     */
    public LDAPDITStructureRuleSchema getDITStructureRuleSchema( int ID ) {
        return (LDAPDITStructureRuleSchema)_structureRulesById.get(
            new Integer( ID ) );
    }

    /**
     * Get an enumeration of the structure rules in this schema.
     * @return an enumeration of structure rule objects
     */
    public Enumeration getDITStructureRuleSchemas() {
        return _structureRulesByName.elements();
    }

    /**
     * Get an enumeration of the names of the matching rules in this schema.
     * @return an enumeration of matching rule names (all lower-case).
     */
    public Enumeration getMatchingRuleNames() {
        return _matchingRules.keys();
    }

    /**
     * Gets the definition of a matching rule with the specified name.
     * @param name name of the matching rule to find
     * @return an <CODE>LDAPMatchingRuleSchema</CODE> object representing
     * the matching rule definition, or <CODE>null</CODE> if not found.
     */
    public LDAPMatchingRuleSchema getMatchingRuleSchema( String name ) {
        return (LDAPMatchingRuleSchema)_matchingRules.get( name.toLowerCase() );
    }

    /**
     * Gets an enumeration ofthe matching rule definitions in this schema.
     * @return an enumeration of matching rule definitions.
     */
    public Enumeration getMatchingRuleSchemas() {
        return _matchingRules.elements();
    }

    /**
     * Get an enumeration of the names of the matching rule uses in this schema.
     * @return an enumeration of matching rule use names (all lower-case).
     */
    public Enumeration getMatchingRuleUseNames() {
        return _matchingRuleUses.keys();
    }

    /**
     * Gets the definition of a matching rule use with the specified name.
     * @param name name of the matching rule use to find
     * @return an <CODE>LDAPMatchingRuleUseSchema</CODE> object representing
     * the matching rule definition, or <CODE>null</CODE> if not found.
     */
    public LDAPMatchingRuleUseSchema getMatchingRuleUseSchema( String name ) {
        return (LDAPMatchingRuleUseSchema)_matchingRuleUses.get( name.toLowerCase() );
    }

    /**
     * Gets an enumeration ofthe matching rule use definitions in this schema.
     * @return an enumeration of matching rule use definitions.
     */
    public Enumeration getMatchingRuleUseSchemas() {
        return _matchingRuleUses.elements();
    }

    /**
     * Get an enumeration of the names of the name forms in this schema.
     * @return an enumeration of names of name form objects
     */
    public Enumeration getNameFormNames() {
        return _nameForms.keys();
    }

    /**
     * Gets the definition of a name form with the specified name.
     * @param name name of the name form to find
     * @return an <CODE>LDAPNameFormSchema</CODE> object representing
     * the syntax definition, or <CODE>null</CODE> if not found.
     */
    public LDAPNameFormSchema getNameFormSchema( String name ) {
        return (LDAPNameFormSchema)_nameForms.get( name.toLowerCase() );
    }

    /**
     * Get an enumeration of the name forms in this schema.
     * @return an enumeration of name form objects
     */
    public Enumeration getNameFormSchemas() {
        return _nameForms.elements();
    }

    /**
     * Get an enumeration of the names of the object classes in this schema.
     * @return an enumeration of object class names (all lower-case).
     */
    public Enumeration getObjectClassNames() {
        return _objectClasses.keys();
    }

    /**
     * Gets the definition of the object class with the specified name.
     * @param name name of the object class to find
     * @return an <CODE>LDAPObjectClassSchema</CODE> object representing
     * the object class definition, or <CODE>null</CODE> if not found.
     */
    public LDAPObjectClassSchema getObjectClassSchema( String name ) {
        return (LDAPObjectClassSchema)_objectClasses.get( name.toLowerCase() );
    }

    /**
     * Gets an enumeration ofthe object class definitions in this schema.
     * @return an enumeration of object class definitions.
     */
    public Enumeration getObjectClassSchemas() {
        return _objectClasses.elements();
    }

    /**
     * Get an enumeration of the names of the syntaxes in this schema.
     * @return an enumeration of syntax names (all lower-case).
     */
    public Enumeration getSyntaxNames() {
        return _syntaxes.keys();
    }

    /**
     * Gets the definition of a syntax with the specified name.
     * @param name name of the syntax to find
     * @return an <CODE>LDAPSyntaxSchema</CODE> object representing
     * the syntax definition, or <CODE>null</CODE> if not found.
     */
    public LDAPSyntaxSchema getSyntaxSchema( String name ) {
        return (LDAPSyntaxSchema)_syntaxes.get( name.toLowerCase() );
    }

    /**
     * Get an enumeration of the syntaxes in this schema.
     * @return an enumeration of syntax objects
     */
    public Enumeration getSyntaxSchemas() {
        return _syntaxes.elements();
    }

    /**
     * Modifies a schema element definition of the schema object. If the
     * schema object does not contain an element with the same class and OID
     * as el, it is added. Changes are applied to a directory by calling
     * saveSchema.
     *
     * @param el A non-null schema definition
     */
    public void modify( LDAPSchemaElement el ) {
        modify( el, true );
    }

    /**
     * Modifies a schema element definition of the schema object. If the
     * schema object does not contain an element with the same class and OID
     * as el, it is added. Changes are applied to a directory by calling
     * saveSchema.
     *
     * @param el A non-null schema definition
     * @param isChange <CODE>true</CODE> if called by a client, meaning that
     * a directory is to be changed and not just this object
     */
    private void modify( LDAPSchemaElement el,
                         boolean isChange ) {
        String ID = el.getID();
        if ( isChange ) {
            // Remove if already present and then add
            LDAPSchemaElement oldEl = ( ID != null ) ?
                (LDAPSchemaElement)_IDToElement.get( ID ) : null;
            if ( oldEl != null ) {
                _changeList.add(
                    getModificationForElement( oldEl,
                                               LDAPModification.DELETE,
                                               null ) );
            }
            _changeList.add(
                getModificationForElement( el,
                                           LDAPModification.ADD,
                                           null ) );
        }
        if ( ID != null ) {
            _IDToElement.put( ID, el );
        }
        String name = (el.getNames().length > 0) ?
            el.getNames()[0].toLowerCase() : "";
        if ( el instanceof LDAPAttributeSchema ) {
            _attributes.put( name, el );
        } else if ( el instanceof LDAPObjectClassSchema ) {
            _objectClasses.put( name, el );
        } else if ( el instanceof LDAPMatchingRuleSchema ) {
            _matchingRules.put( name, el );
        } else if ( el instanceof LDAPMatchingRuleUseSchema ) {
            _matchingRuleUses.put( name, el );
        } else if ( el instanceof LDAPNameFormSchema ) {
            _nameForms.put( name, el );
        } else if ( el instanceof LDAPSyntaxSchema ) {
            if ( name.length() < 1 ) {
                name = el.getID();
            }
            _syntaxes.put( name, el );
        } else if ( el instanceof LDAPDITContentRuleSchema ) {
            _contentRules.put( name, el );
        } else if ( el instanceof LDAPDITStructureRuleSchema ) {
            _structureRulesByName.put( name, el );
            _structureRulesById.put( new Integer(
                ((LDAPDITStructureRuleSchema)el).getRuleID() ), el );
        }
    }

    /**
     * Removes a schema element definition of the schema object. If the
     * schema object does not contain an element with the same class and OID
     * as el, this is a noop. Changes are applied to a directory by calling
     * saveSchema.
     *
     * @param el A non-null schema definition
     */
    public void remove( LDAPSchemaElement el ) {
        remove( el, true );
    }

    /**
     * Removes a schema element definition of the schema object. If the
     * schema object does not contain an element with the same class and OID
     * as el, this is a noop. Changes are applied to a directory by calling
     * saveSchema.
     *
     * @param el A non-null schema definition
     * @param isChange <CODE>true</CODE> if called by a client, meaning that
     * a directory is to be changed and not just this object
     */
    private void remove( LDAPSchemaElement el, boolean isChange ) {
        String ID = el.getID();
        if ( isChange ) {
            // Remove it if present
            if ( (ID != null) && _IDToElement.containsKey( ID ) ) {
                _changeList.add(
                    getModificationForElement( el,
                                               LDAPModification.DELETE,
                                               null ) );
            }
        }
        if ( ID != null ) {
            _IDToElement.remove( ID );
        }
        String name = (el.getNames().length > 0) ?
            el.getNames()[0].toLowerCase() : "";
        if ( el instanceof LDAPAttributeSchema ) {
            _attributes.remove( name );
        } else if ( el instanceof LDAPObjectClassSchema ) {
            _objectClasses.remove( name );
        } else if ( el instanceof LDAPMatchingRuleSchema ) {
            _matchingRules.remove( name );
        } else if ( el instanceof LDAPMatchingRuleUseSchema ) {
            _matchingRuleUses.remove( name );
        } else if ( el instanceof LDAPNameFormSchema ) {
            _nameForms.remove( name );
        } else if ( el instanceof LDAPSyntaxSchema ) {
            if ( name.length() < 1 ) {
                name = el.getID();
            }
            _syntaxes.remove( name );
        } else if ( el instanceof LDAPDITContentRuleSchema ) {
            _contentRules.remove( name );
        } else if ( el instanceof LDAPDITStructureRuleSchema ) {
            _structureRulesByName.remove( name );
            _structureRulesById.remove( new Integer(
                ((LDAPDITStructureRuleSchema)el).getRuleID() ) );
        }
    }

    /**
     * Save the schema changes to the root of a Directory Server
     *
     * @param ld an active connection to a Directory Server
     * @exception LDAPException on failure.
     */
    public void saveSchema( LDAPConnection ld ) throws LDAPException {
        saveSchema( ld, "" );
    }

    /**
     * Save the schema changes to a specific entry
     *
     * @param ld an active connection to a Directory Server
     * @param dn the entry at which to save schema
     * @exception LDAPException on failure.
     */
    public void saveSchema( LDAPConnection ld, String dn )
        throws LDAPException {

        // Get array of changes to be made
        LDAPModification[] mods =
            (LDAPModification[])_changeList.toArray( new LDAPModification[0] );
        if ( mods.length < 1 ) {
            return;
        }

        /* Find the subschemasubentry value for this DN */
        String entryName = getSchemaDN( ld, dn );
        /* Make the changes; exception thrown on error */
        ld.modify( entryName, mods );
        /* If we got this far, the changes were saved and we can reset
           the change list */
        _changeList.clear();
    }

    /**
     * Displays the schema (including the descriptions of its object
     * classes, attribute types, and matching rules) in an easily
     * readable format (not the same as the format expected by
     * an LDAP server).
     * @return a string containing the schema in printable format.
     */
    public String toString() {
        String s = "Object classes:\n";
        Enumeration en = getObjectClassSchemas();
        while( en.hasMoreElements() ) {
            s += en.nextElement().toString();
            s += '\n';
        }
        s += "Attributes:\n";
        en = getAttributeSchemas();
        while( en.hasMoreElements() ) {
            s += en.nextElement().toString();
            s += '\n';
        }
        s += "Matching rules:\n";
        en = getMatchingRuleSchemas();
        while( en.hasMoreElements() ) {
            s += en.nextElement().toString();
            s += '\n';
        }
        s += "Syntaxes:\n";
        en = getSyntaxSchemas();
        while( en.hasMoreElements() ) {
            s += en.nextElement().toString();
            s += '\n';
        }
        return s;
    }

    /**
     * Removes all definitions
     */
    void clear() {
        _attributes.clear();
        _objectClasses.clear();
        _matchingRules.clear();
        _matchingRuleUses.clear();
        _nameForms.clear();
        _syntaxes.clear();
        _contentRules.clear();
        _structureRulesByName.clear();
        _structureRulesById.clear();
        _IDToElement.clear();
        _changeList.clear();
    }

    /**
     * Creates a modification object corresponding to a schema element and
     * a change type
     *
     * @param el The schema element
     * @param op Type of modification to make
     * @param ld Optional connection to server where change is to be made
     */
    private static LDAPModification getModificationForElement(
        LDAPSchemaElement el,
        int op,
        LDAPConnection ld ) {
        boolean quotingBug;
        try {
             quotingBug = (ld != null) ?
                 !LDAPSchema.isAttributeSyntaxStandardsCompliant( ld ) : false;
        } catch ( LDAPException e ) {
            quotingBug = false;
        }
        LDAPAttribute attr = new LDAPAttribute( el.getAttributeName(),
                                                el.getValue( quotingBug ) );
        return new LDAPModification( op, attr );
    }

    /**
     * Retrieve the DN of the schema definitions for a specific entry.
     * @param ld an active connection to a Directory Server
     * @param dn the entry for which to fetch schema
     * @exception LDAPException on failure.
     */
    private static String getSchemaDN( LDAPConnection ld,
                                       String dn )
        throws LDAPException {
        if ( (ld == null) || !ld.isConnected() ) {
            throw new LDAPException( "No connection", LDAPException.OTHER );
        }
        String[] attrs = { "subschemasubentry" };
        LDAPEntry entry = ld.read( dn, attrs );
        if ( entry == null ) {
            throw new LDAPException( "", LDAPException.NO_SUCH_OBJECT );
        }
        LDAPAttribute attr = entry.getAttribute( attrs[0] );
        String entryName = "cn=schema";
        if ( attr != null ) {
            Enumeration en = attr.getStringValues();
            if ( en.hasMoreElements() ) {
                entryName = (String)en.nextElement();
            }
        }
        return entryName;
    }

    /**
     * Extracts all schema elements from subschema entry
     *
     * @param entry entry containing schema definitions
     */
    protected void initialize( LDAPEntry entry ) {
        /* Get all object class definitions */
        LDAPAttribute attr = entry.getAttribute( "objectclasses" );
        Enumeration en;
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPObjectClassSchema sch =
                    new LDAPObjectClassSchema( (String)en.nextElement() );
                add( sch, false );
            }
        }

        /* Get all attribute definitions */
        attr = entry.getAttribute( "attributetypes" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPAttributeSchema sch =
                    new LDAPAttributeSchema( (String)en.nextElement() );
                add( sch, false );
            }
        }

        /* Get all syntax definitions */
        attr = entry.getAttribute( "ldapsyntaxes" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPSyntaxSchema sch =
                    new LDAPSyntaxSchema( (String)en.nextElement() );
                add( sch, false );
            }
        }

        /* Get all structure rule definitions */
        attr = entry.getAttribute( "ldapditstructurerules" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPDITStructureRuleSchema sch =
                    new LDAPDITStructureRuleSchema(
                        (String)en.nextElement() );
                add( sch, false );
            }
        }

        /* Get all content rule definitions */
        attr = entry.getAttribute( "ldapditcontentrules" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPDITContentRuleSchema sch =
                    new LDAPDITContentRuleSchema(
                        (String)en.nextElement() );
                add( sch, false );
            }
        }

        /* Get all name form definitions */
        attr = entry.getAttribute( "nameforms" );
        if ( attr != null ) {
            en = attr.getStringValues();
            while( en.hasMoreElements() ) {
                LDAPNameFormSchema sch =
                    new LDAPNameFormSchema(
                        (String)en.nextElement() );
                add( sch, false );
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
                h.put( sch.getID(), use );
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
                String use = (String)h.get( sch.getID() );
                if ( use != null )
                    sch = new LDAPMatchingRuleSchema( raw, use );
                add( sch, false );
            }
        }
    }

    /**
     * Read one attribute definition from a server to determine if
     * attribute syntaxes are quoted (a bug, present in Netscape
     * and Novell servers).
     * @param ld an active connection to a Directory Server
     * @return <CODE>true</CODE> if standards-compliant.
     * @exception LDAPException on failure.
     */
    static boolean isAttributeSyntaxStandardsCompliant( LDAPConnection ld )
        throws LDAPException {

        /* Check if this has already been investigated */
        String schemaBug = (String)ld.getProperty( ld.SCHEMA_BUG_PROPERTY );
        if ( schemaBug != null ) {
            return schemaBug.equalsIgnoreCase( "standard" );
        }

        boolean compliant = true;
        /* Get the schema definitions for attributes */
        String entryName = getSchemaDN( ld, "" );
        String[] attrs = { "attributetypes" };
        LDAPEntry entry = ld.read( entryName, attrs );
        /* Get all attribute definitions, and check the first one */
        LDAPAttribute attr = entry.getAttribute( "attributetypes" );
        if ( attr != null ) {
            Enumeration en = attr.getStringValues();
            if( en.hasMoreElements() ) {
                compliant = !isSyntaxQuoted( (String)en.nextElement() );
            }
        }
        ld.setProperty( ld.SCHEMA_BUG_PROPERTY, compliant ? "standard" :
                        "NetscapeBug" );
        return compliant;
    }

    /**
     * Parses an attribute schema definition to see if the SYNTAX value
     * is quoted. It shouldn't be (according to RFC 2252), but it is for
     * some LDAP servers. It will either be:<BR>
     * <CODE>SYNTAX 1.3.6.1.4.1.1466.115.121.1.15</CODE> or<BR>
     * <CODE>SYNTAX '1.3.6.1.4.1.1466.115.121.1.15'</CODE>
     * @param raw Definition of the attribute type in the
     * AttributeTypeDescription format.
     */
    static boolean isSyntaxQuoted( String raw ) {
        int ind = raw.indexOf( " SYNTAX " );
        if ( ind >= 0 ) {
            ind += 8;
            int l = raw.length() - ind;
            // Extract characters
            char[] ch = new char[l];
            raw.getChars( ind, raw.length(), ch, 0 );
            ind = 0;
            // Skip to ' or start of syntax value
            while( (ind < ch.length) && (ch[ind] == ' ') ) {
                ind++;
            }
            if ( ind < ch.length ) {
                return ( ch[ind] == '\'' );
            }
        }
        return false;
    }

    /**
     * Compares to schema elements for identical names; order and case are
     * ignored
     *
     * @param el1 First element to compare
     * @param el2 Second element to compare
     * @return <CODE>true</CODE> if the elements have the same names
     */
    private static boolean namesTheSame( LDAPSchemaElement el1,
                                         LDAPSchemaElement el2 ) {
        // Don't bother if either element is null
        if ( (el1 == null) || (el2 == null) ) {
            return false;
        }
        String[] names1 = el1.getNames();
        String[] names2 = el2.getNames();
        if ( (names1 == null) ^ (names2 == null) ) {
            return false;
        }
        // If neither have any names, return true
        if ( names1 == null ) {
            return true;
        }
        if ( names1.length != names2.length ) {
            return false;
        }
        if ( names1.length == 0 ) {
            return true;
        }
        if ( names1.length == 1 ) {
            return ( names1[0].equalsIgnoreCase( names2[0] ) );
        }
        Comparator comp = new Comparator() {
            public int compare( Object o1, Object o2 ) {
                return ((String)o1).compareToIgnoreCase( (String)o2 );
            }
            public boolean equals( Object obj ) {
                return false;
            }
        };
        Arrays.sort( names1, comp );
        Arrays.sort( names2, comp );
        for( int i = 0; i < names1.length; i++ ) {
            if ( !names1[i].equalsIgnoreCase( names2[i] ) ) {
                return false;
            }
        }
        return true;
    }

    /**
     * Helper for "main" to print out schema elements.
     *
     * @param en enumeration of schema elements
     */
    private static void printEnum( Enumeration en ) {
        while( en.hasMoreElements() ) {
            LDAPSchemaElement s = (LDAPSchemaElement)en.nextElement();
            System.out.println( "  " + s );
        }
    }

    /**
     * Reads the schema definitions from a particular subschema entry
     */
    private static LDAPEntry readSchema( LDAPConnection ld,
                                         String dn,
                                         String[] attrs )
        throws LDAPException {
        LDAPSearchResults results = ld.search (dn, ld.SCOPE_BASE,
                                               "objectclass=subschema",
                                               attrs, false);
        if ( !results.hasMore() ) {
            throw new LDAPException( "Cannot read schema",
                                     LDAPException.INSUFFICIENT_ACCESS_RIGHTS );
        }
        return results.next();
    }

    /**
     * Reads the schema definitions from a particular subschema entry
     */
    private static LDAPEntry readSchema( LDAPConnection ld,
                                         String dn )
        throws LDAPException {
        return readSchema( ld, dn, new String[] { "*", "ldapSyntaxes",
                        "matchingRules", "attributeTypes", "objectClasses" } );        
    }

    /**
     * Fetch the schema from the LDAP server at the specified
     * host and port, and print out the schema (including descriptions
     * of its object classes, attribute types, and matching rules).
     * The schema is printed in an easily readable format (not the
     * same as the format expected by an LDAP server).  For example,
     * you can enter the following command to print the schema:
     * <PRE>
     * java org.ietf.ldap.LDAPSchema myhost.mydomain.com 389
     * </PRE>
     *
     * @param args the host name and the port number of the LDAP server
     * (for example, <CODE>org.ietf.ldap.LDAPSchema directory.netscape.com
     * 389</CODE>)
     */
    public static void main( String[] args ) {
        if ( args.length < 2 ) {
            System.err.println( "Usage: org.ietf.ldap.LDAPSchema HOST PORT" );
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
            printEnum( schema.getObjectClassSchemas() );
            System.out.println( "\nAttributes: " );
            printEnum( schema.getAttributeSchemas() );
            System.out.println( "\nMatching rules: " );
            printEnum( schema.getMatchingRuleSchemas() );
            System.out.println( "\nSyntaxes: " );
            printEnum( schema.getSyntaxSchemas() );
            System.exit( 0 );
        } catch ( LDAPException e ) {
            System.err.println( e );
        }
    }

    private Hashtable _objectClasses = new Hashtable();
    private Hashtable _attributes = new Hashtable();
    private Hashtable _matchingRules = new Hashtable();
    private Hashtable _matchingRuleUses = new Hashtable();
    private Hashtable _syntaxes = new Hashtable();
    private Hashtable _structureRulesByName = new Hashtable();
    private Hashtable _structureRulesById = new Hashtable();
    private Hashtable _contentRules = new Hashtable();
    private Hashtable _nameForms = new Hashtable();
    private Hashtable _IDToElement = new Hashtable();
    private ArrayList _changeList = new ArrayList();
}
