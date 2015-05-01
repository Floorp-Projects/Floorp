/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"

#include "Accessible-inl.h"
#include "ApplicationAccessibleWrap.h"
#include "InterfaceInitFuncs.h"
#include "nsAccUtils.h"
#include "mozilla/a11y/PDocAccessible.h"
#include "ProxyAccessible.h"
#include "RootAccessible.h"
#include "nsMai.h"
#include "nsMaiHyperlink.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "prprf.h"
#include "nsStateMap.h"
#include "mozilla/a11y/Platform.h"
#include "Relation.h"
#include "RootAccessible.h"
#include "States.h"
#include "nsISimpleEnumerator.h"

#include "mozilla/ArrayUtils.h"
#include "nsXPCOMStrings.h"
#include "nsComponentManagerUtils.h"
#include "nsIPersistentProperties2.h"

using namespace mozilla;
using namespace mozilla::a11y;

AccessibleWrap::EAvailableAtkSignals AccessibleWrap::gAvailableAtkSignals =
  eUnknown;

//defined in ApplicationAccessibleWrap.cpp
extern "C" GType g_atk_hyperlink_impl_type;

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

static const char* kNonUserInputEvent = ":system";
    
static const GInterfaceInfo atk_if_infos[] = {
    {(GInterfaceInitFunc)componentInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr}, 
    {(GInterfaceInitFunc)actionInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)valueInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)editableTextInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)hypertextInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)hyperlinkImplInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)selectionInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)tableInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)textInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)documentInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr},
    {(GInterfaceInitFunc)imageInterfaceInitCB,
     (GInterfaceFinalizeFunc) nullptr, nullptr}
};

static GQuark quark_mai_hyperlink = 0;

AtkHyperlink*
MaiAtkObject::GetAtkHyperlink()
{
  NS_ASSERTION(quark_mai_hyperlink, "quark_mai_hyperlink not initialized");
  MaiHyperlink* maiHyperlink =
    (MaiHyperlink*)g_object_get_qdata(G_OBJECT(this), quark_mai_hyperlink);
  if (!maiHyperlink) {
    maiHyperlink = new MaiHyperlink(accWrap);
    g_object_set_qdata(G_OBJECT(this), quark_mai_hyperlink, maiHyperlink);
  }

  return maiHyperlink->GetAtkHyperlink();
}

void
MaiAtkObject::Shutdown()
{
  accWrap = 0;
  MaiHyperlink* maiHyperlink =
    (MaiHyperlink*)g_object_get_qdata(G_OBJECT(this), quark_mai_hyperlink);
  if (maiHyperlink) {
    delete maiHyperlink;
    g_object_set_qdata(G_OBJECT(this), quark_mai_hyperlink, nullptr);
  }
}

struct MaiAtkObjectClass
{
    AtkObjectClass parent_class;
};

static guint mai_atk_object_signals [LAST_SIGNAL] = { 0, };

static void MaybeFireNameChange(AtkObject* aAtkObj, const nsString& aNewName);

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
static const gchar* GetLocaleCB(AtkObject*);
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

static GType GetMaiAtkType(uint16_t interfacesBits);
static const char * GetUniqueMaiAtkTypeName(uint16_t interfacesBits);

static gpointer parent_class = nullptr;

GType
mai_atk_object_get_type(void)
{
    static GType type = 0;

    if (!type) {
        static const GTypeInfo tinfo = {
            sizeof(MaiAtkObjectClass),
            (GBaseInitFunc)nullptr,
            (GBaseFinalizeFunc)nullptr,
            (GClassInitFunc)classInitCB,
            (GClassFinalizeFunc)nullptr,
            nullptr, /* class data */
            sizeof(MaiAtkObject), /* instance size */
            0, /* nb preallocs */
            (GInstanceInitFunc)nullptr,
            nullptr /* value table */
        };

        type = g_type_register_static(ATK_TYPE_OBJECT,
                                      "MaiAtkObject", &tinfo, GTypeFlags(0));
        quark_mai_hyperlink = g_quark_from_static_string("MaiHyperlink");
    }
    return type;
}

AccessibleWrap::
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  Accessible(aContent, aDoc), mAtkObject(nullptr)
{
}

AccessibleWrap::~AccessibleWrap()
{
    NS_ASSERTION(!mAtkObject, "ShutdownAtkObject() is not called");
}

void
AccessibleWrap::ShutdownAtkObject()
{
  if (!mAtkObject)
    return;

  NS_ASSERTION(IS_MAI_OBJECT(mAtkObject), "wrong type of atk object");
  if (IS_MAI_OBJECT(mAtkObject))
    MAI_ATK_OBJECT(mAtkObject)->Shutdown();

  g_object_unref(mAtkObject);
  mAtkObject = nullptr;
}

void
AccessibleWrap::Shutdown()
{
  ShutdownAtkObject();
  Accessible::Shutdown();
}

void
AccessibleWrap::GetNativeInterface(void** aOutAccessible)
{
  *aOutAccessible = nullptr;

  if (!mAtkObject) {
    if (IsDefunct() || !nsAccUtils::IsEmbeddedObject(this)) {
      // We don't create ATK objects for node which has been shutdown or
      // plain text leaves
      return;
    }

    GType type = GetMaiAtkType(CreateMaiInterfaces());
    if (!type)
      return;

    mAtkObject = reinterpret_cast<AtkObject*>(g_object_new(type, nullptr));
    if (!mAtkObject)
      return;

    atk_object_initialize(mAtkObject, this);
    mAtkObject->role = ATK_ROLE_INVALID;
    mAtkObject->layer = ATK_LAYER_INVALID;
  }

  *aOutAccessible = mAtkObject;
}

AtkObject *
AccessibleWrap::GetAtkObject(void)
{
    void *atkObj = nullptr;
    GetNativeInterface(&atkObj);
    return static_cast<AtkObject *>(atkObj);
}

// Get AtkObject from Accessible interface
/* static */
AtkObject *
AccessibleWrap::GetAtkObject(Accessible* acc)
{
    void *atkObjPtr = nullptr;
    acc->GetNativeInterface(&atkObjPtr);
    return atkObjPtr ? ATK_OBJECT(atkObjPtr) : nullptr;    
}

/* private */
uint16_t
AccessibleWrap::CreateMaiInterfaces(void)
{
  uint16_t interfacesBits = 0;

  // The Component interface is supported by all accessibles.
  interfacesBits |= 1 << MAI_INTERFACE_COMPONENT;

  // Add Action interface if the action count is more than zero.
  if (ActionCount() > 0)
    interfacesBits |= 1 << MAI_INTERFACE_ACTION;

  // Text, Editabletext, and Hypertext interface.
  HyperTextAccessible* hyperText = AsHyperText();
  if (hyperText && hyperText->IsTextRole()) {
    interfacesBits |= 1 << MAI_INTERFACE_TEXT;
    interfacesBits |= 1 << MAI_INTERFACE_EDITABLE_TEXT;
    if (!nsAccUtils::MustPrune(this))
      interfacesBits |= 1 << MAI_INTERFACE_HYPERTEXT;
  }

  // Value interface.
  if (HasNumericValue())
    interfacesBits |= 1 << MAI_INTERFACE_VALUE;

  // Document interface.
  if (IsDoc())
    interfacesBits |= 1 << MAI_INTERFACE_DOCUMENT;

  if (IsImage())
    interfacesBits |= 1 << MAI_INTERFACE_IMAGE;

  // HyperLink interface.
  if (IsLink())
    interfacesBits |= 1 << MAI_INTERFACE_HYPERLINK_IMPL;

  if (!nsAccUtils::MustPrune(this)) {  // These interfaces require children
    // Table interface.
    if (AsTable())
      interfacesBits |= 1 << MAI_INTERFACE_TABLE;
 
    // Selection interface.
    if (IsSelect()) {
      interfacesBits |= 1 << MAI_INTERFACE_SELECTION;
    }
  }

  return interfacesBits;
}

static GType
GetMaiAtkType(uint16_t interfacesBits)
{
    GType type;
    static const GTypeInfo tinfo = {
        sizeof(MaiAtkObjectClass),
        (GBaseInitFunc) nullptr,
        (GBaseFinalizeFunc) nullptr,
        (GClassInitFunc) nullptr,
        (GClassFinalizeFunc) nullptr,
        nullptr, /* class data */
        sizeof(MaiAtkObject), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) nullptr,
        nullptr /* value table */
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
    static uint16_t typeRegCount = 0;
    if (typeRegCount++ >= 4095) {
        return G_TYPE_INVALID;
    }
    type = g_type_register_static(MAI_TYPE_ATK_OBJECT,
                                  atkTypeName,
                                  &tinfo, GTypeFlags(0));

    for (uint32_t index = 0; index < ArrayLength(atk_if_infos); index++) {
      if (interfacesBits & (1 << index)) {
        g_type_add_interface_static(type,
                                    GetAtkTypeForMai((MaiInterfaceType)index),
                                    &atk_if_infos[index]);
      }
    }

    return type;
}

static const char*
GetUniqueMaiAtkTypeName(uint16_t interfacesBits)
{
#define MAI_ATK_TYPE_NAME_LEN (30)     /* 10+sizeof(uint16_t)*8/4+1 < 30 */

    static gchar namePrefix[] = "MaiAtkType";   /* size = 10 */
    static gchar name[MAI_ATK_TYPE_NAME_LEN + 1];

    PR_snprintf(name, MAI_ATK_TYPE_NAME_LEN, "%s%x", namePrefix,
                interfacesBits);
    name[MAI_ATK_TYPE_NAME_LEN] = '\0';

    return name;
}

bool
AccessibleWrap::IsValidObject()
{
    // to ensure we are not shut down
    return !IsDefunct();
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
    aClass->get_object_locale = GetLocaleCB;
    aClass->ref_state_set = refStateSetCB;
    aClass->ref_relation_set = refRelationSetCB;

    aClass->initialize = initializeCB;

    gobject_class->finalize = finalizeCB;

    mai_atk_object_signals [ACTIVATE] =
    g_signal_new ("activate",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [CREATE] =
    g_signal_new ("create",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [DEACTIVATE] =
    g_signal_new ("deactivate",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [DESTROY] =
    g_signal_new ("destroy",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [MAXIMIZE] =
    g_signal_new ("maximize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [MINIMIZE] =
    g_signal_new ("minimize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [RESIZE] =
    g_signal_new ("resize",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
    mai_atk_object_signals [RESTORE] =
    g_signal_new ("restore",
                  MAI_TYPE_ATK_OBJECT,
                  G_SIGNAL_RUN_LAST,
                  0, /* default signal handler */
                  nullptr, nullptr,
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
  MAI_ATK_OBJECT(aAtkObj)->accWrap = reinterpret_cast<uintptr_t>(aData);
}

void
finalizeCB(GObject *aObj)
{
    if (!IS_MAI_OBJECT(aObj))
        return;
    NS_ASSERTION(MAI_ATK_OBJECT(aObj)->accWrap == 0, "AccWrap NOT null");

    // call parent finalize function
    // finalize of GObjectClass will unref the accessible parent if has
    if (G_OBJECT_CLASS (parent_class)->finalize)
        G_OBJECT_CLASS (parent_class)->finalize(aObj);
}

const gchar*
getNameCB(AtkObject* aAtkObj)
{
  nsAutoString name;
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (accWrap)
    accWrap->Name(name);
  else if (ProxyAccessible* proxy = GetProxy(aAtkObj))
    proxy->Name(name);
  else
    return nullptr;

  // XXX Firing an event from here does not seem right
  MaybeFireNameChange(aAtkObj, name);

  return aAtkObj->name;
}

static void
MaybeFireNameChange(AtkObject* aAtkObj, const nsString& aNewName)
{
  NS_ConvertUTF16toUTF8 newNameUTF8(aNewName);
  if (aAtkObj->name && newNameUTF8.Equals(aAtkObj->name))
    return;

  // Below we duplicate the functionality of atk_object_set_name(),
  // but without calling atk_object_get_name(). Instead of
  // atk_object_get_name() we directly access aAtkObj->name. This is because
  // atk_object_get_name() would call getNameCB() which would call
  // MaybeFireNameChange() (or atk_object_set_name() before this problem was
  // fixed) and we would get an infinite recursion.
  // See http://bugzilla.mozilla.org/733712

  // Do not notify for initial name setting.
  // See bug http://bugzilla.gnome.org/665870
  bool notify = !!aAtkObj->name;

  free(aAtkObj->name);
  aAtkObj->name = strdup(newNameUTF8.get());

  if (notify)
    g_object_notify(G_OBJECT(aAtkObj), "accessible-name");
}

const gchar *
getDescriptionCB(AtkObject *aAtkObj)
{
  nsAutoString uniDesc;
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (accWrap) {
    if (accWrap->IsDefunct())
      return nullptr;

    accWrap->Description(uniDesc);
  } else if (ProxyAccessible* proxy = GetProxy(aAtkObj)) {
    proxy->Description(uniDesc);
  } else {
    return nullptr;
  }

    NS_ConvertUTF8toUTF16 objDesc(aAtkObj->description);
    if (!uniDesc.Equals(objDesc))
        atk_object_set_description(aAtkObj,
                                   NS_ConvertUTF16toUTF8(uniDesc).get());

    return aAtkObj->description;
}

AtkRole
getRoleCB(AtkObject *aAtkObj)
{
  if (aAtkObj->role != ATK_ROLE_INVALID)
    return aAtkObj->role;

  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  a11y::role role;
  if (!accWrap) {
    ProxyAccessible* proxy = GetProxy(aAtkObj);
    if (!proxy)
      return ATK_ROLE_INVALID;

    role = proxy->Role();
  } else {
#ifdef DEBUG
    NS_ASSERTION(nsAccUtils::IsTextInterfaceSupportCorrect(accWrap),
                 "Does not support Text interface when it should");
#endif

    role = accWrap->Role();
  }

#define ROLE(geckoRole, stringRole, atkRole, macRole, \
             msaaRole, ia2Role, nameRule) \
  case roles::geckoRole: \
    aAtkObj->role = atkRole; \
    break;

  switch (role) {
#include "RoleMap.h"
    default:
      MOZ_CRASH("Unknown role.");
  };

#undef ROLE

  if (aAtkObj->role == ATK_ROLE_LIST_BOX && !IsAtkVersionAtLeast(2, 1))
    aAtkObj->role = ATK_ROLE_LIST;
  else if (aAtkObj->role == ATK_ROLE_TABLE_ROW && !IsAtkVersionAtLeast(2, 1))
    aAtkObj->role = ATK_ROLE_LIST_ITEM;
  else if (aAtkObj->role == ATK_ROLE_MATH && !IsAtkVersionAtLeast(2, 12))
    aAtkObj->role = ATK_ROLE_PANEL;
  else if (aAtkObj->role == ATK_ROLE_STATIC && !IsAtkVersionAtLeast(2, 16))
    aAtkObj->role = ATK_ROLE_TEXT;
  else if ((aAtkObj->role == ATK_ROLE_MATH_FRACTION ||
            aAtkObj->role == ATK_ROLE_MATH_ROOT) && !IsAtkVersionAtLeast(2, 16))
    aAtkObj->role = ATK_ROLE_UNKNOWN;

  return aAtkObj->role;
}

static AtkAttributeSet*
ConvertToAtkAttributeSet(nsIPersistentProperties* aAttributes)
{
    if (!aAttributes)
        return nullptr;

    AtkAttributeSet *objAttributeSet = nullptr;
    nsCOMPtr<nsISimpleEnumerator> propEnum;
    nsresult rv = aAttributes->Enumerate(getter_AddRefs(propEnum));
    NS_ENSURE_SUCCESS(rv, nullptr);

    bool hasMore;
    while (NS_SUCCEEDED(propEnum->HasMoreElements(&hasMore)) && hasMore) {
        nsCOMPtr<nsISupports> sup;
        rv = propEnum->GetNext(getter_AddRefs(sup));
        NS_ENSURE_SUCCESS(rv, objAttributeSet);

        nsCOMPtr<nsIPropertyElement> propElem(do_QueryInterface(sup));
        NS_ENSURE_TRUE(propElem, objAttributeSet);

        nsAutoCString name;
        rv = propElem->GetKey(name);
        NS_ENSURE_SUCCESS(rv, objAttributeSet);

        nsAutoString value;
        rv = propElem->GetValue(value);
        NS_ENSURE_SUCCESS(rv, objAttributeSet);

        AtkAttribute *objAttr = (AtkAttribute *)g_malloc(sizeof(AtkAttribute));
        objAttr->name = g_strdup(name.get());
        objAttr->value = g_strdup(NS_ConvertUTF16toUTF8(value).get());
        objAttributeSet = g_slist_prepend(objAttributeSet, objAttr);
    }

    //libspi will free it
    return objAttributeSet;
}

AtkAttributeSet*
GetAttributeSet(Accessible* aAccessible)
{
  nsCOMPtr<nsIPersistentProperties> attributes = aAccessible->Attributes();
  if (attributes) {
    // There is no ATK state for haspopup, must use object attribute to expose
    // the same info.
    if (aAccessible->State() & states::HASPOPUP) {
      nsAutoString unused;
      attributes->SetStringProperty(NS_LITERAL_CSTRING("haspopup"),
                                    NS_LITERAL_STRING("true"), unused);
    }

    return ConvertToAtkAttributeSet(attributes);
  }

  return nullptr;
}

AtkAttributeSet *
getAttributesCB(AtkObject *aAtkObj)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (accWrap)
    return GetAttributeSet(accWrap);

  ProxyAccessible* proxy = GetProxy(aAtkObj);
  if (!proxy)
    return nullptr;

  nsAutoTArray<Attribute, 10> attrs;
  proxy->Attributes(&attrs);
  if (attrs.IsEmpty())
    return nullptr;

  AtkAttributeSet* objAttributeSet = nullptr;
  for (uint32_t i = 0; i < attrs.Length(); i++) {
    AtkAttribute *objAttr = (AtkAttribute *)g_malloc(sizeof(AtkAttribute));
    objAttr->name = g_strdup(attrs[i].Name().get());
    objAttr->value = g_strdup(NS_ConvertUTF16toUTF8(attrs[i].Value()).get());
    objAttributeSet = g_slist_prepend(objAttributeSet, objAttr);
  }

  return objAttributeSet;
}

const gchar*
GetLocaleCB(AtkObject* aAtkObj)
{
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (!accWrap)
    return nullptr;

  nsAutoString locale;
  accWrap->Language(locale);
  return AccessibleWrap::ReturnString(locale);
}

AtkObject *
getParentCB(AtkObject *aAtkObj)
{
  if (aAtkObj->accessible_parent)
    return aAtkObj->accessible_parent;

  AtkObject* atkParent = nullptr;
  if (AccessibleWrap* wrapper = GetAccessibleWrap(aAtkObj)) {
    Accessible* parent = wrapper->Parent();
    atkParent = parent ? AccessibleWrap::GetAtkObject(parent) : nullptr;
  } else if (ProxyAccessible* proxy = GetProxy(aAtkObj)) {
    ProxyAccessible* parent = proxy->Parent();
    atkParent = parent ? GetWrapperFor(parent) : nullptr;
  }

  if (atkParent)
    atk_object_set_parent(aAtkObj, atkParent);

  return aAtkObj->accessible_parent;
}

gint
getChildCountCB(AtkObject *aAtkObj)
{
    AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
    if (!accWrap || nsAccUtils::MustPrune(accWrap)) {
        return 0;
    }

    return static_cast<gint>(accWrap->EmbeddedChildCount());
}

AtkObject *
refChildCB(AtkObject *aAtkObj, gint aChildIndex)
{
    // aChildIndex should not be less than zero
    if (aChildIndex < 0) {
      return nullptr;
    }

    AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
    if (!accWrap || nsAccUtils::MustPrune(accWrap)) {
        return nullptr;
    }

    Accessible* accChild = accWrap->GetEmbeddedChildAt(aChildIndex);
    if (!accChild)
        return nullptr;

    AtkObject* childAtkObj = AccessibleWrap::GetAtkObject(accChild);

    NS_ASSERTION(childAtkObj, "Fail to get AtkObj");
    if (!childAtkObj)
        return nullptr;
    g_object_ref(childAtkObj);

  if (aAtkObj != childAtkObj->accessible_parent)
    atk_object_set_parent(childAtkObj, aAtkObj);

  return childAtkObj;
}

gint
getIndexInParentCB(AtkObject* aAtkObj)
{
  // We don't use Accessible::IndexInParent() because we don't include text
  // leaf nodes as children in ATK.
    AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
    if (!accWrap) {
        return -1;
    }

    Accessible* parent = accWrap->Parent();
    if (!parent)
        return -1; // No parent

    return parent->GetIndexOfEmbeddedChild(accWrap);
}

static void
TranslateStates(uint64_t aState, AtkStateSet* aStateSet)
{
  // atk doesn't have a read only state so read only things shouldn't be
  // editable.
  if (aState & states::READONLY)
    aState &= ~states::EDITABLE;

  // Convert every state to an entry in AtkStateMap
  uint32_t stateIndex = 0;
  uint64_t bitMask = 1;
  while (gAtkStateMap[stateIndex].stateMapEntryType != kNoSuchState) {
    if (gAtkStateMap[stateIndex].atkState) { // There's potentially an ATK state for this
      bool isStateOn = (aState & bitMask) != 0;
      if (gAtkStateMap[stateIndex].stateMapEntryType == kMapOpposite) {
        isStateOn = !isStateOn;
      }
      if (isStateOn) {
        atk_state_set_add_state(aStateSet, gAtkStateMap[stateIndex].atkState);
      }
    }
    bitMask <<= 1;
    ++ stateIndex;
  }
}

AtkStateSet *
refStateSetCB(AtkObject *aAtkObj)
{
  AtkStateSet *state_set = nullptr;
  state_set = ATK_OBJECT_CLASS(parent_class)->ref_state_set(aAtkObj);

  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (accWrap)
    TranslateStates(accWrap->State(), state_set);
  else if (ProxyAccessible* proxy = GetProxy(aAtkObj))
    TranslateStates(proxy->State(), state_set);
  else
    TranslateStates(states::DEFUNCT, state_set);

  return state_set;
}

static void
UpdateAtkRelation(RelationType aType, Accessible* aAcc,
                  AtkRelationType aAtkType, AtkRelationSet* aAtkSet)
{
  if (aAtkType == ATK_RELATION_NULL)
    return;

  AtkRelation* atkRelation =
    atk_relation_set_get_relation_by_type(aAtkSet, aAtkType);
  if (atkRelation)
    atk_relation_set_remove(aAtkSet, atkRelation);

  Relation rel(aAcc->RelationByType(aType));
  nsTArray<AtkObject*> targets;
  Accessible* tempAcc = nullptr;
  while ((tempAcc = rel.Next()))
    targets.AppendElement(AccessibleWrap::GetAtkObject(tempAcc));

  if (targets.Length()) {
    atkRelation = atk_relation_new(targets.Elements(),
                                   targets.Length(), aAtkType);
    atk_relation_set_add(aAtkSet, atkRelation);
    g_object_unref(atkRelation);
  }
}

AtkRelationSet *
refRelationSetCB(AtkObject *aAtkObj)
{
  AtkRelationSet* relation_set =
    ATK_OBJECT_CLASS(parent_class)->ref_relation_set(aAtkObj);

  const AtkRelationType typeMap[] = {
#define RELATIONTYPE(gecko, s, atk, m, i) atk,
#include "RelationTypeMap.h"
#undef RELATIONTYPE
  };

  if (ProxyAccessible* proxy = GetProxy(aAtkObj)) {
    nsTArray<RelationType> types;
    nsTArray<nsTArray<ProxyAccessible*>> targetSets;
    proxy->Relations(&types, &targetSets);

    size_t relationCount = types.Length();
    for (size_t i = 0; i < relationCount; i++) {
      if (typeMap[static_cast<uint32_t>(types[i])] == ATK_RELATION_NULL)
        continue;

      size_t targetCount = targetSets[i].Length();
      nsAutoTArray<AtkObject*, 5> wrappers;
      for (size_t j = 0; j < targetCount; j++)
        wrappers.AppendElement(GetWrapperFor(targetSets[i][j]));

      AtkRelationType atkType = typeMap[static_cast<uint32_t>(types[i])];
      AtkRelation* atkRelation =
        atk_relation_set_get_relation_by_type(relation_set, atkType);
      if (atkRelation)
        atk_relation_set_remove(relation_set, atkRelation);

      atkRelation = atk_relation_new(wrappers.Elements(), wrappers.Length(),
                                     atkType);
      atk_relation_set_add(relation_set, atkRelation);
      g_object_unref(atkRelation);
    }
  }

  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (!accWrap)
    return relation_set;

#define RELATIONTYPE(geckoType, geckoTypeName, atkType, msaaType, ia2Type) \
  UpdateAtkRelation(RelationType::geckoType, accWrap, atkType, relation_set);

#include "RelationTypeMap.h"

#undef RELATIONTYPE

  return relation_set;
}

// Check if aAtkObj is a valid MaiAtkObject, and return the AccessibleWrap
// for it.
AccessibleWrap*
GetAccessibleWrap(AtkObject* aAtkObj)
{
  NS_ENSURE_TRUE(IS_MAI_OBJECT(aAtkObj), nullptr);

  // Make sure its native is an AccessibleWrap not a proxy.
  if (MAI_ATK_OBJECT(aAtkObj)->accWrap & IS_PROXY)
    return nullptr;

    AccessibleWrap* accWrap =
      reinterpret_cast<AccessibleWrap*>(MAI_ATK_OBJECT(aAtkObj)->accWrap);

  // Check if the accessible was deconstructed.
  if (!accWrap)
    return nullptr;

  NS_ENSURE_TRUE(accWrap->GetAtkObject() == aAtkObj, nullptr);

  AccessibleWrap* appAccWrap = ApplicationAcc();
  if (appAccWrap != accWrap && !accWrap->IsValidObject())
    return nullptr;

  return accWrap;
}

ProxyAccessible*
GetProxy(AtkObject* aObj)
{
  if (!aObj || !(MAI_ATK_OBJECT(aObj)->accWrap & IS_PROXY))
    return nullptr;

  return reinterpret_cast<ProxyAccessible*>(MAI_ATK_OBJECT(aObj)->accWrap
      & ~IS_PROXY);
}

AtkObject*
GetWrapperFor(ProxyAccessible* aProxy)
{
  return reinterpret_cast<AtkObject*>(aProxy->GetWrapper() & ~IS_PROXY);
}

static uint16_t
GetInterfacesForProxy(ProxyAccessible* aProxy, uint32_t aInterfaces)
{
  uint16_t interfaces = 1 << MAI_INTERFACE_COMPONENT;
  if (aInterfaces & Interfaces::HYPERTEXT)
    interfaces |= (1 << MAI_INTERFACE_HYPERTEXT) | (1 << MAI_INTERFACE_TEXT)
        | (1 << MAI_INTERFACE_EDITABLE_TEXT);

  if (aInterfaces & Interfaces::HYPERLINK)
    interfaces |= MAI_INTERFACE_HYPERLINK_IMPL;

  if (aInterfaces & Interfaces::VALUE)
    interfaces |= MAI_INTERFACE_VALUE;

  if (aInterfaces & Interfaces::TABLE)
    interfaces |= MAI_INTERFACE_TABLE;

  if (aInterfaces & Interfaces::IMAGE)
    interfaces |= MAI_INTERFACE_IMAGE;

  return interfaces;
}

void
a11y::ProxyCreated(ProxyAccessible* aProxy, uint32_t aInterfaces)
{
  GType type = GetMaiAtkType(GetInterfacesForProxy(aProxy, aInterfaces));
  NS_ASSERTION(type, "why don't we have a type!");

  AtkObject* obj =
    reinterpret_cast<AtkObject *>
    (g_object_new(type, nullptr));
  if (!obj)
    return;

  uintptr_t inner = reinterpret_cast<uintptr_t>(aProxy) | IS_PROXY;
  atk_object_initialize(obj, reinterpret_cast<gpointer>(inner));
  obj->role = ATK_ROLE_INVALID;
  obj->layer = ATK_LAYER_INVALID;
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(obj) | IS_PROXY);
}

void
a11y::ProxyDestroyed(ProxyAccessible* aProxy)
{
  auto obj = reinterpret_cast<MaiAtkObject*>(aProxy->GetWrapper() & ~IS_PROXY);
  obj->Shutdown();
  g_object_unref(obj);
  aProxy->SetWrapper(0);
}

nsresult
AccessibleWrap::HandleAccEvent(AccEvent* aEvent)
{
  nsresult rv = Accessible::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

    Accessible* accessible = aEvent->GetAccessible();
    NS_ENSURE_TRUE(accessible, NS_ERROR_FAILURE);

    // The accessible can become defunct if we have an xpcom event listener
    // which decides it would be fun to change the DOM and flush layout.
    if (accessible->IsDefunct())
        return NS_OK;

    uint32_t type = aEvent->GetEventType();

    AtkObject* atkObj = AccessibleWrap::GetAtkObject(accessible);

  // We don't create ATK objects for plain text leaves, just return NS_OK in
  // such case.
    if (!atkObj) {
        NS_ASSERTION(type == nsIAccessibleEvent::EVENT_SHOW ||
                     type == nsIAccessibleEvent::EVENT_HIDE,
                     "Event other than SHOW and HIDE fired for plain text leaves");
        return NS_OK;
    }

    AccessibleWrap* accWrap = GetAccessibleWrap(atkObj);
    if (!accWrap) {
        return NS_OK; // Node is shut down
    }

    switch (type) {
    case nsIAccessibleEvent::EVENT_STATE_CHANGE:
        return FireAtkStateChangeEvent(aEvent, atkObj);

    case nsIAccessibleEvent::EVENT_TEXT_REMOVED:
    case nsIAccessibleEvent::EVENT_TEXT_INSERTED:
        return FireAtkTextChangedEvent(aEvent, atkObj);

    case nsIAccessibleEvent::EVENT_FOCUS:
      {
        a11y::RootAccessible* rootAccWrap = accWrap->RootAccessible();
        if (rootAccWrap && rootAccWrap->mActivated) {
            atk_focus_tracker_notify(atkObj);
            // Fire state change event for focus
            atk_object_notify_state_change(atkObj, ATK_STATE_FOCUSED, true);
            return NS_OK;
        }
      } break;

    case nsIAccessibleEvent::EVENT_NAME_CHANGE:
      {
        nsAutoString newName;
        accessible->Name(newName);

        MaybeFireNameChange(atkObj, newName);

        break;
      }

  case nsIAccessibleEvent::EVENT_VALUE_CHANGE:
    if (accessible->HasNumericValue()) {
      // Make sure this is a numeric value. Don't fire for string value changes
      // (e.g. text editing) ATK values are always numeric.
      g_object_notify((GObject*)atkObj, "accessible-value");
    }
    break;

    case nsIAccessibleEvent::EVENT_SELECTION:
    case nsIAccessibleEvent::EVENT_SELECTION_ADD:
    case nsIAccessibleEvent::EVENT_SELECTION_REMOVE:
    {
      // XXX: dupe events may be fired
      AccSelChangeEvent* selChangeEvent = downcast_accEvent(aEvent);
      g_signal_emit_by_name(AccessibleWrap::GetAtkObject(selChangeEvent->Widget()),
                            "selection_changed");
      break;
    }

    case nsIAccessibleEvent::EVENT_SELECTION_WITHIN:
    {
      g_signal_emit_by_name(atkObj, "selection_changed");
      break;
    }

    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED:
        g_signal_emit_by_name(atkObj, "text_selection_changed");
        break;

    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED:
      {
        AccCaretMoveEvent* caretMoveEvent = downcast_accEvent(aEvent);
        NS_ASSERTION(caretMoveEvent, "Event needs event data");
        if (!caretMoveEvent)
            break;

        int32_t caretOffset = caretMoveEvent->GetCaretOffset();
        g_signal_emit_by_name(atkObj, "text_caret_moved", caretOffset);
      } break;

    case nsIAccessibleEvent::EVENT_TEXT_ATTRIBUTE_CHANGED:
        g_signal_emit_by_name(atkObj, "text-attributes-changed");
        break;

    case nsIAccessibleEvent::EVENT_TABLE_MODEL_CHANGED:
        g_signal_emit_by_name(atkObj, "model_changed");
        break;

    case nsIAccessibleEvent::EVENT_TABLE_ROW_INSERT:
      {
        AccTableChangeEvent* tableEvent = downcast_accEvent(aEvent);
        NS_ENSURE_TRUE(tableEvent, NS_ERROR_FAILURE);

        int32_t rowIndex = tableEvent->GetIndex();
        int32_t numRows = tableEvent->GetCount();

        g_signal_emit_by_name(atkObj, "row_inserted", rowIndex, numRows);
     } break;

   case nsIAccessibleEvent::EVENT_TABLE_ROW_DELETE:
     {
        AccTableChangeEvent* tableEvent = downcast_accEvent(aEvent);
        NS_ENSURE_TRUE(tableEvent, NS_ERROR_FAILURE);

        int32_t rowIndex = tableEvent->GetIndex();
        int32_t numRows = tableEvent->GetCount();

        g_signal_emit_by_name(atkObj, "row_deleted", rowIndex, numRows);
      } break;

    case nsIAccessibleEvent::EVENT_TABLE_ROW_REORDER:
      {
        g_signal_emit_by_name(atkObj, "row_reordered");
        break;
      }

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_INSERT:
      {
        AccTableChangeEvent* tableEvent = downcast_accEvent(aEvent);
        NS_ENSURE_TRUE(tableEvent, NS_ERROR_FAILURE);

        int32_t colIndex = tableEvent->GetIndex();
        int32_t numCols = tableEvent->GetCount();
        g_signal_emit_by_name(atkObj, "column_inserted", colIndex, numCols);
      } break;

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_DELETE:
      {
        AccTableChangeEvent* tableEvent = downcast_accEvent(aEvent);
        NS_ENSURE_TRUE(tableEvent, NS_ERROR_FAILURE);

        int32_t colIndex = tableEvent->GetIndex();
        int32_t numCols = tableEvent->GetCount();
        g_signal_emit_by_name(atkObj, "column_deleted", colIndex, numCols);
      } break;

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_REORDER:
        g_signal_emit_by_name(atkObj, "column_reordered");
        break;

    case nsIAccessibleEvent::EVENT_SECTION_CHANGED:
        g_signal_emit_by_name(atkObj, "visible_data_changed");
        break;

    case nsIAccessibleEvent::EVENT_SHOW:
        return FireAtkShowHideEvent(aEvent, atkObj, true);

    case nsIAccessibleEvent::EVENT_HIDE:
        // XXX - Handle native dialog accessibles.
        if (!accessible->IsRoot() && accessible->HasARIARole() &&
            accessible->ARIARole() == roles::DIALOG) {
          guint id = g_signal_lookup("deactivate", MAI_TYPE_ATK_OBJECT);
          g_signal_emit(atkObj, id, 0);
        }
        return FireAtkShowHideEvent(aEvent, atkObj, false);

        /*
         * Because dealing with menu is very different between nsIAccessible
         * and ATK, and the menu activity is important, specially transfer the
         * following two event.
         * Need more verification by AT test.
         */
    case nsIAccessibleEvent::EVENT_MENU_START:
    case nsIAccessibleEvent::EVENT_MENU_END:
        break;

    case nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE:
      {
        accessible->AsRoot()->mActivated = true;
        guint id = g_signal_lookup("activate", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);

        // Always fire a current focus event after activation.
        FocusMgr()->ForceFocusEvent();
      } break;

    case nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE:
      {
        accessible->AsRoot()->mActivated = false;
        guint id = g_signal_lookup("deactivate", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
      } break;

    case nsIAccessibleEvent::EVENT_WINDOW_MAXIMIZE:
      {
        guint id = g_signal_lookup("maximize", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
      } break;

    case nsIAccessibleEvent::EVENT_WINDOW_MINIMIZE:
      {
        guint id = g_signal_lookup("minimize", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
      } break;

    case nsIAccessibleEvent::EVENT_WINDOW_RESTORE:
      {
        guint id = g_signal_lookup("restore", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
      } break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
        g_signal_emit_by_name (atkObj, "load_complete");
        // XXX - Handle native dialog accessibles.
        if (!accessible->IsRoot() && accessible->HasARIARole() &&
            accessible->ARIARole() == roles::DIALOG) {
          guint id = g_signal_lookup("activate", MAI_TYPE_ATK_OBJECT);
          g_signal_emit(atkObj, id, 0);
        }
      break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD:
        g_signal_emit_by_name (atkObj, "reload");
      break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED:
        g_signal_emit_by_name (atkObj, "load_stopped");
      break;

    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
        atk_focus_tracker_notify(atkObj); // fire extra focus event
        atk_object_notify_state_change(atkObj, ATK_STATE_VISIBLE, true);
        atk_object_notify_state_change(atkObj, ATK_STATE_SHOWING, true);
        break;

    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
        atk_object_notify_state_change(atkObj, ATK_STATE_VISIBLE, false);
        atk_object_notify_state_change(atkObj, ATK_STATE_SHOWING, false);
        break;
    }

    return NS_OK;
}

void
a11y::ProxyEvent(ProxyAccessible* aTarget, uint32_t aEventType)
{
  AtkObject* wrapper = GetWrapperFor(aTarget);
  if (aEventType == nsIAccessibleEvent::EVENT_FOCUS) {
    atk_focus_tracker_notify(wrapper);
    atk_object_notify_state_change(wrapper, ATK_STATE_FOCUSED, true);
  }
}

nsresult
AccessibleWrap::FireAtkStateChangeEvent(AccEvent* aEvent,
                                        AtkObject* aObject)
{
    AccStateChangeEvent* event = downcast_accEvent(aEvent);
    NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

    bool isEnabled = event->IsStateEnabled();
    int32_t stateIndex = AtkStateMap::GetStateIndexFor(event->GetState());
    if (stateIndex >= 0) {
        NS_ASSERTION(gAtkStateMap[stateIndex].stateMapEntryType != kNoSuchState,
                     "No such state");

        if (gAtkStateMap[stateIndex].atkState != kNone) {
            NS_ASSERTION(gAtkStateMap[stateIndex].stateMapEntryType != kNoStateChange,
                         "State changes should not fired for this state");

            if (gAtkStateMap[stateIndex].stateMapEntryType == kMapOpposite)
                isEnabled = !isEnabled;

            // Fire state change for first state if there is one to map
            atk_object_notify_state_change(aObject,
                                           gAtkStateMap[stateIndex].atkState,
                                           isEnabled);
        }
    }

    return NS_OK;
}

nsresult
AccessibleWrap::FireAtkTextChangedEvent(AccEvent* aEvent,
                                        AtkObject* aObject)
{
    AccTextChangeEvent* event = downcast_accEvent(aEvent);
    NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

    int32_t start = event->GetStartOffset();
    uint32_t length = event->GetLength();
    bool isInserted = event->IsTextInserted();
    bool isFromUserInput = aEvent->IsFromUserInput();
    char* signal_name = nullptr;

  if (gAvailableAtkSignals == eUnknown)
    gAvailableAtkSignals =
      g_signal_lookup("text-insert", G_OBJECT_TYPE(aObject)) ?
        eHaveNewAtkTextSignals : eNoNewAtkSignals;

  if (gAvailableAtkSignals == eNoNewAtkSignals) {
    // XXX remove this code and the gHaveNewTextSignals check when we can
    // stop supporting old atk since it doesn't really work anyway
    // see bug 619002
    signal_name = g_strconcat(isInserted ? "text_changed::insert" :
                              "text_changed::delete",
                              isFromUserInput ? "" : kNonUserInputEvent, nullptr);
    g_signal_emit_by_name(aObject, signal_name, start, length);
  } else {
    nsAutoString text;
    event->GetModifiedText(text);
    signal_name = g_strconcat(isInserted ? "text-insert" : "text-remove",
                              isFromUserInput ? "" : "::system", nullptr);
    g_signal_emit_by_name(aObject, signal_name, start, length,
                          NS_ConvertUTF16toUTF8(text).get());
  }

  g_free(signal_name);
  return NS_OK;
}

nsresult
AccessibleWrap::FireAtkShowHideEvent(AccEvent* aEvent,
                                     AtkObject* aObject, bool aIsAdded)
{
    int32_t indexInParent = getIndexInParentCB(aObject);
    AtkObject *parentObject = getParentCB(aObject);
    NS_ENSURE_STATE(parentObject);

    bool isFromUserInput = aEvent->IsFromUserInput();
    char *signal_name = g_strconcat(aIsAdded ? "children_changed::add" :  "children_changed::remove",
                                    isFromUserInput ? "" : kNonUserInputEvent, nullptr);
    g_signal_emit_by_name(parentObject, signal_name, indexInParent, aObject, nullptr);
    g_free(signal_name);

    return NS_OK;
}
