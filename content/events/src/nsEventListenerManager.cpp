/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsISupports.h"
#include "nsGUIEvent.h"
#include "nsDOMEvent.h"
#include "nsEventListenerManager.h"
#include "nsICaret.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMTextListener.h"
#include "nsIDOMCompositionListener.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMUIListener.h"
#include "nsITextControlFrame.h"
#ifdef MOZ_SVG
#include "nsIDOMSVGListener.h"
#include "nsIDOMSVGZoomListener.h"
#include "nsGkAtoms.h"
#endif // MOZ_SVG
#include "nsIEventStateManager.h"
#include "nsPIDOMWindow.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIJSEventListener.h"
#include "prmem.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptRuntime.h"
#include "nsLayoutUtils.h"
#ifdef MOZ_XUL
// XXXbz the fact that this is ifdef MOZ_XUL is good indication that
// it doesn't belong here...
#include "nsITreeBoxObject.h"
#include "nsITreeColumns.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#endif
#include "nsINameSpaceManager.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIJSContextStack.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsMutationEvent.h"
#include "nsIXPConnect.h"
#include "nsDOMCID.h"
#include "nsIScriptObjectOwner.h" // for nsIScriptEventHandlerOwner
#include "nsIFocusController.h"
#include "nsIDOMElement.h"
#include "nsIBoxObject.h"
#include "nsIDOMNSDocument.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#include "nsJSUtils.h"
#include "nsIDOMEventGroup.h"
#include "nsContentCID.h"
#include "nsEventDispatcher.h"
#include "nsDOMJSUtils.h"
#include "nsDOMScriptObjectHolder.h"
#include "nsDataHashtable.h"

#define EVENT_TYPE_EQUALS( ls, type, userType ) \
  (ls->mEventType && ls->mEventType == type && \
  (ls->mEventType != NS_USER_DEFINED_EVENT || ls->mTypeAtom == userType))

#define EVENT_TYPE_DATA_EQUALS( type1, type2 ) \
  (type1 && type2 && type1->iid && type2->iid && \
   type1->iid->Equals(*(type2->iid)))

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID,
                     NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_CID(kDOMEventGroupCID, NS_DOMEVENTGROUP_CID);

static const PRUint32 kAllMutationBits =
  NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED |
  NS_EVENT_BITS_MUTATION_NODEINSERTED |
  NS_EVENT_BITS_MUTATION_NODEREMOVED |
  NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT |
  NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT |
  NS_EVENT_BITS_MUTATION_ATTRMODIFIED |
  NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;

static PRUint32
MutationBitForEventType(PRUint32 aEventType)
{
  switch (aEventType) {
    case NS_MUTATION_SUBTREEMODIFIED:
      return NS_EVENT_BITS_MUTATION_SUBTREEMODIFIED;
    case NS_MUTATION_NODEINSERTED:
      return NS_EVENT_BITS_MUTATION_NODEINSERTED;
    case NS_MUTATION_NODEREMOVED:
      return NS_EVENT_BITS_MUTATION_NODEREMOVED;
    case NS_MUTATION_NODEREMOVEDFROMDOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEREMOVEDFROMDOCUMENT;
    case NS_MUTATION_NODEINSERTEDINTODOCUMENT:
      return NS_EVENT_BITS_MUTATION_NODEINSERTEDINTODOCUMENT;
    case NS_MUTATION_ATTRMODIFIED:
      return NS_EVENT_BITS_MUTATION_ATTRMODIFIED;
    case NS_MUTATION_CHARACTERDATAMODIFIED:
      return NS_EVENT_BITS_MUTATION_CHARACTERDATAMODIFIED;
    default:
      break;
  }
  return 0;
}

typedef
NS_STDCALL_FUNCPROTO(nsresult,
                     GenericHandler,
                     nsIDOMEventListener, HandleEvent, 
                     (nsIDOMEvent*));

/*
 * Things here are not as they appear.  Namely, |ifaceListener| below is
 * not really a pointer to the nsIDOMEventListener interface, and aMethod is
 * not really a pointer-to-member for nsIDOMEventListener.  They both
 * actually refer to the event-type-specific listener interface.  The casting
 * magic allows us to use a single dispatch method.  This relies on the
 * assumption that nsIDOMEventListener and the event type listener interfaces
 * have the same object layout and will therefore have compatible
 * pointer-to-member implementations.
 */

static nsresult DispatchToInterface(nsIDOMEvent* aEvent,
                                    nsIDOMEventListener* aListener,
                                    GenericHandler aMethod,
                                    const nsIID& aIID)
{
  nsIDOMEventListener* ifaceListener = nsnull;
  nsresult rv = NS_OK;
  aListener->QueryInterface(aIID, (void**) &ifaceListener);
  NS_WARN_IF_FALSE(ifaceListener,
                   "DispatchToInterface couldn't QI to the right interface");
  if (ifaceListener) {
    rv = (ifaceListener->*aMethod)(aEvent);
    NS_RELEASE(ifaceListener);
  }
  return rv;
}

struct EventDispatchData
{
  PRUint32 message;
  GenericHandler method;
};

struct EventTypeData
{
  const EventDispatchData* events;
  int                      numEvents;
  const nsIID*             iid;
};

#define HANDLER(x) NS_REINTERPRET_CAST(GenericHandler, x)

static const EventDispatchData sMouseEvents[] = {
  { NS_MOUSE_BUTTON_DOWN,        HANDLER(&nsIDOMMouseListener::MouseDown)     },
  { NS_MOUSE_BUTTON_UP,          HANDLER(&nsIDOMMouseListener::MouseUp)       },
  { NS_MOUSE_CLICK,              HANDLER(&nsIDOMMouseListener::MouseClick)    },
  { NS_MOUSE_DOUBLECLICK,        HANDLER(&nsIDOMMouseListener::MouseDblClick) },
  { NS_MOUSE_ENTER_SYNTH,        HANDLER(&nsIDOMMouseListener::MouseOver)     },
  { NS_MOUSE_EXIT_SYNTH,         HANDLER(&nsIDOMMouseListener::MouseOut)      }
};

static const EventDispatchData sMouseMotionEvents[] = {
  { NS_MOUSE_MOVE, HANDLER(&nsIDOMMouseMotionListener::MouseMove) }
};

static const EventDispatchData sContextMenuEvents[] = {
  { NS_CONTEXTMENU, HANDLER(&nsIDOMContextMenuListener::ContextMenu) }
};

static const EventDispatchData sCompositionEvents[] = {
  { NS_COMPOSITION_START,
    HANDLER(&nsIDOMCompositionListener::HandleStartComposition)  },
  { NS_COMPOSITION_END,
    HANDLER(&nsIDOMCompositionListener::HandleEndComposition)    },
  { NS_COMPOSITION_QUERY,
    HANDLER(&nsIDOMCompositionListener::HandleQueryComposition)  },
  { NS_RECONVERSION_QUERY,
    HANDLER(&nsIDOMCompositionListener::HandleQueryReconversion) },
  { NS_QUERYCARETRECT,
    HANDLER(&nsIDOMCompositionListener::HandleQueryCaretRect)    }
};

static const EventDispatchData sTextEvents[] = {
  { NS_TEXT_TEXT, HANDLER(&nsIDOMTextListener::HandleText) }
};

static const EventDispatchData sKeyEvents[] = {
  { NS_KEY_UP,    HANDLER(&nsIDOMKeyListener::KeyUp)    },
  { NS_KEY_DOWN,  HANDLER(&nsIDOMKeyListener::KeyDown)  },
  { NS_KEY_PRESS, HANDLER(&nsIDOMKeyListener::KeyPress) }
};

static const EventDispatchData sFocusEvents[] = {
  { NS_FOCUS_CONTENT, HANDLER(&nsIDOMFocusListener::Focus) },
  { NS_BLUR_CONTENT,  HANDLER(&nsIDOMFocusListener::Blur)  }
};

static const EventDispatchData sFormEvents[] = {
  { NS_FORM_SUBMIT,   HANDLER(&nsIDOMFormListener::Submit) },
  { NS_FORM_RESET,    HANDLER(&nsIDOMFormListener::Reset)  },
  { NS_FORM_CHANGE,   HANDLER(&nsIDOMFormListener::Change) },
  { NS_FORM_SELECTED, HANDLER(&nsIDOMFormListener::Select) },
  { NS_FORM_INPUT,    HANDLER(&nsIDOMFormListener::Input)  }
};

static const EventDispatchData sLoadEvents[] = {
  { NS_LOAD,               HANDLER(&nsIDOMLoadListener::Load)         },
  { NS_PAGE_UNLOAD,        HANDLER(&nsIDOMLoadListener::Unload)       },
  { NS_LOAD_ERROR,         HANDLER(&nsIDOMLoadListener::Error)        },
  { NS_BEFORE_PAGE_UNLOAD, HANDLER(&nsIDOMLoadListener::BeforeUnload) }
};

static const EventDispatchData sDragEvents[] = {
  { NS_DRAGDROP_ENTER,       HANDLER(&nsIDOMDragListener::DragEnter)   },
  { NS_DRAGDROP_OVER_SYNTH,  HANDLER(&nsIDOMDragListener::DragOver)    },
  { NS_DRAGDROP_EXIT_SYNTH,  HANDLER(&nsIDOMDragListener::DragExit)    },
  { NS_DRAGDROP_DRAGDROP,    HANDLER(&nsIDOMDragListener::DragDrop)    },
  { NS_DRAGDROP_GESTURE,     HANDLER(&nsIDOMDragListener::DragGesture) },
  { NS_DRAGDROP_DRAG,        HANDLER(&nsIDOMDragListener::Drag)        },
  { NS_DRAGDROP_END,         HANDLER(&nsIDOMDragListener::DragEnd)     },
  { NS_DRAGDROP_START,       HANDLER(&nsIDOMDragListener::DragStart)   },
  { NS_DRAGDROP_LEAVE_SYNTH, HANDLER(&nsIDOMDragListener::DragLeave)   },
  { NS_DRAGDROP_DROP,        HANDLER(&nsIDOMDragListener::Drop)        },
};

static const EventDispatchData sXULEvents[] = {
  { NS_XUL_POPUP_SHOWING,  HANDLER(&nsIDOMXULListener::PopupShowing)  },
  { NS_XUL_POPUP_SHOWN,    HANDLER(&nsIDOMXULListener::PopupShown)    },
  { NS_XUL_POPUP_HIDING,   HANDLER(&nsIDOMXULListener::PopupHiding)   },
  { NS_XUL_POPUP_HIDDEN,   HANDLER(&nsIDOMXULListener::PopupHidden)   },
  { NS_XUL_CLOSE,          HANDLER(&nsIDOMXULListener::Close)         },
  { NS_XUL_COMMAND,        HANDLER(&nsIDOMXULListener::Command)       },
  { NS_XUL_BROADCAST,      HANDLER(&nsIDOMXULListener::Broadcast)     },
  { NS_XUL_COMMAND_UPDATE, HANDLER(&nsIDOMXULListener::CommandUpdate) }
};

static const EventDispatchData sUIEvents[] = {
  { NS_UI_ACTIVATE, HANDLER(&nsIDOMUIListener::Activate) },
  { NS_UI_FOCUSIN,  HANDLER(&nsIDOMUIListener::FocusIn)  },
  { NS_UI_FOCUSOUT, HANDLER(&nsIDOMUIListener::FocusOut) }
};

#ifdef MOZ_SVG
static const EventDispatchData sSVGEvents[] = {
  { NS_SVG_LOAD,   HANDLER(&nsIDOMSVGListener::Load)   },
  { NS_SVG_UNLOAD, HANDLER(&nsIDOMSVGListener::Unload) },
  { NS_SVG_ABORT,  HANDLER(&nsIDOMSVGListener::Abort)  },
  { NS_SVG_ERROR,  HANDLER(&nsIDOMSVGListener::Error)  },
  { NS_SVG_RESIZE, HANDLER(&nsIDOMSVGListener::Resize) },
  { NS_SVG_SCROLL, HANDLER(&nsIDOMSVGListener::Scroll) }
};

static const EventDispatchData sSVGZoomEvents[] = {
  { NS_SVG_ZOOM, HANDLER(&nsIDOMSVGZoomListener::Zoom) }
};
#endif // MOZ_SVG

#define IMPL_EVENTTYPEDATA(type) \
{ \
  s##type##Events, \
  NS_ARRAY_LENGTH(s##type##Events), \
  &NS_GET_IID(nsIDOM##type##Listener) \
}
 
// IMPORTANT: indices match up with eEventArrayType_ enum values

static const EventTypeData sEventTypes[] = {
  IMPL_EVENTTYPEDATA(Mouse),
  IMPL_EVENTTYPEDATA(MouseMotion),
  IMPL_EVENTTYPEDATA(ContextMenu),
  IMPL_EVENTTYPEDATA(Key),
  IMPL_EVENTTYPEDATA(Load),
  IMPL_EVENTTYPEDATA(Focus),
  IMPL_EVENTTYPEDATA(Form),
  IMPL_EVENTTYPEDATA(Drag),
  IMPL_EVENTTYPEDATA(Text),
  IMPL_EVENTTYPEDATA(Composition),
  IMPL_EVENTTYPEDATA(XUL),
  IMPL_EVENTTYPEDATA(UI)
#ifdef MOZ_SVG
 ,
  IMPL_EVENTTYPEDATA(SVG),
  IMPL_EVENTTYPEDATA(SVGZoom)
#endif // MOZ_SVG
};

// Strong references to event groups
nsIDOMEventGroup* gSystemEventGroup = nsnull;
nsIDOMEventGroup* gDOM2EventGroup = nsnull;

nsDataHashtable<nsISupportsHashKey, PRUint32>* gEventIdTable = nsnull;

PRUint32 nsEventListenerManager::mInstanceCount = 0;

nsEventListenerManager::nsEventListenerManager() :
  mTarget(nsnull),
  mListenersRemoved(PR_FALSE),
  mListenerRemoved(PR_FALSE),
  mHandlingEvent(PR_FALSE),
  mMayHaveMutationListeners(PR_FALSE),
  mNoListenerForEvent(NS_EVENT_TYPE_NULL)
{
  ++mInstanceCount;
}

nsEventListenerManager::~nsEventListenerManager() 
{
  NS_ASSERTION(!mTarget, "didn't call Disconnect");
  RemoveAllListeners();

  --mInstanceCount;
  if(mInstanceCount == 0) {
    NS_IF_RELEASE(gSystemEventGroup);
    NS_IF_RELEASE(gDOM2EventGroup);
    delete gEventIdTable;
    gEventIdTable = nsnull;
  }
}

nsresult
nsEventListenerManager::RemoveAllListeners()
{
  mListenersRemoved = PR_TRUE;
  PRInt32 count = mListeners.Count();
  for (PRInt32 i = 0; i < count; i++) {
    delete NS_STATIC_CAST(nsListenerStruct*, mListeners.ElementAt(i));
  }
  mListeners.Clear();
  return NS_OK;
}

void
nsEventListenerManager::Shutdown()
{
  sAddListenerID = JSVAL_VOID;
  nsDOMEvent::Shutdown();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsEventListenerManager)

NS_INTERFACE_MAP_BEGIN(nsEventListenerManager)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIEventListenerManager)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
   NS_INTERFACE_MAP_ENTRY(nsIDOM3EventTarget)
   NS_INTERFACE_MAP_ENTRY(nsIDOMEventReceiver)
   NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsEventListenerManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsEventListenerManager, nsIEventListenerManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsEventListenerManager, nsIEventListenerManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsEventListenerManager)
  PRInt32 i, count = tmp->mListeners.Count();
  nsListenerStruct *ls;
  for (i = 0; i < count; i++) {
    ls = NS_STATIC_CAST(nsListenerStruct*, tmp->mListeners.ElementAt(i));
    if (ls) {
      cb.NoteXPCOMChild(ls->mListener.get());
    }
  }  
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsEventListenerManager)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END


const EventTypeData*
nsEventListenerManager::GetTypeDataForIID(const nsIID& aIID)
{
  for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sEventTypes); ++i) {
    if (aIID.Equals(*(sEventTypes[i].iid))) {
      return &sEventTypes[i];
    }
  }
  return nsnull;
}

const EventTypeData*
nsEventListenerManager::GetTypeDataForEventName(nsIAtom* aName)
{
  PRUint32 event = nsContentUtils::GetEventId(aName);
  if (event != NS_USER_DEFINED_EVENT) {
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sEventTypes); ++i) {
      for (PRInt32 j = 0; j < sEventTypes[i].numEvents; ++j) {
         if (event == sEventTypes[i].events[j].message) {
           return &sEventTypes[i];
         }
      }
    }
  }
  return nsnull;
}

nsresult
nsEventListenerManager::AddEventListener(nsIDOMEventListener *aListener,
                                         PRUint32 aType,
                                         nsIAtom* aTypeAtom,
                                         const EventTypeData* aTypeData,
                                         PRInt32 aFlags,
                                         nsIDOMEventGroup* aEvtGrp)
{
  NS_ENSURE_TRUE(aListener, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aType || aTypeData, NS_ERROR_FAILURE);

  nsRefPtr<nsIDOMEventListener> kungFuDeathGrip = aListener;

  PRBool isSame = PR_FALSE;
  PRUint16 group = 0;
  nsCOMPtr<nsIDOMEventGroup> sysGroup;
  GetSystemEventGroupLM(getter_AddRefs(sysGroup));
  if (sysGroup) {
    sysGroup->IsSameEventGroup(aEvtGrp, &isSame);
    if (isSame) {
      group = NS_EVENT_FLAG_SYSTEM_EVENT;
    }
  }

  if (!aTypeData) {
    // If we don't have type data, we can try to QI listener to the right
    // interface and set mTypeData only if QI succeeds. This way we can save
    // calls to DispatchToInterface (in HandleEvent) in those cases when QI
    // would fail.
    // @see also DispatchToInterface()
    const EventTypeData* td = GetTypeDataForEventName(aTypeAtom);
    if (td && td->iid) {
      nsIDOMEventListener* ifaceListener = nsnull;
      aListener->QueryInterface(*(td->iid), (void**) &ifaceListener);
      if (ifaceListener) {
        aTypeData = td;
        NS_RELEASE(ifaceListener);
      }
    }
  }

  nsListenerStruct* ls = nsnull;
  PRInt32 count = mListeners.Count();
  for (PRInt32 i = 0; i < count; i++) {
    ls = NS_STATIC_CAST(nsListenerStruct*, mListeners.ElementAt(i));
    if (ls->mListener == aListener && ls->mFlags == aFlags &&
        ls->mGroupFlags == group &&
        (EVENT_TYPE_EQUALS(ls, aType, aTypeAtom) ||
         EVENT_TYPE_DATA_EQUALS(aTypeData, ls->mTypeData))) {
      return NS_OK;
    }
  }

  mNoListenerForEvent = NS_EVENT_TYPE_NULL;
  mNoListenerForEventAtom = nsnull;

  ls = new nsListenerStruct();
  NS_ENSURE_TRUE(ls, NS_ERROR_OUT_OF_MEMORY);

  ls->mListener = aListener;
  ls->mEventType = aType;
  ls->mTypeAtom = aTypeAtom;
  ls->mFlags = aFlags;
  ls->mGroupFlags = group;
  ls->mHandlerIsString = PR_FALSE;
  ls->mTypeData = aTypeData;
  mListeners.AppendElement((void*)ls);

  // For mutation listeners, we need to update the global bit on the DOM window.
  // Otherwise we won't actually fire the mutation event.
  if (aType >= NS_MUTATION_START && aType <= NS_MUTATION_END) {
    mMayHaveMutationListeners = PR_TRUE;
    // Go from our target to the nearest enclosing DOM window.
    nsCOMPtr<nsPIDOMWindow> window;
    nsCOMPtr<nsIDocument> document;
    nsCOMPtr<nsINode> node(do_QueryInterface(mTarget));
    if (node) {
      // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
      // if that's the XBL document?
      document = node->GetOwnerDoc();
      if (document) {
        window = document->GetInnerWindow();
      }
    }

    if (!window) {
      window = do_QueryInterface(mTarget);
    }
    if (window) {
      NS_ASSERTION(window->IsInnerWindow(),
                   "Setting mutation listener bits on outer window?");
      // If aType is NS_MUTATION_SUBTREEMODIFIED, we need to listen all
      // mutations.
      window->SetMutationListeners((aType == NS_MUTATION_SUBTREEMODIFIED) ?
                                   kAllMutationBits :
                                   MutationBitForEventType(aType));
    }
  }

  return NS_OK;
}

nsresult
nsEventListenerManager::RemoveEventListener(nsIDOMEventListener *aListener, 
                                            PRUint32 aType,
                                            nsIAtom* aUserType,
                                            const EventTypeData* aTypeData,
                                            PRInt32 aFlags,
                                            nsIDOMEventGroup* aEvtGrp)
{
  if (!aListener || !(aType || aTypeData)) {
    return NS_OK;
  }

  PRBool isSame = PR_FALSE;
  PRUint16 group = 0;
  nsCOMPtr<nsIDOMEventGroup> sysGroup;
  GetSystemEventGroupLM(getter_AddRefs(sysGroup));
  if (sysGroup) {
    sysGroup->IsSameEventGroup(aEvtGrp, &isSame);
    if (isSame) {
      group = NS_EVENT_FLAG_SYSTEM_EVENT;
    }
  }

  nsListenerStruct* ls = nsnull;
  aFlags &= ~NS_PRIV_EVENT_UNTRUSTED_PERMITTED;

  PRInt32 count = mListeners.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    ls = NS_STATIC_CAST(nsListenerStruct*, mListeners.ElementAt(i));
    if (ls->mListener == aListener &&
        ls->mGroupFlags == group &&
        ((ls->mFlags & ~NS_PRIV_EVENT_UNTRUSTED_PERMITTED) == aFlags) &&
        (EVENT_TYPE_EQUALS(ls, aType, aUserType) ||
         (!(ls->mEventType) &&
          EVENT_TYPE_DATA_EQUALS(ls->mTypeData, aTypeData)))) {
      mListeners.RemoveElementAt(i);
      delete ls;
      mNoListenerForEvent = NS_EVENT_TYPE_NULL;
      mNoListenerForEventAtom = nsnull;
      mListenerRemoved = PR_TRUE;
      break;
    }
  }

  return NS_OK;
}

nsresult
nsEventListenerManager::AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                              const nsIID& aIID,
                                              PRInt32 aFlags)
{
  AddEventListener(aListener, NS_EVENT_TYPE_NULL, nsnull,
                   GetTypeDataForIID(aIID), aFlags, nsnull);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::RemoveEventListenerByIID(nsIDOMEventListener *aListener, 
                                                 const nsIID& aIID,
                                                 PRInt32 aFlags)
{
  RemoveEventListener(aListener, NS_EVENT_TYPE_NULL, nsnull,
                      GetTypeDataForIID(aIID), aFlags, nsnull);
  return NS_OK;
}

PRBool
nsEventListenerManager::ListenerCanHandle(nsListenerStruct* aLs,
                                          nsEvent* aEvent)
{
  if (aEvent->message == NS_USER_DEFINED_EVENT) {
    // We don't want to check aLs->mEventType here, bug 276846.
    return (aEvent->userType && aLs->mTypeAtom == aEvent->userType);
  }
  return (aLs->mEventType == aEvent->message);
}

NS_IMETHODIMP
nsEventListenerManager::AddEventListenerByType(nsIDOMEventListener *aListener, 
                                               const nsAString& aType,
                                               PRInt32 aFlags,
                                               nsIDOMEventGroup* aEvtGrp)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  PRUint32 type = nsContentUtils::GetEventId(atom);
  AddEventListener(aListener, type, atom, nsnull, aFlags, aEvtGrp);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::RemoveEventListenerByType(nsIDOMEventListener *aListener, 
                                                  const nsAString& aType,
                                                  PRInt32 aFlags,
                                                  nsIDOMEventGroup* aEvtGrp)
{
  nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + aType);
  PRUint32 type = nsContentUtils::GetEventId(atom);
  RemoveEventListener(aListener, type, atom, nsnull, aFlags, aEvtGrp);
  return NS_OK;
}

nsListenerStruct*
nsEventListenerManager::FindJSEventListener(PRUint32 aEventType,
                                            nsIAtom* aTypeAtom)
{
  // Run through the listeners for this type and see if a script
  // listener is registered
  nsListenerStruct *ls;
  PRInt32 count = mListeners.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    ls = NS_STATIC_CAST(nsListenerStruct*, mListeners.ElementAt(i));
    if (EVENT_TYPE_EQUALS(ls, aEventType, aTypeAtom) &&
        ls->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) {
      return ls;
    }
  }
  return nsnull;
}

nsresult
nsEventListenerManager::SetJSEventListener(nsIScriptContext *aContext,
                                           void *aScopeObject,
                                           nsISupports *aObject,
                                           nsIAtom* aName,
                                           PRBool aIsString,
                                           PRBool aPermitUntrustedEvents)
{
  nsresult rv = NS_OK;
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (!ls) {
    // If we didn't find a script listener or no listeners existed
    // create and add a new one.
    nsCOMPtr<nsIDOMEventListener> scriptListener;
    rv = NS_NewJSEventListener(aContext, aScopeObject, aObject,
                               getter_AddRefs(scriptListener));
    if (NS_SUCCEEDED(rv)) {
      AddEventListener(scriptListener, eventType, aName, nsnull,
                       NS_EVENT_FLAG_BUBBLE | NS_PRIV_EVENT_FLAG_SCRIPT, nsnull);

      ls = FindJSEventListener(eventType, aName);
    }
  }

  if (NS_SUCCEEDED(rv) && ls) {
    // Set flag to indicate possible need for compilation later
    ls->mHandlerIsString = aIsString;

    if (aPermitUntrustedEvents) {
      ls->mFlags |= NS_PRIV_EVENT_UNTRUSTED_PERMITTED;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsEventListenerManager::AddScriptEventListener(nsISupports *aObject,
                                               nsIAtom *aName,
                                               const nsAString& aBody,
                                               PRUint32 aLanguage,
                                               PRBool aDeferCompilation,
                                               PRBool aPermitUntrustedEvents)
{
  NS_PRECONDITION(aLanguage != nsIProgrammingLanguage::UNKNOWN,
                  "Must know the language for the script event listener");
  nsIScriptContext *context = nsnull;

  // |aPermitUntrustedEvents| is set to False for chrome - events
  // *generated* from an unknown source are not allowed.
  // However, for script languages with no 'sandbox', we want to reject
  // such scripts based on the source of their code, not just the source
  // of the event.
  if (aPermitUntrustedEvents && 
      aLanguage != nsIProgrammingLanguage::JAVASCRIPT) {
    NS_WARNING("Discarding non-JS event listener from untrusted source");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsINode> node(do_QueryInterface(aObject));

  nsCOMPtr<nsIDocument> doc;

  nsISupports *objiSupp = aObject;
  nsCOMPtr<nsIScriptGlobalObject> global;

  if (node) {
    // Try to get context from doc
    // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
    // if that's the XBL document?
    doc = node->GetOwnerDoc();
    if (doc)
      global = doc->GetScriptGlobalObject();
  } else {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aObject));
    if (win) {
      NS_ASSERTION(win->IsInnerWindow(),
                   "Event listener added to outer window!");

      nsCOMPtr<nsIDOMDocument> domdoc;
      win->GetDocument(getter_AddRefs(domdoc));
      doc = do_QueryInterface(domdoc);
      global = do_QueryInterface(win);
    } else {
      global = do_QueryInterface(aObject);
    }
  }

  if (global) {
    // This might be the first reference to this language in the global
    // We must init the language before we attempt to fetch its context.
    if (NS_FAILED(global->EnsureScriptEnvironment(aLanguage))) {
      NS_WARNING("Failed to setup script environment for this language");
      // but fall through and let the inevitable failure below handle it.
    }

    context = global->GetScriptContext(aLanguage);
  }
  NS_ENSURE_TRUE(context, NS_ERROR_FAILURE);

  NS_ASSERTION(global, "How could we possibly have a context without an "
               "nsIScriptGlobalObject?");

  void *scope = global->GetScriptGlobal(aLanguage);
  nsresult rv;

  if (!aDeferCompilation) {
    nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
      do_QueryInterface(aObject);

    nsScriptObjectHolder handler(context);
    PRBool done = PR_FALSE;

    if (handlerOwner) {
      rv = handlerOwner->GetCompiledEventHandler(aName, handler);
      if (NS_SUCCEEDED(rv) && handler) {
        rv = context->BindCompiledEventHandler(aObject, scope, aName, handler);
        if (NS_FAILED(rv))
          return rv;
        done = PR_TRUE;
      }
    }

    if (!done) {
      PRUint32 lineNo = 0;
      nsCAutoString url (NS_LITERAL_CSTRING("-moz-evil:lying-event-listener"));
      if (doc) {
        nsIURI *uri = doc->GetDocumentURI();
        if (uri) {
          uri->GetSpec(url);
          lineNo = 1;
        }
      }

      if (handlerOwner) {
        // Always let the handler owner compile the event handler, as
        // it may want to use a special context or scope object.
        rv = handlerOwner->CompileEventHandler(context, aObject, aName,
                                               aBody, url.get(), lineNo, handler);
      }
      else {
        PRInt32 nameSpace = kNameSpaceID_Unknown;
        if (node && node->IsNodeOfType(nsINode::eCONTENT)) {
          nsIContent* content = NS_STATIC_CAST(nsIContent*, node.get());
          nameSpace = content->GetNameSpaceID();
        }
        else if (doc) {
          nsCOMPtr<nsIContent> root = doc->GetRootContent();
          if (root)
            nameSpace = root->GetNameSpaceID();
        }
        PRUint32 argCount;
        const char **argNames;
        nsContentUtils::GetEventArgNames(nameSpace, aName, &argCount,
                                         &argNames);

        rv = context->CompileEventHandler(aName, argCount, argNames,
                                          aBody,
                                          url.get(), lineNo,
                                          handler);
        NS_ENSURE_SUCCESS(rv, rv);
        // And bind it.
        rv = context->BindCompiledEventHandler(aObject, scope,
                                               aName, handler);
      }
      if (NS_FAILED(rv)) return rv;
    }
  }

  return SetJSEventListener(context, scope, objiSupp, aName, aDeferCompilation,
                            aPermitUntrustedEvents);
}

nsresult
nsEventListenerManager::RemoveScriptEventListener(nsIAtom* aName)
{
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (ls) {
    mListeners.RemoveElement((void*)ls);
    delete ls;
    mNoListenerForEvent = NS_EVENT_TYPE_NULL;
    mNoListenerForEventAtom = nsnull;
    mListenerRemoved = PR_TRUE;
  }

  return NS_OK;
}

jsval
nsEventListenerManager::sAddListenerID = JSVAL_VOID;

NS_IMETHODIMP
nsEventListenerManager::RegisterScriptEventListener(nsIScriptContext *aContext,
                                                    void *aScope,
                                                    nsISupports *aObject, 
                                                    nsIAtom *aName)
{
  // Check that we have access to set an event listener. Prevents
  // snooping attacks across domains by setting onkeypress handlers,
  // for instance.
  // You'd think it'd work just to get the JSContext from aContext,
  // but that's actually the JSContext whose private object parents
  // the object in aObject.
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  JSContext *cx;
  if (NS_FAILED(rv = stack->Peek(&cx)))
    return rv;

  if (cx) {
    if (sAddListenerID == JSVAL_VOID) {
      JSAutoRequest ar(cx);
      sAddListenerID =
        STRING_TO_JSVAL(::JS_InternString(cx, "addEventListener"));
    }

    if (aContext->GetScriptTypeID() == nsIProgrammingLanguage::JAVASCRIPT) {
        nsCOMPtr<nsIXPConnectJSObjectHolder> holder;
        rv = nsContentUtils::XPConnect()->
          WrapNative(cx, (JSObject *)aScope, aObject, NS_GET_IID(nsISupports),
                     getter_AddRefs(holder));
        NS_ENSURE_SUCCESS(rv, rv);
        JSObject *jsobj = nsnull;
      
        rv = holder->GetJSObject(&jsobj);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = nsContentUtils::GetSecurityManager()->
          CheckPropertyAccess(cx, jsobj,
                              "EventTarget",
                              sAddListenerID,
                              nsIXPCSecurityManager::ACCESS_SET_PROPERTY);
        if (NS_FAILED(rv)) {
          // XXX set pending exception on the native call context?
          return rv;
        }
    } else {
        NS_WARNING("Skipping CheckPropertyAccess for non JS language");
    }
        
  }

  // Untrusted events are always permitted for non-chrome script
  // handlers.
  return SetJSEventListener(aContext, aScope, aObject, aName,
                            PR_FALSE, !nsContentUtils::IsCallerChrome());
}

nsresult
nsEventListenerManager::CompileScriptEventListener(nsIScriptContext *aContext, 
                                                   void *aScope,
                                                   nsISupports *aObject, 
                                                   nsIAtom *aName,
                                                   PRBool *aDidCompile)
{
  nsresult rv = NS_OK;
  *aDidCompile = PR_FALSE;
  PRUint32 eventType = nsContentUtils::GetEventId(aName);
  nsListenerStruct* ls = FindJSEventListener(eventType, aName);

  if (!ls) {
    //nothing to compile
    return NS_OK;
  }

  if (ls->mHandlerIsString) {
    rv = CompileEventHandlerInternal(aContext, aScope, aObject, aName,
                                     ls, /*XXX fixme*/nsnull);
  }

  // Set *aDidCompile to true even if we didn't really compile
  // anything right now, if we get here it means that this event
  // handler has been compiled at some point, that's good enough for
  // us.

  *aDidCompile = PR_TRUE;

  return rv;
}

nsresult
nsEventListenerManager::CompileEventHandlerInternal(nsIScriptContext *aContext,
                                                    void *aScope,
                                                    nsISupports *aObject,
                                                    nsIAtom *aName,
                                                    nsListenerStruct *aListenerStruct,
                                                    nsISupports* aCurrentTarget)
{
  nsresult result = NS_OK;

  nsCOMPtr<nsIScriptEventHandlerOwner> handlerOwner =
    do_QueryInterface(aObject);
  nsScriptObjectHolder handler(aContext);

  if (handlerOwner) {
    result = handlerOwner->GetCompiledEventHandler(aName,
                                                   handler);
    if (NS_SUCCEEDED(result) && handler) {
      // XXXmarkh - why do we bind here, but not after compilation below?
      result = aContext->BindCompiledEventHandler(aObject, aScope, aName, handler);
      aListenerStruct->mHandlerIsString = PR_FALSE;
    }
  }

  if (aListenerStruct->mHandlerIsString) {
    // This should never happen for anything but content
    // XXX I don't like that we have to reference content
    // from here. The alternative is to store the event handler
    // string on the JS object itself.
    nsCOMPtr<nsIContent> content = do_QueryInterface(aObject);
    NS_ASSERTION(content, "only content should have event handler attributes");
    if (content) {
      nsAutoString handlerBody;
      nsIAtom* attrName = aName;
#ifdef MOZ_SVG
      if (aName == nsGkAtoms::onSVGLoad)
        attrName = nsGkAtoms::onload;
      else if (aName == nsGkAtoms::onSVGUnload)
        attrName = nsGkAtoms::onunload;
      else if (aName == nsGkAtoms::onSVGAbort)
        attrName = nsGkAtoms::onabort;
      else if (aName == nsGkAtoms::onSVGError)
        attrName = nsGkAtoms::onerror;
      else if (aName == nsGkAtoms::onSVGResize)
        attrName = nsGkAtoms::onresize;
      else if (aName == nsGkAtoms::onSVGScroll)
        attrName = nsGkAtoms::onscroll;
      else if (aName == nsGkAtoms::onSVGZoom)
        attrName = nsGkAtoms::onzoom;
#endif // MOZ_SVG

      content->GetAttr(kNameSpaceID_None, attrName, handlerBody);

      PRUint32 lineNo = 0;
      nsCAutoString url (NS_LITERAL_CSTRING("javascript:alert('TODO: FIXME')"));
      nsIDocument* doc = nsnull;
      nsCOMPtr<nsINode> node = do_QueryInterface(aCurrentTarget);
      if (node) {
        doc = node->GetOwnerDoc();
      }
      if (doc) {
        nsIURI *uri = doc->GetDocumentURI();
        if (uri) {
          uri->GetSpec(url);
          lineNo = 1;
        }
      }

      if (handlerOwner) {
        // Always let the handler owner compile the event
        // handler, as it may want to use a special
        // context or scope object.
        result = handlerOwner->CompileEventHandler(aContext, aObject, aName,
                                                   handlerBody,
                                                   url.get(), lineNo,
                                                   handler);
      }
      else {
        PRUint32 argCount;
        const char **argNames;
        nsContentUtils::GetEventArgNames(content->GetNameSpaceID(), aName,
                                         &argCount, &argNames);

        result = aContext->CompileEventHandler(aName,
                                               argCount, argNames,
                                               handlerBody,
                                               url.get(), lineNo,
                                               handler);
        NS_ENSURE_SUCCESS(result, result);
        // And bind it.
        result = aContext->BindCompiledEventHandler(aObject, aScope,
                                                    aName, handler);
        NS_ENSURE_SUCCESS(result, result);
      }

      if (NS_SUCCEEDED(result)) {
        aListenerStruct->mHandlerIsString = PR_FALSE;
      }
    }
  }

  return result;
}

nsresult
nsEventListenerManager::HandleEventSubType(nsListenerStruct* aListenerStruct,
                                           nsIDOMEventListener* aListener,
                                           nsIDOMEvent* aDOMEvent,
                                           nsISupports* aCurrentTarget,
                                           PRUint32 aPhaseFlags)
{
  nsresult result = NS_OK;

  // If this is a script handler and we haven't yet
  // compiled the event handler itself
  if ((aListenerStruct->mFlags & NS_PRIV_EVENT_FLAG_SCRIPT) &&
      aListenerStruct->mHandlerIsString) {
    nsCOMPtr<nsIJSEventListener> jslistener = do_QueryInterface(aListener);
    if (jslistener) {
      nsAutoString eventString;
      if (NS_SUCCEEDED(aDOMEvent->GetType(eventString))) {
        nsCOMPtr<nsIAtom> atom = do_GetAtom(NS_LITERAL_STRING("on") + eventString);

        result = CompileEventHandlerInternal(jslistener->GetEventContext(),
                                             jslistener->GetEventScope(),
                                             jslistener->GetEventTarget(),
                                             atom, aListenerStruct,
                                             aCurrentTarget);
      }
    }
  }

  // nsCxPusher will automatically push and pop the current cx onto the
  // context stack
  nsCxPusher pusher(aCurrentTarget);

  if (NS_SUCCEEDED(result)) {
    // nsIDOMEvent::currentTarget is set in nsEventDispatcher.
    result = aListener->HandleEvent(aDOMEvent);
  }

  return result;
}

static PRUint32                 sLatestEventType = 0;
static const EventTypeData*     sLatestEventTypeData = nsnull;
static const EventDispatchData* sLatestEventDispData = nsnull;

/**
* Causes a check for event listeners and processing by them if they exist.
* @param an event listener
*/

nsresult
nsEventListenerManager::HandleEvent(nsPresContext* aPresContext,
                                    nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                    nsISupports* aCurrentTarget,
                                    PRUint32 aFlags,
                                    nsEventStatus* aEventStatus)
{
  if (mListeners.Count() <= 0 || aEvent->flags & NS_EVENT_FLAG_STOP_DISPATCH) {
    return NS_OK;
  }

  // Check if we already know that there is no event listener for the event.
  if (mNoListenerForEvent == aEvent->message &&
      (mNoListenerForEvent != NS_USER_DEFINED_EVENT ||
       mNoListenerForEventAtom == aEvent->userType)) {
    return NS_OK;
  }

  //Set the value of the internal PreventDefault flag properly based on aEventStatus
  if (*aEventStatus == nsEventStatus_eConsumeNoDefault) {
    aEvent->flags |= NS_EVENT_FLAG_NO_DEFAULT;
  }
  PRUint16 currentGroup = aFlags & NS_EVENT_FLAG_SYSTEM_EVENT;

  if (aEvent->message == NS_CONTEXTMENU &&
      NS_FAILED(FixContextMenuEvent(aPresContext, aCurrentTarget, aEvent,
                                    aDOMEvent))) {
    NS_WARNING("failed to fix context menu event target");
  }

  const EventTypeData* typeData = nsnull;
  const EventDispatchData* dispData = nsnull;
  if (aEvent->message != NS_USER_DEFINED_EVENT) {
    // Check if this is the same type of event as what a listener manager
    // handled last time.
    if (aEvent->message == sLatestEventType) {
      typeData = sLatestEventTypeData;
      dispData = sLatestEventDispData;
      goto found;
    }
    for (PRUint32 i = 0; i < NS_ARRAY_LENGTH(sEventTypes); ++i) {
     typeData = &sEventTypes[i];
     for (PRInt32 j = 0; j < typeData->numEvents; ++j) {
       dispData = &(typeData->events[j]);
       if (aEvent->message == dispData->message) {
         sLatestEventType = aEvent->message;
         sLatestEventTypeData = typeData;
         sLatestEventDispData = dispData;
         goto found;
       }
     }
     typeData = nsnull;
     dispData = nsnull;
    }
  }

found:

  PRBool topMostHandleEvent = !mHandlingEvent;
  if (topMostHandleEvent) {
    mHandlingEvent = PR_TRUE;
    mListenerRemoved = PR_FALSE;
  }

  PRInt32 count = mListeners.Count();
  nsVoidArray originalListeners(count);
  originalListeners = mListeners;
  nsAutoPopupStatePusher popupStatePusher(nsDOMEvent::GetEventPopupControlState(aEvent));
  PRBool hasListener = PR_FALSE;
  for (PRInt32 k = 0; !mListenersRemoved && k < count; ++k) {
    nsListenerStruct* ls =
      NS_STATIC_CAST(nsListenerStruct*, originalListeners.FastElementAt(k));
    if (!ls || (mListenerRemoved && mListeners.IndexOf(ls) == -1)) {
      continue;
    }
    PRBool useTypeInterface =
      EVENT_TYPE_DATA_EQUALS(ls->mTypeData, typeData);
    PRBool useGenericInterface =
      (!useTypeInterface && ListenerCanHandle(ls, aEvent));
    // Don't fire the listener if it's been removed.
    // Check that the phase is same in event and event listener.
    // Handle only trusted events, except when listener permits untrusted events.
    if (useTypeInterface || useGenericInterface) {
      if (ls->mListener) {
        hasListener = PR_TRUE;
        if (ls->mFlags & aFlags &&
            ls->mGroupFlags == currentGroup &&
            (NS_IS_TRUSTED_EVENT(aEvent) ||
             ls->mFlags & NS_PRIV_EVENT_UNTRUSTED_PERMITTED)) {
          if (!*aDOMEvent) {
            nsEventDispatcher::CreateEvent(aPresContext, aEvent,
                                           EmptyString(), aDOMEvent);
          }
          if (*aDOMEvent) {
            if (useTypeInterface) {
              DispatchToInterface(*aDOMEvent, ls->mListener,
                                  dispData->method, *typeData->iid);
            } else if (useGenericInterface) {
              HandleEventSubType(ls, ls->mListener, *aDOMEvent,
                                 aCurrentTarget, aFlags);
            }
          }
        }
      }
    }
  }

  if (!hasListener) {
    mNoListenerForEvent = aEvent->message;
    mNoListenerForEventAtom = aEvent->userType;
  }

  if (aEvent->flags & NS_EVENT_FLAG_NO_DEFAULT) {
    *aEventStatus = nsEventStatus_eConsumeNoDefault;
  }

  if (topMostHandleEvent) {
    mHandlingEvent = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::Disconnect()
{
  mTarget = nsnull;
  return RemoveAllListeners();
}

NS_IMETHODIMP
nsEventListenerManager::SetListenerTarget(nsISupports* aTarget)
{
  NS_PRECONDITION(aTarget, "unexpected null pointer");

  //WEAK reference, must be set back to nsnull when done by calling Disconnect
  mTarget = aTarget;
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::GetSystemEventGroupLM(nsIDOMEventGroup **aGroup)
{
  if (!gSystemEventGroup) {
    nsresult result;
    nsCOMPtr<nsIDOMEventGroup> group(do_CreateInstance(kDOMEventGroupCID,&result));
    if (NS_FAILED(result))
      return result;

    gSystemEventGroup = group;
    NS_ADDREF(gSystemEventGroup);
  }

  *aGroup = gSystemEventGroup;
  NS_ADDREF(*aGroup);
  return NS_OK;
}

nsresult
nsEventListenerManager::GetDOM2EventGroup(nsIDOMEventGroup **aGroup)
{
  if (!gDOM2EventGroup) {
    nsresult result;
    nsCOMPtr<nsIDOMEventGroup> group(do_CreateInstance(kDOMEventGroupCID,&result));
    if (NS_FAILED(result))
      return result;

    gDOM2EventGroup = group;
    NS_ADDREF(gDOM2EventGroup);
  }

  *aGroup = gDOM2EventGroup;
  NS_ADDREF(*aGroup);
  return NS_OK;
}

// nsIDOMEventTarget interface
NS_IMETHODIMP 
nsEventListenerManager::AddEventListener(const nsAString& aType, 
                                         nsIDOMEventListener* aListener, 
                                         PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  return AddEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveEventListener(const nsAString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  return RemoveEventListenerByType(aListener, aType, flags, nsnull);
}

NS_IMETHODIMP
nsEventListenerManager::DispatchEvent(nsIDOMEvent* aEvent, PRBool *_retval)
{
  nsCOMPtr<nsIContent> targetContent(do_QueryInterface(mTarget));
  if (!targetContent) {
    // nothing to dispatch on -- bad!
    return NS_ERROR_FAILURE;
  }
  
  // XXX sXBL/XBL2 issue -- do we really want the owner here?  What
  // if that's the XBL document?  Would we want its presshell?  Or what?
  nsCOMPtr<nsIDocument> document = targetContent->GetOwnerDoc();

  // Do nothing if the element does not belong to a document
  if (!document) {
    return NS_OK;
  }

  // Obtain a presentation shell
  nsIPresShell *shell = document->GetShellAt(0);
  nsCOMPtr<nsPresContext> context;
  if (shell) {
    context = shell->GetPresContext();
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(targetContent, nsnull, aEvent,
                                        context, &status);
  *_retval = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

// nsIDOM3EventTarget interface
NS_IMETHODIMP 
nsEventListenerManager::AddGroupedEventListener(const nsAString& aType, 
                                                nsIDOMEventListener* aListener, 
                                                PRBool aUseCapture,
                                                nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;

  return AddEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveGroupedEventListener(const nsAString& aType, 
                                            nsIDOMEventListener* aListener, 
                                            PRBool aUseCapture,
                                            nsIDOMEventGroup* aEvtGrp)
{
  PRInt32 flags = aUseCapture ? NS_EVENT_FLAG_CAPTURE : NS_EVENT_FLAG_BUBBLE;
  
  return RemoveEventListenerByType(aListener, aType, flags, aEvtGrp);
}

NS_IMETHODIMP
nsEventListenerManager::CanTrigger(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsEventListenerManager::IsRegisteredHere(const nsAString & type, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIDOMEventReceiver interface
NS_IMETHODIMP 
nsEventListenerManager::AddEventListenerByIID(nsIDOMEventListener *aListener, 
                                              const nsIID& aIID)
{
  return AddEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

NS_IMETHODIMP 
nsEventListenerManager::RemoveEventListenerByIID(nsIDOMEventListener *aListener, const nsIID& aIID)
{
  return RemoveEventListenerByIID(aListener, aIID, NS_EVENT_FLAG_BUBBLE);
}

NS_IMETHODIMP 
nsEventListenerManager::GetListenerManager(PRBool aCreateIfNotFound,
                                           nsIEventListenerManager** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ADDREF(*aResult = this);
  return NS_OK;
}

NS_IMETHODIMP
nsEventListenerManager::GetSystemEventGroup(nsIDOMEventGroup **aGroup)
{
  return GetSystemEventGroupLM(aGroup);
}

nsresult
nsEventListenerManager::FixContextMenuEvent(nsPresContext* aPresContext,
                                            nsISupports* aCurrentTarget,
                                            nsEvent* aEvent,
                                            nsIDOMEvent** aDOMEvent)
{
  nsIPresShell* shell = aPresContext ? aPresContext->GetPresShell() : nsnull;
  if (!shell) {
    // Nothing to do.
    return NS_OK;
  }

  nsresult ret = NS_OK;

  PRBool contextMenuKey =
    NS_STATIC_CAST(nsMouseEvent*, aEvent)->context == nsMouseEvent::eContextMenuKey;
  if (nsnull == *aDOMEvent) {
    // If we're here because of the key-equiv for showing context menus, we
    // have to twiddle with the NS event to make sure the context menu comes
    // up in the upper left of the relevant content area before we create
    // the DOM event. Since we never call InitMouseEvent() on the event, 
    // the client X/Y will be 0,0. We can make use of that if the widget is null.
    if (contextMenuKey) {
      NS_IF_RELEASE(((nsGUIEvent*)aEvent)->widget);
      aPresContext->GetViewManager()->GetWidget(&((nsGUIEvent*)aEvent)->widget);
      aEvent->refPoint.x = 0;
      aEvent->refPoint.y = 0;
    }
    ret = NS_NewDOMMouseEvent(aDOMEvent, aPresContext, NS_STATIC_CAST(nsInputEvent*, aEvent));
    NS_ENSURE_SUCCESS(ret, ret);
  }

  // see if we should use the caret position for the popup
  if (contextMenuKey) {
    nsPoint caretPoint;
    if (PrepareToUseCaretPosition(((nsGUIEvent*)aEvent)->widget,
                                  shell, caretPoint)) {
      // caret position is good
      aEvent->refPoint.x = caretPoint.x;
      aEvent->refPoint.y = caretPoint.y;
      return NS_OK;
    }
  }

  // If we're here because of the key-equiv for showing context menus, we
  // have to reset the event target to the currently focused element. Get it
  // from the focus controller.
  nsCOMPtr<nsIDOMEventTarget> currentTarget = do_QueryInterface(aCurrentTarget);
  nsCOMPtr<nsIDOMElement> currentFocus;

  if (contextMenuKey) {
    nsIDocument *doc = shell->GetDocument();
    if (doc) {
      nsPIDOMWindow* privWindow = doc->GetWindow();
      if (privWindow) {
        nsIFocusController *focusController =
          privWindow->GetRootFocusController();
        if (focusController)
          focusController->GetFocusedElement(getter_AddRefs(currentFocus));
      }
    }
  }

  if (currentFocus) {
    // Reset event coordinates relative to focused frame in view
    nsPoint targetPt;
    GetCoordinatesFor(currentFocus, aPresContext, shell, targetPt);
    aEvent->refPoint.x = targetPt.x;
    aEvent->refPoint.y = targetPt.y;

    currentTarget = do_QueryInterface(currentFocus);
    nsCOMPtr<nsIPrivateDOMEvent> pEvent(do_QueryInterface(*aDOMEvent));
    pEvent->SetTarget(currentTarget);
  }

  return ret;
}

// nsEventListenerManager::PrepareToUseCaretPosition
//
//    This checks to see if we should use the caret position for popup context
//    menus. Returns true if the caret position should be used, and the
//    coordinates of that position is returned in aTargetPt. This function
//    will also scroll the window as needed to make the caret visible.
//
//    The event widget should be the widget that generated the event, and
//    whose coordinate system the resulting event's refPoint should be
//    relative to.  The returned point is in device pixels realtive to the
//    widget passed in.

PRBool
nsEventListenerManager::PrepareToUseCaretPosition(nsIWidget* aEventWidget,
                                                  nsIPresShell* aShell,
                                                  nsPoint& aTargetPt)
{
  nsresult rv;

  // check caret visibility
  nsCOMPtr<nsICaret> caret;
  rv = aShell->GetCaret(getter_AddRefs(caret));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(caret, PR_FALSE);

  PRBool caretVisible = PR_FALSE;
  rv = caret->GetCaretVisible(&caretVisible);
  if (NS_FAILED(rv) || ! caretVisible)
    return PR_FALSE;

  // caret selection, watch out: GetCaretDOMSelection can return null but NS_OK
  nsCOMPtr<nsISelection> domSelection;
  rv = caret->GetCaretDOMSelection(getter_AddRefs(domSelection));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(domSelection, PR_FALSE);

  // since the match could be an anonymous textnode inside a
  // <textarea> or text <input>, we need to get the outer frame
  // note: frames are not refcounted
  nsIFrame* frame = nsnull; // may be NULL
  nsCOMPtr<nsIDOMNode> node;
  rv = domSelection->GetFocusNode(getter_AddRefs(node));
  NS_ENSURE_SUCCESS(rv, PR_FALSE);
  NS_ENSURE_TRUE(node, PR_FALSE);
  nsCOMPtr<nsIContent> content(do_QueryInterface(node));
  for ( ; content; content = content->GetParent()) {
    if (!content->IsNativeAnonymous()) {
      // It seems like selCon->ScrollSelectionIntoView should be enough, but it's
      // not. The problem is that scrolling the selection into view when it is
      // below the current viewport will align the top line of the frame exactly
      // with the bottom of the window. This is fine, BUT, the popup event causes
      // the control to be re-focused which does this exact call to
      // ScrollContentIntoView, which has a one-pixel disagreement of whether the
      // frame is actually in view. The result is that the frame is aligned with
      // the top of the window, but the menu is still at the bottom.
      //
      // Doing this call first forces the frame to be in view, eliminating the
      // problem. The only difference in the result is that if your cursor is in
      // an edit box below the current view, you'll get the edit box aligned with
      // the top of the window. This is arguably better behavior anyway.
      rv = aShell->ScrollContentIntoView(content,
                                         NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE,
                                         NS_PRESSHELL_SCROLL_IF_NOT_VISIBLE);
      NS_ENSURE_SUCCESS(rv, PR_FALSE);
      frame = aShell->GetPrimaryFrameFor(content);
      NS_ASSERTION(frame, "No frame for focused content?");
      break;
    }
  }

  // Actually scroll the selection (ie caret) into view. Note that this must
  // be synchronous since we will be checking the caret position on the screen.
  //
  // Be easy about errors, and just don't scroll in those cases. Better to have
  // the correct menu at a weird place than the wrong menu.
  nsCOMPtr<nsISelectionController> selCon;
  if (frame)
    frame->GetSelectionController(aShell->GetPresContext(),
                                  getter_AddRefs(selCon));
  else
    selCon = do_QueryInterface(aShell);
  if (selCon) {
    rv = selCon->ScrollSelectionIntoView(nsISelectionController::SELECTION_NORMAL,
        nsISelectionController::SELECTION_FOCUS_REGION, PR_TRUE);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);
  }

  // get caret position relative to some view (normally the same as the
  // event widget view, but this is not guaranteed)
  PRBool isCollapsed;
  nsIView* view;
  nsRect caretCoords;
  rv = caret->GetCaretCoordinates(nsICaret::eRenderingViewCoordinates,
                                  domSelection, &caretCoords, &isCollapsed,
                                  &view);
  NS_ENSURE_SUCCESS(rv, PR_FALSE);

  // in case the view used for caret coordinates was something else, we need
  // to bring those coordinates into the space of the widget view
  nsIView* widgetView = nsIView::GetViewFor(aEventWidget);
  NS_ENSURE_TRUE(widgetView, PR_FALSE);
  nsPoint viewToWidget;
  widgetView->GetNearestWidget(&viewToWidget);
  nsPoint viewDelta = view->GetOffsetTo(widgetView) + viewToWidget;

  // caret coordinates are in twips, convert to pixels
  nsPresContext* presContext = aShell->GetPresContext();
  aTargetPt.x = presContext->AppUnitsToDevPixels(viewDelta.x + caretCoords.x + caretCoords.width);
  aTargetPt.y = presContext->AppUnitsToDevPixels(viewDelta.y + caretCoords.y + caretCoords.height);

  return PR_TRUE;
}

// Get coordinates in device pixels relative to root view for element, 
// first ensuring the element is onscreen
void
nsEventListenerManager::GetCoordinatesFor(nsIDOMElement *aCurrentEl, 
                                          nsPresContext *aPresContext,
                                          nsIPresShell *aPresShell, 
                                          nsPoint& aTargetPt)
{
  nsCOMPtr<nsIContent> focusedContent(do_QueryInterface(aCurrentEl));
  aPresShell->ScrollContentIntoView(focusedContent,
                                    NS_PRESSHELL_SCROLL_ANYWHERE,
                                    NS_PRESSHELL_SCROLL_ANYWHERE);
  nsIFrame *frame = aPresShell->GetPrimaryFrameFor(focusedContent);
  if (frame) {
    nsPoint frameOrigin(0, 0);

    // Get the frame's origin within its view
    nsIView *view = frame->GetClosestView(&frameOrigin);
    NS_ASSERTION(view, "No view for frame");

    nsIViewManager* vm = aPresShell->GetViewManager();
    nsIView *rootView = nsnull;
    vm->GetRootView(rootView);
    NS_ASSERTION(rootView, "No root view in pres shell");

    // View's origin within its root view
    frameOrigin += view->GetOffsetTo(rootView);

    // Start context menu down and to the right from top left of frame
    // use the lineheight. This is a good distance to move the context
    // menu away from the top left corner of the frame. If we always 
    // used the frame height, the context menu could end up far away,
    // for example when we're focused on linked images.
    // On the other hand, we want to use the frame height if it's less
    // than the current line height, so that the context menu appears
    // associated with the correct frame.
    nscoord extra = frame->GetSize().height;
    nsIScrollableView *scrollView =
      nsLayoutUtils::GetNearestScrollingView(view, nsLayoutUtils::eEither);
    if (scrollView) {
      nscoord scrollViewLineHeight;
      scrollView->GetLineHeight(&scrollViewLineHeight);
      if (extra > scrollViewLineHeight) {
        extra = scrollViewLineHeight; 
      }
    }

    PRInt32 extraPixelsY = 0;
#ifdef MOZ_XUL
    // Tree view special case (tree items have no frames)
    // Get the focused row and add its coordinates, which are already in pixels
    // XXX Boris, should we create a new interface so that event listener manager doesn't
    // need to know about trees? Something like nsINodelessChildCreator which
    // could provide the current focus coordinates?
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(aCurrentEl));
    if (xulElement) {
      nsCOMPtr<nsIBoxObject> box;
      xulElement->GetBoxObject(getter_AddRefs(box));
      nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
      if (treeBox) {
        // Factor in focused row
        nsCOMPtr<nsIDOMXULMultiSelectControlElement> multiSelect =
          do_QueryInterface(aCurrentEl);
        NS_ASSERTION(multiSelect, "No multi select interface for tree");

        PRInt32 currentIndex;
        multiSelect->GetCurrentIndex(&currentIndex);
        if (currentIndex >= 0) {
          treeBox->EnsureRowIsVisible(currentIndex);
          PRInt32 firstVisibleRow, rowHeight;
          treeBox->GetFirstVisibleRow(&firstVisibleRow);
          treeBox->GetRowHeight(&rowHeight);
          extraPixelsY = (currentIndex - firstVisibleRow + 1) * rowHeight;
          extra = 0;

          nsCOMPtr<nsITreeColumns> cols;
          treeBox->GetColumns(getter_AddRefs(cols));
          if (cols) {
            nsCOMPtr<nsITreeColumn> col;
            cols->GetFirstColumn(getter_AddRefs(col));
            if (col) {
              nsCOMPtr<nsIDOMElement> colElement;
              col->GetElement(getter_AddRefs(colElement));
              nsCOMPtr<nsIContent> colContent(do_QueryInterface(colElement));
              if (colContent) {
                frame = aPresShell->GetPrimaryFrameFor(colContent);
                if (frame) {
                  frameOrigin.y += frame->GetSize().height;
                }
              }
            }
          }
        }
      }
    }
#endif

    aTargetPt.x = aPresContext->AppUnitsToDevPixels(frameOrigin.x + extra);
    aTargetPt.y = aPresContext->AppUnitsToDevPixels(frameOrigin.y + extra) + extraPixelsY;
  }
}

NS_IMETHODIMP
nsEventListenerManager::HasMutationListeners(PRBool* aListener)
{
  *aListener = PR_FALSE;
  if (mMayHaveMutationListeners) {
    PRInt32 count = mListeners.Count();
    for (PRInt32 i = 0; i < count; ++i) {
      nsListenerStruct* ls = NS_STATIC_CAST(nsListenerStruct*,
                                            mListeners.FastElementAt(i));
      if (ls &&
          ls->mEventType >= NS_MUTATION_START &&
          ls->mEventType <= NS_MUTATION_END) {
        *aListener = PR_TRUE;
        break;
      }
    }
  }

  return NS_OK;
}

PRUint32
nsEventListenerManager::MutationListenerBits()
{
  PRUint32 bits = 0;
  if (mMayHaveMutationListeners) {
    PRInt32 i, count = mListeners.Count();
    for (i = 0; i < count; ++i) {
      nsListenerStruct* ls = NS_STATIC_CAST(nsListenerStruct*,
                                            mListeners.FastElementAt(i));
      if (ls &&
          (ls->mEventType >= NS_MUTATION_START &&
           ls->mEventType <= NS_MUTATION_END)) {
        if (ls->mEventType == NS_MUTATION_SUBTREEMODIFIED) {
          return kAllMutationBits;
        }
        bits |= MutationBitForEventType(ls->mEventType);
      }
    }
  }
  return bits;
}

PRBool
nsEventListenerManager::HasUnloadListeners()
{
  PRInt32 count = mListeners.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    nsListenerStruct* ls = NS_STATIC_CAST(nsListenerStruct*,
                                          mListeners.FastElementAt(i));
    if (ls &&
        (ls->mEventType == NS_PAGE_UNLOAD ||
         ls->mEventType == NS_BEFORE_PAGE_UNLOAD) ||
        (ls->mTypeData && ls->mTypeData->iid &&
         ls->mTypeData->iid->Equals(NS_GET_IID(nsIDOMLoadListener)))) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

nsresult
NS_NewEventListenerManager(nsIEventListenerManager** aInstancePtrResult) 
{
  nsIEventListenerManager* l = new nsEventListenerManager();

  if (!l) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  return CallQueryInterface(l, aInstancePtrResult);
}
