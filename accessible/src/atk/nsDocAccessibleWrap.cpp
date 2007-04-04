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
 *   Ginn Chen (ginn.chen@sun.com)
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
#include "nsAutoPtr.h"
#include "nsRootAccessible.h"
#include "nsDocAccessibleWrap.h"
#include "nsAccessibleEventData.h"
#include "nsIAccessibleValue.h"

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "nsStateMap.h"

//----- nsDocAccessibleWrap -----

/*
 * Must keep sychronization with enumerate AtkProperty in 
 * accessible/src/base/nsAccessibleEventData.h
 */
static char * sAtkPropertyNameArray[PROP_LAST] = {
    0,
    "accessible-name",
    "accessible-description",
    "accessible-parent",
    "accessible-role",
    "accessible-layer",
    "accessible-mdi-zorder",
    "accessible-table-caption",
    "accessible-table-column-description",
    "accessible-table-column-header",
    "accessible-table-row-description",
    "accessible-table-row-header",
    "accessible-table-summary"
};

nsDocAccessibleWrap::nsDocAccessibleWrap(nsIDOMNode *aDOMNode,
                                         nsIWeakReference *aShell): 
  nsDocAccessible(aDOMNode, aShell), mActivated(PR_FALSE)
{
}

nsDocAccessibleWrap::~nsDocAccessibleWrap()
{
}

NS_IMETHODIMP nsDocAccessibleWrap::FireToolkitEvent(PRUint32 aEvent,
                                                    nsIAccessible* aAccessible,
                                                    void* aEventData)
{
    NS_ENSURE_ARG_POINTER(aAccessible);

    // First fire nsIObserver event for internal xpcom accessibility clients
    nsDocAccessible::FireToolkitEvent(aEvent, aAccessible, aEventData);

    nsresult rv = NS_ERROR_FAILURE;

    nsAccessibleWrap *accWrap =
        NS_STATIC_CAST(nsAccessibleWrap *, aAccessible);
    MAI_LOG_DEBUG(("\n\nReceived event: aEvent=%u, obj=0x%x, data=0x%x \n",
                   aEvent, aAccessible, aEventData));
    // We don't create ATK objects for nsIAccessible plain text leaves,
    // just return NS_OK in such case
    AtkObject *atkObj = accWrap->GetAtkObject();
    if (!atkObj) {
      return NS_OK;
    }

    nsAccessibleWrap *oldAccWrap = nsnull, *newAccWrap = nsnull;
    AtkTableChange * pAtkTableChange = nsnull;

    switch (aEvent) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_FOCUS\n"));
        nsRefPtr<nsRootAccessible> rootAccWrap = accWrap->GetRootAccessible();
        if (rootAccWrap && rootAccWrap->mActivated) {
          atk_focus_tracker_notify(atkObj);
        }
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_STATE_CHANGE:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_STATE_CHANGE\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        StateChange *pStateChange = NS_REINTERPRET_CAST(StateChange *, aEventData);
        PRInt32 stateIndex = AtkStateMap::GetStateIndexFor(pStateChange->state);
        if (stateIndex >= 0) {
          const AtkStateMap *atkStateMap = pStateChange->isExtendedState ? gAtkStateMapExt : gAtkStateMap;
          NS_ASSERTION(atkStateMap[stateIndex].stateMapEntryType != kNoSuchState, "No such state");
          if (atkStateMap[stateIndex].atkState != kNone) {
            NS_ASSERTION(atkStateMap[stateIndex].stateMapEntryType != kNoStateChange,
                         "State changes should not fired for this state");
            if (atkStateMap[stateIndex].stateMapEntryType == kMapOpposite) {
              pStateChange->enable = !pStateChange->enable;
            }
            // Fire state change for first state if there is one to map
            atk_object_notify_state_change(atkObj,
                                           atkStateMap[stateIndex].atkState,
                                           pStateChange->enable);
          }
        }
        rv = NS_OK;
      }
      break;
      
    case nsIAccessibleEvent::EVENT_VALUE_CHANGE :
      {
        nsCOMPtr<nsIAccessibleValue> value(do_QueryInterface(aAccessible));
        if (value) {    // Make sure this is a numeric value
          // Don't fire for MSAA string value changes (e.g. text editing)
          // ATK values are always numeric
          g_object_notify( (GObject*)atkObj, "accessible-value" );
        }
      }
      break;
        
    /*
     * Need handle each type of property change separately.
     */        
    case nsIAccessibleEvent::EVENT_PROPERTY_CHANGED :
      {
        AtkPropertyChange *pAtkPropChange;
        AtkPropertyValues values = { NULL };

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_PROPERTY_CHANGED\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkPropChange = NS_REINTERPRET_CAST(AtkPropertyChange *, aEventData);
        values.property_name = sAtkPropertyNameArray[pAtkPropChange->type];
        
        MAI_LOG_DEBUG(("\n\nthe type of EVENT_PROPERTY_CHANGED: %d\n\n",
                       pAtkPropChange->type));

        switch (pAtkPropChange->type) {
        case PROP_TABLE_CAPTION:
        case PROP_TABLE_SUMMARY:

            if (pAtkPropChange->oldvalue)
                oldAccWrap = NS_REINTERPRET_CAST(nsAccessibleWrap *,
                                                 pAtkPropChange->oldvalue);

            if (pAtkPropChange->newvalue)
                newAccWrap = NS_REINTERPRET_CAST(nsAccessibleWrap *,
                                                 pAtkPropChange->newvalue);

            if (oldAccWrap && newAccWrap) {
                g_value_init(&values.old_value, G_TYPE_POINTER);
                g_value_set_pointer(&values.old_value,
                                    oldAccWrap->GetAtkObject());
                g_value_init(&values.new_value, G_TYPE_POINTER);
                g_value_set_pointer(&values.new_value,
                                    newAccWrap->GetAtkObject());
                rv = NS_OK;
            }
            break;

        case PROP_TABLE_COLUMN_DESCRIPTION:
        case PROP_TABLE_COLUMN_HEADER:
        case PROP_TABLE_ROW_HEADER:
        case PROP_TABLE_ROW_DESCRIPTION:
            g_value_init(&values.new_value, G_TYPE_INT);
            g_value_set_int(&values.new_value,
                            *NS_REINTERPRET_CAST(gint *,
                                                 pAtkPropChange->newvalue));
            rv = NS_OK;
            break;
  
            //Perhaps need more cases in the future
        default:
            g_value_init (&values.old_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.old_value, pAtkPropChange->oldvalue);
            g_value_init (&values.new_value, G_TYPE_POINTER);
            g_value_set_pointer (&values.new_value, pAtkPropChange->newvalue);
            rv = NS_OK;
        }
        if (NS_SUCCEEDED(rv)) {
            char *signal_name = g_strconcat("property_change::",
                                            values.property_name, NULL);
            g_signal_emit_by_name(atkObj, signal_name, &values, NULL);
            g_free (signal_name);
        }
      }
        break;

    case nsIAccessibleEvent::EVENT_SELECTION_CHANGED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_SELECTION_CHANGED\n"));
        g_signal_emit_by_name(atkObj, "selection_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TEXT_CHANGED:
        AtkTextChange *pAtkTextChange;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TEXT_CHANGED\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkTextChange = NS_REINTERPRET_CAST(AtkTextChange *, aEventData);
        g_signal_emit_by_name (atkObj,
                               pAtkTextChange->add ? \
                               "text_changed::insert":"text_changed::delete",
                               pAtkTextChange->start,
                               pAtkTextChange->length);
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TEXT_SELECTION_CHANGED\n"));
        g_signal_emit_by_name(atkObj, "text_selection_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TEXT_CARET_MOVED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TEXT_CARET_MOVED\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        MAI_LOG_DEBUG(("\n\nCaret postion: %d", *(gint *)aEventData ));
        g_signal_emit_by_name(atkObj,
                              "text_caret_moved",
                              // Curent caret position
                              *(gint *)aEventData);
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TABLE_MODEL_CHANGED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_MODEL_CHANGED\n"));
        g_signal_emit_by_name(atkObj, "model_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TABLE_ROW_INSERT:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_ROW_INSERT\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(atkObj,
                              "row_inserted",
                              // After which the rows are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;
        
    case nsIAccessibleEvent::EVENT_TABLE_ROW_DELETE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_ROW_DELETE\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(atkObj,
                              "row_deleted",
                              // After which the rows are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;
        
    case nsIAccessibleEvent::EVENT_TABLE_ROW_REORDER:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_ROW_REORDER\n"));
        g_signal_emit_by_name(atkObj, "row_reordered");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_INSERT:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_COLUMN_INSERT\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(atkObj,
                              "column_inserted",
                              // After which the columns are inserted
                              pAtkTableChange->index,
                              // The number of the inserted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_DELETE:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_COLUMN_DELETE\n"));
        NS_ASSERTION(aEventData, "Event needs event data");
        if (!aEventData)
            break;

        pAtkTableChange = NS_REINTERPRET_CAST(AtkTableChange *, aEventData);

        g_signal_emit_by_name(atkObj,
                              "column_deleted",
                              // After which the columns are deleted
                              pAtkTableChange->index,
                              // The number of the deleted
                              pAtkTableChange->count);
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_TABLE_COLUMN_REORDER:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_TABLE_COLUMN_REORDER\n"));
        g_signal_emit_by_name(atkObj, "column_reordered");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_SECTION_CHANGED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_SECTION_CHANGED\n"));
        g_signal_emit_by_name(atkObj, "visible_data_changed");
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_HYPERTEXT_LINK_SELECTED:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_HYPERTEXT_LINK_SELECTED\n"));
        atk_focus_tracker_notify(atkObj);
        g_signal_emit_by_name(atkObj,
                              "link_selected",
                              // Selected link index 
                              *(gint *)aEventData);
        rv = NS_OK;
        break;

        // Is a superclass of ATK event children_changed
    case nsIAccessibleEvent::EVENT_REORDER:
        AtkChildrenChange *pAtkChildrenChange;

        MAI_LOG_DEBUG(("\n\nReceived: EVENT_REORDER(children_change)\n"));

        pAtkChildrenChange = NS_REINTERPRET_CAST(AtkChildrenChange *,
                                                 aEventData);
        nsAccessibleWrap *childAccWrap;
        if (pAtkChildrenChange && pAtkChildrenChange->child) {
            childAccWrap = NS_STATIC_CAST(nsAccessibleWrap *,
                                          pAtkChildrenChange->child);
            g_signal_emit_by_name (atkObj,
                                   pAtkChildrenChange->add ? \
                                   "children_changed::add" : \
                                   "children_changed::remove",
                                   pAtkChildrenChange->index,
                                   childAccWrap->GetAtkObject(),
                                   NULL);
        }
        else {
            //
            // EVENT_REORDER is normally fired by "HTML Document".
            //
            // In GOK, [only] "children_changed::add" can cause foreground
            // window accessible to update it children, which will
            // refresh "UI-Grab" window.
            //
            g_signal_emit_by_name (atkObj,
                                   "children_changed::add",
                                   -1, NULL, NULL);
        }

        rv = NS_OK;
        break;

        /*
         * Because dealing with menu is very different between nsIAccessible
         * and ATK, and the menu activity is important, specially transfer the
         * following two event.
         * Need more verification by AT test.
         */
    case nsIAccessibleEvent::EVENT_MENU_START:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_MENU_START\n"));
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_MENU_END:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_MENU_END\n"));
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_WINDOW_ACTIVATE:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_WINDOW_ACTIVATED\n"));
        nsDocAccessibleWrap *accDocWrap =
          NS_STATIC_CAST(nsDocAccessibleWrap *, aAccessible);
        accDocWrap->mActivated = PR_TRUE;
        guint id = g_signal_lookup ("activate", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_WINDOW_DEACTIVATE:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_WINDOW_DEACTIVATED\n"));
        nsDocAccessibleWrap *accDocWrap =
          NS_STATIC_CAST(nsDocAccessibleWrap *, aAccessible);
        accDocWrap->mActivated = PR_FALSE;
        guint id = g_signal_lookup ("deactivate", MAI_TYPE_ATK_OBJECT);
        g_signal_emit(atkObj, id, 0);
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_COMPLETE:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_DOCUMENT_LOAD_COMPLETE\n"));
        g_signal_emit_by_name (atkObj, "load_complete");
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_RELOAD:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_DOCUMENT_RELOAD\n"));
        g_signal_emit_by_name (atkObj, "reload");
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_LOAD_STOPPED:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_DOCUMENT_LOAD_STOPPED\n"));
        g_signal_emit_by_name (atkObj, "load_stopped");
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_DOCUMENT_ATTRIBUTES_CHANGED:
      {
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_DOCUMENT_ATTRIBUTES_CHANGED\n"));
        g_signal_emit_by_name (atkObj, "attributes_changed");
        rv = NS_OK;
      } break;

    case nsIAccessibleEvent::EVENT_SHOW:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_START:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_SHOW\n"));
        atk_object_notify_state_change(atkObj, ATK_STATE_VISIBLE, PR_TRUE);
        atk_object_notify_state_change(atkObj, ATK_STATE_SHOWING, PR_TRUE);
        rv = NS_OK;
        break;

    case nsIAccessibleEvent::EVENT_HIDE:
    case nsIAccessibleEvent::EVENT_MENUPOPUP_END:
        MAI_LOG_DEBUG(("\n\nReceived: EVENT_HIDE\n"));
        atk_object_notify_state_change(atkObj, ATK_STATE_VISIBLE, PR_FALSE);
        atk_object_notify_state_change(atkObj, ATK_STATE_SHOWING, PR_FALSE);
        rv = NS_OK;
        break;

    default:
        // Don't transfer others
        MAI_LOG_DEBUG(("\n\nReceived an unknown event=0x%u\n", aEvent));
        break;
    }

    return rv;
}
