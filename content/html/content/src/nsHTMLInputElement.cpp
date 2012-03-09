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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Geoff Lankow <geoff@darktrojan.net>
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

#include "mozilla/Util.h"

#include "nsHTMLInputElement.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIRadioVisitor.h"
#include "nsIPhonetic.h"

#include "nsIControllers.h"
#include "nsFocusManager.h"
#include "nsPIDOMWindow.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIFormControl.h"
#include "nsIForm.h"
#include "nsFormSubmission.h"
#include "nsFormSubmissionConstants.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsITextControlFrame.h"
#include "nsIFrame.h"
#include "nsEventStates.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEditor.h"
#include "nsGUIEvent.h"
#include "nsIIOService.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"

#include "nsPresState.h"
#include "nsLayoutErrors.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsLinebreakConverter.h" //to strip out carriage returns
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsEventDispatcher.h"
#include "nsLayoutUtils.h"

#include "nsIDOMMutationEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsMutationEvent.h"
#include "nsEventListenerManager.h"

#include "nsRuleData.h"

// input type=radio
#include "nsIRadioGroupContainer.h"

// input type=file
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"
#include "nsIFilePicker.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIContentPrefService.h"
#include "nsIObserverService.h"
#include "nsIPopupWindowManager.h"
#include "nsGlobalWindow.h"

// input type=image
#include "nsImageLoadingContent.h"

#include "mozAutoDocUpdate.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsRadioVisitor.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/Util.h" // DebugOnly

#include "nsIIDNService.h"

using namespace mozilla;
using namespace mozilla::dom;

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

// First bits are needed for the control type.
#define NS_OUTER_ACTIVATE_EVENT   (1 << 9)
#define NS_ORIGINAL_CHECKED_VALUE (1 << 10)
#define NS_NO_CONTENT_DISPATCH    (1 << 11)
#define NS_ORIGINAL_INDETERMINATE_VALUE (1 << 12)
#define NS_CONTROL_TYPE(bits)  ((bits) & ~( \
  NS_OUTER_ACTIVATE_EVENT | NS_ORIGINAL_CHECKED_VALUE | NS_NO_CONTENT_DISPATCH | \
  NS_ORIGINAL_INDETERMINATE_VALUE))

// whether textfields should be selected once focused:
//  -1: no, 1: yes, 0: uninitialized
static PRInt32 gSelectTextFieldOnFocus;
UploadLastDir* nsHTMLInputElement::gUploadLastDir;

static const nsAttrValue::EnumTable kInputTypeTable[] = {
  { "button", NS_FORM_INPUT_BUTTON },
  { "checkbox", NS_FORM_INPUT_CHECKBOX },
  { "email", NS_FORM_INPUT_EMAIL },
  { "file", NS_FORM_INPUT_FILE },
  { "hidden", NS_FORM_INPUT_HIDDEN },
  { "reset", NS_FORM_INPUT_RESET },
  { "image", NS_FORM_INPUT_IMAGE },
  { "password", NS_FORM_INPUT_PASSWORD },
  { "radio", NS_FORM_INPUT_RADIO },
  { "search", NS_FORM_INPUT_SEARCH },
  { "submit", NS_FORM_INPUT_SUBMIT },
  { "tel", NS_FORM_INPUT_TEL },
  { "text", NS_FORM_INPUT_TEXT },
  { "url", NS_FORM_INPUT_URL },
  { 0 }
};

// Default type is 'text'.
static const nsAttrValue::EnumTable* kInputDefaultType = &kInputTypeTable[12];

static const PRUint8 NS_INPUT_AUTOCOMPLETE_OFF     = 0;
static const PRUint8 NS_INPUT_AUTOCOMPLETE_ON      = 1;
static const PRUint8 NS_INPUT_AUTOCOMPLETE_DEFAULT = 2;

static const nsAttrValue::EnumTable kInputAutocompleteTable[] = {
  { "", NS_INPUT_AUTOCOMPLETE_DEFAULT },
  { "on", NS_INPUT_AUTOCOMPLETE_ON },
  { "off", NS_INPUT_AUTOCOMPLETE_OFF },
  { 0 }
};

// Default autocomplete value is "".
static const nsAttrValue::EnumTable* kInputDefaultAutocomplete = &kInputAutocompleteTable[0];

#define NS_INPUT_ELEMENT_STATE_IID                 \
{ /* dc3b3d14-23e2-4479-b513-7b369343e3a0 */       \
  0xdc3b3d14,                                      \
  0x23e2,                                          \
  0x4479,                                          \
  {0xb5, 0x13, 0x7b, 0x36, 0x93, 0x43, 0xe3, 0xa0} \
}

class nsHTMLInputElementState MOZ_FINAL : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_INPUT_ELEMENT_STATE_IID)
    NS_DECL_ISUPPORTS

    bool IsCheckedSet() {
      return mCheckedSet;
    }

    bool GetChecked() {
      return mChecked;
    }

    void SetChecked(bool aChecked) {
      mChecked = aChecked;
      mCheckedSet = true;
    }

    const nsString& GetValue() {
      return mValue;
    }

    void SetValue(const nsAString &aValue) {
      mValue = aValue;
    }

    const nsCOMArray<nsIDOMFile>& GetFiles() {
      return mFiles;
    }

    void SetFiles(const nsCOMArray<nsIDOMFile> &aFiles) {
      mFiles.Clear();
      mFiles.AppendObjects(aFiles);
    }

    nsHTMLInputElementState()
      : mValue()
      , mChecked(false)
      , mCheckedSet(false)
    {};
 
  protected:
    nsString mValue;
    nsCOMArray<nsIDOMFile> mFiles;
    bool mChecked;
    bool mCheckedSet;
};

NS_IMPL_ISUPPORTS1(nsHTMLInputElementState, nsHTMLInputElementState)
NS_DEFINE_STATIC_IID_ACCESSOR(nsHTMLInputElementState, NS_INPUT_ELEMENT_STATE_IID)

class AsyncClickHandler : public nsRunnable {
public:
  AsyncClickHandler(nsHTMLInputElement* aInput)
   : mInput(aInput) {
    
    nsPIDOMWindow* win = aInput->OwnerDoc()->GetWindow();
    if (win) {
      mPopupControlState = win->GetPopupControlState();
    }
  };

  NS_IMETHOD Run();

protected:
  nsRefPtr<nsHTMLInputElement> mInput;
  PopupControlState mPopupControlState;
};

NS_IMETHODIMP
AsyncClickHandler::Run()
{
  // Get parent nsPIDOMWindow object.
  nsCOMPtr<nsIDocument> doc = mInput->OwnerDoc();

  nsPIDOMWindow* win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  // Check if page is allowed to open the popup
  if (mPopupControlState > openControlled) {
    nsCOMPtr<nsIPopupWindowManager> pm =
      do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);

    if (!pm) {
      return NS_OK;
    }

    PRUint32 permission;
    pm->TestPermission(doc->GetDocumentURI(), &permission);
    if (permission == nsIPopupWindowManager::DENY_POPUP) {
      nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(doc);
      nsGlobalWindow::FirePopupBlockedEvent(domDoc, win, nsnull, EmptyString(), EmptyString());
      return NS_OK;
    }
  }

  // Get Loc title
  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "FileUpload", title);

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  bool multi = mInput->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple);

  nsresult rv = filePicker->Init(win, title,
                                 multi
                                  ? static_cast<PRInt16>(nsIFilePicker::modeOpenMultiple)
                                  : static_cast<PRInt16>(nsIFilePicker::modeOpen));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mInput->HasAttr(kNameSpaceID_None, nsGkAtoms::accept)) {
    PRInt32 filters = mInput->GetFilterFromAccept();

    if (filters) {
      // We add |filterAll| to be sure the user always has a sane fallback.
      filePicker->AppendFilters(filters | nsIFilePicker::filterAll);

      // If the accept attribute asked for a filter, we need to make it default.
      // |filterAll| will always use index=0 so we need to set index=1 as the
      // current filter.
      filePicker->SetFilterIndex(1);
    } else {
      filePicker->AppendFilters(nsIFilePicker::filterAll);
    }
  } else {
    filePicker->AppendFilters(nsIFilePicker::filterAll);
  }

  // Set default directry and filename
  nsAutoString defaultName;

  const nsCOMArray<nsIDOMFile>& oldFiles = mInput->GetFiles();

  if (oldFiles.Count()) {
    nsString path;

    oldFiles[0]->GetMozFullPathInternal(path);

    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewLocalFile(path, false, getter_AddRefs(localFile));

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> parentFile;
      rv = localFile->GetParent(getter_AddRefs(parentFile));
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocalFile> parentLocalFile = do_QueryInterface(parentFile, &rv);
        if (parentLocalFile) {
          filePicker->SetDisplayDirectory(parentLocalFile);
        }
      }
    }

    // Unfortunately nsIFilePicker doesn't allow multiple files to be
    // default-selected, so only select something by default if exactly
    // one file was selected before.
    if (oldFiles.Count() == 1) {
      nsAutoString leafName;
      oldFiles[0]->GetName(leafName);
      if (!leafName.IsEmpty()) {
        filePicker->SetDefaultString(leafName);
      }
    }
  } else {
    // Attempt to retrieve the last used directory from the content pref service
    nsCOMPtr<nsILocalFile> localFile;
    nsHTMLInputElement::gUploadLastDir->FetchLastUsedDirectory(doc->GetDocumentURI(),
                                                               getter_AddRefs(localFile));
    if (!localFile) {
      // Default to "desktop" directory for each platform
      nsCOMPtr<nsIFile> homeDir;
      NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(homeDir));
      localFile = do_QueryInterface(homeDir);
    }
    filePicker->SetDisplayDirectory(localFile);
  }

  // Open dialog
  PRInt16 mode;
  rv = filePicker->Show(&mode);
  NS_ENSURE_SUCCESS(rv, rv);
  if (mode == nsIFilePicker::returnCancel) {
    return NS_OK;
  }

  // Collect new selected filenames
  nsCOMArray<nsIDOMFile> newFiles;
  if (multi) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    rv = filePicker->GetFiles(getter_AddRefs(iter));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsISupports> tmp;
    bool prefSaved = false;
    bool loop = true;
    while (NS_SUCCEEDED(iter->HasMoreElements(&loop)) && loop) {
      iter->GetNext(getter_AddRefs(tmp));
      nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(tmp);
      if (localFile) {
        nsString unicodePath;
        rv = localFile->GetPath(unicodePath);
        if (!unicodePath.IsEmpty()) {
          nsCOMPtr<nsIDOMFile> domFile =
            do_QueryObject(new nsDOMFileFile(localFile));
          newFiles.AppendObject(domFile);
        }
        if (!prefSaved) {
          // Store the last used directory using the content pref service
          nsHTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(doc->GetDocumentURI(),
                                                                     localFile);
          prefSaved = true;
        }
      }
    }
  }
  else {
    nsCOMPtr<nsILocalFile> localFile;
    rv = filePicker->GetFile(getter_AddRefs(localFile));
    if (localFile) {
      nsString unicodePath;
      rv = localFile->GetPath(unicodePath);
      if (!unicodePath.IsEmpty()) {
        nsCOMPtr<nsIDOMFile> domFile=
          do_QueryObject(new nsDOMFileFile(localFile));
        newFiles.AppendObject(domFile);
      }
      // Store the last used directory using the content pref service
      nsHTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(doc->GetDocumentURI(),
                                                                 localFile);
    }
  }

  // Set new selected files
  if (newFiles.Count()) {
    // The text control frame (if there is one) isn't going to send a change
    // event because it will think this is done by a script.
    // So, we can safely send one by ourself.
    mInput->SetFiles(newFiles, true);
    nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                                         static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                                         NS_LITERAL_STRING("change"), true,
                                         false);
  }

  return NS_OK;
}

#define CPS_PREF_NAME NS_LITERAL_STRING("browser.upload.lastDir")

NS_IMPL_ISUPPORTS2(UploadLastDir, nsIObserver, nsISupportsWeakReference)

void
nsHTMLInputElement::InitUploadLastDir() {
  gUploadLastDir = new UploadLastDir();
  NS_ADDREF(gUploadLastDir);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService && gUploadLastDir) {
    observerService->AddObserver(gUploadLastDir, "browser:purge-session-history", true);
  }
}

void 
nsHTMLInputElement::DestroyUploadLastDir() {
  NS_IF_RELEASE(gUploadLastDir);
}

nsresult
UploadLastDir::FetchLastUsedDirectory(nsIURI* aURI, nsILocalFile** aFile)
{
  NS_PRECONDITION(aURI, "aURI is null");
  NS_PRECONDITION(aFile, "aFile is null");
  // Attempt to get the CPS, if it's not present we'll just return
  nsCOMPtr<nsIContentPrefService> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  if (!contentPrefService)
    return NS_ERROR_NOT_AVAILABLE;
  nsCOMPtr<nsIWritableVariant> uri = do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  uri->SetAsISupports(aURI);

  // Get the last used directory, if it is stored
  bool hasPref;
  if (NS_SUCCEEDED(contentPrefService->HasPref(uri, CPS_PREF_NAME, &hasPref)) && hasPref) {
    nsCOMPtr<nsIVariant> pref;
    contentPrefService->GetPref(uri, CPS_PREF_NAME, nsnull, getter_AddRefs(pref));
    nsString prefStr;
    pref->GetAsAString(prefStr);

    nsCOMPtr<nsILocalFile> localFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    if (!localFile)
      return NS_ERROR_OUT_OF_MEMORY;
    localFile->InitWithPath(prefStr);
    localFile.forget(aFile);
  }
  return NS_OK;
}

nsresult
UploadLastDir::StoreLastUsedDirectory(nsIURI* aURI, nsILocalFile* aFile)
{
  NS_PRECONDITION(aURI, "aURI is null");
  NS_PRECONDITION(aFile, "aFile is null");
  nsCOMPtr<nsIFile> parentFile;
  aFile->GetParent(getter_AddRefs(parentFile));
  if (!parentFile) {
    return NS_OK;
  }
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(parentFile);

  // Attempt to get the CPS, if it's not present we'll just return
  nsCOMPtr<nsIContentPrefService> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  if (!contentPrefService)
    return NS_ERROR_NOT_AVAILABLE;
  nsCOMPtr<nsIWritableVariant> uri = do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
  uri->SetAsISupports(aURI);
 
  // Find the parent of aFile, and store it
  nsString unicodePath;
  parentFile->GetPath(unicodePath);
  if (unicodePath.IsEmpty()) // nothing to do
    return NS_OK;
  nsCOMPtr<nsIWritableVariant> prefValue = do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!prefValue)
    return NS_ERROR_OUT_OF_MEMORY;
  prefValue->SetAsAString(unicodePath);
  return contentPrefService->SetPref(uri, CPS_PREF_NAME, prefValue);
}

NS_IMETHODIMP
UploadLastDir::Observe(nsISupports *aSubject, char const *aTopic, PRUnichar const *aData)
{
  if (strcmp(aTopic, "browser:purge-session-history") == 0) {
    nsCOMPtr<nsIContentPrefService> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
    if (contentPrefService)
      contentPrefService->RemovePrefsByName(CPS_PREF_NAME);
  }
  return NS_OK;
}

#ifdef ACCESSIBILITY
//Helper method
static nsresult FireEventForAccessibility(nsIDOMHTMLInputElement* aTarget,
                                          nsPresContext* aPresContext,
                                          const nsAString& aEventType);
#endif

//
// construction, destruction
//

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Input)

nsHTMLInputElement::nsHTMLInputElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                       FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo)
  , mType(kInputDefaultType->value)
  , mDisabledChanged(false)
  , mValueChanged(false)
  , mCheckedChanged(false)
  , mChecked(false)
  , mHandlingSelectEvent(false)
  , mShouldInitChecked(false)
  , mParserCreating(aFromParser != NOT_FROM_PARSER)
  , mInInternalActivate(false)
  , mCheckedIsToggled(false)
  , mIndeterminate(false)
  , mInhibitRestoration(aFromParser & FROM_PARSER_FRAGMENT)
  , mCanShowValidUI(true)
  , mCanShowInvalidUI(true)
{
  mInputData.mState = new nsTextEditorState(this);
  NS_ADDREF(mInputData.mState);
  
  if (!gUploadLastDir)
    nsHTMLInputElement::InitUploadLastDir();

  // Set up our default state.  By default we're enabled (since we're
  // a control type that can be disabled but not actually disabled
  // right now), optional, and valid.  We are NOT readwrite by default
  // until someone calls UpdateEditableState on us, apparently!  Also
  // by default we don't have to show validity UI and so forth.
  AddStatesSilently(NS_EVENT_STATE_ENABLED |
                    NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
}

nsHTMLInputElement::~nsHTMLInputElement()
{
  DestroyImageLoadingContent();
  FreeData();
}

void
nsHTMLInputElement::FreeData()
{
  if (!IsSingleLineTextControl(false)) {
    nsMemory::Free(mInputData.mValue);
    mInputData.mValue = nsnull;
  } else {
    UnbindFromFrame(nsnull);
    NS_IF_RELEASE(mInputData.mState);
  }
}

nsTextEditorState*
nsHTMLInputElement::GetEditorState() const
{
  if (!IsSingleLineTextControl(false)) {
    return nsnull;
  }

  NS_ASSERTION(mInputData.mState,
    "Single line text controls need to have a state associated with them");

  return mInputData.mState;
}


// nsISupports

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLInputElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLInputElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mControllers)
  if (tmp->IsSingleLineTextControl(false)) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mInputData.mState, nsTextEditorState)
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mFiles)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFileList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsHTMLInputElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mFiles)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFileList);
  //XXX should unlink more?
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
                                                              
NS_IMPL_ADDREF_INHERITED(nsHTMLInputElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLInputElement, nsGenericElement) 


DOMCI_NODE_DATA(HTMLInputElement, nsHTMLInputElement)

// QueryInterface implementation for nsHTMLInputElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLInputElement)
  NS_HTML_CONTENT_INTERFACE_TABLE8(nsHTMLInputElement,
                                   nsIDOMHTMLInputElement,
                                   nsITextControlElement,
                                   nsIPhonetic,
                                   imgIDecoderObserver,
                                   nsIImageLoadingContent,
                                   imgIContainerObserver,
                                   nsIDOMNSEditableElement,
                                   nsIConstraintValidation)
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLInputElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLInputElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(nsHTMLInputElement)

// nsIDOMNode

nsresult
nsHTMLInputElement::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsRefPtr<nsHTMLInputElement> it =
    new nsHTMLInputElement(ni.forget(), NOT_FROM_PARSER);

  nsresult rv = CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (mType) {
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_URL:
      if (mValueChanged) {
        // We don't have our default value anymore.  Set our value on
        // the clone.
        nsAutoString value;
        GetValueInternal(value);
        // SetValueInternal handles setting the VALUE_CHANGED bit for us
        it->SetValueInternal(value, false, true);
      }
      break;
    case NS_FORM_INPUT_FILE:
      if (it->OwnerDoc()->IsStaticDocument()) {
        // We're going to be used in print preview.  Since the doc is static
        // we can just grab the pretty string and use it as wallpaper
        GetDisplayFileName(it->mStaticDocFileList);
      } else {
        it->mFiles.Clear();
        it->mFiles.AppendObjects(mFiles);
      }
      break;
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_CHECKBOX:
      if (mCheckedChanged) {
        // We no longer have our original checked state.  Set our
        // checked state on the clone.
        it->DoSetChecked(mChecked, false, true);
      }
      break;
    case NS_FORM_INPUT_IMAGE:
      if (it->OwnerDoc()->IsStaticDocument()) {
        CreateStaticImageClone(it);
      }
      break;
    default:
      break;
  }

  it.forget(aResult);
  return NS_OK;
}

nsresult
nsHTMLInputElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAttrValueOrString* aValue,
                                  bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    //
    // When name or type changes, radio should be removed from radio group.
    // (type changes are handled in the form itself currently)
    // If the parser is not done creating the radio, we also should not do it.
    //
    if ((aName == nsGkAtoms::name ||
         (aName == nsGkAtoms::type && !mForm)) &&
        mType == NS_FORM_INPUT_RADIO &&
        (mForm || !mParserCreating)) {
      WillRemoveFromRadioGroup();
    } else if (aNotify && aName == nsGkAtoms::src &&
               mType == NS_FORM_INPUT_IMAGE) {
      if (aValue) {
        LoadImage(aValue->String(), true, aNotify);
      } else {
        // Null value means the attr got unset; drop the image
        CancelImageRequests(aNotify);
      }
    } else if (aNotify && aName == nsGkAtoms::disabled) {
      mDisabledChanged = true;
    }
  }

  return nsGenericHTMLFormElement::BeforeSetAttr(aNameSpaceID, aName,
                                                 aValue, aNotify);
}

nsresult
nsHTMLInputElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAttrValue* aValue, bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    //
    // When name or type changes, radio should be added to radio group.
    // (type changes are handled in the form itself currently)
    // If the parser is not done creating the radio, we also should not do it.
    //
    if ((aName == nsGkAtoms::name ||
         (aName == nsGkAtoms::type && !mForm)) &&
        mType == NS_FORM_INPUT_RADIO &&
        (mForm || !mParserCreating)) {
      AddedToRadioGroup();
      UpdateValueMissingValidityStateForRadio(false);
    }

    // If @value is changed and BF_VALUE_CHANGED is false, @value is the value
    // of the element so, if the value of the element is different than @value,
    // we have to re-set it. This is only the case when GetValueMode() returns
    // VALUE_MODE_VALUE.
    if (aName == nsGkAtoms::value &&
        !mValueChanged && GetValueMode() == VALUE_MODE_VALUE) {
      SetDefaultValueAsValue();
    }

    //
    // Checked must be set no matter what type of control it is, since
    // mChecked must reflect the new value
    if (aName == nsGkAtoms::checked && !mCheckedChanged) {
      // Delay setting checked if the parser is creating this element (wait
      // until everything is set)
      if (mParserCreating) {
        mShouldInitChecked = true;
      } else {
        DoSetChecked(DefaultChecked(), true, true);
        SetCheckedChanged(false);
      }
    }

    if (aName == nsGkAtoms::type) {
      if (!aValue) {
        // We're now a text input.  Note that we have to handle this manually,
        // since removing an attribute (which is what happened, since aValue is
        // null) doesn't call ParseAttribute.
        HandleTypeChange(kInputDefaultType->value);
      }

      UpdateBarredFromConstraintValidation();

      if (mType != NS_FORM_INPUT_IMAGE) {
        // We're no longer an image input.  Cancel our image requests, if we have
        // any.  Note that doing this when we already weren't an image is ok --
        // just does nothing.
        CancelImageRequests(aNotify);
      } else if (aNotify) {
        // We just got switched to be an image input; we should see
        // whether we have an image to load;
        nsAutoString src;
        if (GetAttr(kNameSpaceID_None, nsGkAtoms::src, src)) {
          LoadImage(src, false, aNotify);
        }
      }
    }

    if (mType == NS_FORM_INPUT_RADIO && aName == nsGkAtoms::required) {
      nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();

      if (container) {
        nsAutoString name;
        GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
        container->RadioRequiredChanged(name, this);
      }
    }

    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }
    } else if (MaxLengthApplies() && aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
    } else if (aName == nsGkAtoms::pattern) {
      UpdatePatternMismatchValidityState();
    } else if (aName == nsGkAtoms::multiple) {
      UpdateTypeMismatchValidityState();
    }

    UpdateEditableState(aNotify);
    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElement::AfterSetAttr(aNameSpaceID, aName,
                                                aValue, aNotify);
}

// nsIDOMHTMLInputElement

NS_IMETHODIMP
nsHTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

NS_IMPL_STRING_ATTR(nsHTMLInputElement, DefaultValue, value)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, DefaultChecked, checked)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Accept, accept)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Autocomplete, autocomplete,
                                kInputDefaultAutocomplete->tag)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Autofocus, autofocus)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled)
NS_IMPL_ACTION_ATTR(nsHTMLInputElement, FormAction, formaction)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, FormEnctype, formenctype,
                                kFormDefaultEnctype->tag)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, FormMethod, formmethod,
                                kFormDefaultMethod->tag)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, FormNoValidate, formnovalidate)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, FormTarget, formtarget)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Multiple, multiple)
NS_IMPL_NON_NEGATIVE_INT_ATTR(nsHTMLInputElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Name, name)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, ReadOnly, readonly)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Required, required)
NS_IMPL_URI_ATTR(nsHTMLInputElement, Src, src)
NS_IMPL_INT_ATTR(nsHTMLInputElement, TabIndex, tabindex)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(nsHTMLInputElement, Size, size, DEFAULT_COLS)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Pattern, pattern)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Placeholder, placeholder)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Type, type,
                                kInputDefaultType->tag)

NS_IMETHODIMP
nsHTMLInputElement::GetIndeterminate(bool* aValue)
{
  *aValue = mIndeterminate;
  return NS_OK;
}

nsresult
nsHTMLInputElement::SetIndeterminateInternal(bool aValue,
                                             bool aShouldInvalidate)
{
  mIndeterminate = aValue;

  if (aShouldInvalidate) {
    // Repaint the frame
    nsIFrame* frame = GetPrimaryFrame();
    if (frame)
      frame->InvalidateFrameSubtree();
  }

  UpdateState(true);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetIndeterminate(bool aValue)
{
  return SetIndeterminateInternal(aValue, true);
}

NS_IMETHODIMP
nsHTMLInputElement::GetValue(nsAString& aValue)
{
  return GetValueInternal(aValue);
}

nsresult
nsHTMLInputElement::GetValueInternal(nsAString& aValue) const
{
  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      mInputData.mState->GetValue(aValue, true);
      return NS_OK;

    case VALUE_MODE_FILENAME:
      if (nsContentUtils::CallerHasUniversalXPConnect()) {
        if (mFiles.Count()) {
          return mFiles[0]->GetMozFullPath(aValue);
        }
        else {
          aValue.Truncate();
        }
      } else {
        // Just return the leaf name
        if (mFiles.Count() == 0 || NS_FAILED(mFiles[0]->GetName(aValue))) {
          aValue.Truncate();
        }
      }

      return NS_OK;

    case VALUE_MODE_DEFAULT:
      // Treat defaultValue as value.
      GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue);
      return NS_OK;

    case VALUE_MODE_DEFAULT_ON:
      // Treat default value as value and returns "on" if no value.
      if (!GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue)) {
        aValue.AssignLiteral("on");
      }
      return NS_OK;
  }

  // This return statement is required for some compilers.
  return NS_OK;
}

bool
nsHTMLInputElement::IsValueEmpty() const
{
  nsAutoString value;
  GetValueInternal(value);

  return value.IsEmpty();
}

NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsAString& aValue)
{
  // check security.  Note that setting the value to the empty string is always
  // OK and gives pages a way to clear a file input if necessary.
  if (mType == NS_FORM_INPUT_FILE) {
    if (!aValue.IsEmpty()) {
      if (!nsContentUtils::CallerHasUniversalXPConnect()) {
        // setting the value of a "FILE" input widget requires the
        // UniversalXPConnect privilege
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      const PRUnichar *name = PromiseFlatString(aValue).get();
      return MozSetFileNameArray(&name, 1);
    }
    else {
      ClearFiles(true);
    }
  }
  else {
    SetValueInternal(aValue, false, true);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetList(nsIDOMHTMLElement** aValue)
{
  *aValue = nsnull;

  nsAutoString dataListId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::list, dataListId);
  if (dataListId.IsEmpty()) {
    return NS_OK;
  }

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return NS_OK;
  }

  Element* element = doc->GetElementById(dataListId);
  if (!element || !element->IsHTML(nsGkAtoms::datalist)) {
    return NS_OK;
  }

  CallQueryInterface(element, aValue);
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::MozGetFileNameArray(PRUint32 *aLength, PRUnichar ***aFileNames)
{
  if (!nsContentUtils::CallerHasUniversalXPConnect()) {
    // Since this function returns full paths it's important that normal pages
    // can't call it.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  *aLength = mFiles.Count();
  PRUnichar **ret =
    static_cast<PRUnichar **>(NS_Alloc(mFiles.Count() * sizeof(PRUnichar*)));
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (PRInt32 i = 0; i < mFiles.Count(); i++) {
    nsString str;
    mFiles[i]->GetMozFullPathInternal(str);
    ret[i] = NS_strdup(str.get());
  }

  *aFileNames = ret;

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::MozSetFileNameArray(const PRUnichar **aFileNames, PRUint32 aLength)
{
  if (!nsContentUtils::CallerHasUniversalXPConnect()) {
    // setting the value of a "FILE" input widget requires the
    // UniversalXPConnect privilege
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsCOMArray<nsIDOMFile> files;
  for (PRUint32 i = 0; i < aLength; ++i) {
    nsCOMPtr<nsIFile> file;
    if (StringBeginsWith(nsDependentString(aFileNames[i]),
                         NS_LITERAL_STRING("file:"),
                         nsASCIICaseInsensitiveStringComparator())) {
      // Converts the URL string into the corresponding nsIFile if possible
      // A local file will be created if the URL string begins with file://
      NS_GetFileFromURLSpec(NS_ConvertUTF16toUTF8(aFileNames[i]),
                            getter_AddRefs(file));
    }

    if (!file) {
      // this is no "file://", try as local file
      nsCOMPtr<nsILocalFile> localFile;
      NS_NewLocalFile(nsDependentString(aFileNames[i]),
                      false, getter_AddRefs(localFile));
      file = do_QueryInterface(localFile);
    }

    if (file) {
      nsCOMPtr<nsIDOMFile> domFile = new nsDOMFileFile(file);
      files.AppendObject(domFile);
    } else {
      continue; // Not much we can do if the file doesn't exist
    }

  }

  SetFiles(files, true);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::MozIsTextField(bool aExcludePassword, bool* aResult)
{
  *aResult = IsSingleLineTextControl(aExcludePassword);

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::SetUserInput(const nsAString& aValue)
{
  if (!nsContentUtils::IsCallerTrustedForWrite()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (mType == NS_FORM_INPUT_FILE)
  {
    const PRUnichar* name = PromiseFlatString(aValue).get();
    return MozSetFileNameArray(&name, 1);
  } else {
    SetValueInternal(aValue, true, true);
  }

  return nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(this),
                                              NS_LITERAL_STRING("input"), true,
                                              true);
}

NS_IMETHODIMP_(nsIEditor*)
nsHTMLInputElement::GetTextEditor()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetEditor();
  }
  return nsnull;
}

NS_IMETHODIMP_(nsISelectionController*)
nsHTMLInputElement::GetSelectionController()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetSelectionController();
  }
  return nsnull;
}

nsFrameSelection*
nsHTMLInputElement::GetConstFrameSelection()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetConstFrameSelection();
  }
  return nsnull;
}

NS_IMETHODIMP
nsHTMLInputElement::BindToFrame(nsTextControlFrame* aFrame)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->BindToFrame(aFrame);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  nsTextEditorState *state = GetEditorState();
  if (state && aFrame) {
    state->UnbindFromFrame(aFrame);
  }
}

NS_IMETHODIMP
nsHTMLInputElement::CreateEditor()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->PrepareEditor();
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLInputElement::GetRootEditorNode()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetRootNode();
  }
  return nsnull;
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLInputElement::CreatePlaceholderNode()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    NS_ENSURE_SUCCESS(state->CreatePlaceholderNode(), nsnull);
    return state->GetPlaceholderNode();
  }
  return nsnull;
}

NS_IMETHODIMP_(nsIContent*)
nsHTMLInputElement::GetPlaceholderNode()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetPlaceholderNode();
  }
  return nsnull;
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::UpdatePlaceholderText(bool aNotify)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->UpdatePlaceholderText(aNotify);
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::SetPlaceholderClass(bool aVisible, bool aNotify)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->SetPlaceholderClass(aVisible, aNotify);
  }
}

void
nsHTMLInputElement::GetDisplayFileName(nsAString& aValue) const
{
  if (OwnerDoc()->IsStaticDocument()) {
    aValue = mStaticDocFileList;
    return;
  }

  aValue.Truncate();
  for (PRInt32 i = 0; i < mFiles.Count(); ++i) {
    nsString str;
    mFiles[i]->GetMozFullPathInternal(str);
    if (i == 0) {
      aValue.Append(str);
    }
    else {
      aValue.Append(NS_LITERAL_STRING(", ") + str);
    }
  }
}

void
nsHTMLInputElement::SetFiles(const nsCOMArray<nsIDOMFile>& aFiles,
                             bool aSetValueChanged)
{
  mFiles.Clear();
  mFiles.AppendObjects(aFiles);

  AfterSetFiles(aSetValueChanged);
}

void
nsHTMLInputElement::SetFiles(nsIDOMFileList* aFiles,
                             bool aSetValueChanged)
{
  mFiles.Clear();

  if (aFiles) {
    PRUint32 listLength;
    aFiles->GetLength(&listLength);
    for (PRUint32 i = 0; i < listLength; i++) {
      nsCOMPtr<nsIDOMFile> file;
      aFiles->Item(i, getter_AddRefs(file));
      mFiles.AppendObject(file);
    }
  }

  AfterSetFiles(aSetValueChanged);
}

void
nsHTMLInputElement::AfterSetFiles(bool aSetValueChanged)
{
  // No need to flush here, if there's no frame at this point we
  // don't need to force creation of one just to tell it about this
  // new value.  We just want the display to update as needed.
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(false);
  if (formControlFrame) {
    nsAutoString readableValue;
    GetDisplayFileName(readableValue);
    formControlFrame->SetFormProperty(nsGkAtoms::value, readableValue);
  }

  UpdateFileList();

  if (aSetValueChanged) {
    SetValueChanged(true);
  }

  UpdateAllValidityStates(true);
}

const nsCOMArray<nsIDOMFile>&
nsHTMLInputElement::GetFiles() const
{
  return mFiles;
}

nsresult
nsHTMLInputElement::UpdateFileList()
{
  if (mFileList) {
    mFileList->Clear();

    const nsCOMArray<nsIDOMFile>& files = GetFiles();
    for (PRInt32 i = 0; i < files.Count(); ++i) {
      if (!mFileList->Append(files[i])) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult
nsHTMLInputElement::SetValueInternal(const nsAString& aValue,
                                     bool aUserInput,
                                     bool aSetValueChanged)
{
  NS_PRECONDITION(GetValueMode() != VALUE_MODE_FILENAME,
                  "Don't call SetValueInternal for file inputs");

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
    {
      // At the moment, only single line text control have to sanitize their value
      // Because we have to create a new string for that, we should prevent doing
      // it if it's useless.
      nsAutoString value(aValue);

      if (!mParserCreating) {
        SanitizeValue(value);
      }

      if (aSetValueChanged) {
        SetValueChanged(true);
      }

      mInputData.mState->SetValue(value, aUserInput);

      // This call might be useless in some situations because if the element is
      // a single line text control, nsTextEditorState::SetValue will call
      // nsHTMLInputElement::OnValueChanged which is going to call UpdateState()
      // if the element is focused. This bug 665547.
      if (PlaceholderApplies() &&
          HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)) {
        UpdateState(true);
      }

      return NS_OK;
    }

    case VALUE_MODE_DEFAULT:
    case VALUE_MODE_DEFAULT_ON:
      // If the value of a hidden input was changed, we mark it changed so that we
      // will know we need to save / restore the value.  Yes, we are overloading
      // the meaning of ValueChanged just a teensy bit to save a measly byte of
      // storage space in nsHTMLInputElement.  Yes, you are free to make a new flag,
      // NEED_TO_SAVE_VALUE, at such time as mBitField becomes a 16-bit value.
      if (mType == NS_FORM_INPUT_HIDDEN) {
        SetValueChanged(true);
      }

      // Treat value == defaultValue for other input elements.
      return nsGenericHTMLFormElement::SetAttr(kNameSpaceID_None,
                                               nsGkAtoms::value, aValue,
                                               true);

    case VALUE_MODE_FILENAME:
      return NS_ERROR_UNEXPECTED;
  }

  // This return statement is required for some compilers.
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetValueChanged(bool aValueChanged)
{
  bool valueChangedBefore = mValueChanged;

  mValueChanged = aValueChanged;

  if (valueChangedBefore != aValueChanged) {
    UpdateState(true);
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(bool* aChecked)
{
  *aChecked = mChecked;
  return NS_OK;
}

void
nsHTMLInputElement::SetCheckedChanged(bool aCheckedChanged)
{
  DoSetCheckedChanged(aCheckedChanged, true);
}

void
nsHTMLInputElement::DoSetCheckedChanged(bool aCheckedChanged,
                                        bool aNotify)
{
  if (mType == NS_FORM_INPUT_RADIO) {
    if (mCheckedChanged != aCheckedChanged) {
      nsCOMPtr<nsIRadioVisitor> visitor =
        new nsRadioSetCheckedChangedVisitor(aCheckedChanged);
      VisitGroup(visitor, aNotify);
    }
  } else {
    SetCheckedChangedInternal(aCheckedChanged);
  }
}

void
nsHTMLInputElement::SetCheckedChangedInternal(bool aCheckedChanged)
{
  bool checkedChangedBefore = mCheckedChanged;

  mCheckedChanged = aCheckedChanged;

  // This method can't be called when we are not authorized to notify
  // so we do not need a aNotify parameter.
  if (checkedChangedBefore != aCheckedChanged) {
    UpdateState(true);
  }
}

NS_IMETHODIMP
nsHTMLInputElement::SetChecked(bool aChecked)
{
  return DoSetChecked(aChecked, true, true);
}

nsresult
nsHTMLInputElement::DoSetChecked(bool aChecked, bool aNotify,
                                 bool aSetValueChanged)
{
  // If the user or JS attempts to set checked, whether it actually changes the
  // value or not, we say the value was changed so that defaultValue don't
  // affect it no more.
  if (aSetValueChanged) {
    DoSetCheckedChanged(true, aNotify);
  }

  // Don't do anything if we're not changing whether it's checked (it would
  // screw up state actually, especially when you are setting radio button to
  // false)
  if (mChecked == aChecked) {
    return NS_OK;
  }

  // Set checked
  if (mType != NS_FORM_INPUT_RADIO) {
    SetCheckedInternal(aChecked, aNotify);
    return NS_OK;
  }

  // For radio button, we need to do some extra fun stuff
  if (aChecked) {
    return RadioSetChecked(aNotify);
  }

  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    container->SetCurrentRadioButton(name, nsnull);
  }
  // SetCheckedInternal is going to ask all radios to update their
  // validity state. We have to be sure the radio group container knows
  // the currently selected radio.
  SetCheckedInternal(false, aNotify);
  return NS_OK;
}

nsresult
nsHTMLInputElement::RadioSetChecked(bool aNotify)
{
  // Find the selected radio button so we can deselect it
  nsCOMPtr<nsIDOMHTMLInputElement> currentlySelected = GetSelectedRadioButton();

  // Deselect the currently selected radio button
  if (currentlySelected) {
    // Pass true for the aNotify parameter since the currently selected
    // button is already in the document.
    static_cast<nsHTMLInputElement*>(currentlySelected.get())
      ->SetCheckedInternal(false, true);
  }

  // Let the group know that we are now the One True Radio Button
  nsresult rv = NS_OK;
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    rv = container->SetCurrentRadioButton(name, this);
  }

  // SetCheckedInternal is going to ask all radios to update their
  // validity state. We have to be sure the radio group container knows
  // the currently selected radio.
  if (NS_SUCCEEDED(rv)) {
    SetCheckedInternal(true, aNotify);
  }

  return rv;
}

nsIRadioGroupContainer*
nsHTMLInputElement::GetRadioGroupContainer() const
{
  NS_ASSERTION(mType == NS_FORM_INPUT_RADIO,
               "GetRadioGroupContainer should only be called when type='radio'");

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  if (name.IsEmpty()) {
    return nsnull;
  }

  if (mForm) {
    return mForm;
  }

  return static_cast<nsDocument*>(GetCurrentDoc());
}

already_AddRefed<nsIDOMHTMLInputElement>
nsHTMLInputElement::GetSelectedRadioButton()
{
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (!container) {
    return nsnull;
  }

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  nsCOMPtr<nsIDOMHTMLInputElement> selected;
  container->GetCurrentRadioButton(name, getter_AddRefs(selected));
  return selected.forget();
}

nsresult
nsHTMLInputElement::MaybeSubmitForm(nsPresContext* aPresContext)
{
  if (!mForm) {
    // Nothing to do here.
    return NS_OK;
  }
  
  nsCOMPtr<nsIPresShell> shell = aPresContext->GetPresShell();
  if (!shell) {
    return NS_OK;
  }

  // Get the default submit element
  nsIFormControl* submitControl = mForm->GetDefaultSubmitElement();
  if (submitControl) {
    nsCOMPtr<nsIContent> submitContent = do_QueryInterface(submitControl);
    NS_ASSERTION(submitContent, "Form control not implementing nsIContent?!");
    // Fire the button's onclick handler and let the button handle
    // submitting the form.
    nsMouseEvent event(true, NS_MOUSE_CLICK, nsnull, nsMouseEvent::eReal);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(submitContent, &event, &status);
  } else if (mForm->HasSingleTextControl() &&
             (mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate) ||
              mForm->CheckValidFormSubmission())) {
    // TODO: removing this code and have the submit event sent by the form,
    // bug 592124.
    // If there's only one text control, just submit the form
    // Hold strong ref across the event
    nsRefPtr<nsHTMLFormElement> form = mForm;
    nsFormEvent event(true, NS_FORM_SUBMIT);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(mForm, &event, &status);
  }

  return NS_OK;
}

void
nsHTMLInputElement::SetCheckedInternal(bool aChecked, bool aNotify)
{
  // Set the value
  mChecked = aChecked;

  // Notify the frame
  if (mType == NS_FORM_INPUT_CHECKBOX || mType == NS_FORM_INPUT_RADIO) {
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
    }
  }

  UpdateAllValidityStates(aNotify);

  // Notify the document that the CSS :checked pseudoclass for this element
  // has changed state.
  UpdateState(aNotify);
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  if (mType != NS_FORM_INPUT_FILE) {
    return nsGenericHTMLElement::Focus();
  }

  // For file inputs, focus the button instead.
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    for (nsIFrame* childFrame = frame->GetFirstPrincipalChild();
         childFrame;
         childFrame = childFrame->GetNextSibling()) {
      // See if the child is a button control.
      nsCOMPtr<nsIFormControl> formCtrl =
        do_QueryInterface(childFrame->GetContent());
      if (formCtrl && formCtrl->GetType() == NS_FORM_INPUT_BUTTON) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(formCtrl);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm && element) {
          fm->SetFocus(element, 0);
        }
        break;
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  if (!IsSingleLineTextControl(false)) {
    return NS_OK;
  }

  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  FocusTristate state = FocusState();
  if (state == eUnfocusable) {
    return NS_OK;
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();

  nsRefPtr<nsPresContext> presContext = GetPresContext();
  if (state == eInactiveWindow) {
    if (fm)
      fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);
    SelectAll(presContext);
    return NS_OK;
  }

  if (DispatchSelectEvent(presContext) && fm) {
    fm->SetFocus(this, nsIFocusManager::FLAG_NOSCROLL);

    // ensure that the element is actually focused
    nsCOMPtr<nsIDOMElement> focusedElement;
    fm->GetFocusedElement(getter_AddRefs(focusedElement));
    if (SameCOMIdentity(static_cast<nsIDOMNode *>(this), focusedElement)) {
      // Now Select all the text!
      SelectAll(presContext);
    }
  }

  return NS_OK;
}

bool
nsHTMLInputElement::DispatchSelectEvent(nsPresContext* aPresContext)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  // If already handling select event, don't dispatch a second.
  if (!mHandlingSelectEvent) {
    nsEvent event(nsContentUtils::IsCallerChrome(), NS_FORM_SELECTED);

    mHandlingSelectEvent = true;
    nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                aPresContext, &event, nsnull, &status);
    mHandlingSelectEvent = false;
  }

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  return (status == nsEventStatus_eIgnore);
}
    
void
nsHTMLInputElement::SelectAll(nsPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  if (mType == NS_FORM_INPUT_FILE)
    FireAsyncClickHandler();

  return nsGenericHTMLElement::Click();
}

NS_IMETHODIMP
nsHTMLInputElement::FireAsyncClickHandler()
{
  nsCOMPtr<nsIRunnable> event = new AsyncClickHandler(this);
  return NS_DispatchToMainThread(event);
}

bool
nsHTMLInputElement::NeedToInitializeEditorForEvent(nsEventChainPreVisitor& aVisitor) const
{
  // We only need to initialize the editor for single line input controls because they
  // are lazily initialized.  We don't need to initialize the control for
  // certain types of events, because we know that those events are safe to be
  // handled without the editor being initialized.  These events include:
  // mousein/move/out, and DOM mutation events.
  if (!IsSingleLineTextControl(false) ||
      aVisitor.mEvent->eventStructType == NS_MUTATION_EVENT) {
    return false;
  }

  switch (aVisitor.mEvent->message) {
  case NS_MOUSE_MOVE:
  case NS_MOUSE_ENTER:
  case NS_MOUSE_EXIT:
  case NS_MOUSE_ENTER_SYNTH:
  case NS_MOUSE_EXIT_SYNTH:
    return false;
  default:
    return true;
  }
}

nsresult
nsHTMLInputElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = false;
  if (IsElementDisabledForEvents(aVisitor.mEvent->message, GetPrimaryFrame())) {
    return NS_OK;
  }

  // Initialize the editor if needed.
  if (NeedToInitializeEditorForEvent(aVisitor)) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(GetPrimaryFrame());
    if (textControlFrame)
      textControlFrame->EnsureEditorInitialized();
  }

  //FIXME Allow submission etc. also when there is no prescontext, Bug 329509.
  if (!aVisitor.mPresContext) {
    return nsGenericHTMLElement::PreHandleEvent(aVisitor);
  }
  //
  // Web pages expect the value of a radio button or checkbox to be set
  // *before* onclick and DOMActivate fire, and they expect that if they set
  // the value explicitly during onclick or DOMActivate it will not be toggled
  // or any such nonsense.
  // In order to support that (bug 57137 and 58460 are examples) we toggle
  // the checked attribute *first*, and then fire onclick.  If the user
  // returns false, we reset the control to the old checked value.  Otherwise,
  // we dispatch DOMActivate.  If DOMActivate is cancelled, we also reset
  // the control to the old checked value.  We need to keep track of whether
  // we've already toggled the state from onclick since the user could
  // explicitly dispatch DOMActivate on the element.
  //
  // This is a compatibility hack.
  //

  // Track whether we're in the outermost Dispatch invocation that will
  // cause activation of the input.  That is, if we're a click event, or a
  // DOMActivate that was dispatched directly, this will be set, but if we're
  // a DOMActivate dispatched from click handling, it will not be set.
  bool outerActivateEvent =
    (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) ||
     (aVisitor.mEvent->message == NS_UI_ACTIVATE && !mInInternalActivate));

  if (outerActivateEvent) {
    aVisitor.mItemFlags |= NS_OUTER_ACTIVATE_EVENT;
  }

  bool originalCheckedValue = false;

  if (outerActivateEvent) {
    mCheckedIsToggled = false;

    switch(mType) {
      case NS_FORM_INPUT_CHECKBOX:
        {
          if (mIndeterminate) {
            // indeterminate is always set to FALSE when the checkbox is toggled
            SetIndeterminateInternal(false, false);
            aVisitor.mItemFlags |= NS_ORIGINAL_INDETERMINATE_VALUE;
          }

          GetChecked(&originalCheckedValue);
          DoSetChecked(!originalCheckedValue, true, true);
          mCheckedIsToggled = true;
        }
        break;

      case NS_FORM_INPUT_RADIO:
        {
          nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton = GetSelectedRadioButton();
          aVisitor.mItemData = selectedRadioButton;

          originalCheckedValue = mChecked;
          if (!originalCheckedValue) {
            DoSetChecked(true, true, true);
            mCheckedIsToggled = true;
          }
        }
        break;

      case NS_FORM_INPUT_SUBMIT:
      case NS_FORM_INPUT_IMAGE:
        if(mForm) {
          // tell the form that we are about to enter a click handler.
          // that means that if there are scripted submissions, the
          // latest one will be deferred until after the exit point of the handler. 
          mForm->OnSubmitClickBegin(this);
        }
        break;

      default:
        break;
    }
  }

  if (originalCheckedValue) {
    aVisitor.mItemFlags |= NS_ORIGINAL_CHECKED_VALUE;
  }

  // If NS_EVENT_FLAG_NO_CONTENT_DISPATCH is set we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow 
  // middle clicks to go to text fields anyway.
  if (aVisitor.mEvent->flags & NS_EVENT_FLAG_NO_CONTENT_DISPATCH) {
    aVisitor.mItemFlags |= NS_NO_CONTENT_DISPATCH;
  }
  if (IsSingleLineTextControl(false) &&
      aVisitor.mEvent->message == NS_MOUSE_CLICK &&
      aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
      static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
        nsMouseEvent::eMiddleButton) {
    aVisitor.mEvent->flags &= ~NS_EVENT_FLAG_NO_CONTENT_DISPATCH;
  }

  // We must cache type because mType may change during JS event (bug 2369)
  aVisitor.mItemFlags |= mType;

  // Fire onchange (if necessary), before we do the blur, bug 357684.
  if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    nsIFrame* primaryFrame = GetPrimaryFrame();
    if (primaryFrame) {
      nsITextControlFrame* textFrame = do_QueryFrame(primaryFrame);
      if (textFrame) {
        textFrame->CheckFireOnChange();
      }
    }
  }

  return nsGenericHTMLFormElement::PreHandleEvent(aVisitor);
}

static bool
SelectTextFieldOnFocus()
{
  if (!gSelectTextFieldOnFocus) {
    PRInt32 selectTextfieldsOnKeyFocus = -1;
    nsresult rv =
      LookAndFeel::GetInt(LookAndFeel::eIntID_SelectTextfieldsOnKeyFocus,
                          &selectTextfieldsOnKeyFocus);
    if (NS_FAILED(rv)) {
      gSelectTextFieldOnFocus = -1;
    } else {
      gSelectTextFieldOnFocus = selectTextfieldsOnKeyFocus != 0 ? 1 : -1;
    }
  }

  return gSelectTextFieldOnFocus == 1;
}

nsresult
nsHTMLInputElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (!aVisitor.mPresContext) {
    return NS_OK;
  }

  if (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
      aVisitor.mEvent->message == NS_BLUR_CONTENT) {

    UpdateValidityUIBits(aVisitor.mEvent->message == NS_FOCUS_CONTENT);

    UpdateState(true);
  }

  // ignore the activate event fired by the "Browse..." button
  // (file input controls fire their own) (bug 500885)
  if (mType == NS_FORM_INPUT_FILE) {
    nsCOMPtr<nsIContent> maybeButton =
      do_QueryInterface(aVisitor.mEvent->originalTarget);
    if (maybeButton &&
      maybeButton->IsRootOfNativeAnonymousSubtree() &&
      maybeButton->AttrValueIs(kNameSpaceID_None,
                               nsGkAtoms::type,
                               nsGkAtoms::button,
                               eCaseMatters)) {
        return NS_OK;
    }
  }

  nsresult rv = NS_OK;
  bool outerActivateEvent = !!(aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT);
  bool originalCheckedValue =
    !!(aVisitor.mItemFlags & NS_ORIGINAL_CHECKED_VALUE);
  bool noContentDispatch = !!(aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH);
  PRUint8 oldType = NS_CONTROL_TYPE(aVisitor.mItemFlags);
  // Ideally we would make the default action for click and space just dispatch
  // DOMActivate, and the default action for DOMActivate flip the checkbox/
  // radio state and fire onchange.  However, for backwards compatibility, we
  // need to flip the state before firing click, and we need to fire click
  // when space is pressed.  So, we just nest the firing of DOMActivate inside
  // the click event handling, and allow cancellation of DOMActivate to cancel
  // the click.
  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault &&
      !IsSingleLineTextControl(true) &&
      NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
    nsUIEvent actEvent(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_UI_ACTIVATE, 1);

    nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
    if (shell) {
      nsEventStatus status = nsEventStatus_eIgnore;
      mInInternalActivate = true;
      rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
      mInInternalActivate = false;

      // If activate is cancelled, we must do the same as when click is
      // cancelled (revert the checkbox to its original value).
      if (status == nsEventStatus_eConsumeNoDefault)
        aVisitor.mEventStatus = status;
    }
  }

  if (outerActivateEvent) {
    switch(oldType) {
      case NS_FORM_INPUT_SUBMIT:
      case NS_FORM_INPUT_IMAGE:
        if(mForm) {
          // tell the form that we are about to exit a click handler
          // so the form knows not to defer subsequent submissions
          // the pending ones that were created during the handler
          // will be flushed or forgoten.
          mForm->OnSubmitClickEnd();
        }
        break;
    }
  }

  // Reset the flag for other content besides this text field
  aVisitor.mEvent->flags |=
    noContentDispatch ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  // now check to see if the event was "cancelled"
  if (mCheckedIsToggled && outerActivateEvent) {
    if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
      // if it was cancelled and a radio button, then set the old
      // selected btn to TRUE. if it is a checkbox then set it to its
      // original value
      if (oldType == NS_FORM_INPUT_RADIO) {
        nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton =
          do_QueryInterface(aVisitor.mItemData);
        if (selectedRadioButton) {
          selectedRadioButton->SetChecked(true);
        }
        // If there was no checked radio button or this one is no longer a
        // radio button we must reset it back to false to cancel the action.
        // See how the web of hack grows?
        if (!selectedRadioButton || mType != NS_FORM_INPUT_RADIO) {
          DoSetChecked(false, true, true);
        }
      } else if (oldType == NS_FORM_INPUT_CHECKBOX) {
        bool originalIndeterminateValue =
          !!(aVisitor.mItemFlags & NS_ORIGINAL_INDETERMINATE_VALUE);
        SetIndeterminateInternal(originalIndeterminateValue, false);
        DoSetChecked(originalCheckedValue, true, true);
      }
    } else {
      nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                           static_cast<nsIDOMHTMLInputElement*>(this),
                                           NS_LITERAL_STRING("change"), true,
                                           false);
#ifdef ACCESSIBILITY
      // Fire an event to notify accessibility
      if (mType == NS_FORM_INPUT_CHECKBOX) {
        FireEventForAccessibility(this, aVisitor.mPresContext,
                                  NS_LITERAL_STRING("CheckboxStateChange"));
      } else {
        FireEventForAccessibility(this, aVisitor.mPresContext,
                                  NS_LITERAL_STRING("RadioStateChange"));
        // Fire event for the previous selected radio.
        nsCOMPtr<nsIDOMHTMLInputElement> previous =
          do_QueryInterface(aVisitor.mItemData);
        if(previous) {
          FireEventForAccessibility(previous, aVisitor.mPresContext,
                                    NS_LITERAL_STRING("RadioStateChange"));
        }
      }
#endif
    }
  }

  if (NS_SUCCEEDED(rv)) {
    if (nsEventStatus_eIgnore == aVisitor.mEventStatus) {
      switch (aVisitor.mEvent->message) {

        case NS_FOCUS_CONTENT:
        {
          // see if we should select the contents of the textbox. This happens
          // for text and password fields when the field was focused by the
          // keyboard or a navigation, the platform allows it, and it wasn't
          // just because we raised a window.
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm && IsSingleLineTextControl(false) &&
              !(static_cast<nsFocusEvent *>(aVisitor.mEvent))->fromRaise &&
              SelectTextFieldOnFocus()) {
            nsIDocument* document = GetCurrentDoc();
            if (document) {
              PRUint32 lastFocusMethod;
              fm->GetLastFocusMethod(document->GetWindow(), &lastFocusMethod);
              if (lastFocusMethod &
                  (nsIFocusManager::FLAG_BYKEY | nsIFocusManager::FLAG_BYMOVEFOCUS)) {
                nsRefPtr<nsPresContext> presContext = GetPresContext();
                if (DispatchSelectEvent(presContext)) {
                  SelectAll(presContext);
                }
              }
            }
          }
          break;
        }

        case NS_KEY_PRESS:
        case NS_KEY_UP:
        {
          // For backwards compat, trigger checks/radios/buttons with
          // space or enter (bug 25300)
          nsKeyEvent * keyEvent = (nsKeyEvent *)aVisitor.mEvent;

          if ((aVisitor.mEvent->message == NS_KEY_PRESS &&
               keyEvent->keyCode == NS_VK_RETURN) ||
              (aVisitor.mEvent->message == NS_KEY_UP &&
               keyEvent->keyCode == NS_VK_SPACE)) {
            switch(mType) {
              case NS_FORM_INPUT_CHECKBOX:
              case NS_FORM_INPUT_RADIO:
              {
                // Checkbox and Radio try to submit on Enter press
                if (keyEvent->keyCode != NS_VK_SPACE) {
                  MaybeSubmitForm(aVisitor.mPresContext);

                  break;  // If we are submitting, do not send click event
                }
                // else fall through and treat Space like click...
              }
              case NS_FORM_INPUT_BUTTON:
              case NS_FORM_INPUT_RESET:
              case NS_FORM_INPUT_SUBMIT:
              case NS_FORM_INPUT_IMAGE: // Bug 34418
              {
                nsMouseEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                                   NS_MOUSE_CLICK, nsnull, nsMouseEvent::eReal);
                event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
                nsEventStatus status = nsEventStatus_eIgnore;

                nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                            aVisitor.mPresContext, &event,
                                            nsnull, &status);
                aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
              } // case
            } // switch
          }
          if (aVisitor.mEvent->message == NS_KEY_PRESS &&
              mType == NS_FORM_INPUT_RADIO && !keyEvent->isAlt &&
              !keyEvent->isControl && !keyEvent->isMeta) {
            bool isMovingBack = false;
            switch (keyEvent->keyCode) {
              case NS_VK_UP: 
              case NS_VK_LEFT:
                isMovingBack = true;
                // FALLTHROUGH
              case NS_VK_DOWN:
              case NS_VK_RIGHT:
              // Arrow key pressed, focus+select prev/next radio button
              nsIRadioGroupContainer* container = GetRadioGroupContainer();
              if (container) {
                nsAutoString name;
                GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
                nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton;
                container->GetNextRadioButton(name, isMovingBack, this,
                                              getter_AddRefs(selectedRadioButton));
                nsCOMPtr<nsIContent> radioContent =
                  do_QueryInterface(selectedRadioButton);
                if (radioContent) {
                  rv = selectedRadioButton->Focus();
                  if (NS_SUCCEEDED(rv)) {
                    nsEventStatus status = nsEventStatus_eIgnore;
                    nsMouseEvent event(NS_IS_TRUSTED_EVENT(aVisitor.mEvent),
                                       NS_MOUSE_CLICK, nsnull,
                                       nsMouseEvent::eReal);
                    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
                    rv = nsEventDispatcher::Dispatch(radioContent,
                                                     aVisitor.mPresContext,
                                                     &event, nsnull, &status);
                    if (NS_SUCCEEDED(rv)) {
                      aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
                    }
                  }
                }
              }
            }
          }

          /*
           * For some input types, if the user hits enter, the form is submitted.
           *
           * Bug 99920, bug 109463 and bug 147850:
           * (a) if there is a submit control in the form, click the first
           *     submit control in the form.
           * (b) if there is just one text control in the form, submit by
           *     sending a submit event directly to the form
           * (c) if there is more than one text input and no submit buttons, do
           *     not submit, period.
           */

          if (aVisitor.mEvent->message == NS_KEY_PRESS &&
              (keyEvent->keyCode == NS_VK_RETURN ||
               keyEvent->keyCode == NS_VK_ENTER) &&
               IsSingleLineTextControl(false, mType)) {
            nsIFrame* primaryFrame = GetPrimaryFrame();
            if (primaryFrame) {
              nsITextControlFrame* textFrame = do_QueryFrame(primaryFrame);

              // Fire onChange (if necessary)
              if (textFrame) {
                textFrame->CheckFireOnChange();
              }
            }

            rv = MaybeSubmitForm(aVisitor.mPresContext);
            NS_ENSURE_SUCCESS(rv, rv);
          }

        } break; // NS_KEY_PRESS || NS_KEY_UP

        case NS_MOUSE_BUTTON_DOWN:
        case NS_MOUSE_BUTTON_UP:
        case NS_MOUSE_DOUBLECLICK:
        {
          // cancel all of these events for buttons
          //XXXsmaug Why?
          if (aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
              (static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                 nsMouseEvent::eMiddleButton ||
               static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
                 nsMouseEvent::eRightButton)) {
            if (mType == NS_FORM_INPUT_BUTTON ||
                mType == NS_FORM_INPUT_RESET ||
                mType == NS_FORM_INPUT_SUBMIT) {
              if (aVisitor.mDOMEvent) {
                aVisitor.mDOMEvent->StopPropagation();
              } else {
                rv = NS_ERROR_FAILURE;
              }
            }

          }
          break;
        }
        default:
          break;
      }

      if (outerActivateEvent) {
        if (mForm && (oldType == NS_FORM_INPUT_SUBMIT ||
                      oldType == NS_FORM_INPUT_IMAGE)) {
          if (mType != NS_FORM_INPUT_SUBMIT && mType != NS_FORM_INPUT_IMAGE) {
            // If the type has changed to a non-submit type, then we want to
            // flush the stored submission if there is one (as if the submit()
            // was allowed to succeed)
            mForm->FlushPendingSubmission();
          }
        }
        switch(mType) {
        case NS_FORM_INPUT_RESET:
        case NS_FORM_INPUT_SUBMIT:
        case NS_FORM_INPUT_IMAGE:
          if (mForm) {
            nsFormEvent event(true, (mType == NS_FORM_INPUT_RESET) ?
                              NS_FORM_RESET : NS_FORM_SUBMIT);
            event.originator      = this;
            nsEventStatus status  = nsEventStatus_eIgnore;

            nsCOMPtr<nsIPresShell> presShell =
              aVisitor.mPresContext->GetPresShell();

            // If |nsIPresShell::Destroy| has been called due to
            // handling the event the pres context will return a null
            // pres shell.  See bug 125624.
            // TODO: removing this code and have the submit event sent by the
            // form, see bug 592124.
            if (presShell && (event.message != NS_FORM_SUBMIT ||
                              mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate) ||
                              // We know the element is a submit control, if this check is moved,
                              // make sure formnovalidate is used only if it's a submit control.
                              HasAttr(kNameSpaceID_None, nsGkAtoms::formnovalidate) ||
                              mForm->CheckValidFormSubmission())) {
              // Hold a strong ref while dispatching
              nsRefPtr<nsHTMLFormElement> form(mForm);
              presShell->HandleDOMEventWithTarget(mForm, &event, &status);
              aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
            }
          }
          break;

        default:
          break;
        } //switch 
      } //click or outer activate event
    } else if (outerActivateEvent &&
               (oldType == NS_FORM_INPUT_SUBMIT ||
                oldType == NS_FORM_INPUT_IMAGE) &&
               mForm) {
      // tell the form to flush a possible pending submission.
      // the reason is that the script returned false (the event was
      // not ignored) so if there is a stored submission, it needs to
      // be submitted immediately.
      mForm->FlushPendingSubmission();
    }
  } // if

  return rv;
}

void
nsHTMLInputElement::MaybeLoadImage()
{
  // Our base URI may have changed; claim that our URI changed, and the
  // nsImageLoadingContent will decide whether a new image load is warranted.
  nsAutoString uri;
  if (mType == NS_FORM_INPUT_IMAGE &&
      GetAttr(kNameSpaceID_None, nsGkAtoms::src, uri) &&
      (NS_FAILED(LoadImage(uri, false, true)) ||
       !LoadingEnabled())) {
    CancelImageRequests(true);
  }
}

nsresult
nsHTMLInputElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mType == NS_FORM_INPUT_IMAGE) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
      // FIXME: Bug 660963 it would be nice if we could just have
      // ClearBrokenState update our state and do it fast...
      ClearBrokenState();
      RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &nsHTMLInputElement::MaybeLoadImage));
    }
  }

  // Add radio to document if we don't have a form already (if we do it's
  // already been added into that group)
  if (aDocument && !mForm && mType == NS_FORM_INPUT_RADIO) {
    AddedToRadioGroup();
  }

  // An element can't suffer from value missing if it is not in a document.
  // We have to check if we suffer from that as we are now in a document.
  UpdateValueMissingValidityState();

  // If there is a disabled fieldset in the parent chain, the element is now
  // barred from constraint validation and can't suffer from value missing
  // (call done before).
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);

  return rv;
}

void
nsHTMLInputElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // If we have a form and are unbound from it,
  // nsGenericHTMLFormElement::UnbindFromTree() will unset the form and
  // that takes care of form's WillRemove so we just have to take care
  // of the case where we're removing from the document and we don't
  // have a form
  if (!mForm && mType == NS_FORM_INPUT_RADIO) {
    WillRemoveFromRadioGroup();
  }

  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);

  // GetCurrentDoc is returning nsnull so we can update the value
  // missing validity state to reflect we are no longer into a doc.
  UpdateValueMissingValidityState();
  // We might be no longer disabled because of parent chain changed.
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

void
nsHTMLInputElement::HandleTypeChange(PRUint8 aNewType)
{
  ValueModeType aOldValueMode = GetValueMode();
  nsAutoString aOldValue;

  if (aOldValueMode == VALUE_MODE_VALUE && !mParserCreating) {
    GetValue(aOldValue);
  }

  // Only single line text inputs have a text editor state.
  bool isNewTypeSingleLine = IsSingleLineTextControl(false, aNewType);
  bool isCurrentTypeSingleLine = IsSingleLineTextControl(false, mType);

  if (isNewTypeSingleLine && !isCurrentTypeSingleLine) {
    FreeData();
    mInputData.mState = new nsTextEditorState(this);
    NS_ADDREF(mInputData.mState);
  } else if (isCurrentTypeSingleLine && !isNewTypeSingleLine) {
    FreeData();
  }

  mType = aNewType;

  if (!mParserCreating) {
    /**
     * The following code is trying to reproduce the algorithm described here:
     * http://www.whatwg.org/specs/web-apps/current-work/complete.html#input-type-change
     */
    switch (GetValueMode()) {
      case VALUE_MODE_DEFAULT:
      case VALUE_MODE_DEFAULT_ON:
        // If the previous value mode was value, we need to set the value content
        // attribute to the previous value.
        // There is no value sanitizing algorithm for elements in this mode.
        if (aOldValueMode == VALUE_MODE_VALUE && !aOldValue.IsEmpty()) {
          SetAttr(kNameSpaceID_None, nsGkAtoms::value, aOldValue, true);
        }
        break;
      case VALUE_MODE_VALUE:
        // If the previous value mode wasn't value, we have to set the value to
        // the value content attribute.
        // SetValueInternal is going to sanitize the value.
        {
          nsAutoString value;
          if (aOldValueMode != VALUE_MODE_VALUE) {
            GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
          } else {
            // We get the current value so we can sanitize it.
            GetValue(value);
          }
          SetValueInternal(value, false, false);
        }
        break;
      case VALUE_MODE_FILENAME:
      default:
        // We don't care about the value.
        // There is no value sanitizing algorithm for elements in this mode.
        break;
    }
  }

  // Do not notify, it will be done after if needed.
  UpdateAllValidityStates(false);
}

void
nsHTMLInputElement::SanitizeValue(nsAString& aValue)
{
  NS_ASSERTION(!mParserCreating, "The element parsing should be finished!");

  switch (mType) {
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_PASSWORD:
      {
        PRUnichar crlf[] = { PRUnichar('\r'), PRUnichar('\n'), 0 };
        aValue.StripChars(crlf);
      }
      break;
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      {
        PRUnichar crlf[] = { PRUnichar('\r'), PRUnichar('\n'), 0 };
        aValue.StripChars(crlf);

        aValue = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(aValue);
      }
      break;
  }
}

bool
nsHTMLInputElement::ParseAttribute(PRInt32 aNamespaceID,
                                   nsIAtom* aAttribute,
                                   const nsAString& aValue,
                                   nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      // XXX ARG!! This is major evilness. ParseAttribute
      // shouldn't set members. Override SetAttr instead
      PRInt32 newType;
      bool success = aResult.ParseEnumValue(aValue, kInputTypeTable, false);
      if (success) {
        newType = aResult.GetEnumValue();
      } else {
        newType = kInputDefaultType->value;
      }

      if (newType != mType) {
        // Make sure to do the check for newType being NS_FORM_INPUT_FILE and
        // the corresponding SetValueInternal() call _before_ we set mType.
        // That way the logic in SetValueInternal() will work right (that logic
        // makes assumptions about our frame based on mType, but we won't have
        // had time to recreate frames yet -- that happens later in the
        // SetAttr() process).
        if (newType == NS_FORM_INPUT_FILE || mType == NS_FORM_INPUT_FILE) {
          // This call isn't strictly needed any more since we'll never
          // confuse values and filenames. However it's there for backwards
          // compat.
          ClearFiles(false);
        }

        HandleTypeChange(newType);
      }

      return success;
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::maxlength) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::size) {
      return aResult.ParsePositiveIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::border) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::formmethod) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, false);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, false);
    }
    if (aAttribute == nsGkAtoms::autocomplete) {
      return aResult.ParseEnumValue(aValue, kInputAutocompleteTable, false);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      // We have to call |ParseImageAttribute| unconditionally since we
      // don't know if we're going to have a type="image" attribute yet,
      // (or could have it set dynamically in the future).  See bug
      // 214077.
      return true;
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
                      nsRuleData* aData)
{
  const nsAttrValue* value = aAttributes->GetAttr(nsGkAtoms::type);
  if (value && value->Type() == nsAttrValue::eEnum &&
      value->GetEnumValue() == NS_FORM_INPUT_IMAGE) {
    nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
    // Images treat align as "float"
    nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  } 

  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
nsHTMLInputElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                           PRInt32 aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLFormElement::GetAttributeChangeHint(aAttribute, aModType);
  if (aAttribute == nsGkAtoms::type) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  } else if (mType == NS_FORM_INPUT_IMAGE &&
             (aAttribute == nsGkAtoms::alt ||
              aAttribute == nsGkAtoms::value)) {
    // We might need to rebuild our alt text.  Just go ahead and
    // reconstruct our frame.  This should be quite rare..
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  } else if (aAttribute == nsGkAtoms::value) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  } else if (aAttribute == nsGkAtoms::size &&
             IsSingleLineTextControl(false)) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  } else if (PlaceholderApplies() && aAttribute == nsGkAtoms::placeholder) {
    NS_UpdateHint(retval, NS_STYLE_HINT_FRAMECHANGE);
  }
  return retval;
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::type },
    { nsnull },
  };

  static const MappedAttributeEntry* const map[] = {
    attributes,
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc
nsHTMLInputElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}


// Controllers Methods

NS_IMETHODIMP
nsHTMLInputElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  //XXX: what about type "file"?
  if (IsSingleLineTextControl(false))
  {
    if (!mControllers)
    {
      nsresult rv;
      mControllers = do_CreateInstance(kXULControllersCID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIController>
        controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1",
                                     &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      mControllers->AppendController(controller);

      controller = do_CreateInstance("@mozilla.org/editor/editingcontroller;1",
                                     &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      mControllers->AppendController(controller);
    }
  }

  *aResult = mControllers;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetTextLength(PRInt32* aTextLength)
{
  nsAutoString val;

  nsresult rv = GetValue(val);

  *aTextLength = val.Length();

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionRange(PRInt32 aSelectionStart,
                                      PRInt32 aSelectionEnd,
                                      const nsAString& aDirection)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      // Default to forward, even if not specified.
      // Note that we don't currently support directionless selections, so
      // "none" is treated like "forward".
      nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eForward;
      if (aDirection.EqualsLiteral("backward")) {
        dir = nsITextControlFrame::eBackward;
      }

      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd, dir);
      if (NS_SUCCEEDED(rv)) {
        rv = textControlFrame->ScrollSelectionIntoView();
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionStart(PRInt32* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);

  PRInt32 selEnd;
  nsresult rv = GetSelectionRange(aSelectionStart, &selEnd);

  if (NS_FAILED(rv)) {
    nsTextEditorState *state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      *aSelectionStart = state->GetSelectionProperties().mStart;
      return NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsTextEditorState *state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().mStart = aSelectionStart;
    return NS_OK;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 start, end;
  rv = GetSelectionRange(&start, &end);
  NS_ENSURE_SUCCESS(rv, rv);
  start = aSelectionStart;
  if (end < start) {
    end = start;
  }
  return SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionEnd(PRInt32* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);

  PRInt32 selStart;
  nsresult rv = GetSelectionRange(&selStart, aSelectionEnd);

  if (NS_FAILED(rv)) {
    nsTextEditorState *state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      *aSelectionEnd = state->GetSelectionProperties().mEnd;
      return NS_OK;
    }
  }
  return rv;
}


NS_IMETHODIMP
nsHTMLInputElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsTextEditorState *state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().mEnd = aSelectionEnd;
    return NS_OK;
  }

  nsAutoString direction;
  nsresult rv = GetSelectionDirection(direction);
  NS_ENSURE_SUCCESS(rv, rv);
  PRInt32 start, end;
  rv = GetSelectionRange(&start, &end);
  NS_ENSURE_SUCCESS(rv, rv);
  end = aSelectionEnd;
  if (start > end) {
    start = end;
  }
  return SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
nsHTMLInputElement::GetFiles(nsIDOMFileList** aFileList)
{
  *aFileList = nsnull;

  if (mType != NS_FORM_INPUT_FILE) {
    return NS_OK;
  }

  if (!mFileList) {
    mFileList = new nsDOMFileList();
    if (!mFileList) return NS_ERROR_OUT_OF_MEMORY;

    UpdateFileList();
  }

  NS_ADDREF(*aFileList = mFileList);

  return NS_OK;
}

nsresult
nsHTMLInputElement::GetSelectionRange(PRInt32* aSelectionStart,
                                      PRInt32* aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

static void
DirectionToName(nsITextControlFrame::SelectionDirection dir, nsAString& aDirection)
{
  if (dir == nsITextControlFrame::eNone) {
    aDirection.AssignLiteral("none");
  } else if (dir == nsITextControlFrame::eForward) {
    aDirection.AssignLiteral("forward");
  } else if (dir == nsITextControlFrame::eBackward) {
    aDirection.AssignLiteral("backward");
  } else {
    NS_NOTREACHED("Invalid SelectionDirection value");
  }
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionDirection(nsAString& aDirection)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      nsITextControlFrame::SelectionDirection dir;
      rv = textControlFrame->GetSelectionRange(nsnull, nsnull, &dir);
      if (NS_SUCCEEDED(rv)) {
        DirectionToName(dir, aDirection);
      }
    }
  }

  if (NS_FAILED(rv)) {
    nsTextEditorState *state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      DirectionToName(state->GetSelectionProperties().mDirection, aDirection);
      return NS_OK;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionDirection(const nsAString& aDirection) {
  nsTextEditorState *state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    state->GetSelectionProperties().mDirection = dir;
    return NS_OK;
  }

  PRInt32 start, end;
  nsresult rv = GetSelectionRange(&start, &end);
  if (NS_SUCCEEDED(rv)) {
    rv = SetSelectionRange(start, end, aDirection);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate();
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      textControlFrame->GetPhonetic(aPhonetic);
  }

  return NS_OK;
}

#ifdef ACCESSIBILITY
/*static*/ nsresult
FireEventForAccessibility(nsIDOMHTMLInputElement* aTarget,
                          nsPresContext* aPresContext,
                          const nsAString& aEventType)
{
  nsCOMPtr<nsIDOMEvent> event;
  if (NS_SUCCEEDED(nsEventDispatcher::CreateEvent(aPresContext, nsnull,
                                                  NS_LITERAL_STRING("Events"),
                                                  getter_AddRefs(event)))) {
    event->InitEvent(aEventType, true, true);

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
    if (privateEvent) {
      privateEvent->SetTrusted(true);
    }

    nsEventDispatcher::DispatchDOMEvent(aTarget, nsnull, event, aPresContext, nsnull);
  }

  return NS_OK;
}
#endif

nsresult
nsHTMLInputElement::SetDefaultValueAsValue()
{
  NS_ASSERTION(GetValueMode() == VALUE_MODE_VALUE,
               "GetValueMode() should return VALUE_MODE_VALUE!");

  // The element has a content attribute value different from it's value when
  // it's in the value mode value.
  nsAutoString resetVal;
  GetDefaultValue(resetVal);

  // SetValueInternal is going to sanitize the value.
  return SetValueInternal(resetVal, false, false);
}

NS_IMETHODIMP
nsHTMLInputElement::Reset()
{
  // We should be able to reset all dirty flags regardless of the type.
  SetCheckedChanged(false);
  SetValueChanged(false);

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      return SetDefaultValueAsValue();
    case VALUE_MODE_DEFAULT_ON:
      return DoSetChecked(DefaultChecked(), true, false);
    case VALUE_MODE_FILENAME:
      ClearFiles(false);
      return NS_OK;
    case VALUE_MODE_DEFAULT:
    default:
      return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLInputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  // Disabled elements don't submit
  // For type=reset, and type=button, we just never submit, period.
  // For type=image and type=button, we only submit if we were the button
  // pressed
  // For type=radio and type=checkbox, we only submit if checked=true
  if (IsDisabled() || mType == NS_FORM_INPUT_RESET ||
      mType == NS_FORM_INPUT_BUTTON ||
      ((mType == NS_FORM_INPUT_SUBMIT || mType == NS_FORM_INPUT_IMAGE) &&
       aFormSubmission->GetOriginatingElement() != this) ||
      ((mType == NS_FORM_INPUT_RADIO || mType == NS_FORM_INPUT_CHECKBOX) &&
       !mChecked)) {
    return NS_OK;
  }

  // Get the name
  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  // Submit .x, .y for input type=image
  if (mType == NS_FORM_INPUT_IMAGE) {
    // Get a property set by the frame to find out where it was clicked.
    nsIntPoint* lastClickedPoint =
      static_cast<nsIntPoint*>(GetProperty(nsGkAtoms::imageClickedPoint));
    PRInt32 x, y;
    if (lastClickedPoint) {
      // Convert the values to strings for submission
      x = lastClickedPoint->x;
      y = lastClickedPoint->y;
    } else {
      x = y = 0;
    }

    nsAutoString xVal, yVal;
    xVal.AppendInt(x);
    yVal.AppendInt(y);

    if (!name.IsEmpty()) {
      aFormSubmission->AddNameValuePair(name + NS_LITERAL_STRING(".x"), xVal);
      aFormSubmission->AddNameValuePair(name + NS_LITERAL_STRING(".y"), yVal);
    } else {
      // If the Image Element has no name, simply return x and y
      // to Nav and IE compatibility.
      aFormSubmission->AddNameValuePair(NS_LITERAL_STRING("x"), xVal);
      aFormSubmission->AddNameValuePair(NS_LITERAL_STRING("y"), yVal);
    }

    return NS_OK;
  }

  //
  // Submit name=value
  //

  // If name not there, don't submit
  if (name.IsEmpty()) {
    return NS_OK;
  }

  // Get the value
  nsAutoString value;
  nsresult rv = GetValue(value);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (mType == NS_FORM_INPUT_SUBMIT && value.IsEmpty() &&
      !HasAttr(kNameSpaceID_None, nsGkAtoms::value)) {
    // Get our default value, which is the same as our default label
    nsXPIDLString defaultValue;
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "Submit", defaultValue);
    value = defaultValue;
  }
      
  //
  // Submit file if its input type=file and this encoding method accepts files
  //
  if (mType == NS_FORM_INPUT_FILE) {
    // Submit files

    const nsCOMArray<nsIDOMFile>& files = GetFiles();

    for (PRInt32 i = 0; i < files.Count(); ++i) {
      aFormSubmission->AddNameFilePair(name, files[i]);
    }

    if (files.Count() == 0) {
      // If no file was selected, pretend we had an empty file with an
      // empty filename.
      aFormSubmission->AddNameFilePair(name, nsnull);

    }

    return NS_OK;
  }

  if (mType == NS_FORM_INPUT_HIDDEN && name.EqualsLiteral("_charset_")) {
    nsCString charset;
    aFormSubmission->GetCharset(charset);
    return aFormSubmission->AddNameValuePair(name,
                                             NS_ConvertASCIItoUTF16(charset));
  }
  if (IsSingleLineTextControl(true) &&
      name.EqualsLiteral("isindex") &&
      aFormSubmission->SupportsIsindexSubmission()) {
    return aFormSubmission->AddIsindex(value);
  }
  return aFormSubmission->AddNameValuePair(name, value);
}


NS_IMETHODIMP
nsHTMLInputElement::SaveState()
{
  nsRefPtr<nsHTMLInputElementState> inputState;
  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
      {
        if (mCheckedChanged) {
          inputState = new nsHTMLInputElementState();
          inputState->SetChecked(mChecked);
        }
        break;
      }

    // Never save passwords in session history
    case NS_FORM_INPUT_PASSWORD:
      break;
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_URL:
    case NS_FORM_INPUT_HIDDEN:
      {
        if (mValueChanged) {
          inputState = new nsHTMLInputElementState();
          nsAutoString value;
          GetValue(value);
          DebugOnly<nsresult> rv =
            nsLinebreakConverter::ConvertStringLineBreaks(
                 value,
                 nsLinebreakConverter::eLinebreakPlatform,
                 nsLinebreakConverter::eLinebreakContent);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");
          inputState->SetValue(value);
       }
      break;
    }
    case NS_FORM_INPUT_FILE:
      {
        if (mFiles.Count()) {
          inputState = new nsHTMLInputElementState();
          inputState->SetFiles(mFiles);
        }
        break;
      }
  }
  
  nsresult rv = NS_OK;
  nsPresState* state = nsnull;
  if (inputState) {
    rv = GetPrimaryPresState(this, &state);
    if (state) {
      state->SetStateProperty(inputState);
    }
  }

  if (mDisabledChanged) {
    rv |= GetPrimaryPresState(this, &state);
    if (state) {
      // We do not want to save the real disabled state but the disabled
      // attribute.
      state->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
    }
  }

  return rv;
}

void
nsHTMLInputElement::DoneCreatingElement()
{
  mParserCreating = false;

  //
  // Restore state as needed.  Note that disabled state applies to all control
  // types.
  //
  bool restoredCheckedState =
    !mInhibitRestoration && RestoreFormControlState(this, this);

  //
  // If restore does not occur, we initialize .checked using the CHECKED
  // property.
  //
  if (!restoredCheckedState && mShouldInitChecked) {
    DoSetChecked(DefaultChecked(), false, true);
    DoSetCheckedChanged(false, false);
  }

  // Sanitize the value.
  if (GetValueMode() == VALUE_MODE_VALUE) {
    nsAutoString aValue;
    GetValue(aValue);
    SetValueInternal(aValue, false, false);
  }

  mShouldInitChecked = false;
}

nsEventStates
nsHTMLInputElement::IntrinsicState() const
{
  // If you add states here, and they're type-dependent, you need to add them
  // to the type case in AfterSetAttr.
  
  nsEventStates state = nsGenericHTMLFormElement::IntrinsicState();
  if (mType == NS_FORM_INPUT_CHECKBOX || mType == NS_FORM_INPUT_RADIO) {
    // Check current checked state (:checked)
    if (mChecked) {
      state |= NS_EVENT_STATE_CHECKED;
    }

    // Check current indeterminate state (:indeterminate)
    if (mType == NS_FORM_INPUT_CHECKBOX && mIndeterminate) {
      state |= NS_EVENT_STATE_INDETERMINATE;
    }

    // Check whether we are the default checked element (:default)
    if (DefaultChecked()) {
      state |= NS_EVENT_STATE_DEFAULT;
    }
  } else if (mType == NS_FORM_INPUT_IMAGE) {
    state |= nsImageLoadingContent::ImageState();
  }

  if (DoesRequiredApply() && HasAttr(kNameSpaceID_None, nsGkAtoms::required)) {
    state |= NS_EVENT_STATE_REQUIRED;
  } else {
    state |= NS_EVENT_STATE_OPTIONAL;
  }

  if (IsCandidateForConstraintValidation()) {
    if (IsValid()) {
      state |= NS_EVENT_STATE_VALID;
    } else {
      state |= NS_EVENT_STATE_INVALID;

      if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
          (GetValidityState(VALIDITY_STATE_CUSTOM_ERROR) ||
           (mCanShowInvalidUI && ShouldShowValidityUI()))) {
        state |= NS_EVENT_STATE_MOZ_UI_INVALID;
      }
    }

    // :-moz-ui-valid applies if all of the following conditions are true:
    // 1. The element is not focused, or had either :-moz-ui-valid or
    //    :-moz-ui-invalid applying before it was focused ;
    // 2. The element is either valid or isn't allowed to have
    //    :-moz-ui-invalid applying ;
    // 3. The element has no form owner or its form owner doesn't have the
    //    novalidate attribute set ;
    // 4. The element has already been modified or the user tried to submit the
    //    form owner while invalid.
    if ((!mForm || !mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate)) &&
        (mCanShowValidUI && ShouldShowValidityUI() &&
         (IsValid() || (!state.HasState(NS_EVENT_STATE_MOZ_UI_INVALID) &&
                        !mCanShowInvalidUI)))) {
      state |= NS_EVENT_STATE_MOZ_UI_VALID;
    }
  }

  if (PlaceholderApplies() && HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      !nsContentUtils::IsFocusedContent((nsIContent*)(this)) &&
      IsValueEmpty()) {
    state |= NS_EVENT_STATE_MOZ_PLACEHOLDER;
  }

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  return state;
}

bool
nsHTMLInputElement::RestoreState(nsPresState* aState)
{
  bool restoredCheckedState = false;

  nsCOMPtr<nsHTMLInputElementState> inputState
    (do_QueryInterface(aState->GetStateProperty()));

  if (inputState) {
    switch (mType) {
      case NS_FORM_INPUT_CHECKBOX:
      case NS_FORM_INPUT_RADIO:
        {
          if (inputState->IsCheckedSet()) {
            restoredCheckedState = true;
            DoSetChecked(inputState->GetChecked(), true, true);
          }
          break;
        }

      case NS_FORM_INPUT_EMAIL:
      case NS_FORM_INPUT_SEARCH:
      case NS_FORM_INPUT_TEXT:
      case NS_FORM_INPUT_TEL:
      case NS_FORM_INPUT_URL:
      case NS_FORM_INPUT_HIDDEN:
        {
          SetValueInternal(inputState->GetValue(), false, true);
          break;
        }
      case NS_FORM_INPUT_FILE:
        {
          const nsCOMArray<nsIDOMFile>& files = inputState->GetFiles();
          SetFiles(files, true);
          break;
        }
    }
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return restoredCheckedState;
}

bool
nsHTMLInputElement::AllowDrop()
{
  // Allow drop on anything other than file inputs.

  return mType != NS_FORM_INPUT_FILE;
}

/*
 * Radio group stuff
 */

void
nsHTMLInputElement::AddedToRadioGroup()
{
  // Make sure not to notify if we're still being created by the parser
  bool notify = !mParserCreating;

  //
  //  If the input element is not in a form and
  //  not in a document, we just need to return.
  //
  if (!mForm && !(IsInDoc() && GetParent())) {
    return;
  }

  //
  // If the input element is checked, and we add it to the group, it will
  // deselect whatever is currently selected in that group
  //
  if (mChecked) {
    //
    // If it is checked, call "RadioSetChecked" to perform the selection/
    // deselection ritual.  This has the side effect of repainting the
    // radio button, but as adding a checked radio button into the group
    // should not be that common an occurrence, I think we can live with
    // that.
    //
    RadioSetChecked(notify);
  }

  //
  // For integrity purposes, we have to ensure that "checkedChanged" is
  // the same for this new element as for all the others in the group
  //
  bool checkedChanged = mCheckedChanged;

  nsCOMPtr<nsIRadioVisitor> visitor =
    new nsRadioGetCheckedChangedVisitor(&checkedChanged, this);
  VisitGroup(visitor, notify);

  SetCheckedChangedInternal(checkedChanged);

  //
  // Add the radio to the radio group container.
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    container->AddToRadioGroup(name, static_cast<nsIFormControl*>(this));

    // We initialize the validity of the element to the validity of the group
    // because we assume UpdateValueMissingState() will be called after.
    SetValidityState(VALIDITY_STATE_VALUE_MISSING,
                     container->GetValueMissingState(name));
  }
}

void
nsHTMLInputElement::WillRemoveFromRadioGroup()
{
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (!container) {
    return;
  }

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  // If this button was checked, we need to notify the group that there is no
  // longer a selected radio button
  if (mChecked) {
    container->SetCurrentRadioButton(name, nsnull);
  }

  // Remove this radio from its group in the container.
  // We need to call UpdateValueMissingValidityStateForRadio before to make sure
  // the group validity is updated (with this element being ignored).
  UpdateValueMissingValidityStateForRadio(true);
  container->RemoveFromRadioGroup(name, static_cast<nsIFormControl*>(this));
}

bool
nsHTMLInputElement::IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  if (IsDisabled()) {
    *aIsFocusable = false;
    return true;
  }

  if (IsSingleLineTextControl(false)) {
    *aIsFocusable = true;
    return false;
  }

#ifdef XP_MACOSX
  const bool defaultFocusable = !aWithMouse || nsFocusManager::sMouseFocusesFormControl;
#else
  const bool defaultFocusable = true;
#endif

  if (mType == NS_FORM_INPUT_FILE) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    *aIsFocusable = defaultFocusable;
    return true;
  }

  if (mType == NS_FORM_INPUT_HIDDEN) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    *aIsFocusable = false;
    return false;
  }

  if (!aTabIndex) {
    // The other controls are all focusable
    *aIsFocusable = defaultFocusable;
    return false;
  }

  if (mType != NS_FORM_INPUT_RADIO) {
    *aIsFocusable = defaultFocusable;
    return false;
  }

  if (mChecked) {
    // Selected radio buttons are tabbable
    *aIsFocusable = defaultFocusable;
    return false;
  }

  // Current radio button is not selected.
  // But make it tabbable if nothing in group is selected.
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (!container) {
    *aIsFocusable = defaultFocusable;
    return false;
  }

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio;
  container->GetCurrentRadioButton(name, getter_AddRefs(currentRadio));
  if (currentRadio) {
    *aTabIndex = -1;
  }
  *aIsFocusable = defaultFocusable;
  return false;
}

nsresult
nsHTMLInputElement::VisitGroup(nsIRadioVisitor* aVisitor, bool aFlushContent)
{
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    return container->WalkRadioGroup(name, aVisitor, aFlushContent);
  }

  aVisitor->Visit(this);
  return NS_OK;
}

nsHTMLInputElement::ValueModeType
nsHTMLInputElement::GetValueMode() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_IMAGE:
      return VALUE_MODE_DEFAULT;
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
      return VALUE_MODE_DEFAULT_ON;
    case NS_FORM_INPUT_FILE:
      return VALUE_MODE_FILENAME;
#ifdef DEBUG
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      return VALUE_MODE_VALUE;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in GetValueMode()");
      return VALUE_MODE_VALUE;
#else // DEBUG
    default:
      return VALUE_MODE_VALUE;
#endif // DEBUG
  }
}

bool
nsHTMLInputElement::IsMutable() const
{
  return !IsDisabled() && GetCurrentDoc() &&
         !(DoesReadOnlyApply() &&
           HasAttr(kNameSpaceID_None, nsGkAtoms::readonly));
}

bool
nsHTMLInputElement::DoesReadOnlyApply() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_CHECKBOX:
    // TODO:
    // case NS_FORM_INPUT_COLOR:
    // case NS_FORM_INPUT_RANGE:
      return false;
#ifdef DEBUG
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      return true;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesReadOnlyApply()");
      return true;
#else // DEBUG
    default:
      return true;
#endif // DEBUG
  }
}

bool
nsHTMLInputElement::DoesRequiredApply() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    // TODO:
    // case NS_FORM_INPUT_COLOR:
    // case NS_FORM_INPUT_RANGE:
      return false;
#ifdef DEBUG
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      return true;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesRequiredApply()");
      return true;
#else // DEBUG
    default:
      return true;
#endif // DEBUG
  }
}

bool
nsHTMLInputElement::DoesPatternApply() const
{
  return IsSingleLineTextControl(false);
}

// nsIConstraintValidation

NS_IMETHODIMP
nsHTMLInputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

bool
nsHTMLInputElement::IsTooLong()
{
  if (!MaxLengthApplies() ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength) ||
      !mValueChanged) {
    return false;
  }

  PRInt32 maxLength = -1;
  GetMaxLength(&maxLength);

  // Maxlength of -1 means parsing error.
  if (maxLength == -1) {
    return false;
  }

  PRInt32 textLength = -1;
  GetTextLength(&textLength);

  return textLength > maxLength;
}

bool
nsHTMLInputElement::IsValueMissing() const
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) ||
      !DoesRequiredApply()) {
    return false;
  }

  if (GetValueMode() == VALUE_MODE_VALUE) {
    if (!IsMutable()) {
      return false;
    }

    return IsValueEmpty();
  }

  switch (mType)
  {
    case NS_FORM_INPUT_CHECKBOX:
      return !mChecked;
    case NS_FORM_INPUT_FILE:
      {
        const nsCOMArray<nsIDOMFile>& files = GetFiles();
        return !files.Count();
      }
    default:
      return false;
  }
}

bool
nsHTMLInputElement::HasTypeMismatch() const
{
  if (mType != NS_FORM_INPUT_EMAIL && mType != NS_FORM_INPUT_URL) {
    return false;
  }

  nsAutoString value;
  NS_ENSURE_SUCCESS(GetValueInternal(value), false);

  if (value.IsEmpty()) {
    return false;
  }

  if (mType == NS_FORM_INPUT_EMAIL) {
    return HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)
             ? !IsValidEmailAddressList(value) : !IsValidEmailAddress(value);
  } else if (mType == NS_FORM_INPUT_URL) {
    /**
     * TODO:
     * The URL is not checked as the HTML5 specifications want it to be because
     * there is no code to check for a valid URI/IRI according to 3986 and 3987
     * RFC's at the moment, see bug 561586.
     *
     * RFC 3987 (IRI) implementation: bug 42899
     *
     * HTML5 specifications:
     * http://dev.w3.org/html5/spec/infrastructure.html#valid-url
     */
    nsCOMPtr<nsIIOService> ioService = do_GetIOService();
    nsCOMPtr<nsIURI> uri;

    return !NS_SUCCEEDED(ioService->NewURI(NS_ConvertUTF16toUTF8(value), nsnull,
                                           nsnull, getter_AddRefs(uri)));
  }

  return false;
}

bool
nsHTMLInputElement::HasPatternMismatch() const
{
  nsAutoString pattern;
  if (!DoesPatternApply() ||
      !GetAttr(kNameSpaceID_None, nsGkAtoms::pattern, pattern)) {
    return false;
  }

  nsAutoString value;
  NS_ENSURE_SUCCESS(GetValueInternal(value), false);

  if (value.IsEmpty()) {
    return false;
  }

  nsIDocument* doc = OwnerDoc();

  return !nsContentUtils::IsPatternMatching(value, pattern, doc);
}

void
nsHTMLInputElement::UpdateTooLongValidityState()
{
  // TODO: this code will be re-enabled with bug 613016 and bug 613019.
#if 0
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
#endif
}

void
nsHTMLInputElement::UpdateValueMissingValidityStateForRadio(bool aIgnoreSelf)
{
  bool notify = !mParserCreating;
  nsCOMPtr<nsIDOMHTMLInputElement> selection = GetSelectedRadioButton();

  // If there is no selection, that might mean the radio is not in a group.
  // In that case, we can look for the checked state of the radio.
  bool selected = selection || (!aIgnoreSelf && mChecked);
  bool required = !aIgnoreSelf && HasAttr(kNameSpaceID_None, nsGkAtoms::required);
  bool valueMissing = false;

  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();

  if (!container) {
    SetValidityState(VALIDITY_STATE_VALUE_MISSING, required && !selected);
    return;
  }

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  // If the current radio is required and not ignored, we can assume the entire
  // group is required.
  if (!required) {
    required = (aIgnoreSelf && HasAttr(kNameSpaceID_None, nsGkAtoms::required))
                 ? container->GetRequiredRadioCount(name) - 1
                 : container->GetRequiredRadioCount(name);
  }

  valueMissing = required && !selected;

  if (container->GetValueMissingState(name) != valueMissing) {
    container->SetValueMissingState(name, valueMissing);

    SetValidityState(VALIDITY_STATE_VALUE_MISSING, valueMissing);

    // nsRadioSetValueMissingState will call ContentStateChanged while visiting.
    nsAutoScriptBlocker scriptBlocker;
    nsCOMPtr<nsIRadioVisitor> visitor =
      new nsRadioSetValueMissingState(this, valueMissing, notify);
    VisitGroup(visitor, notify);
  }
}

void
nsHTMLInputElement::UpdateValueMissingValidityState()
{
  if (mType == NS_FORM_INPUT_RADIO) {
    UpdateValueMissingValidityStateForRadio(false);
    return;
  }

  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

void
nsHTMLInputElement::UpdateTypeMismatchValidityState()
{
    SetValidityState(VALIDITY_STATE_TYPE_MISMATCH, HasTypeMismatch());
}

void
nsHTMLInputElement::UpdatePatternMismatchValidityState()
{
  SetValidityState(VALIDITY_STATE_PATTERN_MISMATCH, HasPatternMismatch());
}

void
nsHTMLInputElement::UpdateAllValidityStates(bool aNotify)
{
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();
  UpdateTypeMismatchValidityState();
  UpdatePatternMismatchValidityState();

  if (validBefore != IsValid()) {
    UpdateState(aNotify);
  }
}

void
nsHTMLInputElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(mType == NS_FORM_INPUT_HIDDEN ||
                                    mType == NS_FORM_INPUT_BUTTON ||
                                    mType == NS_FORM_INPUT_RESET ||
                                    mType == NS_FORM_INPUT_SUBMIT ||
                                    mType == NS_FORM_INPUT_IMAGE ||
                                    HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) ||
                                    IsDisabled());
}

nsresult
nsHTMLInputElement::GetValidationMessage(nsAString& aValidationMessage,
                                         ValidityStateType aType)
{
  nsresult rv = NS_OK;

  switch (aType)
  {
    case VALIDITY_STATE_TOO_LONG:
    {
      nsXPIDLString message;
      PRInt32 maxLength = -1;
      PRInt32 textLength = -1;
      nsAutoString strMaxLength;
      nsAutoString strTextLength;

      GetMaxLength(&maxLength);
      GetTextLength(&textLength);

      strMaxLength.AppendInt(maxLength);
      strTextLength.AppendInt(textLength);

      const PRUnichar* params[] = { strMaxLength.get(), strTextLength.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "FormValidationTextTooLong",
                                                 params, message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_VALUE_MISSING:
    {
      nsXPIDLString message;
      nsCAutoString key;
      switch (mType)
      {
        case NS_FORM_INPUT_FILE:
          key.Assign("FormValidationFileMissing");
          break;
        case NS_FORM_INPUT_CHECKBOX:
          key.Assign("FormValidationCheckboxMissing");
          break;
        case NS_FORM_INPUT_RADIO:
          key.Assign("FormValidationRadioMissing");
          break;
        default:
          key.Assign("FormValidationValueMissing");
      }
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              key.get(), message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_TYPE_MISMATCH:
    {
      nsXPIDLString message;
      nsCAutoString key;
      if (mType == NS_FORM_INPUT_EMAIL) {
        key.AssignLiteral("FormValidationInvalidEmail");
      } else if (mType == NS_FORM_INPUT_URL) {
        key.AssignLiteral("FormValidationInvalidURL");
      } else {
        return NS_ERROR_UNEXPECTED;
      }
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              key.get(), message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_PATTERN_MISMATCH:
    {
      nsXPIDLString message;
      nsAutoString title;
      GetAttr(kNameSpaceID_None, nsGkAtoms::title, title);
      if (title.IsEmpty()) {
        rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                "FormValidationPatternMismatch",
                                                message);
      } else {
        if (title.Length() > nsIConstraintValidation::sContentSpecifiedMaxLengthMessage) {
          title.Truncate(nsIConstraintValidation::sContentSpecifiedMaxLengthMessage);
        }
        const PRUnichar* params[] = { title.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationPatternMismatchWithTitle",
                                                   params, message);
      }
      aValidationMessage = message;
      break;
    }
    default:
      rv = nsIConstraintValidation::GetValidationMessage(aValidationMessage, aType);
  }

  return rv;
}

//static
bool
nsHTMLInputElement::IsValidEmailAddressList(const nsAString& aValue)
{
  HTMLSplitOnSpacesTokenizer tokenizer(aValue, ',');

  while (tokenizer.hasMoreTokens()) {
    if (!IsValidEmailAddress(tokenizer.nextToken())) {
      return false;
    }
  }

  return !tokenizer.lastTokenEndedWithSeparator();
}

//static
bool
nsHTMLInputElement::IsValidEmailAddress(const nsAString& aValue)
{
  nsCAutoString value = NS_ConvertUTF16toUTF8(aValue);
  PRUint32 i = 0;
  PRUint32 length = value.Length();

  // Puny-encode the string if needed before running the validation algorithm.
  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (idnSrv) {
    bool ace;
    if (NS_SUCCEEDED(idnSrv->IsACE(value, &ace)) && !ace) {
      nsCAutoString punyCodedValue;
      if (NS_SUCCEEDED(idnSrv->ConvertUTF8toACE(value, punyCodedValue))) {
        value = punyCodedValue;
        length = value.Length();
      }
    }
  } else {
    NS_ERROR("nsIIDNService isn't present!");
  }

  // If the email address is empty, begins with a '@' or ends with a '.',
  // we know it's invalid.
  if (length == 0 || value[0] == '@' || value[length-1] == '.') {
    return false;
  }

  // Parsing the username.
  for (; i < length && value[i] != '@'; ++i) {
    PRUnichar c = value[i];

    // The username characters have to be in this list to be valid.
    if (!(nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) ||
          c == '.' || c == '!' || c == '#' || c == '$' || c == '%' ||
          c == '&' || c == '\''|| c == '*' || c == '+' || c == '-' ||
          c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
          c == '`' || c == '{' || c == '|' || c == '}' || c == '~' )) {
      return false;
    }
  }

  // There is no domain name (or it's one-character long),
  // that's not a valid email address.
  if (++i >= length) {
    return false;
  }

  // The domain name can't begin with a dot.
  if (value[i] == '.') {
    return false;
  }

  // Parsing the domain name.
  for (; i < length; ++i) {
    PRUnichar c = value[i];

    if (c == '.') {
      // A dot can't follow a dot.
      if (value[i-1] == '.') {
        return false;
      }
    } else if (!(nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) ||
                 c == '-')) {
      // The domain characters have to be in this list to be valid.
      return false;
    }
  }

  return true;
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::IsSingleLineTextControl() const
{
  return IsSingleLineTextControl(false);
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::IsTextArea() const
{
  return false;
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return true;
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::IsPasswordTextControl() const
{
  return mType == NS_FORM_INPUT_PASSWORD;
}

NS_IMETHODIMP_(PRInt32)
nsHTMLInputElement::GetCols()
{
  // Else we know (assume) it is an input with size attr
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::size);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    PRInt32 cols = attr->GetIntegerValue();
    if (cols > 0) {
      return cols;
    }
  }

  return DEFAULT_COLS;
}

NS_IMETHODIMP_(PRInt32)
nsHTMLInputElement::GetWrapCols()
{
  return -1; // only textarea's can have wrap cols
}

NS_IMETHODIMP_(PRInt32)
nsHTMLInputElement::GetRows()
{
  return DEFAULT_ROWS;
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::GetDefaultValueFromContent(nsAString& aValue)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    GetDefaultValue(aValue);
    // This is called by the frame to show the value.
    // We have to sanitize it when needed.
    if (!mParserCreating) {
      SanitizeValue(aValue);
    }
  }
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::ValueChanged() const
{
  return mValueChanged;
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::GetTextEditorValue(nsAString& aValue,
                                       bool aIgnoreWrap) const
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->GetValue(aValue, aIgnoreWrap);
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::SetTextEditorValue(const nsAString& aValue,
                                       bool aUserInput)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->SetValue(aValue, aUserInput);
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::InitializeKeyboardEventListeners()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->InitializeKeyboardEventListeners();
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::OnValueChanged(bool aNotify)
{
  UpdateAllValidityStates(aNotify);

  // :-moz-placeholder pseudo-class may change when the value changes.
  // However, we don't want to waste cycles if the state doesn't apply.
  if (PlaceholderApplies() &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      !nsContentUtils::IsFocusedContent(this)) {
    UpdateState(aNotify);
  }
}

NS_IMETHODIMP_(bool)
nsHTMLInputElement::HasCachedSelection()
{
  bool isCached = false;
  nsTextEditorState *state = GetEditorState();
  if (state) {
    isCached = state->IsSelectionCached() &&
               state->HasNeverInitializedBefore() &&
               !state->GetSelectionProperties().IsDefault();
    if (isCached) {
      state->WillInitEagerly();
    }
  }
  return isCached;
}

void
nsHTMLInputElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElement::FieldSetDisabledChanged(aNotify);
}

PRInt32
nsHTMLInputElement::GetFilterFromAccept()
{
  NS_ASSERTION(HasAttr(kNameSpaceID_None, nsGkAtoms::accept),
               "You should not call GetFileFiltersFromAccept if the element"
               " has no accept attribute!");

  PRInt32 filter = 0;
  nsAutoString accept;
  GetAttr(kNameSpaceID_None, nsGkAtoms::accept, accept);

  HTMLSplitOnSpacesTokenizer tokenizer(accept, ',');

  while (tokenizer.hasMoreTokens()) {
    const nsDependentSubstring token = tokenizer.nextToken();

    PRInt32 tokenFilter = 0;
    if (token.EqualsLiteral("image/*")) {
      tokenFilter = nsIFilePicker::filterImages;
    } else if (token.EqualsLiteral("audio/*")) {
      tokenFilter = nsIFilePicker::filterAudio;
    } else if (token.EqualsLiteral("video/*")) {
      tokenFilter = nsIFilePicker::filterVideo;
    }

    if (tokenFilter) {
      // We do not want to set more than one filter so if we found two different
      // kwown tokens, we will return 0 (no filter).
      if (filter && filter != tokenFilter) {
        return 0;
      }
      filter = tokenFilter;
    }
  }

  return filter;
}

void
nsHTMLInputElement::UpdateValidityUIBits(bool aIsFocused)
{
  if (aIsFocused) {
    // If the invalid UI is shown, we should show it while focusing (and
    // update). Otherwise, we should not.
    mCanShowInvalidUI = !IsValid() && ShouldShowValidityUI();

    // If neither invalid UI nor valid UI is shown, we shouldn't show the valid
    // UI while typing.
    mCanShowValidUI = ShouldShowValidityUI();
  } else {
    mCanShowInvalidUI = true;
    mCanShowValidUI = true;
  }
}

