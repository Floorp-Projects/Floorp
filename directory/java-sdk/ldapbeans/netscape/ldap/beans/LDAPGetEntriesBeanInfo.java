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
package netscape.ldap.beans;

import java.beans.SimpleBeanInfo;
import java.beans.BeanDescriptor;
import java.beans.EventSetDescriptor;
import java.beans.MethodDescriptor;
import java.beans.PropertyDescriptor;
import java.beans.ParameterDescriptor;
import java.beans.BeanInfo;

/**
 * BeanInfo for LDAPGetEntries
 */

public class LDAPGetEntriesBeanInfo extends SimpleBeanInfo {

    public LDAPGetEntriesBeanInfo() throws Exception {

    beanClass = Class.forName( "netscape.ldap.beans.LDAPGetEntries" );

        try {
            PropertyDescriptor host =
                new PropertyDescriptor("host", beanClass);
            PropertyDescriptor port =
                new PropertyDescriptor("port", beanClass);
            PropertyDescriptor authDN =
                new PropertyDescriptor("authDN", beanClass);
            PropertyDescriptor authPassword =
                new PropertyDescriptor("authPassword", beanClass);
            PropertyDescriptor base =
                new PropertyDescriptor("base", beanClass);
            PropertyDescriptor userName =
                new PropertyDescriptor("userName", beanClass);
            PropertyDescriptor userID =
                new PropertyDescriptor("userID", beanClass);
            PropertyDescriptor filter =
                new PropertyDescriptor("filter", beanClass);
            PropertyDescriptor scope =
                new PropertyDescriptor("scope", beanClass);
            PropertyDescriptor debug =
                new PropertyDescriptor("debug", beanClass);

            PropertyDescriptor rv[] =
                {host, port, authDN, authPassword, base, filter,
                 scope, userName, userID, debug};
            _propertyDescriptor = new PropertyDescriptor[rv.length];
            for( int i = 0; i < rv.length; i++ )
                _propertyDescriptor[i] = rv[i];
        } catch (Exception e) {
            throw new Error(e.toString());
        }

        // Publish events --------------------------------------------------
        try {
            _eventSetDescriptor = new EventSetDescriptor[1];

            _eventSetDescriptor[0] = new EventSetDescriptor(beanClass,
                    "propertyChange",
                    Class.forName("java.beans.PropertyChangeListener"),
                    "propertyChange");
        } catch (Exception e) {
            throw new Error(e.toString());
        }

        // Publish descriptor ---------------------------------------------
        try {
            _beanDescriptor = new BeanDescriptor(beanClass);
            _beanDescriptor.setDisplayName( "LDAP entry retrieval" );
            _beanDescriptor.setShortDescription(
                "LDAP property retrieval -"
                + " provided a host, port, base, search filter,"
                + " and optionally a username and password,"
                + " return an array of string values both as a"
                + " function return and as a Property change event." );
        } catch (Exception e) {
        }
    }

    /**
     * @return the public properties
     */
    public PropertyDescriptor[] getPropertyDescriptors() {
        return _propertyDescriptor;
    }

    public EventSetDescriptor[] getEventSetDescriptors() {
        return _eventSetDescriptor;
    }

    public BeanInfo[] getAdditionalBeanInfo() {
        return null;
    }

    public int getDefaultEventIndex() {
        return -1;
    }

    public int getDefaultPropertyIndex() {
        return -1;
    }

    public BeanDescriptor getBeanDescriptor() {
        return _beanDescriptor;
    }

    private static Class beanClass;
    private BeanDescriptor _beanDescriptor;
    private EventSetDescriptor[] _eventSetDescriptor;
    private PropertyDescriptor[] _propertyDescriptor;
}


