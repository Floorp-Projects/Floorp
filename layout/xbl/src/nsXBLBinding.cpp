/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIXBLBinding.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIDOMEventReceiver.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIXMLContent.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMText.h"
#include "nsSupportsArray.h"
#include "nsINameSpace.h"
#include "nsJSUtils.h"

// Event listeners
#include "nsIEventListenerManager.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMLoadListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMPaintListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMFormListener.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMDragListener.h"

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"

#include "nsXBLEventHandler.h"
#include "nsIXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
static char kNameSpaceSeparator = ':';

// Helper classes
// {A2892B81-CED9-11d3-97FB-00400553EEF0}
#define NS_IXBLATTR_IID \
{ 0xa2892b81, 0xced9, 0x11d3, { 0x97, 0xfb, 0x0, 0x40, 0x5, 0x53, 0xee, 0xf0 } }

class nsIXBLAttributeEntry : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXBLATTR_IID; return iid; }

  NS_IMETHOD GetAttribute(nsIAtom** aResult) = 0;
  NS_IMETHOD GetElement(nsIContent** aResult) = 0;
};
  

class nsXBLAttributeEntry : public nsIXBLAttributeEntry {
public:
  NS_IMETHOD GetAttribute(nsIAtom** aResult) { *aResult = mAttribute; NS_IF_ADDREF(*aResult); return NS_OK; };
  NS_IMETHOD GetElement(nsIContent** aResult) { *aResult = mElement; NS_IF_ADDREF(*aResult); return NS_OK; };

  nsCOMPtr<nsIContent> mElement;
  nsCOMPtr<nsIAtom> mAttribute;

  nsXBLAttributeEntry(nsIAtom* aAtom, nsIContent* aContent) {
    NS_INIT_REFCNT(); mAttribute = aAtom; mElement = aContent;
  };

  virtual ~nsXBLAttributeEntry() {};

  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS1(nsXBLAttributeEntry, nsIXBLAttributeEntry)

/***********************************************************************/
//
// The JS class for XBLBinding
//
PR_STATIC_CALLBACK(void)
XBLBindingFinalize(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}

static JSClass XBLBindingClass = { 
  "XBLBinding", JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS, 
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, 
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, XBLBindingFinalize, 
  JSCLASS_NO_OPTIONAL_MEMBERS 
}; 

// *********************************************************************/
// The XBLBinding class

class nsXBLBinding: public nsIXBLBinding, public nsIScriptObjectOwner
{
  NS_DECL_ISUPPORTS

  // nsIXBLBinding
  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult);
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding);

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent);
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent);

  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GetInsertionPoint(nsIContent** aResult);

  NS_IMETHOD GenerateAnonymousContent(nsIContent* aBoundElement);
  NS_IMETHOD InstallEventHandlers(nsIContent* aBoundElement);
  NS_IMETHOD InstallProperties(nsIContent* aBoundElement);

  NS_IMETHOD GetBaseTag(nsIAtom** aResult);

  NS_IMETHOD AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag);

  NS_IMETHOD RemoveScriptReferences(nsIScriptContext* aContext);

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

public:
  nsXBLBinding();
  virtual ~nsXBLBinding();

  NS_IMETHOD AddScriptEventListener(nsIContent* aElement, nsIAtom* aName, const nsString& aValue, REFNSIID aIID);

// Static members
  static PRUint32 gRefCnt;
  
  static nsIAtom* kContentAtom;
  static nsIAtom* kInterfaceAtom;
  static nsIAtom* kHandlersAtom;
  static nsIAtom* kExcludesAtom;
  static nsIAtom* kInheritsAtom;
  static nsIAtom* kTypeAtom;
  static nsIAtom* kCapturerAtom;
  static nsIAtom* kExtendsAtom;
  static nsIAtom* kChildrenAtom;
  static nsIAtom* kMethodAtom;
  static nsIAtom* kArgumentAtom;
  static nsIAtom* kBodyAtom;
  static nsIAtom* kPropertyAtom;
  static nsIAtom* kOnSetAtom;
  static nsIAtom* kOnGetAtom;
  static nsIAtom* kHTMLAtom;
  static nsIAtom* kValueAtom;
  static nsIAtom* kNameAtom;
  static nsIAtom* kReadOnlyAtom;

  // Used to easily obtain the correct IID for an event.
  struct EventHandlerMapEntry {
    const char*  mAttributeName;
    nsIAtom*     mAttributeAtom;
    const nsIID* mHandlerIID;
  };

  static EventHandlerMapEntry kEventHandlerMap[];

// Internal member functions
protected:
  NS_IMETHOD CreateScriptObject(nsIScriptContext* aContext, nsIDocument* aDocument);

  void GetImmediateChild(nsIAtom* aTag, nsIContent** aResult);
  void GetNestedChild(nsIAtom* aTag, nsIContent* aContent, nsIContent** aResult);
  PRBool IsInExcludesList(nsIAtom* aTag, const nsString& aList);

  NS_IMETHOD ConstructAttributeTable(nsIContent* aElement); 

  PRBool IsMouseHandler(const nsString& aName);
  PRBool IsKeyHandler(const nsString& aName);
  PRBool IsXULHandler(const nsString& aName);

  static void GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound);

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsIContent> mBinding; // Strong. As long as we're around, the binding can't go away.
  nsCOMPtr<nsIContent> mContent; // Strong. Our anonymous content stays around with us.
  nsCOMPtr<nsIXBLBinding> mNextBinding; // Strong. The derived binding owns the base class bindings.
  nsCOMPtr<nsIContent> mChildrenElement; // Strong. One of our anonymous content children.
  void* mScriptObject; // Strong
    
  nsIContent* mBoundElement; // [WEAK] We have a reference, but we don't own it.
  
  nsSupportsHashtable* mAttributeTable; // A table for attribute entries.
};

// Static initialization
PRUint32 nsXBLBinding::gRefCnt = 0;
  
nsIAtom* nsXBLBinding::kContentAtom = nsnull;
nsIAtom* nsXBLBinding::kInterfaceAtom = nsnull;
nsIAtom* nsXBLBinding::kHandlersAtom = nsnull;
nsIAtom* nsXBLBinding::kExcludesAtom = nsnull;
nsIAtom* nsXBLBinding::kInheritsAtom = nsnull;
nsIAtom* nsXBLBinding::kTypeAtom = nsnull;
nsIAtom* nsXBLBinding::kCapturerAtom = nsnull;
nsIAtom* nsXBLBinding::kExtendsAtom = nsnull;
nsIAtom* nsXBLBinding::kChildrenAtom = nsnull;
nsIAtom* nsXBLBinding::kValueAtom = nsnull;
nsIAtom* nsXBLBinding::kHTMLAtom = nsnull;
nsIAtom* nsXBLBinding::kMethodAtom = nsnull;
nsIAtom* nsXBLBinding::kArgumentAtom = nsnull;
nsIAtom* nsXBLBinding::kBodyAtom = nsnull;
nsIAtom* nsXBLBinding::kPropertyAtom = nsnull;
nsIAtom* nsXBLBinding::kOnSetAtom = nsnull;
nsIAtom* nsXBLBinding::kOnGetAtom = nsnull;
nsIAtom* nsXBLBinding::kNameAtom = nsnull;
nsIAtom* nsXBLBinding::kReadOnlyAtom = nsnull;

nsXBLBinding::EventHandlerMapEntry
nsXBLBinding::kEventHandlerMap[] = {
    { "click",         nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "dblclick",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mousedown",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseup",       nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseover",     nsnull, &NS_GET_IID(nsIDOMMouseListener)       },
    { "mouseout",      nsnull, &NS_GET_IID(nsIDOMMouseListener)       },

    { "mousemove",     nsnull, &NS_GET_IID(nsIDOMMouseMotionListener) },

    { "keydown",       nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "keyup",         nsnull, &NS_GET_IID(nsIDOMKeyListener)         },
    { "keypress",      nsnull, &NS_GET_IID(nsIDOMKeyListener)         },

    { "load",          nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "unload",        nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "abort",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },
    { "error",         nsnull, &NS_GET_IID(nsIDOMLoadListener)        },

    { "create",        nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "close",         nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "destroy",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "command",       nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "broadcast",     nsnull, &NS_GET_IID(nsIDOMMenuListener)        },
    { "commandupdate", nsnull, &NS_GET_IID(nsIDOMMenuListener)        },

    { "focus",         nsnull, &NS_GET_IID(nsIDOMFocusListener)       },
    { "blur",          nsnull, &NS_GET_IID(nsIDOMFocusListener)       },

    { "submit",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "reset",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "change",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "select",        nsnull, &NS_GET_IID(nsIDOMFormListener)        },
    { "input",         nsnull, &NS_GET_IID(nsIDOMFormListener)        },

    { "paint",         nsnull, &NS_GET_IID(nsIDOMPaintListener)       },
    
    { "dragenter",     nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragover",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragexit",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "dragdrop",      nsnull, &NS_GET_IID(nsIDOMDragListener)        },
    { "draggesture",   nsnull, &NS_GET_IID(nsIDOMDragListener)        },

    { nsnull,            nsnull, nsnull                                 }
};

// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS2(nsXBLBinding, nsIXBLBinding, nsIScriptObjectOwner)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(void)
:mAttributeTable(nsnull), mScriptObject(nsnull)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
    kContentAtom = NS_NewAtom("content");
    kInterfaceAtom = NS_NewAtom("interface");
    kHandlersAtom = NS_NewAtom("handlers");
    kExcludesAtom = NS_NewAtom("excludes");
    kInheritsAtom = NS_NewAtom("inherits");
    kTypeAtom = NS_NewAtom("type");
    kCapturerAtom = NS_NewAtom("capturer");
    kExtendsAtom = NS_NewAtom("extends");
    kChildrenAtom = NS_NewAtom("children");
    kHTMLAtom = NS_NewAtom("html");
    kValueAtom = NS_NewAtom("value");
    kMethodAtom = NS_NewAtom("method");
    kArgumentAtom = NS_NewAtom("argument");
    kBodyAtom = NS_NewAtom("body");
    kPropertyAtom = NS_NewAtom("property");
    kOnSetAtom = NS_NewAtom("onset");
    kOnGetAtom = NS_NewAtom("onget");
    kNameAtom = NS_NewAtom("name");
    kReadOnlyAtom = NS_NewAtom("readonly");

    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      entry->mAttributeAtom = NS_NewAtom(entry->mAttributeName);
      ++entry;
    }

    // Init the XBLBinding class.

  }
}

nsXBLBinding::~nsXBLBinding(void)
{
  NS_ASSERTION(!mScriptObject, "XBL binding hasn't properly cleared its script object out.");

  delete mAttributeTable;

  gRefCnt--;
  if (gRefCnt == 0) {
    NS_RELEASE(kContentAtom);
    NS_RELEASE(kInterfaceAtom);
    NS_RELEASE(kHandlersAtom);
    NS_RELEASE(kExcludesAtom);
    NS_RELEASE(kInheritsAtom);
    NS_RELEASE(kTypeAtom);
    NS_RELEASE(kCapturerAtom);
    NS_RELEASE(kExtendsAtom);
    NS_RELEASE(kChildrenAtom);
    NS_RELEASE(kHTMLAtom);
    NS_RELEASE(kValueAtom);
    NS_RELEASE(kMethodAtom);
    NS_RELEASE(kArgumentAtom);
    NS_RELEASE(kBodyAtom);
    NS_RELEASE(kPropertyAtom); 
    NS_RELEASE(kOnSetAtom);
    NS_RELEASE(kOnGetAtom);
    NS_RELEASE(kNameAtom);
    NS_RELEASE(kReadOnlyAtom);

    EventHandlerMapEntry* entry = kEventHandlerMap;
    while (entry->mAttributeName) {
      NS_IF_RELEASE(entry->mAttributeAtom);
      ++entry;
    }
  }
}

// nsIXBLBinding Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::GetBaseBinding(nsIXBLBinding** aResult)
{
  *aResult = mNextBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBaseBinding(nsIXBLBinding* aBinding)
{
  mNextBinding = aBinding; // Comptr handles rel/add
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetAnonymousContent(nsIContent** aResult)
{
  *aResult = mContent;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetAnonymousContent(nsIContent* aParent)
{
  // First cache the element.
  mContent = aParent;

  // Now we need to ensure two things.
  // (1) The anonymous content should be fooled into thinking it's in the bound
  // element's document.
  nsCOMPtr<nsIDocument> doc;
  mBoundElement->GetDocument(*getter_AddRefs(doc));
  mContent->SetDocument(doc, PR_TRUE);

  // (2) The children's parent back pointer should not be to this synthetic root
  // but should instead point to the bound element.
  PRInt32 childCount;
  mContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mContent->ChildAt(i, *getter_AddRefs(child));
    child->SetParent(mBoundElement);
  }

  // (3) We need to insert entries into our attribute table for any elements
  // that are inheriting attributes.  This table allows us to quickly determine 
  // which elements in our anonymous content need to be updated when attributes change.
  ConstructAttributeTable(aParent);
  
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = mBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  mBinding = aElement;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetInsertionPoint(nsIContent** aResult)
{
  *aResult = mChildrenElement;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GenerateAnonymousContent(nsIContent* aBoundElement)
{
  // Set our bound element.
  mBoundElement = aBoundElement;

  // Fetch the content element for this binding.
  nsCOMPtr<nsIContent> content;
  GetImmediateChild(kContentAtom, getter_AddRefs(content));

  if (!content) {
    // We have no anonymous content.
    if (mNextBinding)
      return mNextBinding->GenerateAnonymousContent(aBoundElement);
    else return NS_OK;
  }

  // Plan to build the content by default.
  PRBool buildContent = PR_TRUE;
  PRInt32 childCount;
  aBoundElement->ChildCount(childCount);
  if (childCount > 0) {
    // See if there's an excludes attribute.
    // We'll only build content if all the explicit children are 
    // in the excludes list.
    nsAutoString excludes;
    content->GetAttribute(kNameSpaceID_None, kExcludesAtom, excludes);
    if (!excludes.Equals("*")) {
      if (!excludes.IsEmpty()) {
        // Walk the children and ensure that all of them
        // are in the excludes array.
        for (PRInt32 i = 0; i < childCount; i++) {
          nsCOMPtr<nsIContent> child;
          aBoundElement->ChildAt(i, *getter_AddRefs(child));
          nsCOMPtr<nsIAtom> tag;
          child->GetTag(*getter_AddRefs(tag));
          if (!IsInExcludesList(tag, excludes)) {
            buildContent = PR_FALSE;
            break;
          }
        }
      }
      else buildContent = PR_FALSE;
    }
  }
  
  nsCOMPtr<nsIContent> childrenElement;
      
  if (!buildContent) {
    // see if we have a <children/> element
    GetNestedChild(kChildrenAtom, content, getter_AddRefs(childrenElement));
    if (childrenElement) {
      buildContent = PR_TRUE;
    }
  }
  
  if (buildContent) {
     // Always check the content element for potential attributes.
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
    nsCOMPtr<nsIDOMNamedNodeMap> namedMap;

    node->GetAttributes(getter_AddRefs(namedMap));
    PRUint32 length;
    namedMap->GetLength(&length);

    nsCOMPtr<nsIDOMNode> attribute;
    for (PRUint32 i = 0; i < length; ++i)
    {
      namedMap->Item(i, getter_AddRefs(attribute));
      nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(attribute));
      nsAutoString name;
      attr->GetName(name);
      if (!name.Equals("excludes")) {
        nsAutoString value;
        nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mBoundElement));
        element->GetAttribute(name, value);
        if (value.IsEmpty()) {
          nsAutoString value2;
          attr->GetValue(value2);
          nsCOMPtr<nsIAtom> atom = getter_AddRefs(NS_NewAtom(name));
          mBoundElement->SetAttribute(kNameSpaceID_None, atom, value2, PR_FALSE);
        }
      }
    }
  
    nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(content);

    nsCOMPtr<nsIDOMNode> clonedNode;
    domElement->CloneNode(PR_TRUE, getter_AddRefs(clonedNode));
    
    nsCOMPtr<nsIContent> clonedContent = do_QueryInterface(clonedNode);
    SetAnonymousContent(clonedContent);

    if (childrenElement) {
      GetNestedChild(kChildrenAtom, clonedContent, getter_AddRefs(mChildrenElement));
    }
  }
  
  if (mNextBinding) {
    return mNextBinding->GenerateAnonymousContent(aBoundElement);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers(nsIContent* aBoundElement)
{
  // Fetch the handlers element for this binding.
  nsCOMPtr<nsIContent> handlers;
  GetImmediateChild(kHandlersAtom, getter_AddRefs(handlers));

  if (handlers) {
    // Now walk the handlers and add event listeners to the bound
    // element.
    PRInt32 childCount;
    handlers->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
      nsCOMPtr<nsIContent> child;
      handlers->ChildAt(i, *getter_AddRefs(child));

      // Fetch the type attribute.
      // XXX Deal with a comma-separated list of types
      nsAutoString type;
      child->GetAttribute(kNameSpaceID_None, kTypeAtom, type);
    
      if (!type.IsEmpty()) {
        nsCOMPtr<nsIAtom> eventAtom = getter_AddRefs(NS_NewAtom(type));
        PRBool found = PR_FALSE;
        nsIID iid;
        GetEventHandlerIID(eventAtom, &iid, &found);
        if (found) {
          // Add an event listener for mouse and key events only.
          PRBool mouse = IsMouseHandler(type);
          PRBool key = IsKeyHandler(type);
          PRBool xul = IsXULHandler(type);

          nsCOMPtr<nsIDOMEventReceiver> receiver = do_QueryInterface(mBoundElement);
            
          if (mouse || key || xul) {
            // Create a new nsXBLEventHandler.
            nsXBLEventHandler* handler;
            NS_NewXBLEventHandler(mBoundElement, child, type, &handler);

            // Figure out if we're using capturing or not.
            PRBool useCapture = PR_FALSE;
            nsAutoString capturer;
            child->GetAttribute(kNameSpaceID_None, kCapturerAtom, capturer);
            if (capturer.Equals("true"))
              useCapture = PR_TRUE;

            // Add the event listener.
            if (mouse)
              receiver->AddEventListener(type, (nsIDOMMouseListener*)handler, useCapture);
            else if(key)
              receiver->AddEventListener(type, (nsIDOMKeyListener*)handler, useCapture);
            else
              receiver->AddEventListener(type, (nsIDOMMenuListener*)handler, useCapture);

            NS_RELEASE(handler);
          }
          else {
            // Call AddScriptEventListener for other IID types
            nsAutoString value;
            child->GetAttribute(kNameSpaceID_None, kValueAtom, value);
            AddScriptEventListener(mBoundElement, eventAtom, value, iid);
          }
        }
      }
    }
  }

  if (mNextBinding)
    mNextBinding->InstallEventHandlers(aBoundElement);

  return NS_OK;
}

const char* gPropertyArg[] = { "val" };

NS_IMETHODIMP
nsXBLBinding::InstallProperties(nsIContent* aBoundElement)
{
  // Always install the base class properties first, so that
  // derived classes can reference the base class properties.
  if (mNextBinding)
    mNextBinding->InstallProperties(aBoundElement);

   // Fetch the handlers element for this binding.
  nsCOMPtr<nsIContent> interfaceElement;
  GetImmediateChild(kInterfaceAtom, getter_AddRefs(interfaceElement));

  if (interfaceElement) {
    // Get our bound element's script context.
    nsresult rv;
    nsCOMPtr<nsIDocument> document;
    mBoundElement->GetDocument(*getter_AddRefs(document));
    if (!document)
      return NS_OK;

    nsCOMPtr<nsIScriptGlobalObject> global;
    document->GetScriptGlobalObject(getter_AddRefs(global));

    if (!global)
      return NS_OK;

    nsCOMPtr<nsIScriptContext> context;
    rv = global->GetContext(getter_AddRefs(context));
    if (NS_FAILED(rv)) return rv;

    // Create our script object.
    if (NS_FAILED(rv = CreateScriptObject(context, document)))
      return rv;

    JSContext* cx = (JSContext*)context->GetNativeContext();

    // Do a walk.
    PRInt32 childCount;
    interfaceElement->ChildCount(childCount);
    for (PRInt32 i = 0; i < childCount; i++) {
      nsCOMPtr<nsIContent> child;
      interfaceElement->ChildAt(i, *getter_AddRefs(child));

      // See if we're a property or a method.
      nsCOMPtr<nsIAtom> tagName;
      child->GetTag(*getter_AddRefs(tagName));

      if (tagName.get() == kMethodAtom) {
        // Obtain our name attribute.
        nsAutoString name, body;
        child->GetAttribute(kNameSpaceID_None, kNameAtom, name);

        // Now walk all of our args.
        // XXX I'm lame. 32 max args allowed.
        char* args[32];
        PRUint32 argCount = 0;
        PRInt32 kidCount;
        child->ChildCount(kidCount);
        for (PRInt32 j = 0; j < kidCount; j++)
        {
          nsCOMPtr<nsIContent> arg;
          child->ChildAt(j, *getter_AddRefs(arg));
          nsCOMPtr<nsIAtom> kidTagName;
          arg->GetTag(*getter_AddRefs(kidTagName));
          if (kidTagName.get() == kArgumentAtom) {
            // Get the argname and add it to the array.
            nsAutoString argName;
            arg->GetAttribute(kNameSpaceID_None, kNameAtom, argName);
            char* argStr = argName.ToNewCString();
            args[argCount] = argStr;
            argCount++;
          }
          else if (kidTagName.get() == kBodyAtom) {
            PRInt32 textCount;
            arg->ChildCount(textCount);
            
            for (PRInt32 k = 0; k < textCount; k++) {
              // Get the child.
              nsCOMPtr<nsIContent> textChild;
              arg->ChildAt(k, *getter_AddRefs(textChild));
              nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
              if (text) {
                nsAutoString data;
                text->GetData(data);
                body += data;
              }
            }
          }
        }

        // Now that we have a body and args, compile the function
        // and then define it as a property.
        if (!body.IsEmpty()) {
          void* myFunc;
          rv = context->CompileFunction(mScriptObject,
                                        name,
                                        argCount,
                                        (const char**)args,
                                        body, 
                                        nsnull,
                                        0,
                                        PR_FALSE,
                                        &myFunc);
        }
      }
      else if (tagName.get() == kPropertyAtom) {
        // Obtain our name attribute.
        nsAutoString name;
        child->GetAttribute(kNameSpaceID_None, kNameAtom, name);

        if (!name.IsEmpty()) {
          // We have a property.
          nsAutoString getter, setter, readOnly;
          child->GetAttribute(kNameSpaceID_None, kOnGetAtom, getter);
          child->GetAttribute(kNameSpaceID_None, kOnSetAtom, setter);
          child->GetAttribute(kNameSpaceID_None, kReadOnlyAtom, readOnly);

          void* getFunc = nsnull;
          void* setFunc = nsnull;
          uintN attrs = 0;

          if (readOnly.Equals("true"))
            attrs |= JSPROP_READONLY;

          if (!getter.IsEmpty()) {
            rv = context->CompileFunction(mScriptObject,
                                          "onget",
                                          0,
                                          nsnull,
                                          getter, 
                                          nsnull,
                                          0,
                                          PR_FALSE,
                                          &getFunc);
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            attrs |= JSPROP_GETTER;
          }

          if (!setter.IsEmpty()) {
            rv = context->CompileFunction(mScriptObject,
                                          "onset",
                                          1,
                                          gPropertyArg,
                                          setter, 
                                          nsnull,
                                          0,
                                          PR_FALSE,
                                          &setFunc);
            if (NS_FAILED(rv)) return NS_ERROR_FAILURE;
            attrs |= JSPROP_SETTER;
          }

          if (getFunc || setFunc) {
            // Having either a getter or setter results in the
            // destruction of any initial value that might be set.
            // This means we only have to worry about defining the getter
            // or setter.
            rv = ::JS_DefineUCProperty(cx, (JSObject*)mScriptObject, name.GetUnicode(), 
                                       name.Length(), JSVAL_VOID,
                                       (JSPropertyOp) getFunc, 
                                       (JSPropertyOp) setFunc, 
                                       attrs); 
          } else {
            // Look for a normal value and just define that.
            nsCOMPtr<nsIContent> textChild;
            PRInt32 textCount;
            child->ChildCount(textCount);
            nsAutoString answer;
            for (PRInt32 j = 0; j < textCount; j++) {
              // Get the child.
              child->ChildAt(j, *getter_AddRefs(textChild));
              nsCOMPtr<nsIDOMText> text(do_QueryInterface(textChild));
              if (text) {
                nsAutoString data;
                text->GetData(data);
                answer += data;
              }
            }

            if (!answer.IsEmpty()) {
              // Evaluate our script and obtain a value.
              jsval* result = nsnull;
              PRBool undefined;
              rv = context->EvaluateStringWithValue(answer, 
                                           mScriptObject,
                                           nsnull, nsnull, 0, nsnull,
                                           (void*)result, &undefined);
              
              if (!undefined) {
                // Define that value as a property
                rv = ::JS_DefineUCProperty(cx, (JSObject*)mScriptObject, name.GetUnicode(), 
                                           name.Length(), *result,
                                           nsnull, nsnull,
                                           attrs); 
              }
            }
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBaseTag(nsIAtom** aResult)
{
  if (mNextBinding)
    return mNextBinding->GetBaseTag(aResult);

  // XXX Cache the value as a "base" attribute so that we don't do this
  // check over and over each time the bound element occurs.

  // We are the base binding. Obtain the extends attribute.
  nsAutoString extends;
  mBinding->GetAttribute(kNameSpaceID_None, kExtendsAtom, extends);

  if (!extends.IsEmpty()) {
    // Obtain the namespace prefix.
    nsAutoString prefix;
    PRInt32 offset = extends.FindChar(kNameSpaceSeparator);
    if (-1 != offset) {
      extends.Left(prefix, offset);
      extends.Cut(0, offset+1);
    }
    if (prefix.Length() > 0) {
      // Look up the prefix.
      nsCOMPtr<nsIAtom> prefixAtom = getter_AddRefs(NS_NewAtom(prefix));
      nsCOMPtr<nsINameSpace> nameSpace;
      nsCOMPtr<nsIXMLContent> xmlContent(do_QueryInterface(mBinding));
      if (xmlContent) {
        xmlContent->GetContainingNameSpace(*getter_AddRefs(nameSpace));

        nsCOMPtr<nsINameSpace> tagSpace;
        nameSpace->FindNameSpace(prefixAtom, *getter_AddRefs(tagSpace));
        if (tagSpace) {
          // Score! Return the tag.
          // XXX We should really return the namespace as well.
          *aResult = NS_NewAtom(extends); // The addref happens here
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::AttributeChanged(nsIAtom* aAttribute, PRInt32 aNameSpaceID, PRBool aRemoveFlag)
{
  if (mNextBinding)
    mNextBinding->AttributeChanged(aAttribute, aNameSpaceID, aRemoveFlag);

  if (!mAttributeTable)
    return NS_OK;

  nsISupportsKey key(aAttribute);
  nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                 mAttributeTable->Get(&key)));

  nsCOMPtr<nsISupportsArray> entry = do_QueryInterface(supports);
  if (!entry)
    return NS_OK;

  // Iterate over the elements in the array.
  PRUint32 count;
  entry->Count(&count);

  for (PRUint32 i=0; i<count; i++) {
    nsCOMPtr<nsISupports> item;
    entry->GetElementAt(i, getter_AddRefs(item));
    nsCOMPtr<nsIXBLAttributeEntry> xblAttr = do_QueryInterface(item);
    if (xblAttr) {
      nsCOMPtr<nsIContent> element;
      nsCOMPtr<nsIAtom> setAttr;
      xblAttr->GetElement(getter_AddRefs(element));
      xblAttr->GetAttribute(getter_AddRefs(setAttr));

      if (aRemoveFlag)
        element->UnsetAttribute(aNameSpaceID, setAttr, PR_TRUE);
      else {
        nsAutoString value;
        nsresult result = mBoundElement->GetAttribute(aNameSpaceID, aAttribute, value);
        PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                              result == NS_CONTENT_ATTR_HAS_VALUE);

        if (attrPresent)
          element->SetAttribute(aNameSpaceID, setAttr, value, PR_TRUE);
      }

      // See if we're the <html> tag in XUL, and see if value is being
      // set or unset on us.
      nsCOMPtr<nsIAtom> tag;
      element->GetTag(*getter_AddRefs(tag));
      if ((tag.get() == kHTMLAtom) && (setAttr.get() == kValueAtom)) {
        // Flush out all our kids.
        PRInt32 childCount;
        element->ChildCount(childCount);
        if (childCount > 0)
          element->RemoveChildAt(0, PR_TRUE);
        
        if (!aRemoveFlag) {
          // Construct a new text node and insert it.
          nsAutoString value;
          nsresult result = mBoundElement->GetAttribute(aNameSpaceID, aAttribute, value);
          if (!value.IsEmpty()) {
            nsCOMPtr<nsIDOMText> textNode;
            nsCOMPtr<nsIDocument> doc;
            mBoundElement->GetDocument(*getter_AddRefs(doc));
            nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
            domDoc->CreateTextNode(value, getter_AddRefs(textNode));
            nsCOMPtr<nsIDOMNode> dummy;
            nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(element));
            domElement->AppendChild(textNode, getter_AddRefs(dummy));
          }
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::RemoveScriptReferences(nsIScriptContext* aContext)
{
  if (mNextBinding)
    mNextBinding->RemoveScriptReferences(aContext);

  if (mScriptObject) {
    aContext->RemoveReference((void*) &mScriptObject, mScriptObject);
    mScriptObject = nsnull;
  }

  return NS_OK;
}

// nsIScriptObjectOwner methods ///////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::GetScriptObject(nsIScriptContext* aContext, void** aScriptObject)
{
  if (!mScriptObject && mNextBinding) {
    nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(mNextBinding));
    return owner->GetScriptObject(aContext, aScriptObject);
  }

  *aScriptObject = mScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetScriptObject(void *aScriptObject)
{
  // DO NOT EVER CALL THIS WITH NULL!
  NS_ASSERTION(aScriptObject, "Attempt to void out an XBL binding script object using SetScriptObject. Bad!");
  mScriptObject = aScriptObject;
  return NS_OK;
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::CreateScriptObject(nsIScriptContext* aContext, nsIDocument* aDocument)
{
  // First ensure our JS class is initialized.
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *proto = nsnull;
  JSObject *constructor = nsnull;
  JSObject *parent_proto = nsnull;
  JSObject *global = JS_GetGlobalObject(jscontext);
  jsval vp;

  if ((PR_TRUE != JS_LookupProperty(jscontext, global, "XBLBinding", &vp)) ||
      !JSVAL_IS_OBJECT(vp) ||
      ((constructor = JSVAL_TO_OBJECT(vp)) == nsnull) ||
      (PR_TRUE != JS_LookupProperty(jscontext, JSVAL_TO_OBJECT(vp), "prototype", &vp)) || 
      !JSVAL_IS_OBJECT(vp)) {
    proto = JS_InitClass(jscontext,     // context
                         global,        // global object
                         parent_proto,  // parent proto 
                         &XBLBindingClass,      // JSClass
                         nsnull,            // JSNative ctor
                         0,             // ctor args
                         nsnull,  // proto props
                         nsnull,     // proto funcs
                         nsnull,        // ctor props (static)
                         nsnull);       // ctor funcs (static)
    if (nsnull == proto) {
      return NS_ERROR_FAILURE;
    }

  }
  else if ((nsnull != constructor) && JSVAL_IS_OBJECT(vp)) {
    proto = JSVAL_TO_OBJECT(vp);
  }
  else {
    return NS_ERROR_FAILURE;
  }

  // Obtain the bound element's current script object.
  nsCOMPtr<nsIScriptObjectOwner> owner(do_QueryInterface(mBoundElement));
 
  // create a js object for this binding
  void* parent;
  nsCOMPtr<nsIScriptObjectOwner> docOwner(do_QueryInterface(aDocument));
  docOwner->GetScriptObject(aContext, &parent);
  if (!parent)
    return NS_ERROR_FAILURE;

  void* xulElementProto;
  nsCOMPtr<nsIScriptObjectOwner> elementOwner(do_QueryInterface(mBoundElement));
  elementOwner->GetScriptObject(aContext, &xulElementProto);
  JSObject* xulElementProtoObject = (JSObject*)xulElementProto;

  JSObject* parentObject = (JSObject*)parent;
  JSObject* object = JS_NewObject(jscontext, &XBLBindingClass, xulElementProtoObject, parentObject);

  if (object) {
    // connect the native object to the js object
    void* privateData = JS_GetPrivate(jscontext, xulElementProtoObject);
    JS_SetPrivate(jscontext, object, privateData);

    // Set ourselves as the new script object.
    SetScriptObject(object);

    // Need to addref on copy of proto chain
    NS_IF_ADDREF(NS_REINTERPRET_CAST(nsISupports*, privateData));

    // Ensure that a reference exists to this binding
    aContext->AddNamedReference((void*) &mScriptObject, mScriptObject, "nsXBLBinding::mScriptObject");
  }

  return NS_OK;
}

void
nsXBLBinding::GetImmediateChild(nsIAtom* aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  mBinding->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    mBinding->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

void
nsXBLBinding::GetNestedChild(nsIAtom* aTag, nsIContent* aContent, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aContent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aContent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = aContent; // We return the parent of the correct child.
      NS_ADDREF(*aResult);
      return;
    }
    else {
      GetNestedChild(aTag, child, aResult);
      if (*aResult)
        return;
    }
  }

  return;
}


PRBool
nsXBLBinding::IsInExcludesList(nsIAtom* aTag, const nsString& aList) 
{ 
  nsAutoString element;
  aTag->ToString(element);

  if (aList.Equals("*"))
      return PR_TRUE; // match _everything_!

  PRInt32 indx = aList.Find(element);
  if (indx == -1)
    return PR_FALSE; // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    PRUnichar ch = aList[indx - 1];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  if (indx + element.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + element.Length()];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXBLBinding::ConstructAttributeTable(nsIContent* aElement)
{
  // XXX This function still needs to deal with the
  // ability to map one attribute to another.
  nsAutoString inherits;
  aElement->GetAttribute(kNameSpaceID_None, kInheritsAtom, inherits);
  if (!inherits.IsEmpty()) {
    if (!mAttributeTable) {
        mAttributeTable = new nsSupportsHashtable(8);
    }

    // The user specified at least one attribute.
    char* str = inherits.ToNewCString();
    char* newStr;
    char* token = nsCRT::strtok( str, ", ", &newStr );   
    while( token != NULL ) {
      // Build an atom out of this attribute.
      nsCOMPtr<nsIAtom> atom;
      nsCOMPtr<nsIAtom> attribute;

      // Figure out if this token contains a :. 
      nsAutoString attr(token);
      PRInt32 index = attr.Find(":", PR_TRUE);
      if (index != -1) {
        // This attribute maps to something different.
        nsAutoString left, right;
        attr.Left(left, index);
        attr.Right(right, attr.Length()-index-1);

        atom = getter_AddRefs(NS_NewAtom(left));
        attribute = getter_AddRefs(NS_NewAtom(right));
      }
      else {
        atom = getter_AddRefs(NS_NewAtom(token));
        attribute = getter_AddRefs(NS_NewAtom(token));
      }
      
      // Create an XBL attribute entry.
      nsXBLAttributeEntry* xblAttr = new nsXBLAttributeEntry(attribute, aElement);

      // Now we should see if some element within our anonymous
      // content is already observing this attribute.
      nsISupportsKey key(atom);
      nsCOMPtr<nsISupports> supports = getter_AddRefs(NS_STATIC_CAST(nsISupports*, 
                                                                     mAttributeTable->Get(&key)));
  
      nsCOMPtr<nsISupportsArray> entry = do_QueryInterface(supports);
      if (!entry) {
        // Make a new entry.
        NS_NewISupportsArray(getter_AddRefs(entry));

        // Put it in the table.
        mAttributeTable->Put(&key, entry);
      }

      // Append ourselves to our entry.
      entry->AppendElement(xblAttr);

      // Now make sure that this attribute is initially set.
      // XXX How to deal with NAMESPACES!!!?
      nsAutoString value;
      nsresult result = mBoundElement->GetAttribute(kNameSpaceID_None, atom, value);
      PRBool attrPresent = (result == NS_CONTENT_ATTR_NO_VALUE ||
                            result == NS_CONTENT_ATTR_HAS_VALUE);

      if (attrPresent) {
        aElement->SetAttribute(kNameSpaceID_None, attribute, value, PR_TRUE);
        nsCOMPtr<nsIAtom> tag;
        aElement->GetTag(*getter_AddRefs(tag));
        if ((tag.get() == kHTMLAtom) && (attribute.get() == kValueAtom) && !value.IsEmpty()) {
          nsCOMPtr<nsIDOMText> textNode;
          nsCOMPtr<nsIDocument> doc;
          mBoundElement->GetDocument(*getter_AddRefs(doc));
          nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
          domDoc->CreateTextNode(value, getter_AddRefs(textNode));
          nsCOMPtr<nsIDOMNode> dummy;
          nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(aElement));
          domElement->AppendChild(textNode, getter_AddRefs(dummy));
        }
      }

      token = nsCRT::strtok( newStr, ", ", &newStr );
    }

    nsAllocator::Free(str);
  }

  // Recur into our children.
  PRInt32 childCount;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    ConstructAttributeTable(child);
  }
  return NS_OK;
}

void
nsXBLBinding::GetEventHandlerIID(nsIAtom* aName, nsIID* aIID, PRBool* aFound)
{
  *aFound = PR_FALSE;

  EventHandlerMapEntry* entry = kEventHandlerMap;
  while (entry->mAttributeAtom) {
    if (entry->mAttributeAtom == aName) {
        *aIID = *entry->mHandlerIID;
        *aFound = PR_TRUE;
        break;
    }
    ++entry;
  }
}

PRBool
nsXBLBinding::IsMouseHandler(const nsString& aName)
{
  return ((aName.Equals("click")) || (aName.Equals("dblclick")) || (aName.Equals("mousedown")) ||
          (aName.Equals("mouseover")) || (aName.Equals("mouseout")) || (aName.Equals("mouseup")));
}

PRBool
nsXBLBinding::IsKeyHandler(const nsString& aName)
{
  return ((aName.Equals("keypress")) || (aName.Equals("keydown")) || (aName.Equals("keyup")));
}

PRBool
nsXBLBinding::IsXULHandler(const nsString& aName)
{
  return ((aName.Equals("create")) || (aName.Equals("destroy")) || (aName.Equals("broadcast")) ||
          (aName.Equals("command")) || (aName.Equals("commandupdate")) || (aName.Equals("close")));
}

NS_IMETHODIMP
nsXBLBinding::AddScriptEventListener(nsIContent* aElement, nsIAtom* aName, const nsString& aValue, REFNSIID aIID)
{
  nsAutoString val;
  aName->ToString(val);
  
  nsAutoString eventStr("on");
  eventStr += val;

  nsCOMPtr<nsIAtom> eventName = getter_AddRefs(NS_NewAtom(eventStr));

  nsresult rv;
  nsCOMPtr<nsIDocument> document;
  aElement->GetDocument(*getter_AddRefs(document));
  if (!document)
    return NS_OK;

  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(aElement));
  if (!receiver)
    return NS_OK;

  nsCOMPtr<nsIScriptGlobalObject> global;
  document->GetScriptGlobalObject(getter_AddRefs(global));

  // This can happen normally as part of teardown code.
  if (!global)
    return NS_OK;

  nsCOMPtr<nsIScriptContext> context;
  rv = global->GetContext(getter_AddRefs(context));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIEventListenerManager> manager;
  rv = receiver->GetListenerManager(getter_AddRefs(manager));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIScriptObjectOwner> scriptOwner(do_QueryInterface(receiver));
  if (!scriptOwner)
    return NS_OK;

  rv = manager->AddScriptEventListener(context, scriptOwner, eventName, aValue, aIID, PR_FALSE);

  return rv;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLBinding(nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

