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
#include "nsString.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsCRT.h"

#include "calIEvent.h"
#include "calBaseCID.h"

extern "C" {
#    include "ical.h"
}

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
    
    NS_DECL_ISUPPORTS
    NS_DECL_CALIICALPROPERTY

    friend class calIcalComponent;
protected:
    icalproperty *mProperty;
    nsCOMPtr<calIIcalComponent> mParent;
};

NS_IMPL_ISUPPORTS1(calIcalProperty, calIIcalProperty)

NS_IMETHODIMP
calIcalProperty::GetStringValue(nsACString &str)
{
    const char *icalstr = icalproperty_get_value_as_string(mProperty);
    if (!icalstr) {
        if (icalerrno == ICAL_BADARG_ERROR) {
            str.Truncate();
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
calIcalProperty::SetStringValue(const nsACString &str)
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
    const char *icalstr = icalproperty_get_name(mProperty);
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
    if (!icalparam) {
        name.Truncate();
        name.SetIsVoid(PR_TRUE);
        return NS_OK;
    }

    icalparameter_kind paramkind = icalparameter_isa(icalparam);
    if (paramkind == ICAL_X_PARAMETER)
        name.Assign(icalparameter_get_xname(icalparam));
    else
        name.Assign(icalparameter_kind_to_string(paramkind));
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
        mComponent(ical), mParent(parent) { }

    virtual ~calIcalComponent()
    {
        if (!mParent)
            icalcomponent_free(mComponent);
    }

    NS_DECL_ISUPPORTS
    NS_DECL_CALIICALCOMPONENT
protected:

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
        icalvalue *val = icalvalue_new_string(PromiseFlatCString(str).get());
        if (!val)
            return NS_ERROR_OUT_OF_MEMORY;
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

    icalcomponent              *mComponent;
    nsCOMPtr<calIIcalComponent>  mParent;
};

nsresult
calIcalComponent::SetPropertyValue(icalproperty_kind kind, icalvalue *val)
{
    ClearAllProperties(kind);
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
    if (!prop) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
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
    icalproperty_##lcname val =                                         \
        icalproperty_string_to_##lcname(PromiseFlatCString(str).get()); \
    icalproperty *prop = icalproperty_new_##lcname(val);                \
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


#define RO_COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                      \
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

#define COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                         \
RO_COMP_DATE_ATTRIBUTE(Attrname, ICALNAME)                              \
                                                                        \
NS_IMETHODIMP                                                           \
calIcalComponent::Set##Attrname(calIDateTime *dt)                       \
{                                                                       \
    struct icaltimetype itt;                                            \
    if (!dt)                                                            \
        return NS_ERROR_INVALID_ARG;                                    \
    dt->ToIcalTime(&itt);                                               \
    icalvalue *val = icalvalue_new_datetime(itt);                       \
    if (!val)                                                           \
        return NS_ERROR_OUT_OF_MEMORY;                                  \
    return SetPropertyValue(ICAL_##ICALNAME##_PROPERTY, val);           \
}

NS_IMPL_ISUPPORTS1(calIcalComponent, calIIcalComponent)

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
COMP_DATE_ATTRIBUTE(DueTime, DUE)
COMP_DATE_ATTRIBUTE(StampTime, DTSTAMP)
COMP_DATE_ATTRIBUTE(LastModified, LASTMODIFIED)
COMP_DATE_ATTRIBUTE(CreatedTime, CREATED)
COMP_DATE_ATTRIBUTE(CompletedTime, COMPLETED)

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

NS_IMETHODIMP
calIcalComponent::SerializeToICS(nsACString &serialized)
{
    char *icalstr = icalcomponent_as_ical_string(mComponent);
    if (!icalstr) {
#ifdef DEBUG
        fprintf(stderr, "Error serializing: %d (%s)\n",
                icalerrno, icalerror_strerror(icalerrno));
#endif
        return NS_ERROR_FAILURE;
    }

    serialized.Assign(icalstr);
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
