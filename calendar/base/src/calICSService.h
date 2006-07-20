/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is
 *  Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Shaver <mike.x.shaver@oracle.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "calIICSService.h"

#include "nsClassHashtable.h"

extern "C" {
#   include "ical.h"
}

class calIIcalComponent;
class calIcalComponent;

struct TimezoneEntry
{
    nsCString const mLatitude;
    nsCString const mLongitude;
    nsCOMPtr<calIIcalComponent> const mTzCal;
    
    TimezoneEntry(nsACString const& latitude,
                  nsACString const& longitude,
                  nsCOMPtr<calIIcalComponent> const& tzCal)
        : mLatitude(latitude), mLongitude(longitude), mTzCal(tzCal) {}
};

class calICSService : public calIICSService
{
public:
    calICSService();
    virtual ~calICSService() { }
    
    NS_DECL_ISUPPORTS
    NS_DECL_CALIICSSERVICE
protected:
    nsClassHashtable<nsCStringHashKey, TimezoneEntry> mTzHash;
    TimezoneEntry const* getTimezoneEntry(nsACString const& tzid);
};

class calIcalProperty : public calIIcalProperty
{
public:
    calIcalProperty(icalproperty *prop, calIIcalComponent *parent) :
        mProperty(prop), mParent(parent) { }
    virtual ~calIcalProperty()
    {
        if (!mParent)
            icalproperty_free(mProperty);
    }

    icalproperty *getIcalProperty() { return mProperty; }
    
    NS_DECL_ISUPPORTS
    NS_DECL_CALIICALPROPERTY

    friend class calIcalComponent;
protected:
    icalproperty *mProperty;
    nsCOMPtr<calIIcalComponent> mParent;
};
