/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): John Sun (john.sun@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsMai.h"
#include "nsAccessibleWrap.h"
#include "nsAppRootAccessible.h"
#include "nsString.h"

#include "nsMaiInterfaceComponent.h"
#include "nsMaiInterfaceAction.h"
#include "nsMaiInterfaceText.h"
#include "nsMaiInterfaceEditableText.h"
#include "nsMaiInterfaceSelection.h"
#include "nsMaiInterfaceValue.h"
#include "nsMaiInterfaceHypertext.h"
#include "nsMaiInterfaceTable.h"

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
static const gchar*        getDescriptionCB (AtkObject *aAtkObj);
static AtkRole             getRoleCB(AtkObject *aAtkObj);
static AtkObject*          getParentCB(AtkObject *aAtkObj);
static gint                getChildCountCB(AtkObject *aAtkObj);
static AtkObject*          refChildCB(AtkObject *aAtkObj, gint aChildIndex);
static gint                getIndexInParentCB(AtkObject *aAtkObj);
static AtkStateSet*        refStateSetCB(AtkObject *aAtkObj);

/* the missing atkobject virtual functions */
/*
  static AtkRelationSet*     refRelationSetCB(AtkObject *aAtkObj);
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

PRUint32 nsAccessibleWrap::mAtkTypeNameIndex = 0;

nsAccessibleWrap::nsAccessibleWrap(nsIDOMNode* aNode,
                                   nsIWeakReference *aShell)
    : nsAccessible(aNode, aShell),
      mMaiAtkObject(nsnull),
      mInterfaces(nsnull),
      mInterfaceCount(0)
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

    if (mMaiAtkObject) {
        MAI_ATK_OBJECT(mMaiAtkObject)->accWrap = nsnull;
        g_object_unref(mMaiAtkObject);
    }
    if (mInterfaces) {
        for (int index = 0; index < MAI_INTERFACE_NUM; ++index)
            delete mInterfaces[index];
        delete [] mInterfaces;
    }
}

NS_IMETHODIMP nsAccessibleWrap::GetExtState(PRUint32 *aState)
{
    PRUint32 state;
    nsAccessible::GetState(&state);
    if (!(state & STATE_INVISIBLE))
      *aState |= STATE_SHOWING;
    return NS_OK;
}

NS_IMETHODIMP nsAccessibleWrap::GetNativeInterface(void **aOutAccessible)
{
    *aOutAccessible = nsnull;

    if (!mMaiAtkObject) {
        CreateMaiInterfaces();
        mMaiAtkObject =
            NS_REINTERPRET_CAST(AtkObject *,
                                g_object_new(GetMaiAtkType(), NULL));
        NS_ENSURE_TRUE(mMaiAtkObject, NS_ERROR_OUT_OF_MEMORY);

        atk_object_initialize(mMaiAtkObject, this);
        mMaiAtkObject->role = ATK_ROLE_INVALID;
        mMaiAtkObject->layer = ATK_LAYER_INVALID;
    }

    *aOutAccessible = mMaiAtkObject;
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
nsresult
nsAccessibleWrap::CreateMaiInterfaces(void)
{
    typedef MaiInterface * MaiInterfacePointer;
    if (!mInterfaces) {
        mInterfaces = new MaiInterfacePointer[MAI_INTERFACE_NUM];
        for (PRUint16 index = 0; index < MAI_INTERFACE_NUM; ++index) {
            mInterfaces[index] = nsnull;
        }
        NS_ENSURE_TRUE(mInterfaces, NS_ERROR_OUT_OF_MEMORY);
    }
    // Add Interfaces for each nsIAccessible.ext interfaces

    nsresult rv;

    // the Component interface are supported by all nsIAccessible
    MaiInterfaceComponent *maiInterfaceComponent =
        new MaiInterfaceComponent(this);
    NS_ENSURE_TRUE(maiInterfaceComponent, NS_ERROR_OUT_OF_MEMORY);
    rv = AddMaiInterface(maiInterfaceComponent);
    NS_ENSURE_SUCCESS(rv, rv);

    // Add Action interface if the action count is more than zero.
    PRUint8 actionCount = 0;
    rv = GetNumActions(&actionCount);
    if (NS_SUCCEEDED(rv) && actionCount > 0) {
        MaiInterfaceAction *maiInterfaceAction = new MaiInterfaceAction(this);
        NS_ENSURE_TRUE(maiInterfaceAction, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceAction);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleText
    nsCOMPtr<nsIAccessibleText> accessInterfaceText;
    QueryInterface(NS_GET_IID(nsIAccessibleText),
                   getter_AddRefs(accessInterfaceText));
    if (accessInterfaceText) {
        MaiInterfaceText *maiInterfaceText = new MaiInterfaceText(this);
        NS_ENSURE_TRUE(maiInterfaceText, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceText);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleEditableText
    nsCOMPtr<nsIAccessibleEditableText> accessInterfaceEditableText;
    QueryInterface(NS_GET_IID(nsIAccessibleEditableText),
                   getter_AddRefs(accessInterfaceEditableText));
    if (accessInterfaceEditableText) {
        MaiInterfaceEditableText *maiInterfaceEditableText =
            new MaiInterfaceEditableText(this);
        NS_ENSURE_TRUE(maiInterfaceEditableText, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceEditableText);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleSelection
    nsCOMPtr<nsIAccessibleSelectable> accessInterfaceSelection;
    QueryInterface(NS_GET_IID(nsIAccessibleSelectable),
                   getter_AddRefs(accessInterfaceSelection));
    if (accessInterfaceSelection) {
        MaiInterfaceSelection *maiInterfaceSelection =
            new MaiInterfaceSelection(this);
        NS_ENSURE_TRUE(maiInterfaceSelection, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceSelection);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleValue
    nsCOMPtr<nsIAccessibleValue> accessInterfaceValue;
    QueryInterface(NS_GET_IID(nsIAccessibleValue),
                   getter_AddRefs(accessInterfaceValue));
    if (accessInterfaceValue) {
        MaiInterfaceValue *maiInterfaceValue = new MaiInterfaceValue(this);
        NS_ENSURE_TRUE(maiInterfaceValue, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceValue);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleHypertext
    nsCOMPtr<nsIAccessibleHyperText> accessInterfaceHypertext;
    QueryInterface(NS_GET_IID(nsIAccessibleHyperText),
                   getter_AddRefs(accessInterfaceHypertext));
    if (accessInterfaceHypertext) {
        MaiInterfaceHypertext *maiInterfaceHypertext =
            new MaiInterfaceHypertext(this, mWeakShell);
        NS_ENSURE_TRUE(maiInterfaceHypertext, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceHypertext);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    //nsIAccessibleTable
    nsCOMPtr<nsIAccessibleTable> accessInterfaceTable;
    QueryInterface(NS_GET_IID(nsIAccessibleTable),
                   getter_AddRefs(accessInterfaceTable));
    if (accessInterfaceTable) {
        MaiInterfaceTable *maiInterfaceTable = new MaiInterfaceTable(this);
        NS_ENSURE_TRUE(maiInterfaceTable, NS_ERROR_OUT_OF_MEMORY);
        rv = AddMaiInterface(maiInterfaceTable);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    return rv;
}

nsresult
nsAccessibleWrap::AddMaiInterface(MaiInterface *aMaiIface)
{
    NS_ENSURE_ARG_POINTER(aMaiIface);
    MaiInterfaceType aMaiIfaceType = aMaiIface->GetType();

    if ((aMaiIfaceType <= MAI_INTERFACE_INVALID) ||
        (aMaiIfaceType >= MAI_INTERFACE_NUM))
        return NS_ERROR_FAILURE;

    // if same type of If has been added, release previous one
    if (mInterfaces[aMaiIfaceType]) {
        delete mInterfaces[aMaiIfaceType];
    }
    mInterfaces[aMaiIfaceType] = aMaiIface;
    mInterfaceCount++;
    return NS_OK;
}
MaiInterface *
nsAccessibleWrap::GetMaiInterface(PRInt16 aIfaceType)
{
    NS_ENSURE_TRUE(aIfaceType > MAI_INTERFACE_INVALID, nsnull);
    NS_ENSURE_TRUE(aIfaceType < MAI_INTERFACE_NUM, nsnull);
    return mInterfaces[aIfaceType];
}

PRUint32
nsAccessibleWrap::GetMaiAtkType(void)
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

    if (mInterfaceCount == 0)
        return MAI_TYPE_ATK_OBJECT;

    type = g_type_register_static(MAI_TYPE_ATK_OBJECT,
                                  nsAccessibleWrap::GetUniqueMaiAtkTypeName(),
                                  &tinfo, GTypeFlags(0));

    for (int index = 0; index < MAI_INTERFACE_NUM; index++) {
        if (!mInterfaces[index])
            continue;
        g_type_add_interface_static(type,
                                    mInterfaces[index]->GetAtkType(),
                                    mInterfaces[index]->GetInterfaceInfo());
    }
    return type;
}

const char *
nsAccessibleWrap::GetUniqueMaiAtkTypeName(void)
{
#define MAI_ATK_TYPE_NAME_LEN (30)     /* 10+sizeof(gulong)*8/4+1 < 30 */

    static gchar namePrefix[] = "MaiAtkType";   /* size = 10 */
    static gchar name[MAI_ATK_TYPE_NAME_LEN + 1];

    sprintf(name, "%s%lx", namePrefix, mAtkTypeNameIndex++);
    name[MAI_ATK_TYPE_NAME_LEN] = '\0';

    MAI_LOG_DEBUG(("MaiWidget::LastedTypeName=%s\n", name));

    return name;
}

/******************************************************************************
The following nsIAccessible states aren't translated, just ignored.
  STATE_MIXED:         For a three-state check box.
  STATE_READONLY:      The object is designated read-only.
  STATE_HOTTRACKED:    Means its appearance has changed to indicate mouse
                       over it.
  STATE_DEFAULT:       Represents the default button in a window.
  STATE_FLOATING:      Not supported yet.
  STATE_MARQUEED:      Indicate scrolling or moving text or graphics.
  STATE_ANIMATED:
  STATE_OFFSCREEN:     Has no on-screen representation.
  STATE_MOVEABLE:
  STATE_SELFVOICING:   The object has self-TTS.
  STATE_LINKED:        The object is formatted as a hyperlink.
  STATE_TRAVERSE:      The object is a hyperlink that has been visited.
  STATE_EXTSELECTABLE: Indicates that an object extends its selectioin.
  STATE_ALERT_LOW:     Not supported yet.
  STATE_ALERT_MEDIUM:  Not supported yet.
  STATE_ALERT_HIGH:    Not supported yet.
  STATE_PROTECTED:     The object is a password-protected edit control.
  STATE_HASPOPUP:      Object displays a pop-up menu or window when invoked.

Returned AtkStatusSet never contain the following AtkStates.
  ATK_STATE_ARMED:     Indicates that the object is armed.
  ATK_STATE_DEFUNCT:   Indicates the user interface object corresponding to
                       thus object no longer exists.
  ATK_STATE_EDITABLE:  Indicates the user can change the contents of the object.
  ATK_STATE_HORIZONTAL:Indicates the orientation of this object is horizontal.
  ATK_STATE_ICONIFIED:
  ATK_STATE_OPAQUE:     Indicates the object paints every pixel within its
                        rectangular region
  ATK_STATE_STALE:      The index associated with this object has changed since
                        the user accessed the object
******************************************************************************/

void
nsAccessibleWrap::TranslateStates(PRUint32 aState, void *aAtkStateSet)
{
    if (!aAtkStateSet)
        return;
    AtkStateSet *state_set = NS_STATIC_CAST(AtkStateSet *, aAtkStateSet);

    if (aState & nsIAccessible::STATE_SELECTED)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTED);

    if (aState & nsIAccessible::STATE_FOCUSED)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);

    if (aState & nsIAccessible::STATE_PRESSED)
        atk_state_set_add_state (state_set, ATK_STATE_PRESSED);

    if (aState & nsIAccessible::STATE_CHECKED)
        atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

    if (aState & nsIAccessible::STATE_EXPANDED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDED);

    if (aState & nsIAccessible::STATE_COLLAPSED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);
                   
    // The control can't accept input at this time
    if (aState & nsIAccessible::STATE_BUSY)
        atk_state_set_add_state (state_set, ATK_STATE_BUSY);

    if (aState & nsIAccessible::STATE_FOCUSABLE)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

    if (!(aState & nsIAccessible::STATE_INVISIBLE))
        atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

    if (aState & nsIAccessible::STATE_SELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

    if (aState & nsIAccessible::STATE_SIZEABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aState & nsIAccessible::STATE_MULTISELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTISELECTABLE);

    if (!(aState & nsIAccessible::STATE_UNAVAILABLE)) {
        atk_state_set_add_state (state_set, ATK_STATE_ENABLED);
        atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
    }

    // The following state is
    // Extended state flags (for now non-MSAA, for Java and Gnome/ATK support)
    // This is only the states that there isn't already a mapping for in MSAA
    // See www.accessmozilla.org/article.php?sid=11 for information on the
    // mappings between accessibility API state
    if (aState & nsIAccessible::STATE_INVALID)
        atk_state_set_add_state (state_set, ATK_STATE_INVALID);

    if (aState & nsIAccessible::STATE_ACTIVE)
        atk_state_set_add_state (state_set, ATK_STATE_ACTIVE);

    if (aState & nsIAccessible::STATE_EXPANDABLE)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);

    if (aState & nsIAccessible::STATE_MODAL)
        atk_state_set_add_state (state_set, ATK_STATE_MODAL);

    if (aState & nsIAccessible::STATE_MULTI_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

    if (aState & nsIAccessible::STATE_SENSITIVE)
        atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);

    if (aState & nsIAccessible::STATE_RESIZABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aState & nsIAccessible::STATE_SHOWING)
        atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

    if (aState & nsIAccessible::STATE_SINGLE_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

    if (aState & nsIAccessible::STATE_TRANSIENT)
        atk_state_set_add_state (state_set, ATK_STATE_TRANSIENT);

    if (aState & nsIAccessible::STATE_VERTICAL)
        atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);

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
    aClass->ref_state_set = refStateSetCB;

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
    NS_ASSERTION((MAI_IS_ATK_OBJECT(aAtkObj)), "Invalid AtkObject");
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
    if (!MAI_IS_ATK_OBJECT(aObj))
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

    if (!aAtkObj->name) {
        gchar default_name[] = "no name";
        gint len;
        nsAutoString uniName;

        nsAccessibleWrap *accWrap =
            NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

        /* nsIAccessible is responsible for the non-NULL name */
        nsresult rv = accWrap->GetName(uniName);
        NS_ENSURE_SUCCESS(rv, nsnull);
        len = uniName.Length();
        if (len > 0) {
            atk_object_set_name(aAtkObj,
                                NS_ConvertUCS2toUTF8(uniName).get());
        }
        else {
            atk_object_set_name(aAtkObj, default_name);
        }
    }
    return aAtkObj->name;
}

const gchar *
getDescriptionCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);

    if (!aAtkObj->description) {
        gchar default_description[] = "no description";
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
                                       NS_ConvertUCS2toUTF8(uniDesc).get());
        }
        else {
            atk_object_set_description(aAtkObj, default_description);
        }
    }
    return aAtkObj->description;
}

AtkRole
getRoleCB(AtkObject *aAtkObj)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), ATK_ROLE_INVALID);

    if (aAtkObj->role == ATK_ROLE_INVALID) {
        nsAccessibleWrap *accWrap =
            NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

        PRUint32 accRole;
        nsresult rv = accWrap->GetRole(&accRole);
        NS_ENSURE_SUCCESS(rv, ATK_ROLE_INVALID);

        //the cross-platform Accessible object returns the same value for
        //both "ROLE_MENUITEM" and "ROLE_MENUPOPUP"
        if (accRole == nsIAccessible::ROLE_MENUITEM) {
            PRInt32 childCount = 0;
            accWrap->GetChildCount(&childCount);
            if (childCount > 0)
                accRole = nsIAccessible::ROLE_MENUPOPUP;
        }
        else if (accRole == nsIAccessible::ROLE_LINK) {
            //ATK doesn't have role-link now
            //register it on runtime
            static AtkRole linkRole = (AtkRole)0;
            if (linkRole == 0) {
                linkRole = atk_role_register("hyper link");
            }
            accRole = linkRole;
        }
        aAtkObj->role = NS_STATIC_CAST(AtkRole, accRole);
    }
    return aAtkObj->role;
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
    accWrap->GetChildCount(&count);
    return count;
}

AtkObject *
refChildCB(AtkObject *aAtkObj, gint aChildIndex)
{
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), nsnull);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    nsresult rv;
    nsCOMPtr<nsIAccessible> accChild;
    rv = accWrap->GetChildAt(aChildIndex, getter_AddRefs(accChild));

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
    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), -1);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    // we use accId to decide two accessible objects are the same.
    void *accId = nsnull;
    NS_ENSURE_SUCCESS(accWrap->GetUniqueID(&accId), -1);

    nsCOMPtr<nsIAccessible> accParent;
    nsresult rv = accWrap->GetParent(getter_AddRefs(accParent));
    if (NS_FAILED(rv) || !accParent)
        return -1;

    nsCOMPtr<nsIAccessible> accChild;
    nsCOMPtr<nsIAccessible> accTmpChild;
    accParent->GetFirstChild(getter_AddRefs(accChild));

    PRInt32 currentIndex = -1;
    void *currentAccId = nsnull;
    while (accChild) {
        ++currentIndex;
        nsCOMPtr<nsIAccessNode> accNode(do_QueryInterface(accChild));
        if (accNode) {
            accNode->GetUniqueID(&currentAccId);
            if (currentAccId == accId)
                break;
        }
        accChild->GetNextSibling(getter_AddRefs(accTmpChild));
        accChild = accTmpChild;
    }
    return currentIndex;
}

AtkStateSet *
refStateSetCB(AtkObject *aAtkObj)
{
    AtkStateSet *state_set = nsnull;
    state_set = ATK_OBJECT_CLASS(parent_class)->ref_state_set(aAtkObj);

    NS_ENSURE_SUCCESS(CheckMaiAtkObject(aAtkObj), state_set);
    nsAccessibleWrap *accWrap =
        NS_REINTERPRET_CAST(MaiAtkObject*, aAtkObj)->accWrap;

    PRUint32 accState = 0;
    nsresult rv = accWrap->GetState(&accState);
    NS_ENSURE_SUCCESS(rv, state_set);

    rv = accWrap->GetExtState(&accState);
    NS_ENSURE_SUCCESS(rv, state_set);

    if (accState == 0)
      return state_set;

    nsAccessibleWrap::TranslateStates(accState, state_set);
    return state_set;
}

// Check if aAtkObj is a valid MaiAtkObject
nsresult
CheckMaiAtkObject(AtkObject *aAtkObj)
{
    NS_ENSURE_ARG(MAI_IS_ATK_OBJECT(aAtkObj));
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
    NS_ENSURE_TRUE(MAI_IS_ATK_OBJECT(aAtkObj), nsnull);
    nsAccessibleWrap * tmpAccWrap = MAI_ATK_OBJECT(aAtkObj)->accWrap;
    NS_ENSURE_TRUE(tmpAccWrap != nsnull, nsnull);
    NS_ENSURE_TRUE(tmpAccWrap->GetAtkObject() == aAtkObj, nsnull);
    return tmpAccWrap;
}
