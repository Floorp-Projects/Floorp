/*
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Frank
 * Mitchell. Portions created by Frank Mitchell are
 * Copyright (C) 1999 Frank Mitchell. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
package org.mozilla.xpcom;

class nsISupports__Proxy extends ComObject implements nsISupports {

    public Object QueryInterface(nsID uuid) {
	Object[] args = new Object[2];
	args[0] = uuid;
	args[1] = null;
	XPComUtilities.InvokeMethodByIndex(NS_ISUPPORTS_IID, objptr, 0, args);
	return args[1];
    }

    // PENDING: should reimplement identity, hash, etc.
}
