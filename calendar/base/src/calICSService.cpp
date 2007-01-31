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
 *   Michiel van Leeuwen <mvl@exedo.nl>
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
#include "calDuration.h"
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsInterfaceHashtable.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsStringEnumerator.h"
#include "nsCRT.h"
#include "nsStringStream.h"

#ifndef MOZILLA_1_8_BRANCH
#include "nsIClassInfoImpl.h"
#endif

#include "calIEvent.h"
#include "calBaseCID.h"
#include "calIErrors.h"

static NS_DEFINE_CID(kCalICSService, CAL_ICSSERVICE_CID);

extern "C" {
#    include "ical.h"
}

NS_IMPL_ISUPPORTS1(calIcalProperty, calIIcalProperty)

icalproperty*
calIcalProperty::GetIcalProperty()
{
    return mProperty;
}

icalcomponent*
calIcalProperty::GetIcalComponent()
{
    return mParent->GetIcalComponent();
}

NS_IMETHODIMP
calIcalProperty::GetIcalString(nsACString &str)
{
    char const* icalstr = icalproperty_as_ical_string(mProperty);
    if (icalstr == 0) {
#ifdef DEBUG
        fprintf(stderr, "Error getting ical string: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        return calIErrors::ICS_ERROR_BASE + icalerrno;
    }
    str.Assign(icalstr);
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::GetValue(nsACString &str)
{
    icalvalue_kind kind = icalproperty_kind_to_value_kind(icalproperty_isa(mProperty));

    const char *icalstr;
    if (kind == ICAL_TEXT_VALUE) {
        icalvalue *v = icalproperty_get_value(mProperty);
        icalstr = icalvalue_get_text(v);
    } else if (kind == ICAL_X_VALUE) {
        icalvalue *v = icalproperty_get_value(mProperty);
        icalstr = icalvalue_get_x(v);
    } else {
        icalstr = icalproperty_get_value_as_string(mProperty);
    }

    if (!icalstr) {
        if (icalerrno == ICAL_BADARG_ERROR) {
            str.Truncate();
            // Set string to null, because we don't have a value
            // (which is something different then an empty value)
            str.SetIsVoid(PR_TRUE);
            return NS_OK;
        }
        
#ifdef DEBUG
        fprintf(stderr, "Error getting string value: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        return NS_ERROR_FAILURE;
    }

    str.Assign(icalstr);
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::SetValue(const nsACString &str)
{
    icalvalue_kind kind = icalproperty_kind_to_value_kind(icalproperty_isa(mProperty));
    if (kind == ICAL_TEXT_VALUE) {
        icalvalue *v = icalvalue_new_text(PromiseFlatCString(str).get());
        icalproperty_set_value(mProperty, v);
    } else if (kind == ICAL_X_VALUE) {
        icalvalue *v = icalvalue_new_x(PromiseFlatCString(str).get());
        icalproperty_set_value(mProperty, v);
    } else {
        icalproperty_set_value_from_string(mProperty,
                                           PromiseFlatCString(str).get(),
                                           icalvalue_kind_to_string(kind));
    }
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::GetValueAsIcalString(nsACString &str)
{
    const char *icalstr = icalproperty_get_value_as_string(mProperty);
    if (!icalstr) {
        if (icalerrno == ICAL_BADARG_ERROR) {
            str.Truncate();
            // Set string to null, because we don't have a value
            // (which is something different then an empty value)
            str.SetIsVoid(PR_TRUE);
            return NS_OK;
        }
        
#ifdef DEBUG
        fprintf(stderr, "Error getting string value: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        return NS_ERROR_FAILURE;
    }

    str.Assign(icalstr);
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::SetValueAsIcalString(const nsACString &str)
{
    const char *kindstr = 
        icalvalue_kind_to_string(icalproperty_kind_to_value_kind(icalproperty_isa(mProperty)));
    icalproperty_set_value_from_string(mProperty,
                                       PromiseFlatCString(str).get(),
                                       kindstr);
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::GetPropertyName(nsACString &name)
{
    const char *icalstr = icalproperty_get_property_name(mProperty);
    if (!icalstr) {
#ifdef DEBUG
        fprintf(stderr, "Error getting property name: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        return NS_ERROR_FAILURE;
    }
    name.Assign(icalstr);
    return NS_OK;
}

static icalparameter*
FindXParameter(icalproperty *prop, const nsACString &param)
{
    for (icalparameter *icalparam =
             icalproperty_get_first_parameter(prop, ICAL_X_PARAMETER);
         icalparam;
         icalparam = icalproperty_get_next_parameter(prop, ICAL_X_PARAMETER)) {
        if (param.Equals(icalparameter_get_xname(icalparam)))
            return icalparam;
    }
    return nsnull;
}

NS_IMETHODIMP
calIcalProperty::GetParameter(const nsACString &param, nsACString &value)
{
    // More ridiculous parameter/X-PARAMETER handling.
    icalparameter_kind paramkind = 
        icalparameter_string_to_kind(PromiseFlatCString(param).get());

    if (paramkind == ICAL_NO_PARAMETER)
        return NS_ERROR_INVALID_ARG;

    const char *icalstr = nsnull;
    if (paramkind == ICAL_X_PARAMETER) {
        icalparameter *icalparam = FindXParameter(mProperty, param);
        if (icalparam)
            icalstr = icalparameter_get_xvalue(icalparam);
    } else {
        icalstr = icalproperty_get_parameter_as_string(mProperty,
                                                       PromiseFlatCString(param).get());
    }

    if (!icalstr) {
        value.Truncate();
        value.SetIsVoid(PR_TRUE);
    } else {
        value.Assign(icalstr);
    }
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::SetParameter(const nsACString &param, const nsACString &value)
{
    icalparameter_kind paramkind = 
        icalparameter_string_to_kind(PromiseFlatCString(param).get());

    if (paramkind == ICAL_NO_PARAMETER)
        return NS_ERROR_INVALID_ARG;

    // Because libical's support for manipulating parameters is weak, and
    // X-PARAMETERS doubly so, we walk the list looking for an existing one of
    // that name, and reset its value if found.
    if (paramkind == ICAL_X_PARAMETER) {
        icalparameter *icalparam = FindXParameter(mProperty, param);
        if (icalparam) {
            icalparameter_set_xvalue(icalparam,
                                     PromiseFlatCString(value).get());
            return NS_OK;
        }
        // If not found, fall through to adding a new parameter below.
    } else {
        // We could try getting an existing parameter here and resetting its
        // value, but this is easier and I don't care that much about parameter
        // performance at this point.
        RemoveParameter(param);
    }

    icalparameter *icalparam = 
        icalparameter_new_from_value_string(paramkind,
                                            PromiseFlatCString(value).get());
    if (!icalparam)
        return NS_ERROR_OUT_OF_MEMORY;

    // You might ask me "why does libical not do this for us?" and I would 
    // just nod knowingly but sadly at you in return.
    //
    // You might also, if you were not too distracted by the first question,
    // ask why we have icalproperty_set_x_name but icalparameter_set_xname.
    // More nodding would ensue.
    if (paramkind == ICAL_X_PARAMETER)
        icalparameter_set_xname(icalparam, PromiseFlatCString(param).get());
    
    icalproperty_add_parameter(mProperty, icalparam);
    // XXX check ical errno
    return NS_OK;
}

static nsresult
FillParameterName(icalparameter *icalparam, nsACString &name)
{
    const char *propname = nsnull;
    if (icalparam) {
        icalparameter_kind paramkind = icalparameter_isa(icalparam);
        if (paramkind == ICAL_X_PARAMETER)
            propname = icalparameter_get_xname(icalparam);
        else if (paramkind != ICAL_NO_PARAMETER)
            propname = icalparameter_kind_to_string(paramkind);
    }

    if (propname) {
        name.Assign(propname);
    } else {
        name.Truncate();
        name.SetIsVoid(PR_TRUE);
    }

    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::GetFirstParameterName(nsACString &name)
{
    icalparameter *icalparam =
        icalproperty_get_first_parameter(mProperty,
                                         ICAL_ANY_PARAMETER);
    return FillParameterName(icalparam, name);
}

NS_IMETHODIMP
calIcalProperty::GetNextParameterName(nsACString &name)
{
    icalparameter *icalparam =
        icalproperty_get_next_parameter(mProperty,
                                        ICAL_ANY_PARAMETER);
    return FillParameterName(icalparam, name);
}

NS_IMETHODIMP
calIcalProperty::RemoveParameter(const nsACString &param)
{
    icalparameter_kind paramkind =
        icalparameter_string_to_kind(PromiseFlatCString(param).get());

    if (paramkind == ICAL_NO_PARAMETER || paramkind == ICAL_X_PARAMETER)
        return NS_ERROR_INVALID_ARG;

    icalproperty_remove_parameter(mProperty, paramkind);
    // XXX check ical errno
    return NS_OK;
}

NS_IMETHODIMP
calIcalProperty::ClearXParameters()
{
    int oldcount, paramcount = 0;
    do {
        oldcount = paramcount;
        icalproperty_remove_parameter(mProperty, ICAL_X_PARAMETER);
        paramcount = icalproperty_count_parameters(mProperty);
    } while (oldcount != paramcount);
    return NS_OK;
}

class calIcalComponent : public calIIcalComponent
{
public:
    calIcalComponent(icalcomponent *ical, calIIcalComponent *parent) : 
        mComponent(ical), mParent(parent)
    {
        mTimezones.Init();
    }

    virtual ~calIcalComponent()
    {
        if (!mParent)
            icalcomponent_free(mComponent);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_CALIICALCOMPONENT
protected:
    
    nsresult GetDateTimeAttribute(icalproperty_kind kind, calIDateTime ** dtp);
    nsresult SetDateTimeAttribute(icalproperty_kind kind, calIDateTime * dt);
    
    nsresult SetPropertyValue(icalproperty_kind kind, icalvalue *val);
    nsresult SetProperty(icalproperty_kind kind, icalproperty *prop);

    nsresult GetStringProperty(icalproperty_kind kind, nsACString &str)
    {
        icalproperty *prop =
            icalcomponent_get_first_property(mComponent, kind);
        if (!prop) {
            str.Truncate();
            str.SetIsVoid(PR_TRUE);
        } else {
            str.Assign(icalvalue_get_string(icalproperty_get_value(prop)));
        }
        return NS_OK;
    }

    nsresult SetStringProperty(icalproperty_kind kind, const nsACString &str)
    {
        icalvalue *val = nsnull;
        if (!str.IsVoid()) {
            val = icalvalue_new_string(PromiseFlatCString(str).get());
            if (!val)
                return NS_ERROR_OUT_OF_MEMORY;
        }
        return SetPropertyValue(kind, val);
    }

    nsresult GetIntProperty(icalproperty_kind kind, PRInt32 *valp)
    {
        icalproperty *prop =
            icalcomponent_get_first_property(mComponent, kind);
        if (!prop)
            *valp = calIIcalComponent::INVALID_VALUE;
        else
            *valp = (PRInt32)icalvalue_get_integer(icalproperty_get_value(prop));
        return NS_OK;
    }

    nsresult SetIntProperty(icalproperty_kind kind, PRInt32 i)
    {
        icalvalue *val = icalvalue_new_integer(i);
        if (!val)
            return NS_ERROR_OUT_OF_MEMORY;
        return SetPropertyValue(kind, val);
    }

    void ClearAllProperties(icalproperty_kind kind);

    nsresult Serialize(char **icalstr);
    icalcomponent *mComponent;
    nsCOMPtr<calIIcalComponent> mParent;
    nsInterfaceHashtable<nsCStringHashKey, calIIcalComponent> mTimezones;
};

static nsresult unwrapVTimeZoneComponent(
    calIIcalComponent *tzcal, nsCOMPtr<calIIcalComponent> & rtz)
{
    NS_ENSURE_ARG_POINTER(tzcal);
    
    nsresult rv;

    // verify that we're dealing with a VTIMEZONE in a VCALENDAR
    nsCAutoString s;
    nsCOMPtr<calIIcalComponent> comp;

    rv = tzcal->GetComponentType(s);
    if (NS_FAILED(rv) || !s.EqualsLiteral("VCALENDAR")) {
        NS_WARNING("unwrapVTimeZoneComponent() - "
                   "component given is not a VCALENDAR");
        return NS_ERROR_FAILURE;
    }
    rv = tzcal->GetFirstSubcomponent(NS_LITERAL_CSTRING("VTIMEZONE"),
                                     getter_AddRefs(rtz));
    if (NS_FAILED(rv) || !rtz) {
        NS_WARNING("unwrapVTimeZoneComponent - "
                   "no VTIMEZONE subcomponent");
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

static nsresult unwrapVTimeZoneComponentWithId(
    calIIcalComponent *tzcal,
    nsACString & rtzid, nsCOMPtr<calIIcalComponent> & rtz)
{
    nsresult rv = unwrapVTimeZoneComponent(tzcal, rtz);
    if (NS_FAILED(rv))
        return rv;
    // fetch the tzid
    nsCOMPtr<calIIcalProperty> tzidProp;
    rv = rtz->GetFirstProperty(NS_LITERAL_CSTRING("TZID"),
                               getter_AddRefs(tzidProp));
    if (NS_FAILED(rv) || !tzidProp) {
        NS_WARNING("unwrapVTimeZoneComponentWithId - failed to get TZID");
        return NS_ERROR_FAILURE;
    }    
    return tzidProp->GetValue(rtzid);
}

NS_IMETHODIMP
calIcalComponent::AddTimezoneReference(calIIcalComponent *aTimezone)
{
    nsCAutoString tzid;
    nsCOMPtr<calIIcalComponent> tz;
    nsresult rv = unwrapVTimeZoneComponentWithId(aTimezone, tzid, tz);
    if (NS_FAILED(rv))
        return rv;
    
    // figure out if we already have this tzid 
    if (mTimezones.Get(tzid, nsnull))
        return NS_OK;

    if (!mTimezones.Put(tzid, aTimezone))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
TimezoneHashToTimezoneArray(const nsACString& aTzid,
                            calIIcalComponent *aComponent,
                            void *aArg)
{
    calIIcalComponent ***arrayPtr = NS_STATIC_CAST(calIIcalComponent ***, aArg);
    NS_ADDREF(**arrayPtr = aComponent);
    (*arrayPtr)++;
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
calIcalComponent::GetReferencedTimezones(PRUint32 *aCount,
                                         calIIcalComponent ***aTimezones)
{
    PRUint32 count = mTimezones.Count();

    if (count == 0) {
        *aCount = 0;
        *aTimezones = nsnull;
        return NS_OK;
    }

    calIIcalComponent **timezones = (calIIcalComponent **) nsMemory::Alloc(sizeof(calIIcalComponent*) * count);
    if (!timezones)
        return NS_ERROR_OUT_OF_MEMORY;

    // tzptr will get used as an iterator by the enumerator function
    calIIcalComponent **tzptr = timezones;
    mTimezones.EnumerateRead(TimezoneHashToTimezoneArray, (void *) &tzptr);

    *aTimezones = timezones;
    *aCount = count;
    return NS_OK;
}

nsresult
calIcalComponent::SetPropertyValue(icalproperty_kind kind, icalvalue *val)
{
    ClearAllProperties(kind);
    if (!val)
        return NS_OK;

    icalproperty *prop = icalproperty_new(kind);
    if (!prop) {
        icalvalue_free(val);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    icalproperty_set_value(prop, val);
    icalcomponent_add_property(mComponent, prop);
    return NS_OK;
}

nsresult
calIcalComponent::SetProperty(icalproperty_kind kind, icalproperty *prop)
{
    ClearAllProperties(kind);
    if (!prop)
        return NS_OK;
    icalcomponent_add_property(mComponent, prop);
    return NS_OK;
}

#define COMP_STRING_TO_GENERAL_ENUM_ATTRIBUTE(Attrname, ICALNAME, lcname) \
NS_IMETHODIMP                                                           \
calIcalComponent::Get##Attrname(nsACString &str)                        \
{                                                                       \
    PRInt32 val;                                                        \
    nsresult rv = GetIntProperty(ICAL_##ICALNAME##_PROPERTY, &val);     \
    if (NS_FAILED(rv))                                                  \
        return rv;                                                      \
    if (val == -1) {                                                    \
        str.Truncate();                                                 \
        str.SetIsVoid(PR_TRUE);                                         \
    } else {                                                            \
        str.Assign(icalproperty_enum_to_string(val));                   \
    }                                                                   \
    return NS_OK;                                                       \
}                                                                       \
                                                                        \
NS_IMETHODIMP                                                           \
calIcalComponent::Set##Attrname(const nsACString &str)                  \
{                                                                       \
    int val = icalproperty_string_to_enum(PromiseFlatCString(str).get()); \
    icalvalue *ival = icalvalue_new_##lcname((icalproperty_##lcname)val); \
    return SetPropertyValue(ICAL_##ICALNAME##_PROPERTY, ival);          \
}                                                                       \

#define COMP_STRING_TO_ENUM_ATTRIBUTE(Attrname, ICALNAME, lcname)       \
NS_IMETHODIMP                                                           \
calIcalComponent::Get##Attrname(nsACString &str)                        \
{                                                                       \
    PRInt32 val;                                                        \
    nsresult rv = GetIntProperty(ICAL_##ICALNAME##_PROPERTY, &val);     \
    if (NS_FAILED(rv))                                                  \
        return rv;                                                      \
    if (val == -1) {                                                    \
        str.Truncate();                                                 \
        str.SetIsVoid(PR_TRUE);                                         \
    } else {                                                            \
        str.Assign(icalproperty_##lcname##_to_string((icalproperty_##lcname)val)); \
    }                                                                   \
    return NS_OK;                                                       \
}                                                                       \
                                                                        \
NS_IMETHODIMP                                                           \
calIcalComponent::Set##Attrname(const nsACString &str)                  \
{                                                                       \
    icalproperty *prop = nsnull;                                        \
    if (!str.IsVoid()) {                                                \
        icalproperty_##lcname val =                                     \
            icalproperty_string_to_##lcname(PromiseFlatCString(str).get()); \
        prop = icalproperty_new_##lcname(val);                          \
        if (!prop)                                                      \
            return NS_ERROR_OUT_OF_MEMORY; /* XXX map errno */          \
    }                                                                   \
    return SetProperty(ICAL_##ICALNAME##_PROPERTY, prop);               \
}                                                                       \

#define COMP_GENERAL_STRING_ATTRIBUTE(Attrname, ICALNAME)       \
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

#define COMP_STRING_ATTRIBUTE(Attrname, ICALNAME, lcname)       \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(nsACString &str)                \
{                                                               \
    return GetStringProperty(ICAL_##ICALNAME##_PROPERTY, str);  \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(const nsACString &str)          \
{                                                               \
    icalproperty *prop =                                        \
        icalproperty_new_##lcname(PromiseFlatCString(str).get()); \
    return SetProperty(ICAL_##ICALNAME##_PROPERTY, prop);       \
}

#define COMP_GENERAL_INT_ATTRIBUTE(Attrname, ICALNAME)          \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(PRInt32 *valp)                  \
{                                                               \
    return GetIntProperty(ICAL_##ICALNAME##_PROPERTY, valp);    \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(PRInt32 val)                    \
{                                                               \
    return SetIntProperty(ICAL_##ICALNAME##_PROPERTY, val);     \
}                                                               \

#define COMP_ENUM_ATTRIBUTE(Attrname, ICALNAME, lcname)         \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(PRInt32 *valp)                  \
{                                                               \
    return GetIntProperty(ICAL_##ICALNAME##_PROPERTY, valp);    \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(PRInt32 val)                    \
{                                                               \
    icalproperty *prop =                                        \
      icalproperty_new_##lcname((icalproperty_##lcname)val);    \
    return SetProperty(ICAL_##ICALNAME##_PROPERTY, prop);       \
}                                                               \

#define COMP_INT_ATTRIBUTE(Attrname, ICALNAME, lcname)          \
NS_IMETHODIMP                                                   \
calIcalComponent::Get##Attrname(PRInt32 *valp)                  \
{                                                               \
    return GetIntProperty(ICAL_##ICALNAME##_PROPERTY, valp);    \
}                                                               \
                                                                \
NS_IMETHODIMP                                                   \
calIcalComponent::Set##Attrname(PRInt32 val)                    \
{                                                               \
    icalproperty *prop = icalproperty_new_##lcname(val);        \
    return SetProperty(ICAL_##ICALNAME##_PROPERTY, prop);       \
}                                                               \

nsresult calIcalComponent::GetDateTimeAttribute(
    icalproperty_kind kind, calIDateTime ** dtp)
{
    icalproperty *prop = icalcomponent_get_first_property(mComponent, kind);
    calDateTime *dt;
    if (!prop) {
        *dtp = nsnull;  /* invalid date */
        return NS_OK;
    }
    
    struct icaltimetype itt =
        icalvalue_get_datetime(icalproperty_get_value(prop));

    // Need to get the tzid param. Unfortunatly, libical tends to return raw
    // ics strings, with quotes and everything. That's not what we want. Need
    // to work around.
    icalparameter_kind tzkind = icalparameter_string_to_kind("TZID");
    icalparameter* tzparam = icalproperty_get_first_parameter(prop, tzkind);
    const char *tzid = nsnull;
    if (tzparam)
        tzid = icalparameter_get_xvalue(tzparam);

    if (tzid) {
        // look up tz in ics service, which btw caches commonly used tzones.
        // this hides errors from incorrect ics files, which could state
        // a TZID that is not present in the ics file.
        // The other way round, it makes this product more error tolerant.
        nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
        nsCOMPtr<calIIcalComponent> tzcal;
        nsresult rv = ics->GetTimezone(nsDependentCString(tzid),
                                       getter_AddRefs(tzcal));
        if (NS_FAILED(rv) || !tzcal) {
            /* Now, see, libical sucks.
               We have to walk up to our parent VCALENDAR and try to find
               this tzid */
            icalcomponent *vcalendar = mComponent;
            while (vcalendar && icalcomponent_isa(vcalendar) !=
                   ICAL_VCALENDAR_COMPONENT)
                vcalendar = icalcomponent_get_parent(vcalendar);
            if (!vcalendar) {
                NS_WARNING("VCALENDAR not found while looking for VTIMEZONE!");
                return NS_ERROR_FAILURE;
            }
            icaltimezone *zone = icalcomponent_get_timezone(vcalendar, tzid);
            if (!zone) {
                NS_WARNING("Can't find specified VTIMEZONE in VCALENDAR!");
                return NS_ERROR_FAILURE;
            }
            icalcomponent * tzcomp = icaltimezone_get_component(zone);
            if (tzcomp == nsnull) {
                NS_WARNING("Can't get VTIMEZONE!");
                return NS_ERROR_FAILURE;
            }
            
            // create new VCALENDAR out of VTIMEZONE and publish to ics service.
            // calDateTime works on ics service's timezones.
            rv = ics->CreateIcalComponent(nsDependentCString("VCALENDAR"),
                                          getter_AddRefs(tzcal));
            if (NS_FAILED(rv))
                return rv;
            // VTIMEZONE:
            tzcomp = icalcomponent_new_clone(tzcomp);
            nsCOMPtr<calIIcalComponent> subComp(
                new calIcalComponent(tzcomp, nsnull) );
            if (!subComp) {
                icalcomponent_free(tzcomp);
                return NS_ERROR_OUT_OF_MEMORY;
            }
            rv = tzcal->AddSubcomponent(subComp);
            if (NS_FAILED(rv))
                return rv;
            
            // publish to ics service:
            nsCOMPtr<calIIcalComponent> const tzcal_(tzcal);
            rv = ics->AddTimezone(
                tzcal_, // unknown latitude, longitude:
                nsDependentCString(""), nsDependentCString(""),
                getter_AddRefs(tzcal));
            if (NS_FAILED(rv))
                return rv;
        }
        // assure timezone is referenced in component:
        AddTimezoneReference(tzcal);
        // correct itt which would else appear floating:
        itt.zone = icalcomponent_get_timezone(tzcal->GetIcalComponent(), tzid);
        itt.is_utc = 0;
    }
    dt = new calDateTime(&itt);
    if (!dt)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*dtp = dt);
    return NS_OK;
}

nsresult calIcalComponent::SetDateTimeAttribute(
    icalproperty_kind kind, calIDateTime * dt)
{
    ClearAllProperties(kind);
    PRBool isValid, wantTz = PR_FALSE;
    if (!dt || NS_FAILED(dt->GetIsValid(&isValid)) || !isValid) {
        return NS_OK;
    }
    struct icaltimetype itt;
    dt->ToIcalTime(&itt);
    nsCAutoString tzid;
    if (NS_SUCCEEDED(dt->GetTimezone(tzid))) {
        if (!tzid.IsEmpty() && !tzid.EqualsLiteral("UTC") &&
            !tzid.EqualsLiteral("floating")) {
            wantTz = PR_TRUE;
            nsCOMPtr<calIICSService> ics = do_GetService(kCalICSService);
            nsCOMPtr<calIIcalComponent> tz;
            ics->GetTimezone(tzid, getter_AddRefs(tz));
            if (!tz) {
                /* Uh, we didn't find this timezone.
                   This should somehow be bad. */
                NS_WARNING("Timezone was not found in database!");
                wantTz = PR_FALSE;
            } else {
                AddTimezoneReference(tz);
            }
        }
    }
    icalvalue *val = icalvalue_new_datetime(itt);
    if (!val)
        return NS_ERROR_OUT_OF_MEMORY;
    icalproperty *prop = icalproperty_new(kind);
    if (!prop) {
        icalvalue_free(val);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    icalproperty_set_value(prop, val);
    if (wantTz) {
        icalproperty_set_parameter_from_string(
            prop, "TZID", nsPromiseFlatCString(tzid).get());
    }
    icalcomponent_add_property(mComponent, prop);
    return NS_OK;
}

#define RO_COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                      \
NS_IMETHODIMP                                                           \
calIcalComponent::Get##Attrname(calIDateTime **dtp)                     \
{                                                                       \
    return GetDateTimeAttribute(ICAL_##ICALNAME##_PROPERTY, dtp);       \
}

#define COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                         \
RO_COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                              \
                                                                        \
NS_IMETHODIMP                                                           \
calIcalComponent::Set##Attrname(calIDateTime *dt)                       \
{                                                                       \
    return SetDateTimeAttribute(ICAL_##ICALNAME##_PROPERTY, dt);        \
}

#define RO_COMP_DURATION_ATTRIBUTE(Attrname, ICALNAME)                  \
NS_IMETHODIMP                                                           \
calIcalComponent::Get##Attrname(calIDuration **dtp)                     \
{                                                                       \
    icalproperty *prop =                                                \
        icalcomponent_get_first_property(mComponent,                    \
                                         ICAL_##ICALNAME##_PROPERTY);   \
    calDuration *dt;                                                    \
    if (!prop) {                                                        \
        *dtp = nsnull;  /* invalid duration */                          \
        return NS_OK;                                                   \
    }                                                                   \
    struct icaldurationtype idt =                                       \
        icalvalue_get_duration(icalproperty_get_value(prop));           \
    dt = new calDuration(&idt);                                         \
    if (!dt)                                                            \
        return NS_ERROR_OUT_OF_MEMORY;                                  \
    NS_ADDREF(*dtp = dt);                                               \
    return NS_OK;                                                       \
}

NS_IMPL_ISUPPORTS1(calIcalComponent, calIIcalComponent)

icalcomponent*
calIcalComponent::GetIcalComponent()
{
    return mComponent;
}

NS_IMETHODIMP
calIcalComponent::GetFirstSubcomponent(const nsACString& kind,
                                       calIIcalComponent **subcomp)
{
    icalcomponent_kind compkind =
        icalcomponent_string_to_kind(PromiseFlatCString(kind).get());

    // Maybe someday I'll support X-COMPONENTs
    if (compkind == ICAL_NO_COMPONENT || compkind == ICAL_X_COMPONENT)
        return NS_ERROR_INVALID_ARG;

    icalcomponent *ical =
        icalcomponent_get_first_component(mComponent, compkind);
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
calIcalComponent::GetNextSubcomponent(const nsACString& kind,
                                      calIIcalComponent **subcomp)
{
    icalcomponent_kind compkind =
        icalcomponent_string_to_kind(PromiseFlatCString(kind).get());

    // Maybe someday I'll support X-COMPONENTs
    if (compkind == ICAL_NO_COMPONENT || compkind == ICAL_X_COMPONENT)
        return NS_ERROR_INVALID_ARG;

    icalcomponent *ical =
        icalcomponent_get_next_component(mComponent, compkind);
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
calIcalComponent::GetComponentType(nsACString &componentType)
{
    componentType.Assign(icalcomponent_kind_to_string(icalcomponent_isa(mComponent)));
    return NS_OK;
}


COMP_STRING_ATTRIBUTE(Uid, UID, uid)
COMP_STRING_ATTRIBUTE(Prodid, PRODID, prodid)
COMP_STRING_ATTRIBUTE(Version, VERSION, version)
COMP_STRING_TO_ENUM_ATTRIBUTE(Method, METHOD, method)
COMP_STRING_TO_ENUM_ATTRIBUTE(Status, STATUS, status)
COMP_STRING_TO_GENERAL_ENUM_ATTRIBUTE(Transp, TRANSP, transp)
COMP_STRING_ATTRIBUTE(Summary, SUMMARY, summary)
COMP_STRING_ATTRIBUTE(Description, DESCRIPTION, description)
COMP_STRING_ATTRIBUTE(Location, LOCATION, location)
COMP_STRING_ATTRIBUTE(Categories, CATEGORIES, categories)
COMP_STRING_ATTRIBUTE(URL, URL, url)
COMP_INT_ATTRIBUTE(Priority, PRIORITY, priority)
COMP_STRING_TO_GENERAL_ENUM_ATTRIBUTE(IcalClass, CLASS, class)
COMP_DATE_ATTRIBUTE(StartTime, DTSTART)
COMP_DATE_ATTRIBUTE(EndTime, DTEND)
RO_COMP_DURATION_ATTRIBUTE(Duration, DURATION)
COMP_DATE_ATTRIBUTE(DueTime, DUE)
COMP_DATE_ATTRIBUTE(StampTime, DTSTAMP)
COMP_DATE_ATTRIBUTE(LastModified, LASTMODIFIED)
COMP_DATE_ATTRIBUTE(CreatedTime, CREATED)
COMP_DATE_ATTRIBUTE(CompletedTime, COMPLETED)
COMP_DATE_ATTRIBUTE(RecurrenceId, RECURRENCEID)

void
calIcalComponent::ClearAllProperties(icalproperty_kind kind)
{
    for (icalproperty *prop =
             icalcomponent_get_first_property(mComponent, kind),
             *next;
         prop;
         prop = next) {
        next = icalcomponent_get_next_property(mComponent, kind);
        icalcomponent_remove_property(mComponent, prop);
    }
}

PR_STATIC_CALLBACK(PLDHashOperator)
AddTimezoneComponentToIcal(const nsACString& aTzid,
                           calIIcalComponent *aTimezone,
                           void *aArg)
{
    icalcomponent *comp = NS_STATIC_CAST(icalcomponent *, aArg);
    icalcomponent *tzcomp = icalcomponent_new_clone(aTimezone->GetIcalComponent());
    icalcomponent_merge_component(comp, tzcomp);
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
calIcalComponent::SerializeToICS(nsACString &serialized)
{
    char *icalstr;

    nsresult rv = Serialize(&icalstr);
    if (NS_FAILED(rv)) {
        return rv;
    }

    serialized.Assign(icalstr);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::SerializeToICSStream(nsIInputStream **aStreamResult)
{
    char *icalstr;

    nsresult rv = Serialize(&icalstr);
    if (NS_FAILED(rv)) {
        return rv;
    }

    // NS_NewCStringInputStream copies the dependent string into the
    // input stream that's handed back.  This copy is necessary because 
    // we don't really own icalstr; it's one of libical's ring buffers
    return NS_NewCStringInputStream(aStreamResult, 
                                    nsDependentCString(icalstr));
}

nsresult
calIcalComponent::Serialize(char **icalstr)
{
    // add the timezone bits
    if (icalcomponent_isa(mComponent) == ICAL_VCALENDAR_COMPONENT &&
        mTimezones.Count() > 0)
    {
        mTimezones.EnumerateRead(AddTimezoneComponentToIcal, mComponent);
    }

    *icalstr = icalcomponent_as_ical_string(mComponent);
    if (!*icalstr) {
#ifdef DEBUG
        fprintf(stderr, "Error serializing: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        // The return values in calIError match with libical errnos,
        // so no need for a conversion table or anything.
        return calIErrors::ICS_ERROR_BASE + icalerrno;
    }

    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::Clone(calIIcalComponent **_retval)
{
    icalcomponent * cloned = icalcomponent_new_clone(mComponent);
    if (cloned == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    calIcalComponent *comp = new calIcalComponent(cloned, nsnull);
    if (comp == nsnull) {
        icalcomponent_free(cloned);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*_retval = comp);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::AddSubcomponent(calIIcalComponent *comp)
{
    /* XXX mildly unsafe assumption here.
     * To fix it, I will:
     * - check the object's classinfo to find out if I have one of my
     *   own objects, and if not
     * - use comp->serializeToICS and reparse to create a copy.
     *
     * I should probably also return the new/reused component so that the
     * caller has something it can poke at all live-like.
     */
    calIcalComponent *ical = NS_STATIC_CAST(calIcalComponent *, comp);
    if(ical->mParent)
        ical->mComponent = icalcomponent_new_clone(ical->mComponent);

    PRUint32 tzCount = 0;
    calIIcalComponent **timezones = nsnull;;
    nsresult rv;

    rv = ical->GetReferencedTimezones(&tzCount, &timezones);
    if (NS_FAILED(rv))
        return rv;

    PRBool failed = PR_FALSE;
    for (PRUint32 i = 0; i < tzCount; i++) {
        if (!failed) {
            rv = this->AddTimezoneReference(timezones[i]);
            if (NS_FAILED(rv))
                failed = PR_TRUE;
        }

        NS_RELEASE(timezones[i]);
    }

    nsMemory::Free(timezones);

    if (failed)
        return rv;

    icalcomponent_add_component(mComponent, ical->mComponent);
    ical->mParent = this;
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::RemoveSubcomponent(calIIcalComponent *comp)
{
    calIcalComponent *ical = NS_STATIC_CAST(calIcalComponent *, comp);
    icalcomponent_remove_component(mComponent, ical->mComponent);
    ical->mParent = nsnull;
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::GetFirstProperty(const nsACString &kind,
                                   calIIcalProperty **prop)
{
    icalproperty_kind propkind =
        icalproperty_string_to_kind(PromiseFlatCString(kind).get());

    if (propkind == ICAL_NO_PROPERTY)
        return NS_ERROR_INVALID_ARG;

    icalproperty *icalprop = nsnull;
    if (propkind == ICAL_X_PROPERTY) {
        for (icalprop = 
                 icalcomponent_get_first_property(mComponent, ICAL_X_PROPERTY);
             icalprop;
             icalprop = icalcomponent_get_next_property(mComponent,
                                                        ICAL_X_PROPERTY)) {
            
            if (kind.Equals(icalproperty_get_x_name(icalprop)))
                break;
        }
    } else {
        icalprop = icalcomponent_get_first_property(mComponent, propkind);
    }

    if (!icalprop) {
        *prop = nsnull;
        return NS_OK;
    }

    *prop = new calIcalProperty(icalprop, this);
    if (!*prop)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*prop);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::GetNextProperty(const nsACString &kind,
                                  calIIcalProperty **prop)
{
    icalproperty_kind propkind =
        icalproperty_string_to_kind(PromiseFlatCString(kind).get());

    if (propkind == ICAL_NO_PROPERTY)
        return NS_ERROR_INVALID_ARG;
    icalproperty *icalprop = nsnull;
    if (propkind == ICAL_X_PROPERTY) {
        for (icalprop = 
                 icalcomponent_get_next_property(mComponent, ICAL_X_PROPERTY);
             icalprop;
             icalprop = icalcomponent_get_next_property(mComponent,
                                                        ICAL_X_PROPERTY)) {
            
            if (kind.Equals(icalproperty_get_x_name(icalprop)))
                break;
        }
    } else {
        icalprop = icalcomponent_get_next_property(mComponent, propkind);
    }

    if (!icalprop) {
        *prop = nsnull;
        return NS_OK;
    }

    *prop = new calIcalProperty(icalprop, this);
    if (!*prop)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*prop);
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::AddProperty(calIIcalProperty *prop)
{
    // XXX like AddSubcomponent, this is questionable
    calIcalProperty *ical = NS_STATIC_CAST(calIcalProperty *, prop);
    if(ical->mParent)
        ical->mProperty = icalproperty_new_clone(ical->mProperty);
    icalcomponent_add_property(mComponent, ical->mProperty);
    ical->mParent = this;
    return NS_OK;
}

NS_IMETHODIMP
calIcalComponent::RemoveProperty(calIIcalProperty *prop)
{
    // XXX like AddSubcomponent, this is questionable
    calIcalProperty *ical = NS_STATIC_CAST(calIcalProperty *, prop);
    icalcomponent_remove_property(mComponent, ical->mProperty);
    ical->mParent = nsnull;
    return NS_OK;
}

NS_IMPL_ISUPPORTS1_CI(calICSService, calIICSService)

calICSService::calICSService()
{
    mTzHash.Init();
}

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
        // The return values is calIError match with ical errors,
        // so no need for a conversion table or anything.
        return calIErrors::ICS_ERROR_BASE + icalerrno;
    }
    calIcalComponent *comp = new calIcalComponent(ical, nsnull);
    if (!comp) {
        icalcomponent_free(ical);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*component = comp);
    return NS_OK;
}

NS_IMETHODIMP
calICSService::CreateIcalComponent(const nsACString &kind,
                                   calIIcalComponent **comp)
{
    icalcomponent_kind compkind = 
        icalcomponent_string_to_kind(PromiseFlatCString(kind).get());

    // Maybe someday I'll support X-COMPONENTs
    if (compkind == ICAL_NO_COMPONENT || compkind == ICAL_X_COMPONENT)
        return NS_ERROR_INVALID_ARG;

    icalcomponent *ical = icalcomponent_new(compkind);
    if (!ical)
        return NS_ERROR_OUT_OF_MEMORY; // XXX translate

    *comp = new calIcalComponent(ical, nsnull);
    if (!*comp) {
        icalcomponent_free(ical);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*comp);
    return NS_OK;
}

NS_IMETHODIMP
calICSService::CreateIcalProperty(const nsACString &kind,
                                  calIIcalProperty **prop)
{
    icalproperty_kind propkind = 
        icalproperty_string_to_kind(PromiseFlatCString(kind).get());

    if (propkind == ICAL_NO_PROPERTY)
        return NS_ERROR_INVALID_ARG;

    icalproperty *icalprop = icalproperty_new(propkind);
    if (!icalprop)
        return NS_ERROR_OUT_OF_MEMORY; // XXX translate

    if (propkind == ICAL_X_PROPERTY)
        icalproperty_set_x_name(icalprop, PromiseFlatCString(kind).get());

    *prop = new calIcalProperty(icalprop, nsnull);
    if (!*prop)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*prop);
    return NS_OK;
}

/**
 ** Timezone bits
 **/

// include tzdata, to get ical_timezone_data_struct
#include "tzdata.c"

/* XXX We should really be using the global defined in tzdata.c for this. */
#define MOZCAL_TZID_PREFIX "/mozilla.org/20070129_1/"

#define STRLEN_ARGS(x) x, sizeof(x) - 1

TimezoneEntry const* calICSService::getTimezoneEntry(nsACString const& tzid)
{
    TimezoneEntry * pEntry = nsnull;
    PRBool const bFound = mTzHash.Get(tzid, &pEntry);
    if ((!bFound || pEntry == nsnull) &&
        StringBeginsWith(tzid,
                         nsDependentCString(STRLEN_ARGS(MOZCAL_TZID_PREFIX)))) {
        // not found, look up in static tab:
        for (PRUint32 i = 0; ical_timezone_data[i].tzid != nsnull; ++i) {
            if (tzid.Equals(ical_timezone_data[i].tzid)) {
                ical_timezone_data_struct const& tzdata = ical_timezone_data[i];
                nsCOMPtr<calIIcalComponent> icalComp;
                if (NS_FAILED(
                        ParseICS(nsDependentCString(tzdata.icstimezone),
                                 getter_AddRefs(icalComp) )))
                    return 0;
                
                nsAutoPtr<TimezoneEntry> entry(
                    new TimezoneEntry(nsDependentCString(tzdata.latitude),
                                      nsDependentCString(tzdata.longitude),
                                      icalComp) );
                if (!mTzHash.Put(tzid, entry.get()))
                    return 0;
                pEntry = entry.forget();
                break;
            }
        }
    }
    return pEntry;
}

NS_IMETHODIMP calICSService::AddTimezone(
    calIIcalComponent * tzCal, const nsACString & tzLatitude,
    const nsACString & tzLongitude, calIIcalComponent **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsCAutoString tzid;
    nsCOMPtr<calIIcalComponent> tz;
    nsresult rv = unwrapVTimeZoneComponentWithId(tzCal, tzid, tz);
    if (NS_FAILED(rv))
        return rv;
    
    TimezoneEntry const* pEntry = getTimezoneEntry(tzid);
    if (pEntry == nsnull) {
        nsAutoPtr<TimezoneEntry> entry(
            new TimezoneEntry(tzLatitude, tzLongitude, tzCal) );
        if (!mTzHash.Put(tzid, entry.get()))
            return NS_ERROR_OUT_OF_MEMORY;
        pEntry = entry.forget();
    }
    NS_ADDREF(*_retval = pEntry->mTzCal);
    return NS_OK;
}

NS_IMETHODIMP
calICSService::GetTimezone(const nsACString& tzid,
                           calIIcalComponent **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    TimezoneEntry const* pEntry = getTimezoneEntry(tzid);
    if (pEntry == nsnull)
        return calIErrors::INVALID_TIMEZONE;
    NS_ADDREF(*_retval = pEntry->mTzCal);
    return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator) TimezoneEntryHashToTimezoneIdArray(
    const nsACString& aTzid, TimezoneEntry * pEntry, void * aArg)
{
    nsCStringArray * pArray = NS_STATIC_CAST(nsCStringArray *, aArg);
    // ignore static ones, those are added nevertheless:
    if (!StringBeginsWith(
            aTzid, nsDependentCString(STRLEN_ARGS(MOZCAL_TZID_PREFIX))))
        pArray->AppendCString(aTzid);
    return PL_DHASH_NEXT;
}

NS_IMETHODIMP
calICSService::GetTimezoneIds(nsIUTF8StringEnumerator **aTzids)
{
    NS_ENSURE_ARG_POINTER(aTzids);
    PRUint32 const arlen =
        sizeof(ical_timezone_data) / sizeof(ical_timezone_data[0]);
    nsCStringArray * pArray = new nsCStringArray(mTzHash.Count() + arlen);
    if (pArray == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    // add added ones:
    mTzHash.EnumerateRead(TimezoneEntryHashToTimezoneIdArray, pArray);
    // add static ones:
    for (PRUint32 i = 0; ical_timezone_data[i].tzid != nsnull; ++i) {
        pArray->AppendCString(nsDependentCString(ical_timezone_data[i].tzid));
    }
    pArray->Sort();
    return NS_NewAdoptingUTF8StringEnumerator(aTzids, pArray);
}

NS_IMETHODIMP
calICSService::GetTimezoneLatitude(const nsACString& tzid, nsACString& _retval)
{
    TimezoneEntry const* pEntry = getTimezoneEntry(tzid);
    if (pEntry == nsnull)
        return NS_ERROR_FAILURE;
    _retval = pEntry->mLatitude;
    return NS_OK;
}

NS_IMETHODIMP
calICSService::GetTimezoneLongitude(const nsACString& tzid, nsACString& _retval)
{
    TimezoneEntry const* pEntry = getTimezoneEntry(tzid);
    if (pEntry == nsnull)
        return NS_ERROR_FAILURE;
    _retval = pEntry->mLongitude;
    return NS_OK;
}

NS_IMETHODIMP
calICSService::GetTzIdPrefix(nsACString& _retval)
{
    _retval.Assign(gTzIdPrefix);

    return NS_OK;
}

/**
 * LatestTzId
 * Gets updated timezone ID (tzId) for a given tzId.
 * - If this isn't a mozilla.org tzId:
 *      return nothing
 * - If the tzId name was changed or the tzId was deleted:
 *      return updated tzId
 * - If the tzId's dtstamp is different from our current dtstamp:
 *      return updated tzId
 * - Otherwise, don't change the tzId and return nothing.
 */
NS_IMETHODIMP
calICSService::LatestTzId(const nsACString& tzid, nsACString& _retval) {
    // Ensure _retval is both initialized and empty.
    _retval.Truncate();

    // If it doesn't start with "/mozilla.org/" then it isn't ours.
    if (!StringBeginsWith(
            tzid, nsDependentCString(STRLEN_ARGS("/mozilla.org/")))) {
        return NS_OK;
    }

    // We know that our tzids look like "/mozilla.org/<dtstamp>/continent/..."
    // The ending of the mozilla prefix is the index of that slash before the
    // continent. Therefore, we start looking for the prefix-ending slash
    // after position 13.
    PRInt32 prefixEnd = tzid.FindChar('/', 13);
    PRInt32 continentEnd = tzid.FindChar('/', prefixEnd + 1);

    // Go through our list of deletions and changes in Olsen, and update
    // these to entirely new zones.
    nsCAutoString continent(Substring(tzid, prefixEnd + 1,
                                      continentEnd - (prefixEnd + 1)));

    // XXX We want to make this table-driven at some point in the future.

    if (continent.EqualsLiteral("Africa")) {
        if (tzid.EqualsLiteral("/mozilla.org/20050126_1/Africa/Asmera")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/Africa/Asmara");
        } else if (tzid.EqualsLiteral("/mozilla.org/20050126_1/Africa/Timbuktu")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/Africa/Bamako");
        }
    } else if (continent.EqualsLiteral("Atlantic")) {
        if (tzid.EqualsLiteral("/mozilla.org/20050126_1/Atlantic/Faeroe")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/Atlantic/Faroe");
        }
    } else if (continent.EqualsLiteral("America")) {
        if (tzid.EqualsLiteral("/mozilla.org/20050126_1/America/Argentina/ComodRivadavia")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/America/Argentica/Catamarca");
        } else if (tzid.EqualsLiteral("/mozilla.org/20050126_1/America/Louisville")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/America/Kentucky/Louisville");
        }
    } else if (continent.EqualsLiteral("Europe")) {
        if (tzid.EqualsLiteral("/mozilla.org/20050126_1/Europe/Belfast")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/Europe/London");
        }
    } else if (continent.EqualsLiteral("Pacific")) {
        if (tzid.EqualsLiteral("/mozilla.org/20050126_1/Pacific/Yap")) {
            _retval.AssignLiteral("/mozilla.org/20070129_1/Pacific/Truk");
        }
    }

    // If the zone's locale was not changed and the zone was not
    // deleted (i.e. we haven't already created _retVal, then
    // we just need to update the timestamp.
    if ((_retval.Length() == 0) &&
        (gTzIdPrefix != Substring(tzid, 0, prefixEnd + 1))) {
        // TODO: This assumes that new zones will always be later. If we are
        // ever going to support mixing our TZID headers, then this needs to
        // be a date comparison.
        _retval = gTzIdPrefix + Substring(tzid, prefixEnd + 1,
                                          tzid.Length() - (prefixEnd + 1));
    }
    return NS_OK;
}
