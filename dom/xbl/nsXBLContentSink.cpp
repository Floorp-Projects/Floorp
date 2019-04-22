/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"

#include "nsXBLContentSink.h"
#include "mozilla/dom/Document.h"
#include "nsBindingManager.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsIURI.h"
#include "nsTextFragment.h"
#ifdef MOZ_XUL
#  include "nsXULElement.h"
#endif
#include "nsXBLProtoImplProperty.h"
#include "nsXBLProtoImplMethod.h"
#include "nsXBLProtoImplField.h"
#include "nsXBLPrototypeBinding.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsNodeInfoManager.h"
#include "nsIPrincipal.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::dom;

nsresult NS_NewXBLContentSink(nsIXMLContentSink** aResult, Document* aDoc,
                              nsIURI* aURI, nsISupports* aContainer) {
  NS_ENSURE_ARG_POINTER(aResult);

  RefPtr<nsXBLContentSink> it = new nsXBLContentSink();
  nsresult rv = it->Init(aDoc, aURI, aContainer);
  NS_ENSURE_SUCCESS(rv, rv);

  it.forget(aResult);
  return NS_OK;
}

nsXBLContentSink::nsXBLContentSink()
    : mState(eXBL_InDocument),
      mSecondaryState(eXBL_None),
      mDocInfo(nullptr),
      mIsChromeOrResource(false),
      mFoundFirstBinding(false),
      mBinding(nullptr),
      mHandler(nullptr),
      mImplementation(nullptr),
      mImplMember(nullptr),
      mImplField(nullptr),
      mProperty(nullptr),
      mMethod(nullptr),
      mField(nullptr) {
  mPrettyPrintXML = false;
}

nsXBLContentSink::~nsXBLContentSink() {}

nsresult nsXBLContentSink::Init(Document* aDoc, nsIURI* aURI,
                                nsISupports* aContainer) {
  nsresult rv;
  rv = nsXMLContentSink::Init(aDoc, aURI, aContainer, nullptr);
  return rv;
}

void nsXBLContentSink::MaybeStartLayout(bool aIgnorePendingSheets) {}

nsresult nsXBLContentSink::FlushText(bool aReleaseTextNode) {
  if (mTextLength != 0) {
    const nsAString& text = Substring(mText, mText + mTextLength);
    if (mState == eXBL_InHandlers) {
      NS_ASSERTION(mBinding, "Must have binding here");
      // Get the text and add it to the event handler.
      if (mSecondaryState == eXBL_InHandler) mHandler->AppendHandlerText(text);
      mTextLength = 0;
      return NS_OK;
    } else if (mState == eXBL_InImplementation) {
      NS_ASSERTION(mBinding, "Must have binding here");
      if (mSecondaryState == eXBL_InConstructor ||
          mSecondaryState == eXBL_InDestructor) {
        // Construct a method for the constructor/destructor.
        nsXBLProtoImplMethod* method;
        if (mSecondaryState == eXBL_InConstructor)
          method = mBinding->GetConstructor();
        else
          method = mBinding->GetDestructor();

        // Get the text and add it to the constructor/destructor.
        method->AppendBodyText(text);
      } else if (mSecondaryState == eXBL_InGetter ||
                 mSecondaryState == eXBL_InSetter) {
        // Get the text and add it to the getter/setter
        if (mSecondaryState == eXBL_InGetter)
          mProperty->AppendGetterText(text);
        else
          mProperty->AppendSetterText(text);
      } else if (mSecondaryState == eXBL_InBody) {
        // Get the text and add it to the method
        if (mMethod) mMethod->AppendBodyText(text);
      } else if (mSecondaryState == eXBL_InField) {
        // Get the text and add it to the method
        if (mField) mField->AppendFieldText(text);
      }
      mTextLength = 0;
      return NS_OK;
    }

    nsIContent* content = GetCurrentContent();
    if (content && (content->NodeInfo()->NamespaceEquals(kNameSpaceID_XBL) ||
                    (content->IsXULElement() &&
                     !content->IsAnyOfXULElements(nsGkAtoms::label,
                                                  nsGkAtoms::description)))) {
      bool isWS = true;
      if (mTextLength > 0) {
        const char16_t* cp = mText;
        const char16_t* end = mText + mTextLength;
        while (cp < end) {
          char16_t ch = *cp++;
          if (!dom::IsSpaceCharacter(ch)) {
            isWS = false;
            break;
          }
        }
      }

      if (isWS && mTextLength > 0) {
        mTextLength = 0;
        // Make sure to drop the textnode, if any
        return nsXMLContentSink::FlushText(aReleaseTextNode);
      }
    }
  }

  return nsXMLContentSink::FlushText(aReleaseTextNode);
}

NS_IMETHODIMP
nsXBLContentSink::ReportError(const char16_t* aErrorText,
                              const char16_t* aSourceText,
                              nsIScriptError* aError, bool* _retval) {
  MOZ_ASSERT(aError && aSourceText && aErrorText, "Check arguments!!!");

  // XXX FIXME This function overrides and calls on
  // nsXMLContentSink::ReportError, and probably should die.  See bug 347826.

  // XXX We should make sure the binding has no effect, but that it also
  // gets destroyed properly without leaking.  Perhaps we should even
  // ensure that the content that was bound is displayed with no
  // binding.

#ifdef DEBUG
  // Report the error to stderr.
  fprintf(stderr, "\n%s\n%s\n\n", NS_LossyConvertUTF16toASCII(aErrorText).get(),
          NS_LossyConvertUTF16toASCII(aSourceText).get());
#endif

  // Most of what this does won't be too useful, but whatever...
  // nsXMLContentSink::ReportError will handle the console logging.
  return nsXMLContentSink::ReportError(aErrorText, aSourceText, aError,
                                       _retval);
}

nsresult nsXBLContentSink::ReportUnexpectedElement(nsAtom* aElementName,
                                                   uint32_t aLineNumber) {
  // XXX we should really somehow stop the parse and drop the binding
  // instead of just letting the XML sink build the content model like
  // we do...
  mState = eXBL_Error;
  nsAutoString elementName;
  aElementName->ToString(elementName);

  const char16_t* params[] = {elementName.get()};

  return nsContentUtils::ReportToConsole(
      nsIScriptError::errorFlag, NS_LITERAL_CSTRING("XBL Content Sink"),
      mDocument, nsContentUtils::eXBL_PROPERTIES, "UnexpectedElement", params,
      ArrayLength(params), nullptr, EmptyString() /* source line */,
      aLineNumber);
}

void nsXBLContentSink::AddMember(nsXBLProtoImplMember* aMember) {
  // Add this member to our chain.
  if (mImplMember)
    mImplMember->SetNext(
        aMember);  // Already have a chain. Just append to the end.
  else
    mImplementation->SetMemberList(
        aMember);  // We're the first member in the chain.

  mImplMember = aMember;  // Adjust our pointer to point to the new last member
                          // in the chain.
}

void nsXBLContentSink::AddField(nsXBLProtoImplField* aField) {
  // Add this field to our chain.
  if (mImplField)
    mImplField->SetNext(
        aField);  // Already have a chain. Just append to the end.
  else
    mImplementation->SetFieldList(
        aField);  // We're the first member in the chain.

  mImplField = aField;  // Adjust our pointer to point to the new last field in
                        // the chain.
}

NS_IMETHODIMP
nsXBLContentSink::HandleStartElement(const char16_t* aName,
                                     const char16_t** aAtts,
                                     uint32_t aAttsCount, uint32_t aLineNumber,
                                     uint32_t aColumnNumber) {
  nsresult rv = nsXMLContentSink::HandleStartElement(
      aName, aAtts, aAttsCount, aLineNumber, aColumnNumber);
  if (NS_FAILED(rv)) return rv;

  if (mState == eXBL_InBinding && !mBinding) {
    rv = ConstructBinding(aLineNumber);
    if (NS_FAILED(rv)) return rv;

    // mBinding may still be null, if the binding had no id.  If so,
    // we'll deal with that later in the sink.
  }

  return rv;
}

NS_IMETHODIMP
nsXBLContentSink::HandleEndElement(const char16_t* aName) {
  FlushText();

  if (mState != eXBL_InDocument) {
    int32_t nameSpaceID;
    RefPtr<nsAtom> prefix, localName;
    nsContentUtils::SplitExpatName(aName, getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID == kNameSpaceID_XBL) {
      if (mState == eXBL_Error) {
        // Check whether we've opened this tag before; we may not have if
        // it was a real XBL tag before the error occurred.
        if (!GetCurrentContent()->NodeInfo()->Equals(localName, nameSpaceID)) {
          // OK, this tag was never opened as far as the XML sink is
          // concerned.  Just drop the HandleEndElement
          return NS_OK;
        }
      } else if (mState == eXBL_InHandlers) {
        if (localName == nsGkAtoms::handlers) {
          mState = eXBL_InBinding;
          mHandler = nullptr;
        } else if (localName == nsGkAtoms::handler)
          mSecondaryState = eXBL_None;
        return NS_OK;
      } else if (mState == eXBL_InImplementation) {
        if (localName == nsGkAtoms::implementation)
          mState = eXBL_InBinding;
        else if (localName == nsGkAtoms::property) {
          mSecondaryState = eXBL_None;
          mProperty = nullptr;
        } else if (localName == nsGkAtoms::method) {
          mSecondaryState = eXBL_None;
          mMethod = nullptr;
        } else if (localName == nsGkAtoms::field) {
          mSecondaryState = eXBL_None;
          mField = nullptr;
        } else if (localName == nsGkAtoms::constructor ||
                   localName == nsGkAtoms::destructor)
          mSecondaryState = eXBL_None;
        else if (localName == nsGkAtoms::getter ||
                 localName == nsGkAtoms::setter)
          mSecondaryState = eXBL_InProperty;
        else if (localName == nsGkAtoms::parameter ||
                 localName == nsGkAtoms::body)
          mSecondaryState = eXBL_InMethod;
        return NS_OK;
      } else if (mState == eXBL_InBindings &&
                 localName == nsGkAtoms::bindings) {
        mState = eXBL_InDocument;
      }

      nsresult rv = nsXMLContentSink::HandleEndElement(aName);
      if (NS_FAILED(rv)) return rv;

      if (mState == eXBL_InBinding && localName == nsGkAtoms::binding) {
        mState = eXBL_InBindings;
        if (mBinding) {  // See comment in HandleStartElement()
          mBinding->Initialize();
          mBinding = nullptr;  // Clear our current binding ref.
        }
      }

      return NS_OK;
    }
  }

  return nsXMLContentSink::HandleEndElement(aName);
}

NS_IMETHODIMP
nsXBLContentSink::HandleCDataSection(const char16_t* aData, uint32_t aLength) {
  if (mState == eXBL_InHandlers || mState == eXBL_InImplementation)
    return AddText(aData, aLength);
  return nsXMLContentSink::HandleCDataSection(aData, aLength);
}

#define ENSURE_XBL_STATE(_cond)                     \
  PR_BEGIN_MACRO                                    \
  if (!(_cond)) {                                   \
    ReportUnexpectedElement(aTagName, aLineNumber); \
    return true;                                    \
  }                                                 \
  PR_END_MACRO

bool nsXBLContentSink::OnOpenContainer(const char16_t** aAtts,
                                       uint32_t aAttsCount,
                                       int32_t aNameSpaceID, nsAtom* aTagName,
                                       uint32_t aLineNumber) {
  if (mState == eXBL_Error) {
    return true;
  }

  if (aNameSpaceID != kNameSpaceID_XBL) {
    // Construct non-XBL nodes
    return true;
  }

  bool ret = true;
  if (aTagName == nsGkAtoms::bindings) {
    ENSURE_XBL_STATE(mState == eXBL_InDocument);

    NS_ASSERTION(mDocument, "Must have a document!");
    RefPtr<nsXBLDocumentInfo> info = new nsXBLDocumentInfo(mDocument);

    // We keep a weak ref. We're creating a cycle between doc/binding
    // manager/doc info.
    mDocInfo = info;

    if (!mDocInfo) {
      mState = eXBL_Error;
      return true;
    }

    mDocument->BindingManager()->PutXBLDocumentInfo(mDocInfo);

    nsIURI* uri = mDocument->GetDocumentURI();

    bool isChrome = false;
    bool isRes = false;

    uri->SchemeIs("chrome", &isChrome);
    uri->SchemeIs("resource", &isRes);
    mIsChromeOrResource = isChrome || isRes;

    mState = eXBL_InBindings;
  } else if (aTagName == nsGkAtoms::binding) {
    ENSURE_XBL_STATE(mState == eXBL_InBindings);
    mState = eXBL_InBinding;
  } else if (aTagName == nsGkAtoms::handlers) {
    ENSURE_XBL_STATE(mState == eXBL_InBinding && mBinding);
    mState = eXBL_InHandlers;
    ret = false;
  } else if (aTagName == nsGkAtoms::handler) {
    ENSURE_XBL_STATE(mState == eXBL_InHandlers);
    mSecondaryState = eXBL_InHandler;
    ConstructHandler(aAtts, aLineNumber);
    ret = false;
  } else if (aTagName == nsGkAtoms::implementation) {
    ENSURE_XBL_STATE(mState == eXBL_InBinding && mBinding);
    mState = eXBL_InImplementation;
    ConstructImplementation(aAtts);
    // Note that this mState will cause us to return false, so no need
    // to set ret to false.
  } else if (aTagName == nsGkAtoms::constructor) {
    ENSURE_XBL_STATE(mState == eXBL_InImplementation &&
                     mSecondaryState == eXBL_None);
    NS_ASSERTION(mBinding, "Must have binding here");

    mSecondaryState = eXBL_InConstructor;
    nsAutoString name;
    if (!mCurrentBindingID.IsEmpty()) {
      name.Assign(mCurrentBindingID);
      name.AppendLiteral("_XBL_Constructor");
    } else {
      name.AppendLiteral("XBL_Constructor");
    }
    nsXBLProtoImplAnonymousMethod* newMethod =
        new nsXBLProtoImplAnonymousMethod(name.get());
    newMethod->SetLineNumber(aLineNumber);
    mBinding->SetConstructor(newMethod);
    AddMember(newMethod);
  } else if (aTagName == nsGkAtoms::destructor) {
    ENSURE_XBL_STATE(mState == eXBL_InImplementation &&
                     mSecondaryState == eXBL_None);
    NS_ASSERTION(mBinding, "Must have binding here");
    mSecondaryState = eXBL_InDestructor;
    nsAutoString name;
    if (!mCurrentBindingID.IsEmpty()) {
      name.Assign(mCurrentBindingID);
      name.AppendLiteral("_XBL_Destructor");
    } else {
      name.AppendLiteral("XBL_Destructor");
    }
    nsXBLProtoImplAnonymousMethod* newMethod =
        new nsXBLProtoImplAnonymousMethod(name.get());
    newMethod->SetLineNumber(aLineNumber);
    mBinding->SetDestructor(newMethod);
    AddMember(newMethod);
  } else if (aTagName == nsGkAtoms::field) {
    ENSURE_XBL_STATE(mState == eXBL_InImplementation &&
                     mSecondaryState == eXBL_None);
    NS_ASSERTION(mBinding, "Must have binding here");
    mSecondaryState = eXBL_InField;
    ConstructField(aAtts, aLineNumber);
  } else if (aTagName == nsGkAtoms::property) {
    ENSURE_XBL_STATE(mState == eXBL_InImplementation &&
                     mSecondaryState == eXBL_None);
    NS_ASSERTION(mBinding, "Must have binding here");
    mSecondaryState = eXBL_InProperty;
    ConstructProperty(aAtts, aLineNumber);
  } else if (aTagName == nsGkAtoms::getter) {
    ENSURE_XBL_STATE(mSecondaryState == eXBL_InProperty && mProperty);
    NS_ASSERTION(mState == eXBL_InImplementation, "Unexpected state");
    mProperty->SetGetterLineNumber(aLineNumber);
    mSecondaryState = eXBL_InGetter;
  } else if (aTagName == nsGkAtoms::setter) {
    ENSURE_XBL_STATE(mSecondaryState == eXBL_InProperty && mProperty);
    NS_ASSERTION(mState == eXBL_InImplementation, "Unexpected state");
    mProperty->SetSetterLineNumber(aLineNumber);
    mSecondaryState = eXBL_InSetter;
  } else if (aTagName == nsGkAtoms::method) {
    ENSURE_XBL_STATE(mState == eXBL_InImplementation &&
                     mSecondaryState == eXBL_None);
    NS_ASSERTION(mBinding, "Must have binding here");
    mSecondaryState = eXBL_InMethod;
    ConstructMethod(aAtts);
  } else if (aTagName == nsGkAtoms::parameter) {
    ENSURE_XBL_STATE(mSecondaryState == eXBL_InMethod && mMethod);
    NS_ASSERTION(mState == eXBL_InImplementation, "Unexpected state");
    ConstructParameter(aAtts);
  } else if (aTagName == nsGkAtoms::body) {
    ENSURE_XBL_STATE(mSecondaryState == eXBL_InMethod && mMethod);
    NS_ASSERTION(mState == eXBL_InImplementation, "Unexpected state");
    // stash away the line number
    mMethod->SetLineNumber(aLineNumber);
    mSecondaryState = eXBL_InBody;
  }

  return ret && mState != eXBL_InImplementation;
}

#undef ENSURE_XBL_STATE

nsresult nsXBLContentSink::ConstructBinding(uint32_t aLineNumber) {
  // This is only called from HandleStartElement, so it'd better be an element.
  RefPtr<Element> binding = GetCurrentContent()->AsElement();
  binding->GetAttr(kNameSpaceID_None, nsGkAtoms::id, mCurrentBindingID);
  NS_ConvertUTF16toUTF8 cid(mCurrentBindingID);

  nsresult rv = NS_OK;

  // Don't create a binding with no id. nsXBLPrototypeBinding::Read also
  // performs this check.
  if (!cid.IsEmpty()) {
    mBinding = new nsXBLPrototypeBinding();

    rv = mBinding->Init(cid, mDocInfo, binding, !mFoundFirstBinding);
    if (NS_SUCCEEDED(rv) &&
        NS_SUCCEEDED(mDocInfo->SetPrototypeBinding(cid, mBinding))) {
      if (!mFoundFirstBinding) {
        mFoundFirstBinding = true;
        mDocInfo->SetFirstPrototypeBinding(mBinding);
      }
      binding->UnsetAttr(kNameSpaceID_None, nsGkAtoms::id, false);
    } else {
      delete mBinding;
      mBinding = nullptr;
    }
  } else {
    nsContentUtils::ReportToConsole(
        nsIScriptError::errorFlag, NS_LITERAL_CSTRING("XBL Content Sink"),
        nullptr, nsContentUtils::eXBL_PROPERTIES, "MissingIdAttr", nullptr, 0,
        mDocumentURI, EmptyString(), aLineNumber);
  }

  return rv;
}

static bool FindValue(const char16_t** aAtts, nsAtom* aAtom,
                      const char16_t** aResult) {
  RefPtr<nsAtom> prefix, localName;
  for (; *aAtts; aAtts += 2) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    // Is this attribute one of the ones we care about?
    if (nameSpaceID == kNameSpaceID_None && localName == aAtom) {
      *aResult = aAtts[1];

      return true;
    }
  }

  return false;
}

void nsXBLContentSink::ConstructHandler(const char16_t** aAtts,
                                        uint32_t aLineNumber) {
  const char16_t* event = nullptr;
  const char16_t* modifiers = nullptr;
  const char16_t* button = nullptr;
  const char16_t* clickcount = nullptr;
  const char16_t* keycode = nullptr;
  const char16_t* charcode = nullptr;
  const char16_t* phase = nullptr;
  const char16_t* command = nullptr;
  const char16_t* action = nullptr;
  const char16_t* group = nullptr;
  const char16_t* preventdefault = nullptr;
  const char16_t* allowuntrusted = nullptr;

  RefPtr<nsAtom> prefix, localName;
  for (; *aAtts; aAtts += 2) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID != kNameSpaceID_None) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (localName == nsGkAtoms::event)
      event = aAtts[1];
    else if (localName == nsGkAtoms::modifiers)
      modifiers = aAtts[1];
    else if (localName == nsGkAtoms::button)
      button = aAtts[1];
    else if (localName == nsGkAtoms::clickcount)
      clickcount = aAtts[1];
    else if (localName == nsGkAtoms::keycode)
      keycode = aAtts[1];
    else if (localName == nsGkAtoms::key || localName == nsGkAtoms::charcode)
      charcode = aAtts[1];
    else if (localName == nsGkAtoms::phase)
      phase = aAtts[1];
    else if (localName == nsGkAtoms::command)
      command = aAtts[1];
    else if (localName == nsGkAtoms::action)
      action = aAtts[1];
    else if (localName == nsGkAtoms::group)
      group = aAtts[1];
    else if (localName == nsGkAtoms::preventdefault)
      preventdefault = aAtts[1];
    else if (localName == nsGkAtoms::allowuntrusted)
      allowuntrusted = aAtts[1];
  }

  if (command && !mIsChromeOrResource) {
    // Make sure the XBL doc is chrome or resource if we have a command
    // shorthand syntax.
    mState = eXBL_Error;
    nsContentUtils::ReportToConsole(
        nsIScriptError::errorFlag, NS_LITERAL_CSTRING("XBL Content Sink"),
        mDocument, nsContentUtils::eXBL_PROPERTIES, "CommandNotInChrome",
        nullptr, 0, nullptr, EmptyString() /* source line */, aLineNumber);
    return;  // Don't even make this handler.
  }

  // All of our pointers are now filled in. Construct our handler with all of
  // these parameters.
  nsXBLPrototypeHandler* newHandler;
  newHandler = new nsXBLPrototypeHandler(
      event, phase, action, command, keycode, charcode, modifiers, button,
      clickcount, group, preventdefault, allowuntrusted, mBinding, aLineNumber);

  // Add this handler to our chain of handlers.
  if (mHandler) {
    // Already have a chain. Just append to the end.
    mHandler->SetNextHandler(newHandler);
  } else {
    // We're the first handler in the chain.
    mBinding->SetPrototypeHandlers(newHandler);
  }
  // Adjust our mHandler pointer to point to the new last handler in the
  // chain.
  mHandler = newHandler;
}

void nsXBLContentSink::ConstructImplementation(const char16_t** aAtts) {
  mImplementation = nullptr;
  mImplMember = nullptr;
  mImplField = nullptr;

  if (!mBinding) return;

  const char16_t* name = nullptr;

  RefPtr<nsAtom> prefix, localName;
  for (; *aAtts; aAtts += 2) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID != kNameSpaceID_None) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (localName == nsGkAtoms::name) {
      name = aAtts[1];
    } else if (localName == nsGkAtoms::implements) {
      // Only allow implementation of interfaces via XBL if the principal of
      // our XBL document is the system principal.
      if (nsContentUtils::IsSystemPrincipal(mDocument->NodePrincipal())) {
        mBinding->ConstructInterfaceTable(nsDependentString(aAtts[1]));
      }
    }
  }

  NS_NewXBLProtoImpl(mBinding, name, &mImplementation);
}

void nsXBLContentSink::ConstructField(const char16_t** aAtts,
                                      uint32_t aLineNumber) {
  const char16_t* name = nullptr;
  const char16_t* readonly = nullptr;

  RefPtr<nsAtom> prefix, localName;
  for (; *aAtts; aAtts += 2) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID != kNameSpaceID_None) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (localName == nsGkAtoms::name) {
      name = aAtts[1];
    } else if (localName == nsGkAtoms::readonly) {
      readonly = aAtts[1];
    }
  }

  if (name) {
    // All of our pointers are now filled in. Construct our field with all of
    // these parameters.
    mField = new nsXBLProtoImplField(name, readonly);
    mField->SetLineNumber(aLineNumber);
    AddField(mField);
  }
}

void nsXBLContentSink::ConstructProperty(const char16_t** aAtts,
                                         uint32_t aLineNumber) {
  const char16_t* name = nullptr;
  const char16_t* readonly = nullptr;
  const char16_t* onget = nullptr;
  const char16_t* onset = nullptr;
  bool exposeToUntrustedContent = false;

  RefPtr<nsAtom> prefix, localName;
  for (; *aAtts; aAtts += 2) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[0], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID != kNameSpaceID_None) {
      continue;
    }

    // Is this attribute one of the ones we care about?
    if (localName == nsGkAtoms::name) {
      name = aAtts[1];
    } else if (localName == nsGkAtoms::readonly) {
      readonly = aAtts[1];
    } else if (localName == nsGkAtoms::onget) {
      onget = aAtts[1];
    } else if (localName == nsGkAtoms::onset) {
      onset = aAtts[1];
    } else if (localName == nsGkAtoms::exposeToUntrustedContent &&
               nsDependentString(aAtts[1]).EqualsLiteral("true")) {
      exposeToUntrustedContent = true;
    }
  }

  if (name) {
    // All of our pointers are now filled in. Construct our property with all of
    // these parameters.
    mProperty =
        new nsXBLProtoImplProperty(name, onget, onset, readonly, aLineNumber);
    if (exposeToUntrustedContent) {
      mProperty->SetExposeToUntrustedContent(true);
    }
    AddMember(mProperty);
  }
}

void nsXBLContentSink::ConstructMethod(const char16_t** aAtts) {
  mMethod = nullptr;

  const char16_t* name = nullptr;
  const char16_t* expose = nullptr;
  if (FindValue(aAtts, nsGkAtoms::name, &name)) {
    mMethod = new nsXBLProtoImplMethod(name);
    if (FindValue(aAtts, nsGkAtoms::exposeToUntrustedContent, &expose) &&
        nsDependentString(expose).EqualsLiteral("true")) {
      mMethod->SetExposeToUntrustedContent(true);
    }
  }

  if (mMethod) {
    AddMember(mMethod);
  }
}

void nsXBLContentSink::ConstructParameter(const char16_t** aAtts) {
  if (!mMethod) return;

  const char16_t* name = nullptr;
  if (FindValue(aAtts, nsGkAtoms::name, &name)) {
    mMethod->AddParameter(nsDependentString(name));
  }
}

nsresult nsXBLContentSink::CreateElement(
    const char16_t** aAtts, uint32_t aAttsCount,
    mozilla::dom::NodeInfo* aNodeInfo, uint32_t aLineNumber,
    uint32_t aColumnNumber, nsIContent** aResult, bool* aAppendContent,
    FromParser aFromParser) {
#ifdef MOZ_XUL
  if (!aNodeInfo->NamespaceEquals(kNameSpaceID_XUL)) {
#endif
    return nsXMLContentSink::CreateElement(aAtts, aAttsCount, aNodeInfo,
                                           aLineNumber, aColumnNumber, aResult,
                                           aAppendContent, aFromParser);
#ifdef MOZ_XUL
  }

  // Note that this needs to match the code in
  // nsXBLPrototypeBinding::ReadContentNode.

  *aAppendContent = true;
  RefPtr<nsXULPrototypeElement> prototype = new nsXULPrototypeElement();

  prototype->mNodeInfo = aNodeInfo;

  AddAttributesToXULPrototype(aAtts, aAttsCount, prototype);

  Element* result;
  nsresult rv = nsXULElement::CreateFromPrototype(prototype, mDocument, false,
                                                  false, &result);
  *aResult = result;
  return rv;
#endif
}

nsresult nsXBLContentSink::AddAttributes(const char16_t** aAtts,
                                         Element* aElement) {
  if (aElement->IsXULElement())
    return NS_OK;  // Nothing to do, since the proto already has the attrs.

  return nsXMLContentSink::AddAttributes(aAtts, aElement);
}

#ifdef MOZ_XUL
nsresult nsXBLContentSink::AddAttributesToXULPrototype(
    const char16_t** aAtts, uint32_t aAttsCount,
    nsXULPrototypeElement* aElement) {
  // Add tag attributes to the element
  nsresult rv;

  // Create storage for the attributes
  nsXULPrototypeAttribute* attrs = nullptr;
  if (aAttsCount > 0) {
    attrs = new nsXULPrototypeAttribute[aAttsCount];
  }

  aElement->mAttributes = attrs;
  aElement->mNumAttributes = aAttsCount;

  // Copy the attributes into the prototype
  RefPtr<nsAtom> prefix, localName;

  uint32_t i;
  for (i = 0; i < aAttsCount; ++i) {
    int32_t nameSpaceID;
    nsContentUtils::SplitExpatName(aAtts[i * 2], getter_AddRefs(prefix),
                                   getter_AddRefs(localName), &nameSpaceID);

    if (nameSpaceID == kNameSpaceID_None) {
      attrs[i].mName.SetTo(localName);
    } else {
      RefPtr<NodeInfo> ni;
      ni = mNodeInfoManager->GetNodeInfo(localName, prefix, nameSpaceID,
                                         nsINode::ATTRIBUTE_NODE);
      attrs[i].mName.SetTo(ni);
    }

    rv = aElement->SetAttrAt(i, nsDependentString(aAtts[i * 2 + 1]),
                             mDocumentURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
#endif
