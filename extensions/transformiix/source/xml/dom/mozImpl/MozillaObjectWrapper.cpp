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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Peter Van der Beken are Copyright (C) 2000
 * Peter Van der Beken. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Van der Beken, peter.vanderbeken@pandora.be
 *    -- original author.
 *
 */

/* Base class of all the wrapper classes. Takes care of addref'ing and
   releasing the Mozilla objects.
*/

#include "mozilladom.h"

/**
 * Construct a wrapper with the specified Mozilla object and document owner.
 *
 * @param aNsObject the Mozilla object you want to wrap
 * @param aOwner the document that owns this object
 */
MozillaObjectWrapper::MozillaObjectWrapper(nsISupports* aNsObject,
        Document* aOwner)
{
    nsObject = aNsObject;
    ownerDocument = aOwner;
    if (ownerDocument && (ownerDocument != this))
        ownerDocument->addWrapper(this, nsObject.get());
}

/**
 * Destructor
 */
MozillaObjectWrapper::~MozillaObjectWrapper()
{
    if (ownerDocument && (ownerDocument != this) &&
            !ownerDocument->inHashTableDeletion())
        ownerDocument->removeWrapper(nsObject.get());
}

/**
 * Wrap a different Mozilla object with this wrapper.
 *
 * @param aNsObject the Mozilla object you want to wrap
 */
void MozillaObjectWrapper::setNSObj(nsISupports* aNsObject)
{
    nsObject = aNsObject;
}

/**
 * Wrap a different Mozilla object with this wrapper and set document owner.
 *
 * @param aNsObject the Mozilla object you want to wrap
 */
void MozillaObjectWrapper::setNSObj(nsISupports* aNsObject, Document* aOwner)
{
    nsObject = aNsObject;
    ownerDocument = aOwner;
    if (ownerDocument && (ownerDocument != this))
        ownerDocument->addWrapper(this, nsObject.get());
}

/**
 * Get the hash key for the Mozilla object that this wrapper wraps.
 *
 * @return the wrapper's hash key
 */
void* MozillaObjectWrapper::getKey() const
{
    return nsObject.get();
};
