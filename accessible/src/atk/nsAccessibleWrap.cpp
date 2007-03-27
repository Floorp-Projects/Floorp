/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *   John Sun (john.sun@sun.com)
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

#include "nsMai.h"
#include "nsAccessibleWrap.h"
#include "nsAppRootAccessible.h"
#include "nsString.h"
#include "prprf.h"
#include "nsRoleMap.h"
#include "nsStateMap.h"

#include "nsMaiInterfaceComponent.h"
#include "nsMaiInterfaceAction.h"
#include "nsMaiInterfaceText.h"
#include "nsMaiInterfaceEditableText.h"
#include "nsMaiInterfaceSelection.h"
#include "nsMaiInterfaceValue.h"
#include "nsMaiInterfaceHypertext.h"
#include "nsMaiInterfaceHyperlinkImpl.h"
#include "nsMaiInterfaceTable.h"
#include "nsXPCOMStrings.h"
#include "nsComponentManagerUtils.h"
#include "nsMaiInterfaceDocument.h"
#include "nsMaiInterfaceImage.h"

extern "C" GType g_atk_hyperlink_impl_type; //defined in nsAppRootAccessible.cpp

/* MaiAtkObject */

enum {
  ACTIVATE,
  CREATE,
  DEACTIVATE,
  DESTROY,
  MAXIMIZE,
  MINIMIZE,
  RESIZE,
  RESTORE,
  LAST_SIGNAL
};

enum MaiInterfaceType {
    MAI_INTERFACE_COMPONENT, /* 0 */
    MAI_INTERFACE_ACTION,
    MAI_INTERFACE_VALUE,
    MAI_INTERFACE_EDITABLE_TEXT,
    MAI_INTERFACE_HYPERTEXT,
    MAI_INTERFACE_HYPERLINK_IMPL,
    MAI_INTERFACE_SELECTION,
    MAI_INTERFACE_TABLE,
    MAI_INTERFACE_TEXT,
    MAI_INTERFACE_DOCUMENT, 
    MAI_INTERFACE_IMAGE /* 10 */
};

static GType GetAtkTypeForMai(MaiInterfaceType type)
{
  switch (type) {
    case MAI_INTERFACE_COMPONENT:
      return ATK_TYPE_COMPONENT;
    case MAI_INTERFACE_ACTION:
      return ATK_TYPE_ACTION;
    case MAI_INTERFACE_VALUE:
      return ATK_TYPE_VALUE;
    case MAI_INTERFACE_EDITABLE_TEXT:
      return ATK_TYPE_EDITABLE_TEXT;
    case MAI_INTERFACE_HYPERTEXT:
      return ATK_TYPE_HYPERTEXT;
    case MAI_INTERFACE_HYPERLINK_IMPL:
       return g_atk_hyperlink_impl_type;
    case MAI_INTERFACE_SELECTION:
      return ATK_TYPE_SELECTION;
    case MAI_INTERFACE_TABLE:
      return ATK_TYPE_TABLE;
    case MAI_INTERFACE_TEXT:
      return ATK_TYPE_TEXT;
    case MAI_INTERFACE_DOCUMENT:
      return ATK_TYPE_DOCUMENT;
    case MAI_INTERFACE_IMAGE:
      return ATK_TYPE_IMAGE;
  }
  return G_TYPE_INVALID;
}

static const GInterfaceInfo atk_if_infos[] = {
    {(GInterfaceInitFunc)componentInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL}, 
    {(GInterfaceInitFunc)actionInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)valueInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)editableTextInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)hypertextInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)hyperlinkImplInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)selectionInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)tableInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)textInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)documentInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL},
    {(GInterfaceInitFunc)imageInterfaceInitCB,
     (GInterfaceFinalizeFunc) NULL, NULL}
};

/**
 * This MaiAtkObject is a thin wrapper, in the MAI namespace, for AtkObject
 */
struct MaiAtkObject
{
    AtkObject parent;
    /*
     * The nsAccessibleWrap whose properties and features are exported
     * via this object instance.
     */
    nsAccessibleWrap *accWrap;
};

struct MaiAtkObjectClass
{
    AtkObjectClass parent_class;
};

static guint mai_atk_object_signals [LAST_SIGNAL] = { 0, };

#ifdef MAI_LOGGING
PRInt32 sMaiAtkObjCreated = 0;
PRInt32 sMaiAtkObjDeleted = 0;
#endif

G_BEGIN_DECLS
/* callbacks for MaiAtkObject */
static void classInitCB(AtkObjectClass *aClass);
static void initializeCB(AtkObject *aAtkObj, gpointer aData);
static void finalizeCB(GObject *aObj);

/* callbacks for AtkObject virtual functions */
static const gchar*        getNameCB (AtkObject *aAtkObj);
/* getDescriptionCB is also used by image interface */
       const gchar*        getDescriptionCB (AtkObject *aAtkObj);
static AtkRole             getRoleCB(AtkObject *aAtkObj);
static AtkAttributeSet*    getAttributesCB(AtkObject *aAtkObj);
static AtkObject*          getParentCB(AtkObject *aAtkObj);
static gint                getChildCountCB(AtkObject *aAtkObj);
static AtkObject*          refChildCB(AtkObject *aAtkObj, gint aChildIndex);
static gint                getIndexInParentCB(AtkObject *aAtkObj);
static AtkStateSet*        refStateSetCB(AtkObject *aAtkObj);
static AtkRelationSet*     refRelationSetCB(AtkObject *aAtkObj);

/* the missing atkobject virtual functions */
/*
  static AtkLayer            getLayerCB(AtkObject *aAtkObj);
  static gint                getMdiZorderCB(AtkObject *aAtkObj);
  static void                SetNameCB(AtkObject *aAtkObj,
  const gchar *name);
  static void                SetDescriptionCB(AtkObject *aAtkObj,
  const gchar *description);
  static void                SetParentCB(AtkObject *aAtkObj,
  AtkObject *parent);
  static void                SetRoleCB(AtkObject *aAtkObj,
  AtkRole role);
  static guint               ConnectPropertyChangeHandlerCB(
  AtkObject  *aObj,
  AtkPropertyChangeHandler *handler);
  static void                RemovePropertyChangeHandlerCB(
  AtkObject *aAtkObj,
  guint handler_id);
  static void                InitializeCB(AtkObject *aAtkObj,
  gpointer data);
  static void                ChildrenChangedCB(AtkObject *aAtkObj,
  guint change_index,
  gpointer changed_child);
  static void                FocusEventCB(AtkObject *aAtkObj,
  gboolean focus_in);
  static void                PropertyChangeCB(AtkObject *aAtkObj,
  AtkPropertyValues *values);
  static void                StateChangeCB(AtkObject *aAtkObj,
  const gchar *name,
  gboolean state_set);
  static void                VisibleDataChangedCB(AtkObject *aAtkObj);
*/
G_END_DECLS

static GType GetMaiAtkType(PRUint16 interfacesBits);
static const char * GetUniqueMaiAtkTypeName(PRUint16 interfacesBits);

static gpointer parent_class = NULL;

GType
mai_atk_object_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkObjectClass),
            (GBaseInitFunc)NULL,
            (GBaseFinalizeFunc)NULL,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)NULL,
            NULL, /* class data */
            sizeof(MaiAtkObject), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)NULL,
            NULL /* value table */
        };

        type = g_type_register_static(ATK_TYPE_OBJECT,
                                      "MaiAtkObject", &tinfo, GTypeFlags(0));
    }
    return type;
}

#ifdef MAI_LOGGING
PRInt32 nsAccessibleWrap::mAccWrapCreated = 0;
PRInt32 nsAccessibleWrap::mAccWrapDeleted = 0;
#endif

nsAccessibleWrap::nsAccessibleWrap(nsIDOMNode* aNode,
                                   nsIWeakReference *aShell)
    : nsAccessible(aNode, aShell),
      mAtkObject(nsnull)
{
#ifdef MAI_LOGGING
    ++mAccWrapCreated;
#endif
    MAI_LOG_DEBUG(("==nsAccessibleWrap creating: this=%p,total=%d left=%d\n",
                   (void*)this, mAccWrapCreated,
                  (mAccWrapCreated-mAccWrapDeleted)));
}

nsAccessibleWrap::~nsAccessibleWrap()
{

#ifdef MAI_LOGGING
    ++mAccWrapDeleted;
#endif
    MAI_LOG_DEBUG(("==nsAccessibleWrap deleting: this=%p,total=%d left=%d\n",
                   (void*)this, mAccWrapDeleted,
                   (mAccWrapCreated-mAccWrapDeleted)));

    if (mAtkObject) {
        if (IS_MAI_OBJECT(mAtkObject)) {
            MAI_ATK_OBJECT(mAtkObject)->accWrap = nsnull;
        }
        g_object_unref(mAtkObject);
    }
}

NS_IMETHODIMP nsAccessibleWrap::GetNativeInterface(void **aOutAccessible)
{
    *aOutAccessible = nsnull;

    if (!IsEmbeddedObject(this)) {
      // We don't create ATK objects for nsIAccessible plain text leaves
      return NS_ERROR_FAILURE;
    }
    if (!mAtkObject) {
        GType type = GetMaiAtkType(CreateMaiInterfaces());
        NS_ENSURE_TRUE(type, NS_ERROR_FAILURE);
        mAtkObject =
            NS_REINTERPRET_CAST(AtkObject *,
                                g_object_new(type, NULL));
        NS_ENSURE_TRUE(mAtkObject, NS_ERROR_OUT_OF_MEMORY);

        atk_object_initialize(mAtkObject, this);
        mAtkObject->role = ATK_ROLE_INVALID;
        mAtkObject->layer = ATK_LAYER_INVALID;
    }

    *aOutAccessible = mAtkObject;
    return NS_OK;
}

AtkObject *
nsAccessibleWrap::GetAtkObject(void)
{
    void *atkObj = nsnull;
    GetNativeInterface(&atkObj);
    return NS_STATIC_CAST(AtkObject *, atkObj);
}

/* private */
PRUint16
nsAccessibleWrap::CreateMaiInterfaces(void)
{
    PRUint16 interfacesBits = 0;
    
    // Add Interfaces for each nsIAccessible.ext interfaces

    // the Component interface are supported by all nsIAccessible
    interfacesBits |= 1 << MAI_INTERFACE_COMPONENT;

    // Add Action interface if the action count is more than zero.
    PRUint8 actionCount = 0;
    nsresult rv = GetNumActions(&actionCount);
    if (NS_SUCCEEDED(rv) && actionCount > 0) {
       interfacesBits |= 1 << MAI_INTERFACE_ACTION; 
    }

    PRUint32 accRole;
    GetRole(&accRole);

    //nsIAccessibleText
    nsCOMPtr<nsIAccessibleText> accessInterfaceText;
    QueryInterface(NS_GET_IID(nsIAccessibleText),
                   getter_AddRefs(accessInterfaceText));
    if (accessInterfaceText) {
        interfacesBits |= 1 << MAI_INTERFACE_TEXT;
    }

    //nsIAccessibleEditableText
    nsCOMPtr<nsIAccessibleEditableText> accessInterfaceEditableText;
    QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                   getter_AddRefs(accessInterfaceEditableText));
    if (accessInterfaceEditableText) {
        interfacesBits |= 1 << MAI_INTERFACE_EDITABLE_TEXT;
    }

    //nsIAccessibleSelection
    nsCOMPtr<nsIAccessibleSelectable> accessInterfaceSelection;
    QueryInterface(NS_GET_IID(nsIAccessibleSelectable),
                   getter_AddRefs(accessInterfaceSelection));
    if (accessInterfaceSelection) {
        interfacesBits |= 1 << MAI_INTERFACE_SELECTION;
    }

    //nsIAccessibleValue
    nsCOMPtr<nsIAccessibleValue> accessInterfaceValue;
    QueryInterface(NS_GET_IID(nsIAccessibleValue),
                   getter_AddRefs(accessInterfaceValue));
    if (accessInterfaceValue) {
       interfacesBits |= 1 << MAI_INTERFACE_VALUE; 
    }

    //nsIAccessibleHypertext
    PRInt32 linkCount = 0;
    nsCOMPtr<nsIAccessibleHyperText> accessInterfaceHypertext;
    QueryInterface(NS_GET_IID(nsIAccessibleHyperText),
                   getter_AddRefs(accessInterfaceHypertext));
    if (accessInterfaceHypertext) {
        nsresult rv = accessInterfaceHypertext->GetLinks(&linkCount);
        if (NS_SUCCEEDED(rv) && (linkCount > 0)) {
            interfacesBits |= 1 << MAI_INTERFACE_HYPERTEXT;
        }
    }

    //nsIAccessibleHyperLink
    nsCOMPtr<nsIAccessibleHyperLink> accessInterfaceHyperlink;
    QueryInterface(NS_GET_IID(nsIAccessibleHyperLink),
                   getter_AddRefs(accessInterfaceHyperlink));
    if (accessInterfaceHyperlink) {
       interfacesBits |= 1 << MAI_INTERFACE_HYPERLINK_IMPL;
    }

    //nsIAccessibleTable
    nsCOMPtr<nsIAccessibleTable> accessInterfaceTable;
    QueryInterface(NS_GET_IID(nsIAccessibleTable),
                   getter_AddRefs(accessInterfaceTable));
    if (accessInterfaceTable) {
        interfacesBits |= 1 << MAI_INTERFACE_TABLE;
    }

    //nsIAccessibleDocument
    nsCOMPtr<nsIAccessibleDocument> accessInterfaceDocument;
    QueryInterface(NS_GET_IID(nsIAccessibleDocument),
                              getter_AddRefs(accessInterfaceDocument));
    if (accessInterfaceDocument) {
        interfacesBits |= 1 << MAI_INTERFACE_DOCUMENT;
    }

    //nsIAccessibleImage
    nsCOMPtr<nsIAccessibleImage> accessInterfaceImage;
    QueryInterface(NS_GET_IID(nsIAccessibleImage),
                              getter_AddRefs(accessInterfaceImage));
    if (accessInterfaceImage) {
        interfacesBits |= 1 << MAI_INTERFACE_IMAGE;
    }

    return interfacesBits;
}

static GType
GetMaiAtkType(PRUint16 interfacesBits)
{
    GType type;
    static const GTypeInfo tinfo = {
        sizeof(MaiAtkObjectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) NULL,
        (GClassFinalizeFunc) NULL,
        NULL, /* class data */
        sizeof(MaiAtkObject), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL,
        NULL /* value table */
    };

    /*
     * The members we use to register GTypes are GetAtkTypeForMai
     * and atk_if_infos, which are constant values to each MaiInterface
     * So we can reuse the registered GType when having
     * the same MaiInterface types.
     */
    const char *atkTypeName = GetUniqueMaiAtkTypeName(interfacesBits);
    type = g_type_from_name(atkTypeName);
    if (type) {
        return type;
    }

    /*
     * gobject limits the number of types that can directly derive from any
     * given object type to 4095.
     */
    static PRUint16 typeRegCount = 0;
    if (typeRegCount++ >= 4095) {
        return G_TYPE_INVALID;
    }
    type = g_type_register_static(MAI_TYPE_ATK_OBJECT,
                                  atkTypeName,
                                  &tinfo, GTypeFlags(0));

    for (PRUint32 index = 0; index < NS_ARRAY_LENGTH(atk_if_infos); index++) {
      if (interfacesBits & (1 << index)) {
        g_type_add_interface_static(type,
                                    GetAtkTypeForMai((MaiInterfaceType)index),
                                    &atk_if_infos[index]);
      }
    }

    return type;
}

static const char *
GetUniqueMaiAtkTypeName(PRUint16 interfacesBits)
{
#define MAI_ATK_TYPE_NAME_LEN (30)     /* 10+sizeof(PRUint16)*8/4+1 < 30 */

    static gchar namePrefix[] = "MaiAtkType";   /* size = 10 */
    static gchar name[MAI_ATK_TYPE_NAME_LEN + 1];

    PR_snprintf(name, MAI_ATK_TYPE_NAME_LEN, "%s%x", namePrefix,
                interfacesBits);
    name[MAI_ATK_TYPE_NAME_LEN] = '\0';

    MAI_LOG_DEBUG(("MaiWidget::LastedTypeName=%s\n", name));

    return name;
}

PRBool nsAccessibleWrap::IsValidObject()
{
    // to ensure we are not shut down
    return (mDOMNode != nsnull);
}

/* static functions for ATK callbacks */
void
classInitCB(AtkObjectClass *aClass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(aClass);

    parent_class = g_type_class_peek_parent(aClass);

    aClass->get_name = getNameCB;
    aClass->get_description = getDescriptionCB;
    aClass->get_parent = getParentCB;
    aClass->get_n_children = getChildCountCB;
    aClass->ref_child = refChildCB;
    aClass->get_index_in_parent = getIndexInParentCB;
    aClass->get_role = getRoleCB;
    aClass->get_attributes = getAttributesCB;
    aClass->ref_state_set = refStateSetCB;
    aClass->ref_relation_set = refRelationSetCB;

    aClass->initialize = initializeCB;

    gobject_class->finalize = finalizeCB;

    mai_atk_object_signals [ACTIVATE] =
    g_signal_new ("activate",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [CREATE] =
    g_signal_new ("create",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [DEACTIVATE] =
    g_signal_new ("deactivate",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [DESTROY] =
    g_signal_new ("destroy",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [MAXIMIZE] =
    g_signal_new ("maximize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [MINIMIZE] =
    g_signal_new ("minimize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [RESIZE] =
    g_signal_new ("resize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [RESTORE] =
    g_signal_new ("restore",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

}

void
initializeCB(AtkObject *aAtkObj, gpointer aData)
{
    NS_ASSERTION((IS_MAI_OBJECT(aAtkObj)), "Invalid AtkObject");
    NS_ASSERTION(aData, "Invalid Data to init AtkObject");
    if (!aAtkObj || !aData)
        return;

    /* call parent init function */
    /* AtkObjectClass has not a "initialize" function now,
     * maybe it has later
     */

    if (ATK_OBJECT_CLASS(parent_class)->initialize)
        ATK_OBJECT_CLASS(parent_class)->initialize(aAtkObj, aData);

    /* initialize object */
    MAI_ATK_OBJECT(aAtkObj)->accWrap =
        NS_STATIC_CAST(nsAccessibleWrap*, aData);

#ifdef MAI_LOGGING
    ++sMaiAtkObjCreated;
#endif
    MAI_LOG_DEBUG(("MaiAtkObj Create obj=%p for AccWrap=%p, all=%d, left=%d\n",
                   (void*)aAtkObj, (void*)aData, sMaiAtkObjCreated,
                   (sMaiAtkObjCreated-sMaiAtkObjDeleted)));
}

void
finalizeCB(GObject *aObj)
{
    if (!IS_MAI_OBJECT(aObj))
        return;
    NS_ASSERTION(MAI_ATK_OBJECT(aObj)->accWrap == nsnull, "AccWrap NOT null");

#ifdef MAI_LOGGING
    ++sMaiAtkObjDeleted;
#endif
    MAI_LOG_DEBUG(("MaiAtkObj Delete obj=%p, all=%d, left=%d\n",
                   (void*)aObj, sMaiAtkObjCreated,
                   (sMaiAtkObjCreated-sMaiAtkObjDeleted)));

    // call parent finalize function
    // finalize of GObjectClass will unref the accessible parent if has
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

const gchar *
getNameCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);

    nsAutoString uniName;

    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    /* nsIAccessible is responsible for the non-NULL name */
    nsresult rv = accWrap->GetName(uniName);
    NS_ENSURE_SUCCESS(rv, nsnull);

    if (uniName.Length() > 0) {
        NS_ConvertUTF8toUTF16 objName(aAtkObj->name);
        if (!uniName.Equals(objName)) {
            atk_object_set_name(aAtkObj,
                                NS_ConvertUTF16toUTF8(uniName).get());
        }
    }
    return aAtkObj->name;
}

const gchar *
getDescriptionCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);

    if (!aAtkObj->description) {
        gint len;
        nsAutoString uniDesc;

        nsAccessibleWrap *accWrap =
            NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

        /* nsIAccessible is responsible for the non-NULL description */
        nsresult rv = accWrap->GetDescription(uniDesc);
        NS_ENSURE_SUCCESS(rv, nsnull);
        len = uniDesc.Length();
        if (len > 0) {
            atk_object_set_description(aAtkObj,
                                       NS_ConvertUTF16toUTF8(uniDesc).get());
        }
    }
    return aAtkObj->description;
}

AtkRole
getRoleCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), ATK_ROLE_INVALID);

#ifdef DEBUG_A11Y
        nsAccessibleWrap *testAccWrap =
            NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;
        NS_ASSERTION(nsAccessible::IsTextInterfaceSupportCorrect(testAccWrap), "Does not support nsIAccessibleText when it should");
#endif

    if (aAtkObj->role == ATK_ROLE_INVALID) {
        nsAccessibleWrap *accWrap =
            NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

        PRUint32 accRole, atkRole;
        nsresult rv = accWrap->GetFinalRole(&accRole);
        NS_ENSURE_SUCCESS(rv, ATK_ROLE_INVALID);

        atkRole = atkRoleMap[accRole]; // map to the actual value
        NS_ASSERTION(atkRoleMap[nsIAccessibleRole::ROLE_LAST_ENTRY] ==
                     kROLE_ATK_LAST_ENTRY, "ATK role map skewed");
        aAtkObj->role = NS_STATIC_CAST(AtkRole, atkRole);
    }
    return aAtkObj->role;
}

AtkAttributeSet *
GetAttributeSet(nsIAccessible* aAccessible)
{
    AtkAttributeSet *objAttributeSet = nsnull;
    nsCOMPtr<nsIPersistentProperties> attributes;
    aAccessible->GetAttributes(getter_AddRefs(attributes));

    if (attributes) {
        nsCOMPtr<nsISimpleEnumerator> propEnum;
        nsresult rv = attributes->Enumerate(getter_AddRefs(propEnum));
        NS_ENSURE_SUCCESS(rv, nsnull);

        PRBool hasMore;
        while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
            nsCOMPtr<nsISupports> sup;
            rv = propEnum->GetNext(getter_AddRefs(sup));
            nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(sup));
            NS_ENSURE_TRUE(propElem, nsnull);

            nsCAutoString name;
            rv = propElem->GetKey(name);
            NS_ENSURE_SUCCESS(rv, nsnull);

            nsAutoString value;
            rv = propElem->GetValue(value);
            NS_ENSURE_SUCCESS(rv, nsnull);

            AtkAttribute *objAttribute = (AtkAttribute *)g_malloc(sizeof(AtkAttribute));
            objAttribute->name = g_strdup(name.get());
            objAttribute->value = g_strdup(NS_ConvertUTF16toUTF8(value).get());
            objAttributeSet = g_slist_prepend(objAttributeSet, objAttribute);
        }
    }

    return objAttributeSet;
}

AtkAttributeSet *
getAttributesCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    return GetAttributeSet(accWrap);
}

AtkObject *
getParentCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    nsCOMPtr<nsIAccessible> accParent;
    nsresult rv = accWrap->GetParent(getter_AddRefs(accParent));
    if (NS_FAILED(rv) || !accParent)
        return nsnull;
    nsIAccessible *tmpParent = accParent;
    nsAccessibleWrap *accWrapParent = NS_STATIC_CAST(nsAccessibleWrap *,
                                                     tmpParent);

    AtkObject *parentAtkObj = accWrapParent->GetAtkObject();
    if (parentAtkObj && !aAtkObj->accessible_parent) {
        atk_object_set_parent(aAtkObj, parentAtkObj);
    }
    return parentAtkObj;
}

gint
getChildCountCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), 0);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    PRInt32 count = 0;
    nsCOMPtr<nsIAccessibleHyperText> hyperText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleHyperText), getter_AddRefs(hyperText));
    if (hyperText) {
        // If HyperText, then number of links matches number of children
        hyperText->GetLinks(&count);
    }
    else {
        nsCOMPtr<nsIAccessibleText> accText;
        accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText), getter_AddRefs(accText));
        if (!accText) {    // Accessible text that is not a HyperText has no children
            accWrap->GetChildCount(&count);
        }
    }
    return count;
}

AtkObject *
refChildCB(AtkObject *aAtkObj, gint aChildIndex)
{
    // aChildIndex should not be less than zero
    if (aChildIndex < 0) {
      return nsnull;
    }

    // XXX Fix this so it is not O(n^2) to walk through the children!
    // Either we can cache the last accessed child so that we can just GetNextSibling()
    // or we should cache an array of children in each nsAccessible
    // (instead of mNextSibling on the children)
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    nsresult rv;
    nsCOMPtr<nsIAccessible> accChild;
    nsCOMPtr<nsIAccessibleHyperText> hyperText;
    accWrap->QueryInterface(NS_GET_IID(nsIAccessibleHyperText), getter_AddRefs(hyperText));
    if (hyperText) {
        // If HyperText, then number of links matches number of children
        nsCOMPtr<nsIAccessibleHyperLink> hyperLink;
        rv = hyperText->GetLink(aChildIndex, getter_AddRefs(hyperLink));
        accChild = do_QueryInterface(hyperLink);
    }
    else {
        nsCOMPtr<nsIAccessibleText> accText;
        accWrap->QueryInterface(NS_GET_IID(nsIAccessibleText), getter_AddRefs(accText));
        if (!accText) {  // Accessible Text that is not HyperText has no children
            rv = accWrap->GetChildAt(aChildIndex, getter_AddRefs(accChild));
        }
    }

    if (NS_FAILED(rv) || !accChild)
        return nsnull;

    nsIAccessible *tmpAccChild = accChild;
    nsAccessibleWrap *accWrapChild =
        NS_STATIC_CAST(nsAccessibleWrap*, tmpAccChild);

    //this will addref parent
    AtkObject *childAtkObj = accWrapChild->GetAtkObject();
    NS_ASSERTION(childAtkObj, "Fail to get AtkObj");
    if (!childAtkObj)
        return nsnull;
    atk_object_set_parent(childAtkObj,
                          accWrap->GetAtkObject());
    g_object_ref(childAtkObj);
    return childAtkObj;
}

gint
getIndexInParentCB(AtkObject *aAtkObj)
{
    // We don't use nsIAccessible::GetIndexInParent() because
    // for ATK we don't want to include text leaf nodes as children
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), -1);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    nsCOMPtr<nsIAccessible> parent;
    accWrap->GetParent(getter_AddRefs(parent));
    if (!parent) {
        return -1; // No parent
    }

    nsCOMPtr<nsIAccessible> sibling;
    parent->GetFirstChild(getter_AddRefs(sibling));
    if (!sibling) {
        return -1;  // Error, parent has no children
    }

    PRInt32 currentIndex = 0;

    while (sibling != NS_STATIC_CAST(nsIAccessible*, accWrap)) {
      NS_ASSERTION(sibling, "Never ran into the same child that we started from");

      if (!sibling) {
          return -1;
      }
      if (nsAccessible::IsEmbeddedObject(sibling)) {
        ++ currentIndex;
      }

      nsCOMPtr<nsIAccessible> tempAccessible;
      sibling->GetNextSibling(getter_AddRefs(tempAccessible));
      sibling.swap(tempAccessible);
    }

    return currentIndex;
}

static void TranslateStates(PRUint32 aState, const AtkStateMap *aStateMap,
                            AtkStateSet *aStateSet)
{
  NS_ASSERTION(aStateSet, "Can't pass in null state set");

  // Convert every state to an entry in AtkStateMap
  PRUint32 stateIndex = 0;
  PRUint32 bitMask = 1;
  while (aStateMap[stateIndex].stateMapEntryType != kNoSuchState) {
    if (aStateMap[stateIndex].atkState) {    // There's potentially an ATK state for this
      PRBool isStateOn = (aState & bitMask) != 0;
      if (aStateMap[stateIndex].stateMapEntryType == kMapOpposite) {
        isStateOn = !isStateOn;
      }
      if (isStateOn) {
        atk_state_set_add_state(aStateSet, aStateMap[stateIndex].atkState);
      }
    }
    // Map extended state
    bitMask <<= 1;
    ++ stateIndex;
  }
}

AtkStateSet *
refStateSetCB(AtkObject *aAtkObj)
{
    AtkStateSet *state_set = nsnull;
    state_set = ATK_OBJECT_CLASS(parent_class)->ref_state_set(aAtkObj);

    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), state_set);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    // Map states
    PRUint32 accState;
    nsresult rv = accWrap->GetFinalState(&accState);
    NS_ENSURE_SUCCESS(rv, state_set);
    TranslateStates(accState, gAtkStateMap, state_set);

    // Map extended states
    PRUint32 accExtState;
    rv = accWrap->GetExtState(&accExtState);
    NS_ENSURE_SUCCESS(rv, state_set);

    TranslateStates(accExtState, gAtkStateMapExt, state_set);

    // OFFSCREEN can live with INVISIBLE. However, SHOWING is no meaningful
    // with INVISIBLE.
    if (!atk_state_set_contains_state(state_set, ATK_STATE_VISIBLE)) {
      atk_state_set_remove_state(state_set, ATK_STATE_SHOWING);
    }

    return state_set;
}

AtkRelationSet *
refRelationSetCB(AtkObject *aAtkObj)
{
    AtkRelationSet *relation_set = nsnull;
    relation_set = ATK_OBJECT_CLASS(parent_class)->ref_relation_set(aAtkObj);

    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), relation_set);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    AtkObject *accessible_array[1];
    AtkRelation* relation;
    
    PRUint32 relationType[] = {nsIAccessible::RELATION_LABELLED_BY,
                               nsIAccessible::RELATION_LABEL_FOR,
                               nsIAccessible::RELATION_NODE_CHILD_OF,
                               nsIAccessible::RELATION_CONTROLLED_BY,
                               nsIAccessible::RELATION_CONTROLLER_FOR,
                               nsIAccessible::RELATION_EMBEDS,
                               nsIAccessible::RELATION_FLOWS_TO,
                               nsIAccessible::RELATION_FLOWS_FROM,
                               nsIAccessible::RELATION_DESCRIBED_BY,
                               nsIAccessible::RELATION_DESCRIPTION_FOR,
                               };

    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(relationType); i++) {
        relation = atk_relation_set_get_relation_by_type(relation_set, NS_STATIC_CAST(AtkRelationType, relationType[i]));
        if (relation) {
            atk_relation_set_remove(relation_set, relation);
        }

        nsIAccessible* accRelated;
        nsresult rv = accWrap->GetAccessibleRelated(relationType[i], &accRelated);
        if (NS_SUCCEEDED(rv) && accRelated) {
            accessible_array[0] = NS_STATIC_CAST(nsAccessibleWrap*, accRelated)->GetAtkObject();
            relation = atk_relation_new(accessible_array, 1,
                                        NS_STATIC_CAST(AtkRelationType, relationType[i]));
            atk_relation_set_add(relation_set, relation);
            g_object_unref(relation);
        }
    }

    return relation_set;
}

// Check if aAtkObj is a valid MaiAtkObject
nsresult
CheckMaiAtkObject(AtkObject *aAtkObj)
{
    NS_ENSURE_ARG(IS_MAI_OBJECT(aAtkObj));
    nsAccessibleWrap * tmpAccWrap = MAI_ATK_OBJECT(aAtkObj)->accWrap;
    if (tmpAccWrap == nsnull)
        return NS_ERROR_INVALID_POINTER;
    if (tmpAccWrap != nsAppRootAccessible::Create() && !tmpAccWrap->IsValidObject())
        return NS_ERROR_INVALID_POINTER;
    if (tmpAccWrap->GetAtkObject() != aAtkObj)
        return NS_ERROR_FAILURE;
    return NS_OK;
}

// Check if aAtkObj is a valid MaiAtkObject, and return the nsAccessibleWrap
// for it.
nsAccessibleWrap *GetAccessibleWrap(AtkObject *aAtkObj)
{
    NS_ENSURE_TRUE(IS_MAI_OBJECT(aAtkObj), nsnull);
    nsAccessibleWrap * tmpAccWrap = MAI_ATK_OBJECT(aAtkObj)->accWrap;
    NS_ENSURE_TRUE(tmpAccWrap != nsnull, nsnull);
    NS_ENSURE_TRUE(tmpAccWrap->GetAtkObject() == aAtkObj, nsnull);
    return tmpAccWrap;
}
