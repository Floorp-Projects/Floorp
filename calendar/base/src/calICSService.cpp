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
 *   Mike Shaver <shaver@off.net>
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

#include "calICSService.h"
#include "calDateTime.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"

#include "calIEvent.h"
#include "calBaseCID.h"

extern "C" {
#    include "ical.h"
}

class calIcalComponent : public calIIcalComponent
{
public:
    calIcalComponent(icalcomponent *ical,
                     calIcalComponent *parent) : 
        mComponent(ical), mParent(parent) { }

    virtual ~calIcalComponent()
    {
        if (!mParent)
            icalcomponent_free(mComponent);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_CALIICALCOMPONENT
protected:

    nsresult SetProperty(icalproperty_kind kind, icalvalue *val);

    nsresult GetStringProperty(icalproperty_kind kind, nsACString &str)
    {
        icalproperty *prop =
            icalcomponent_get_first_property(mComponent, kind);
        if (!prop)
            str.Truncate();
        else
            str.Assign(icalvalue_get_string(icalproperty_get_value(prop)));
        return NS_OK;
    }

    nsresult SetStringProperty(icalproperty_kind kind, const nsACString &str)
    {
        icalvalue *val = icalvalue_new_string(PromiseFlatCString(str).get());
        if (!val)
            return NS_ERROR_OUT_OF_MEMORY;
        return SetProperty(kind, val);
    }

    nsresult GetIntProperty(icalproperty_kind kind, PRUint32 *valp)
    {
        icalproperty *prop =
            icalcomponent_get_first_property(mComponent, kind);
        if (!prop)
            *valp = PR_UINT32_MAX;
        else
            *valp = (PRUint32)icalvalue_get_integer(icalproperty_get_value(prop));
        return NS_OK;
    }

    nsresult SetIntProperty(icalproperty_kind kind, PRUint32 i)
    {
        icalvalue *val = icalvalue_new_integer(i);
        if (!val)
            return NS_ERROR_OUT_OF_MEMORY;
        return SetProperty(kind, val);
    }



    icalcomponent              *mComponent;
    nsCOMPtr<calIcalComponent>  mParent;
};

nsresult
calIcalComponent::SetProperty(icalproperty_kind kind, icalvalue *val)
{
    icalproperty *prop =
        icalcomponent_get_first_property(mComponent, kind);
    if (prop) {
        icalcomponent_remove_property(mComponent, prop);
        icalproperty_free(prop);
    }
    prop = icalproperty_new(kind);
    if (!prop) {
        icalvalue_free(val);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    icalproperty_set_value(prop, val);
    icalcomponent_add_property(mComponent, prop);
    return NS_OK;
}

#define STRING_ATTRIBUTE(Attrname, ICALNAME)                    \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(nsACString &str)                \
{                                                               \
    return GetStringProperty(ICAL_##ICALNAME##_PROPERTY, str);  \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(const nsACString &str)          \
{                                                               \
    return SetStringProperty(ICAL_##ICALNAME##_PROPERTY, str);  \
}

#define INT_ATTRIBUTE(Attrname, ICALNAME)                       \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(PRUint32 *valp)                 \
{                                                               \
    return GetIntProperty(ICAL_##ICALNAME##_PROPERTY, valp);    \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(PRUint32 val)                   \
{                                                               \
    return SetIntProperty(ICAL_##ICALNAME##_PROPERTY, val);     \
}                                                               \

#define RO_DATE_ATTRIBUTE(Attrname, ICALNAME)                           \
NS_IMETHODIMP                                                           \
calIcalComponent::Get##Attrname(calIDateTime **dtp)                     \
{                                                                       \
    icalproperty *prop =                                                \
        icalcomponent_get_first_property(mComponent,                    \
                                         ICAL_##ICALNAME##_PROPERTY);   \
    if (!prop) {                                                        \
        *dtp = nsnull;                                                  \
        return NS_OK;                                                   \
    }                                                                   \
                                                                        \
    struct icaltimetype itt =                                           \
        icalvalue_get_datetime(icalproperty_get_value(prop));           \
    calDateTime *dt = new calDateTime(&itt);                            \
    if (!dt)                                                            \
        return NS_ERROR_OUT_OF_MEMORY;                                  \
    NS_ADDREF(*dtp = dt);                                               \
    return NS_OK;                                                       \
}

#define DATE_ATTRIBUTE(Attrname, ICALNAME)                              \
RO_DATE_ATTRIBUTE(Attrname, ICALNAME)                                   \
                                                                        \
NS_IMETHODIMP                                                           \
calIcalComponent::Set##Attrname(calIDateTime *dt)                       \
{                                                                       \
    struct icaltimetype itt;                                            \
    dt->ToIcalTime(&itt);                                               \
    icalvalue *val = icalvalue_new_datetime(itt);                       \
    if (!val)                                                           \
        return NS_ERROR_OUT_OF_MEMORY;                                  \
    return SetProperty(ICAL_##ICALNAME##_PROPERTY, val);                \
}

NS_IMPL_ISUPPORTS1(calIcalComponent, calIIcalComponent)

NS_IMETHODIMP
calIcalComponent::GetFirstSubcomponent(PRUint32 componentType,
                                       calIIcalComponent **subcomp)
{
    icalcomponent *ical =
        icalcomponent_get_first_component(mComponent, (icalcomponent_kind)componentType);
    if (!ical) {
        *subcomp = nsnull;
        return NS_OK;
    }

    *subcomp = new calIcalComponent(ical, this);
    if (!*subcomp)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*subcomp);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::GetNextSubcomponent(PRUint32 componentType,
                                      calIIcalComponent **subcomp)
{
    icalcomponent *ical =
        icalcomponent_get_next_component(mComponent, (icalcomponent_kind)componentType);
    if (!ical) {
        *subcomp = nsnull;
        return NS_OK;
    }

    *subcomp = new calIcalComponent(ical, this);
    if (!*subcomp)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*subcomp);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::GetIsA(PRUint32 *isa)
{
    *isa = icalcomponent_isa(mComponent);
    return NS_OK;
}

STRING_ATTRIBUTE(Uid, UID)
INT_ATTRIBUTE(Method, METHOD)
INT_ATTRIBUTE(Status, STATUS)
STRING_ATTRIBUTE(Summary, SUMMARY)
STRING_ATTRIBUTE(Description, DESCRIPTION)
STRING_ATTRIBUTE(Location, LOCATION)
STRING_ATTRIBUTE(Categories, CATEGORIES)
STRING_ATTRIBUTE(URL, URL)
INT_ATTRIBUTE(Priority, PRIORITY)
INT_ATTRIBUTE(Visibility, CLASS)
DATE_ATTRIBUTE(StartTime, DTSTART)
DATE_ATTRIBUTE(EndTime, DTEND)
DATE_ATTRIBUTE(DueTime, DUE)
DATE_ATTRIBUTE(StampTime, DTSTAMP)
DATE_ATTRIBUTE(LastModified, LASTMODIFIED)
DATE_ATTRIBUTE(CreatedTime, CREATED)
DATE_ATTRIBUTE(CompletedTime, COMPLETED)

NS_IMPL_ISUPPORTS1(calICSService, calIICSService)

NS_IMETHODIMP
calICSService::ParseICS(const nsACString& serialized,
                        calIIcalComponent **component)
{
    icalcomponent *ical =
        icalparser_parse_string(PromiseFlatCString(serialized).get());
    if (!ical) {
#ifdef DEBUG
        fprintf(stderr, "Error parsing: '%20s': %d (%s)\n",
                PromiseFlatCString(serialized).get(), icalerrno,
                icalerror_strerror(icalerrno));
#endif
        return NS_ERROR_FAILURE;
    }
    calIcalComponent *comp = new calIcalComponent(ical, nsnull);
    if (!comp) {
        icalcomponent_free(ical);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*component = comp);
    return NS_OK;
}
