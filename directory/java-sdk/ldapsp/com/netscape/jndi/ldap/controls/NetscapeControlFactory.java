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
package com.netscape.jndi.ldap.controls;


/**
 * Factory for creating controls. Only controls send by the direcory server
 * are processed.
 */

import javax.naming.*;
import javax.naming.ldap.*;
import netscape.ldap.LDAPControl;
import netscape.ldap.controls.*;
import com.netscape.jndi.ldap.common.ExceptionMapper;

public class NetscapeControlFactory extends ControlFactory {

    // "1.2.840.113556.1.4.473" Sort Control (Request) 
    final static String REQ_SORT  = LDAPSortControl.SORTREQUEST;
    
    // "1.2.840.113556.1.4.474" Sort Control (Response)  
    final static String RSP_SORT = LDAPSortControl.SORTRESPONSE;

    // "2.16.840.1.113730.3.4.2" ManageDSAIT Control 
    final static String REQ_MANAGEDSAIT = LDAPControl.MANAGEDSAIT;

    // "2.16.840.1.113730.3.4.3" PersistentSearch Control 
    final static String REQ_PERSISTENTSEARCH  = LDAPPersistSearchControl.PERSISTENTSEARCH;
    
    // "2.16.840.1.113730.3.4.4" PasswordExpired Control
    final static String RSP_PWDEXPIRED = LDAPPasswordExpiredControl.EXPIRED;
    
    // "2.16.840.1.113730.3.4.5" PasswordExpiring Control 
    final static String RSP_PWDEXPIRING = LDAPPasswordExpiringControl.EXPIRING;
    
    // "2.16.840.1.113730.3.4.7" EntryChanged Controle 
    final static String RSP_ENTRYCHANGED = LDAPEntryChangeControl.ENTRYCHANGED;

    // "2.16.840.1.113730.3.4.9" Virtual List (Request) 
    final static String REQ_VIRTUALLIST = LDAPVirtualListControl.VIRTUALLIST;
    
    // "2.16.840.1.113730.3.4.10" Virtual List (Response)
    final static String RSP_VIRTUALLIST = LDAPVirtualListResponse.VIRTUALLISTRESPONSE;

    // "2.16.840.1.113730.3.4.12" Proxed Authentication
    final static String REQ_PROXIEDAUTH  = LDAPProxiedAuthControl.PROXIEDAUTHREQUEST;


    /**
     * Implements abstract getControlInstance() from ConrolFactory
     */
    public Control getControlInstance(Control ctrl) throws NamingException {
        if (ctrl == null) {
            return null;
        }
        LDAPControl rawCtrl = new LDAPControl(
            ctrl.getID(), ctrl.isCritical(), ctrl.getEncodedValue());
        return getControlInstance(rawCtrl);
    }    
        
    /**
     * Create control using parseResponse() methods in ldapjdk controls
     */
    public static Control getControlInstance(LDAPControl rawCtrl) throws NamingException {
        if (rawCtrl == null) {
            return null;
        }

        try { 
            String ctrlID = rawCtrl.getID();
        
             // Entry changed control is parsed by LDAPPersistSearchControl             
            if (ctrlID.equals(RSP_ENTRYCHANGED)) {
                // First get ldapjdk object
                LDAPEntryChangeControl ldapjdkCtrl =
                    LDAPPersistSearchControl.parseResponse(new LDAPControl[] {rawCtrl});
                // then map it to a jndi object                    
                if (ldapjdkCtrl != null) {
                    LdapEntryChangeControl ctrl = new LdapEntryChangeControl(
                        rawCtrl.isCritical(), rawCtrl.getValue());
                    ctrl.setChangeNumber(ldapjdkCtrl.getChangeNumber());
                    ctrl.setChangeType(ldapjdkCtrl.getChangeType());
                    ctrl.setPreviousDN(ldapjdkCtrl.getPreviousDN());
                    return ctrl;
                }
            }
            
            // Password Expired control
            else if(ctrlID.equals(RSP_PWDEXPIRED)) {
                LdapPasswordExpiredControl ctrl = new LdapPasswordExpiredControl(
                        rawCtrl.isCritical(), rawCtrl.getValue());
            }

            // Password Expiring control
            else if(ctrlID.equals(RSP_PWDEXPIRING)) {
                LdapPasswordExpiringControl ctrl = new LdapPasswordExpiringControl(
                        rawCtrl.isCritical(), rawCtrl.getValue());
            }

            // Sort Response control
            else if(ctrlID.equals(RSP_SORT)) {
                // First parse the control
                int[] resultCodes = new int[1];
                String failedAttr = LDAPSortControl.parseResponse(
                    new LDAPControl[] {rawCtrl}, resultCodes);

                LdapSortResponseControl ctrl = new LdapSortResponseControl(
                        rawCtrl.isCritical(), rawCtrl.getValue());
                ctrl.setResultCode(resultCodes[0]);
                ctrl.setFailedAttribute(failedAttr);
                return ctrl;
            }

            // Virtual List Response control
            else if(ctrlID.equals(RSP_VIRTUALLIST)) {
                return new LdapVirtualListResponseControl(rawCtrl.getValue());                
            }

            // No match try another ControlFactory
            return null;
        }
        catch (Exception ex) {
            throw ExceptionMapper.getNamingException(ex);
        }
    }
}
