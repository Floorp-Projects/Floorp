/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * $Id: ObjectFactory.java,v 1.2 2001/07/12 19:57:54 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package sun.jvmp;

import java.util.Enumeration;
import java.security.*;
/**
 * This interface describes interface to be implemented by object factory
 * if it likes to be used by Waterfall actors.
 * Protocol:
 *    1. getCIDs(), if returned null - done
 *    2. call handleConflict() on every conflicting CID (see handleConflict())
 *       XXX: shouldn't interface be locked on conflict resolution time?
 *    3. now object factory is ready to produce object instances using 
 *       instantiate()
 *    4. also permissions provided by this ObjectFactory used by system policy
 **/
public interface ObjectFactory {
    /**
     * Returns name of this object factory. Also can be used for 
     * versioning, i.e. name can be
     * WF-AppletViewer:00001
     * and so useful in conflict resolution
     **/
    public String         getName();
    
    /**
     * Returns list of CIDs  this object factory provides
     **/
    public Enumeration    getCIDs();

    /**
     * This method called with a rejected CID and conficting object factory.
     * Returned value true if ObjectFactory insist to be the owner of this CID,
     * false otherwise.
     * If both returned true, or both returned false - status quo is saved.
     * Otherwise factory returned true becomes owner of CID.
     * Using this protocol, versioning issues can be solved easily.
     **/
    public boolean        handleConflict(String cid, 
					 ObjectFactory f);

    /**
     * When Waterfall asked to create instance of object with given CID
     * - this method got called.
     **/
    public Object         instantiate(String cid, Object arg);

    /**
     * Object factory allowed to extend Waterfall security policy
     * using this method. Usually this method used to grant certain 
     * permissions to itself, but also can be used to define system-wide policy.
     * This isn't recommended in generic case, but some kind of extensions,
     * for example applet viewers, cannot avoid it.
     **/
    public PermissionCollection getPermissions(CodeSource codesource);
}


