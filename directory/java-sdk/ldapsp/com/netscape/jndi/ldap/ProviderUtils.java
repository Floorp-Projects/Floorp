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
package com.netscape.jndi.ldap;

import javax.naming.*;
import javax.naming.directory.*;
import javax.naming.ldap.*;

import netscape.ldap.*;

import java.util.*;

/**
 * Common utility methods
 *
 */
class ProviderUtils {

    public static final String DEFAULT_FILTER = "(objectclass=*)";

    static int jndiSearchScopeToLdap(int jndiScope) throws NamingException {
        int scope = -1;
        if (jndiScope == SearchControls.SUBTREE_SCOPE) {
            scope = LDAPConnection.SCOPE_SUB;
        }
        else if (jndiScope == SearchControls.ONELEVEL_SCOPE) {
            scope = LDAPConnection.SCOPE_ONE;
        }
        else if (jndiScope == SearchControls.OBJECT_SCOPE) {
            scope = LDAPConnection.SCOPE_BASE;
        }
        else {
            throw new InvalidSearchControlsException("Illegal value for the search scope");
        }
        return scope;
    }        

    
    /**
     * Convert Attribute List to a LDAP filter
     *
     * @return LDAP Filter
     * @throw NamingException
     * @param attrs An Attribute List
     */
    static String attributesToFilter(Attributes attrs) throws NamingException{
        
        if (attrs == null || attrs.size() == 0) {
            return DEFAULT_FILTER;
        }
        
        String filter = "";
        
        for (NamingEnumeration attrEnum = attrs.getAll(); attrEnum.hasMore();) {
            Attribute attrib = (Attribute) attrEnum.next();
            
            //Has attributes any values
            if ( attrib.size() == 0) {
                // test only for presence of the attribute
                filter += "(" + attrib.getID() + "=*)";
                continue;
            }
            
            // Add attribute values to the filter, ecsaping if necessary
            String attrValues = "";
            for (NamingEnumeration valEnum = attrib.getAll(); valEnum.hasMore();) {
                Object val = valEnum.next();
                if (val instanceof String) {
                    attrValues += "(" + attrib.getID() + "=" + escapeString((String)val) +")";
                }    
                else if (val instanceof byte[]) {
                    attrValues += "(" + attrib.getID() + "=" + escapeBytes((byte[])val) +")";
                }
                else if (val == null) {
                    //null is allowed value in Attribute.add(Object), accept it just in case
                    attrValues += "(" + attrib.getID() + "=*)";
                }
                else {
                    throw new NamingException(
                    "Wrong Attribute value, expecting String or byte[]");
                }
            }
            filter += (attrib.size() > 1) ? ("(|" + attrValues + ")") : attrValues;
        }
        
        return (attrs.size() > 1) ? ("(&" + filter + ")") : filter;
    }    
            

    /**
     * Expand filterExpr. Each occurrence of a variable "{n}", where n is a non-negative
     * integer, is replaced with a variable from the filterArgs array indexed by the 'n'.
     * FilterArgs can be Strings or byte[] and they are escaped according to the RFC2254
     */
     static String expandFilterExpr(String filterExpr, Object[] filterArgs) throws InvalidSearchFilterException{
        StringTokenizer tok = new StringTokenizer(filterExpr, "{}", /*returnTokens=*/true);
        
        if (tok.countTokens() == 1) {            
            return filterExpr; // No escape characters
        }
        
        StringBuffer out= new StringBuffer();
        boolean expectVarIdx = false, expectVarOff = false;
        Object arg= null;
        while (tok.hasMoreTokens()) {
            String s = tok.nextToken();
            
            if (expectVarIdx) {
                expectVarIdx = false;
                try {
                    int idx = Integer.parseInt(s);
                    arg = filterArgs[idx];
                    expectVarOff = true;
                }
                catch (IndexOutOfBoundsException e) {
                    throw new InvalidSearchFilterException("Filter expression variable index out of bounds");
                }

                catch (Exception e) {
                    throw new InvalidSearchFilterException("Invalid filter expression");
                }
            }
                    
            else if (expectVarOff) {
                expectVarOff = false;
                if (!s.equals("}")) {
                    throw new InvalidSearchFilterException("Invalid filter expression");
                }
                if (arg instanceof String) {
                    out.append(escapeString((String)arg));
                }
                else if (arg instanceof byte[]) {
                    out.append(escapeBytes((byte[])arg));
                }
                else {                
                    throw new InvalidSearchFilterException("Invalid filter argument type");
                }
                arg = null;
            }
            
            else if  (s.equals("{")) {
                expectVarIdx = true;
            }
            else {
                out.append(s);
            }
        }
        if (expectVarIdx || expectVarOff) {
            throw new InvalidSearchFilterException("Invalid filter expression");
        }    
        return out.toString();
    }    

         
    /**
     * Escape a string according to the RFC 2254
     */
    static String escapeString(String str) {
        String charToEscape = "\\*()\000";
        StringTokenizer tok = new StringTokenizer(str, charToEscape, /*returnTokens=*/true);
        
        if (tok.countTokens() == 1) {            
            return str; // No escape characters
        }
        
        StringBuffer out= new StringBuffer();
        while (tok.hasMoreTokens()) {
            String s = tok.nextToken();
            
            if (s.equals("*")) {
                out.append("\\2a");
            }
            else if (s.equals("(")) {
                out.append("\\28");
            }    
            else if (s.equals(")")) {
                out.append("\\29");
            }    
            else if (s.equals("\\")) {
                out.append("\\5c");
            }
            else if (s.equals("\000")) {
                out.append("\\00");
            }
            else {
                out.append(s);
            }
        }
        return out.toString();
    }    

    
    /**
     * Escape a byte array according to the RFC 2254
     */
    static final String hexDigits="0123456789abcdef";
    
    static String escapeBytes(byte[] bytes) {
        StringBuffer out = new StringBuffer("");
        for (int i=0; i < bytes.length; i++) {

            int low  = bytes[i] & 0x0f;
            int high = (bytes[i] & 0xf0) >> 4;
            out.append("\\");
            out.append(hexDigits.charAt(high));
            out.append(hexDigits.charAt(low));
        }
        return out.toString();
    }    

    /**
     * A method used only for testing
     */
    private    static void testAttributesToFilter() {
        try {
            System.out.println(attributesToFilter(null));
            
            BasicAttributes attrs = new BasicAttributes(true);        
            
            System.out.println(attrs + " = " + attributesToFilter(attrs));
            
            attrs.put (new BasicAttribute("attr1", "val1"));
            attrs.put (new BasicAttribute("attr2", "(val2)\\*x"));
            attrs.put (new BasicAttribute("attr3"));
            BasicAttribute attr4 = new BasicAttribute("attr4", "val41");
            attr4.add("val42");
            attrs.put(attr4);
            attrs.put("attr5", new byte[] { (byte)0x23, (byte)0x3, (byte)0x0, (byte)0xab, (byte)0xff});
            System.out.println(attrs + " = " +attributesToFilter(attrs));            
        }
        catch (Exception e) {
            System.err.println(e);
        }    
    }    

    /**
     * A method used only for testing
     */
    private    static void testFilterExpr() {
        try {
            String filterExpr = "(&(attr0={0})(attr1={1}))";
            Object [] args = new Object[] { "val*0", new byte[] { (byte)0xf0, (byte) 0x3a}};
            String filter = null;
            filter = expandFilterExpr(filterExpr, args);
            System.out.println(filterExpr + " -> " + filter);
        }
        catch (Exception e) {
            System.err.println(e);
        }    
    }    

    /**
     * Test
     */
    public static void main(String[] args) {
        /*testAttributesToFilter();
        String x = "012\0003";
        byte[] b = x.getBytes();
        for (int i=0; i < b.length; i++) {
            System.out.println(i+"="+b[i]);
        }*/
        testFilterExpr();
    }
}
