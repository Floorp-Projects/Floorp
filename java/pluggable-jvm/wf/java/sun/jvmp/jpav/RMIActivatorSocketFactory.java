/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is The Waterfall Java Plugin Module
 * 
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 *
 * $Id: RMIActivatorSocketFactory.java,v 1.1 2001/05/10 18:12:28 edburns%acm.org Exp $
 *
 * 
 * Contributor(s): 
 *
 *   Nikolay N. Igotti <inn@sparc.spb.su>
 */

/*
 * @(#)RMIPluginSocketFactory.java	1.8 97/01/22
 * 
 * Copyright (c) 1995, 1996 Sun Microsystems, Inc. All Rights Reserved.
 * 
 * This software is the confidential and proprietary information of Sun
 * Microsystems, Inc. ("Confidential Information").  You shall not
 * disclose such Confidential Information and shall use it only in
 * accordance with the terms of the license agreement you entered into
 * with Sun.
 * 
 * SUN MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
 * SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT. SUN SHALL NOT BE LIABLE FOR ANY DAMAGES
 * SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
 * THIS SOFTWARE OR ITS DERIVATIVES.
 * 
 * CopyrightVersion 1.1_beta
 */
package sun.jvmp.jpav;

import sun.rmi.transport.proxy.RMIMasterSocketFactory;
import sun.rmi.transport.proxy.RMIHttpToPortSocketFactory;
import sun.rmi.transport.proxy.RMIHttpToCGISocketFactory;

/**
 * RMIPluginSocketFactory attempts to create a socket connection to the
 * specified host using successively less efficient mechanisms
 * until one succeeds.  If the host is successfully connected to,
 * the factory for the successful mechanism is stored in an internal
 * hash table keyed by the host name, so that future attempts to
 * connect to the same host will automatically use the same
 * mechanism.
 */
public class RMIActivatorSocketFactory extends RMIMasterSocketFactory {

    /**
     * Create a RMIActivatorSocketFactory object.  Establish order of
     * connection mechanisms to attempt on createSocket, if a direct
     * socket connection fails.
     */
    public RMIActivatorSocketFactory()
    {
        altFactoryList.addElement(new RMIHttpToPortSocketFactory());
        altFactoryList.addElement(new RMIHttpToCGISocketFactory());
    }
}

