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

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "nsAccessibleWrap.h"
#include "nsString.h"

/* MaiAtkObject */

#define MAI_TYPE_ATK_OBJECT             (mai_atk_object_get_type ())
#define MAI_ATK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         MAI_TYPE_ATK_OBJECT, MaiAtkObject))
#define MAI_ATK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
#define MAI_IS_ATK_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_IS_ATK_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))

//typedef struct _MaiAtkObject                MaiAtkObject;
typedef struct _MaiAtkObjectClass           MaiAtkObjectClass;

/**
 * This MaiAtkObject is a thin wrapper, in the MAI namespace, for AtkObject
 */

struct _MaiAtkObject
{
    AtkObject parent;

    /*
     * The nsAccessibleWrap whose properties and features are exported
     * via this object instance.
     */
    nsAccessibleWrap *accWrap;
};

struct _MaiAtkObjectClass
{
    AtkObjectClass parent_class;
};

GType mai_atk_object_get_type(void);

#ifdef MAI_LOGGING
gint num_created_mai_object = 0;
gint num_deleted_mai_object = 0;
#endif

static nsresult CheckMaiAtkObject(AtkObject *aAtkObj);

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

/* implemented by MaiWidget */
//static AtkStateSet*        refStateSetCB(AtkObject *aAtkObj);
//static AtkRole             getRoleCB(AtkObject *aAtkObj);

//static AtkRelationSet*     refRelationSetCB(AtkObject *aAtkObj);
//static AtkLayer            getLayerCB(AtkObject *aAtkObj);
//static gint                getMdiZorderCB(AtkObject *aAtkObj);

/* the missing atkobject virtual functions */
/*
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

nsAccessibleWrap::nsAccessibleWrap(nsIDOMNode* aNode,
                                   nsIWeakReference *aShell)
    : nsAccessible(aNode, aShell),
      mMaiAtkObject(nsnull)
{
#ifdef MAI_LOGGING
    num_created_mai_object++;
#endif
    MAI_LOG_DEBUG(("==nsAccessibleWrap creating this=0x%x,total %d created\n",
                   (unsigned int)this, num_created_mai_object));
}

nsAccessibleWrap::~nsAccessibleWrap()
{

#ifdef MAI_LOGGING
    num_deleted_mai_object++;
#endif
    MAI_LOG_DEBUG(("==nsAccessibleWrap deleting this=0x%x, total %d deleted\n",
                   (unsigned int)this, num_deleted_mai_object));

    if (mMaiAtkObject) {
        mMaiAtkObject->accWrap = nsnull;
        g_object_unref(mMaiAtkObject);
    }
}

AtkObject *
nsAccessibleWrap::GetAtkObject(void)
{
    if (mMaiAtkObject)
        return ATK_OBJECT(mMaiAtkObject);

    CreateMaiInterfaces();
    mMaiAtkObject = (MaiAtkObject*) g_object_new(MAI_TYPE_ATK_OBJECT, NULL);
    g_return_val_if_fail(mMaiAtkObject != NULL, NULL);

    atk_object_initialize(ATK_OBJECT(mMaiAtkObject), this);
    ATK_OBJECT(mMaiAtkObject)->role = ATK_ROLE_INVALID;
    ATK_OBJECT(mMaiAtkObject)->layer = ATK_LAYER_INVALID;
    MAI_LOG_DEBUG(("nsAccessibleWrap: Create MaiAtkObject=%p for AccWrap=%p\n",
                   (void*)mMaiAtkObject, (void*)this));

    return ATK_OBJECT(mMaiAtkObject);
}

/* private */
void
nsAccessibleWrap::CreateMaiInterfaces(void)
{
    return;
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

    aClass->initialize = initializeCB;

    gobject_class->finalize = finalizeCB;
}

void
initializeCB(AtkObject *aAtkObj, gpointer aData)
{
    g_return_if_fail(MAI_IS_ATK_OBJECT(aAtkObj));
    g_return_if_fail(aData != NULL);

    /* call parent init function */
    /* AtkObjectClass has not a "initialize" function now,
     * maybe it has later
     */

    if (ATK_OBJECT_CLASS(parent_class)->initialize)
        ATK_OBJECT_CLASS(parent_class)->initialize(aAtkObj, aData);

    /* initialize object */
    MAI_ATK_OBJECT(aAtkObj)->accWrap =
        NS_STATIC_CAST(nsAccessibleWrap*, aData);
}

void
finalizeCB(GObject *aObj)
{
    if (!MAI_IS_ATK_OBJECT(aObj))
        return;
    NS_ASSERTION(MAI_ATK_OBJECT(aObj)->accWrap == nsnull, "AccWrap NOT null");
    MAI_LOG_DEBUG(("====release MaiAtkObject=0x%x\n", (guint)aObj));

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
        nsresult rv = accWrap->GetAccName(uniName);
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
        nsresult rv = accWrap->GetAccDescription(uniDesc);
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
        nsresult rv = accWrap->GetAccRole(&accRole);
        NS_ENSURE_SUCCESS(rv, ATK_ROLE_INVALID);

        //the cross-platform Accessible object returns the same value for
        //both "ATK_ROLE_MENU_ITEM" and "ATK_ROLE_MENU"
        if (accRole == ATK_ROLE_MENU_ITEM) {
            PRInt32 childCount = 0;
            accWrap->GetAccChildCount(&childCount);
            if (childCount > 0)
                accRole = ATK_ROLE_MENU;
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
    nsresult rv = accWrap->GetAccParent(getter_AddRefs(accParent));
    if (NS_FAILED(rv) || !accParent)
        return nsnull;
    nsIAccessible *tmpAccParent = accParent;
    nsAccessibleWrap *accWrapParent = NS_STATIC_CAST(nsAccessibleWrap *,
                                                     tmpAccParent);

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
    accWrap->GetAccChildCount(&count);
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
    nsresult rv = accWrap->GetAccParent(getter_AddRefs(accParent));
    if (NS_FAILED(rv) || !accParent)
        return -1;

    nsCOMPtr<nsIAccessible> accChild;
    nsCOMPtr<nsIAccessible> accTmpChild;
    accWrap->GetAccFirstChild(getter_AddRefs(accChild));

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
        accChild->GetAccNextSibling(getter_AddRefs(accTmpChild));
        accChild = accTmpChild;
    }
    return currentIndex;
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
nsAccessibleWrap::TranslateStates(PRUint32 aAccState, AtkStateSet *state_set)
{
    g_return_if_fail(state_set);

    if (aAccState & nsIAccessible::STATE_SELECTED)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTED);

    if (aAccState & nsIAccessible::STATE_FOCUSED)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);

    if (aAccState & nsIAccessible::STATE_PRESSED)
        atk_state_set_add_state (state_set, ATK_STATE_PRESSED);

    if (aAccState & nsIAccessible::STATE_CHECKED)
        atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

    if (aAccState & nsIAccessible::STATE_EXPANDED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDED);

    if (aAccState & nsIAccessible::STATE_COLLAPSED)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);
                   
    // The control can't accept input at this time
    if (aAccState & nsIAccessible::STATE_BUSY)
        atk_state_set_add_state (state_set, ATK_STATE_BUSY);

    if (aAccState & nsIAccessible::STATE_FOCUSABLE)
        atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);

    if (!(aAccState & nsIAccessible::STATE_INVISIBLE))
        atk_state_set_add_state (state_set, ATK_STATE_VISIBLE);

    if (aAccState & nsIAccessible::STATE_SELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);

    if (aAccState & nsIAccessible::STATE_SIZEABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aAccState & nsIAccessible::STATE_MULTISELECTABLE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTISELECTABLE);

    if (!(aAccState & nsIAccessible::STATE_UNAVAILABLE))
        atk_state_set_add_state (state_set, ATK_STATE_ENABLED);

    // The following state is
    // Extended state flags (for now non-MSAA, for Java and Gnome/ATK support)
    // This is only the states that there isn't already a mapping for in MSAA
    // See www.accessmozilla.org/article.php?sid=11 for information on the
    // mappings between accessibility API state
    if (aAccState & nsIAccessible::STATE_INVALID)
        atk_state_set_add_state (state_set, ATK_STATE_INVALID);

    if (aAccState & nsIAccessible::STATE_ACTIVE)
        atk_state_set_add_state (state_set, ATK_STATE_ACTIVE);

    if (aAccState & nsIAccessible::STATE_EXPANDABLE)
        atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);

    if (aAccState & nsIAccessible::STATE_MODAL)
        atk_state_set_add_state (state_set, ATK_STATE_MODAL);

    if (aAccState & nsIAccessible::STATE_MULTI_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);

    if (aAccState & nsIAccessible::STATE_SENSITIVE)
        atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);

    if (aAccState & nsIAccessible::STATE_RESIZABLE)
        atk_state_set_add_state (state_set, ATK_STATE_RESIZABLE);

    if (aAccState & nsIAccessible::STATE_SHOWING)
        atk_state_set_add_state (state_set, ATK_STATE_SHOWING);

    if (aAccState & nsIAccessible::STATE_SINGLE_LINE)
        atk_state_set_add_state (state_set, ATK_STATE_SINGLE_LINE);

    if (aAccState & nsIAccessible::STATE_TRANSIENT)
        atk_state_set_add_state (state_set, ATK_STATE_TRANSIENT);

    if (aAccState & nsIAccessible::STATE_VERTICAL)
        atk_state_set_add_state (state_set, ATK_STATE_VERTICAL);

}

nsresult
CheckMaiAtkObject(AtkObject *aAtkObj)
{
    NS_ENSURE_ARG(MAI_IS_ATK_OBJECT(aAtkObj));
    nsAccessibleWrap * tmpAccWrap = MAI_ATK_OBJECT(aAtkObj)->accWrap;
    NS_ENSURE_TRUE(tmpAccWrap != nsnull, NS_ERROR_INVALID_POINTER);
    NS_ENSURE_TRUE(tmpAccWrap->GetAtkObject() == aAtkObj,
                   NS_ERROR_FAILURE);
    return NS_OK;
}
