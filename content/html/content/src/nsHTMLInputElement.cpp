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
#include "nsIDOMEventTarget.h"
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
#include "nsIEventStateManager.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMError.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIEditor.h"
#include "nsGUIEvent.h"
#include "nsIIOService.h"

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
#include "nsWidgetsCID.h"
#include "nsILookAndFeel.h"

#include "nsIDOMMutationEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsMutationEvent.h"
#include "nsIEventListenerManager.h"

#include "nsRuleData.h"

// input type=radio
#include "nsIRadioGroupContainer.h"

// input type=file
#include "nsIFile.h"
#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"
#include "nsFileControlFrame.h"
#include "nsTextControlFrame.h"
#include "nsIFilePicker.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIPrivateBrowsingService.h"
#include "nsIContentURIGrouper.h"
#include "nsIContentPrefService.h"
#include "nsIObserverService.h"
#include "nsIPopupWindowManager.h"
#include "nsGlobalWindow.h"

// input type=image
#include "nsImageLoadingContent.h"
#include "nsIDOMWindowInternal.h"

#include "mozAutoDocUpdate.h"
#include "nsHTMLFormElement.h"
#include "nsContentCreatorFunctions.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsContentUtils.h"

#include "nsTextEditRules.h"

// JS headers are needed for the pattern attribute.
#include "jsapi.h"
#include "jscntxt.h"

#include "nsHTMLInputElement.h"

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);
static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

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

class nsHTMLInputElementState : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_INPUT_ELEMENT_STATE_IID)
    NS_DECL_ISUPPORTS

    PRBool IsCheckedSet() {
      return mCheckedSet;
    }

    PRBool GetChecked() {
      return mChecked;
    }

    void SetChecked(PRBool aChecked) {
      mChecked = aChecked;
      mCheckedSet = PR_TRUE;
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
      , mChecked(PR_FALSE)
      , mCheckedSet(PR_FALSE)
    {};
 
  protected:
    nsString mValue;
    nsCOMArray<nsIDOMFile> mFiles;
    PRPackedBool mChecked;
    PRPackedBool mCheckedSet;
};

NS_IMPL_ISUPPORTS1(nsHTMLInputElementState, nsHTMLInputElementState)
NS_DEFINE_STATIC_IID_ACCESSOR(nsHTMLInputElementState, NS_INPUT_ELEMENT_STATE_IID)

class AsyncClickHandler : public nsRunnable {
public:
  AsyncClickHandler(nsHTMLInputElement* aInput)
   : mInput(aInput) {
    
    nsIDocument* doc = aInput->GetOwnerDoc();
    if (doc) {
      nsPIDOMWindow* win = doc->GetWindow();
      if (win)
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
  nsresult rv;

  // Get parent nsIDOMWindowInternal object.
  nsCOMPtr<nsIDocument> doc = mInput->GetOwnerDoc();
  if (!doc)
    return NS_ERROR_FAILURE;

  nsPIDOMWindow* win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  // Check if page is allowed to open the popup
  if (mPopupControlState != openAllowed) {
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

  nsFileControlFrame* frame =
    static_cast<nsFileControlFrame*>(mInput->GetPrimaryFrame());

  PRBool multi;
  rv = mInput->GetMultiple(&multi);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = filePicker->Init(win, title, multi ?
                        (PRInt16)nsIFilePicker::modeOpenMultiple :
                        (PRInt16)nsIFilePicker::modeOpen);
  NS_ENSURE_SUCCESS(rv, rv);

  // We want to get the file filter from the accept attribute and we add the
  // |filterAll| filter to be sure the user has a valid fallback.
  PRUint32 filter = 0;
  if (frame)
    filter = frame->GetFileFilterFromAccept();
  filePicker->AppendFilters(filter | nsIFilePicker::filterAll);

  // If the accept attribute asks for a filter, it has to be the default one.
  if (filter) {
    // We have two filters: |filterAll| and another one. |filterAll| is
    // always the first one (index=0) so we can assume the one we want to be
    // the default is at index 1.
    filePicker->SetFilterIndex(1);
  }

  // Set default directry and filename
  nsAutoString defaultName;

  const nsCOMArray<nsIDOMFile>& oldFiles = mInput->GetFiles();

  if (oldFiles.Count()) {
    nsString path;

    oldFiles[0]->GetMozFullPathInternal(path);

    nsCOMPtr<nsILocalFile> localFile;
    rv = NS_NewLocalFile(path, PR_FALSE, getter_AddRefs(localFile));

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
    PRBool prefSaved = PR_FALSE;
    PRBool loop = PR_TRUE;
    while (NS_SUCCEEDED(iter->HasMoreElements(&loop)) && loop) {
      iter->GetNext(getter_AddRefs(tmp));
      nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(tmp);
      if (localFile) {
        nsString unicodePath;
        rv = localFile->GetPath(unicodePath);
        if (!unicodePath.IsEmpty()) {
          nsCOMPtr<nsIDOMFile> domFile =
            do_QueryObject(new nsDOMFile(localFile, doc));
          newFiles.AppendObject(domFile);
        }
        if (!prefSaved) {
          // Store the last used directory using the content pref service
          rv = nsHTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(doc->GetDocumentURI(), 
                                                                          localFile);
          NS_ENSURE_SUCCESS(rv, rv);
          prefSaved = PR_TRUE;
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
          do_QueryObject(new nsDOMFile(localFile, doc));
        newFiles.AppendObject(domFile);
      }
      // Store the last used directory using the content pref service
      rv = nsHTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(doc->GetDocumentURI(),
                                                                      localFile);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Set new selected files
  if (newFiles.Count()) {
    // The text control frame (if there is one) isn't going to send a change
    // event because it will think this is done by a script.
    // So, we can safely send one by ourself.
    mInput->SetFiles(newFiles);
    nsContentUtils::DispatchTrustedEvent(mInput->GetOwnerDoc(),
                                         static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                                         NS_LITERAL_STRING("change"), PR_FALSE,
                                         PR_FALSE);
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
    observerService->AddObserver(gUploadLastDir, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_TRUE);
    observerService->AddObserver(gUploadLastDir, "browser:purge-session-history", PR_TRUE);
  }
}

void 
nsHTMLInputElement::DestroyUploadLastDir() {
  NS_IF_RELEASE(gUploadLastDir);
}

UploadLastDir::UploadLastDir():
  mInPrivateBrowsing(PR_FALSE)
{
  nsCOMPtr<nsIPrivateBrowsingService> pbService =
    do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
  if (pbService) {
    pbService->GetPrivateBrowsingEnabled(&mInPrivateBrowsing);
  }

  mUploadLastDirStore.Init();
}

nsresult
UploadLastDir::FetchLastUsedDirectory(nsIURI* aURI, nsILocalFile** aFile)
{
  NS_PRECONDITION(aURI, "aURI is null");
  NS_PRECONDITION(aFile, "aFile is null");
  // Retrieve the data from memory if it's present during private browsing mode,
  // otherwise fall through to check the CPS
  if (mInPrivateBrowsing) {
    nsCOMPtr<nsIContentURIGrouper> hostnameGrouperService =
      do_GetService(NS_HOSTNAME_GROUPER_SERVICE_CONTRACTID);
    if (!hostnameGrouperService)
      return NS_ERROR_NOT_AVAILABLE;
    nsString group;
    hostnameGrouperService->Group(aURI, group);

    if (mUploadLastDirStore.Get(group, aFile)) {
      return NS_OK;
    }
  }

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
  PRBool hasPref;
  if (NS_SUCCEEDED(contentPrefService->HasPref(uri, CPS_PREF_NAME, &hasPref)) && hasPref) {
    nsCOMPtr<nsIVariant> pref;
    contentPrefService->GetPref(uri, CPS_PREF_NAME, nsnull, getter_AddRefs(pref));
    nsString prefStr;
    pref->GetAsAString(prefStr);

    nsCOMPtr<nsILocalFile> localFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
    if (!localFile)
      return NS_ERROR_OUT_OF_MEMORY;
    localFile->InitWithPath(prefStr);

    *aFile = localFile;
    NS_ADDREF(*aFile);
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
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(parentFile);

  // Store the data in memory instead of the CPS during private browsing mode
  if (mInPrivateBrowsing) {
    nsCOMPtr<nsIContentURIGrouper> hostnameGrouperService =
      do_GetService(NS_HOSTNAME_GROUPER_SERVICE_CONTRACTID);
    if (!hostnameGrouperService)
      return NS_ERROR_NOT_AVAILABLE;
    nsString group;
    hostnameGrouperService->Group(aURI, group);

    return mUploadLastDirStore.Put(group, localFile);
  }

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
  if (strcmp(aTopic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
    if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(aData)) {
      mInPrivateBrowsing = PR_TRUE;
    } else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(aData)) {
      mInPrivateBrowsing = PR_FALSE;
      if (mUploadLastDirStore.IsInitialized()) {
        mUploadLastDirStore.Clear();
      }
    }
  } else if (strcmp(aTopic, "browser:purge-session-history") == 0) {
    if (mUploadLastDirStore.IsInitialized()) {
      mUploadLastDirStore.Clear();
    }
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
                                       PRUint32 aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mType(kInputDefaultType->value),
    mBitField(0)
{
  SET_BOOLBIT(mBitField, BF_PARSER_CREATING, aFromParser);
  SET_BOOLBIT(mBitField, BF_INHIBIT_RESTORATION,
      aFromParser & NS_FROM_PARSER_FRAGMENT);
  mInputData.mState = new nsTextEditorState(this);
  NS_ADDREF(mInputData.mState);
  
  if (!gUploadLastDir)
    nsHTMLInputElement::InitUploadLastDir();
}

nsHTMLInputElement::~nsHTMLInputElement()
{
  DestroyImageLoadingContent();
  FreeData();
}

void
nsHTMLInputElement::FreeData()
{
  if (!IsSingleLineTextControl(PR_FALSE)) {
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
  if (!IsSingleLineTextControl(PR_FALSE)) {
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
  if (tmp->IsSingleLineTextControl(PR_FALSE)) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_MEMBER(mInputData.mState, nsTextEditorState)
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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
  nsHTMLInputElement *it = new nsHTMLInputElement(ni.forget(), PR_FALSE);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsCOMPtr<nsINode> kungFuDeathGrip = it;
  nsresult rv = CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (mType) {
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_URL:
      if (GET_BOOLBIT(mBitField, BF_VALUE_CHANGED)) {
        // We don't have our default value anymore.  Set our value on
        // the clone.
        // XXX GetValue should be const
        nsAutoString value;
        const_cast<nsHTMLInputElement*>(this)->GetValue(value);
        // SetValueInternal handles setting the VALUE_CHANGED bit for us
        it->SetValueInternal(value, PR_FALSE, PR_TRUE);
      }
      break;
    case NS_FORM_INPUT_FILE:
      if (it->GetOwnerDoc()->IsStaticDocument()) {
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
      if (GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED)) {
        // We no longer have our original checked state.  Set our
        // checked state on the clone.
        it->DoSetChecked(GetChecked(), PR_FALSE, PR_TRUE);
      }
      break;
    case NS_FORM_INPUT_IMAGE:
      if (it->GetOwnerDoc()->IsStaticDocument()) {
        CreateStaticImageClone(it);
      }
      break;
    default:
      break;
  }

  kungFuDeathGrip.swap(*aResult);

  return NS_OK;
}

nsresult
nsHTMLInputElement::BeforeSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                  const nsAString* aValue,
                                  PRBool aNotify)
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
        (mForm || !(GET_BOOLBIT(mBitField, BF_PARSER_CREATING)))) {
      WillRemoveFromRadioGroup(aNotify);
    } else if (aNotify && aName == nsGkAtoms::src &&
               mType == NS_FORM_INPUT_IMAGE) {
      if (aValue) {
        LoadImage(*aValue, PR_TRUE, aNotify);
      } else {
        // Null value means the attr got unset; drop the image
        CancelImageRequests(aNotify);
      }
    } else if (aNotify && aName == nsGkAtoms::disabled) {
      SET_BOOLBIT(mBitField, BF_DISABLED_CHANGED, PR_TRUE);
    }
  }

  return nsGenericHTMLFormElement::BeforeSetAttr(aNameSpaceID, aName,
                                                 aValue, aNotify);
}

nsresult
nsHTMLInputElement::AfterSetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                                 const nsAString* aValue,
                                 PRBool aNotify)
{
  // States changes that have to be passed to ContentStatesChanged().
  PRInt32 states = 0;

  if (aNameSpaceID == kNameSpaceID_None) {
    //
    // When name or type changes, radio should be added to radio group.
    // (type changes are handled in the form itself currently)
    // If the parser is not done creating the radio, we also should not do it.
    //
    if ((aName == nsGkAtoms::name ||
         (aName == nsGkAtoms::type && !mForm)) &&
        mType == NS_FORM_INPUT_RADIO &&
        (mForm || !(GET_BOOLBIT(mBitField, BF_PARSER_CREATING)))) {
      AddedToRadioGroup();
    }

    // If @value is changed and BF_VALUE_CHANGED is false, @value is the value
    // of the element so, if the value of the element is different than @value,
    // we have to re-set it. This is only the case when GetValueMode() returns
    // VALUE_MODE_VALUE.
    if (aName == nsGkAtoms::value &&
        !GET_BOOLBIT(mBitField, BF_VALUE_CHANGED) &&
        GetValueMode() == VALUE_MODE_VALUE) {
      SetDefaultValueAsValue();
    }

    //
    // Checked must be set no matter what type of control it is, since
    // GetChecked() must reflect the new value
    if (aName == nsGkAtoms::checked &&
        !GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED)) {
      // Delay setting checked if the parser is creating this element (wait
      // until everything is set)
      if (GET_BOOLBIT(mBitField, BF_PARSER_CREATING)) {
        SET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED, PR_TRUE);
      } else {
        PRBool defaultChecked;
        GetDefaultChecked(&defaultChecked);
        DoSetChecked(defaultChecked, PR_TRUE, PR_TRUE);
        SetCheckedChanged(PR_FALSE);
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

      // If we are changing type from File/Text/Tel/Passwd to other input types
      // we need save the mValue into value attribute
      if (mInputData.mValue &&
          mType != NS_FORM_INPUT_EMAIL &&
          mType != NS_FORM_INPUT_TEXT &&
          mType != NS_FORM_INPUT_SEARCH &&
          mType != NS_FORM_INPUT_PASSWORD &&
          mType != NS_FORM_INPUT_TEL &&
          mType != NS_FORM_INPUT_URL &&
          mType != NS_FORM_INPUT_FILE) {
        SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                NS_ConvertUTF8toUTF16(mInputData.mValue), PR_FALSE);
        FreeData();
      }

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
          LoadImage(src, PR_FALSE, aNotify);
        }
      }

      // Changing type affects the applicability of some states.  Just notify
      // on them all now, just in case.  Note that we can't rely on the
      // notifications LoadImage or CancelImageRequests might have sent, because
      // those didn't include all the possibly-changed states in the mask. We
      // have to do this here because we just updated mType, so the code in
      // nsGenericElement::SetAttrAndNotify didn't see the new states.
      states |= NS_EVENT_STATE_CHECKED |
                NS_EVENT_STATE_DEFAULT |
                NS_EVENT_STATE_BROKEN |
                NS_EVENT_STATE_USERDISABLED |
                NS_EVENT_STATE_SUPPRESSED |
                NS_EVENT_STATE_LOADING |
                NS_EVENT_STATE_MOZ_READONLY |
                NS_EVENT_STATE_MOZ_READWRITE |
                NS_EVENT_STATE_REQUIRED |
                NS_EVENT_STATE_OPTIONAL |
                NS_EVENT_STATE_VALID |
                NS_EVENT_STATE_INVALID |
                NS_EVENT_STATE_INDETERMINATE |
                NS_EVENT_STATE_MOZ_PLACEHOLDER |
                NS_EVENT_STATE_MOZ_SUBMITINVALID;
    }

    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }

      states |= NS_EVENT_STATE_REQUIRED | NS_EVENT_STATE_OPTIONAL |
                NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
    } else if (aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
    } else if (aName == nsGkAtoms::pattern) {
      UpdatePatternMismatchValidityState();
      states |= NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID;
    }

    if (aNotify) {
      nsIDocument* doc = GetCurrentDoc();

      if (aName == nsGkAtoms::type) {
        UpdateEditableState();
      } else if (IsSingleLineTextControl(PR_FALSE) && aName == nsGkAtoms::readonly) {
        UpdateEditableState();
        states |= NS_EVENT_STATE_MOZ_READONLY | NS_EVENT_STATE_MOZ_READWRITE;
      }

      if (doc && states) {
        MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
        doc->ContentStatesChanged(this, nsnull, states);
      }
    }
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
NS_IMPL_STRING_ATTR(nsHTMLInputElement, AccessKey, accesskey)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Alt, alt)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Autocomplete, autocomplete,
                                kInputDefaultAutocomplete->tag)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Autofocus, autofocus)
//NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(nsHTMLInputElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, FormAction, formaction)
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
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLInputElement, TabIndex, tabindex, 0)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(nsHTMLInputElement, Value, value)
//NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Size, size, 0)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Pattern, pattern)
NS_IMPL_STRING_ATTR(nsHTMLInputElement, Placeholder, placeholder)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(nsHTMLInputElement, Type, type,
                                kInputDefaultType->tag)

NS_IMETHODIMP
nsHTMLInputElement::GetIndeterminate(PRBool* aValue)
{
  *aValue = GET_BOOLBIT(mBitField, BF_INDETERMINATE);
  return NS_OK;
}

nsresult
nsHTMLInputElement::SetIndeterminateInternal(PRBool aValue,
                                             PRBool aShouldInvalidate)
{
  SET_BOOLBIT(mBitField, BF_INDETERMINATE, aValue);

  if (aShouldInvalidate) {
    // Repaint the frame
    nsIFrame* frame = GetPrimaryFrame();
    if (frame)
      frame->InvalidateFrameSubtree();
  }

  // Notify the document so it can update :indeterminate pseudoclass rules
  nsIDocument* document = GetCurrentDoc();
  if (document) {
    mozAutoDocUpdate upd(document, UPDATE_CONTENT_STATE, PR_TRUE);
    document->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_INDETERMINATE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetIndeterminate(PRBool aValue)
{
  return SetIndeterminateInternal(aValue, PR_TRUE);
}

NS_IMETHODIMP
nsHTMLInputElement::GetSize(PRUint32* aValue)
{
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::size);
  if (attrVal && attrVal->Type() == nsAttrValue::eInteger) {
    *aValue = attrVal->GetIntegerValue();
  }
  else {
    *aValue = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::SetSize(PRUint32 aValue)
{
  nsAutoString val;
  val.AppendInt(aValue);

  return SetAttr(kNameSpaceID_None, nsGkAtoms::size, val, PR_TRUE);
}

NS_IMETHODIMP 
nsHTMLInputElement::GetValue(nsAString& aValue)
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    state->GetValue(aValue, PR_TRUE);
    return NS_OK;
  }

  if (mType == NS_FORM_INPUT_FILE) {
    if (nsContentUtils::IsCallerTrustedForCapability("UniversalFileRead")) {
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
  }

  // Treat value == defaultValue for other input elements
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::value, aValue) &&
      (mType == NS_FORM_INPUT_RADIO || mType == NS_FORM_INPUT_CHECKBOX)) {
    // The default value of a radio or checkbox input is "on".
    aValue.AssignLiteral("on");
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::SetValue(const nsAString& aValue)
{
  // check security.  Note that setting the value to the empty string is always
  // OK and gives pages a way to clear a file input if necessary.
  if (mType == NS_FORM_INPUT_FILE) {
    if (!aValue.IsEmpty()) {
      if (!nsContentUtils::IsCallerTrustedForCapability("UniversalFileRead")) {
        // setting the value of a "FILE" input widget requires the
        // UniversalFileRead privilege
        return NS_ERROR_DOM_SECURITY_ERR;
      }
      const PRUnichar *name = PromiseFlatString(aValue).get();
      return MozSetFileNameArray(&name, 1);
    }
    else {
      ClearFiles();
    }
  }
  else {
    SetValueInternal(aValue, PR_FALSE, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::GetList(nsIDOMHTMLElement** aValue)
{
  nsAutoString dataListId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::list, dataListId);
  if (!dataListId.IsEmpty()) {
    nsIDocument* doc = GetCurrentDoc();

    if (doc) {
      Element* elem = doc->GetElementById(dataListId);

      if (elem) {
        CallQueryInterface(elem, aValue);
        return NS_OK;
      }
    }
  }

  *aValue = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::MozGetFileNameArray(PRUint32 *aLength, PRUnichar ***aFileNames)
{
  if (!nsContentUtils::IsCallerTrustedForCapability("UniversalFileRead")) {
    // Since this function returns full paths it's important that normal pages
    // can't call it.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  *aLength = mFiles.Count();
  PRUnichar **ret =
    static_cast<PRUnichar **>(NS_Alloc(mFiles.Count() * sizeof(PRUnichar*)));
  
  for (PRUint32 i = 0; i <  mFiles.Count(); i++) {
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
  if (!nsContentUtils::IsCallerTrustedForCapability("UniversalFileRead")) {
    // setting the value of a "FILE" input widget requires the
    // UniversalFileRead privilege
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
                      PR_FALSE, getter_AddRefs(localFile));
      file = do_QueryInterface(localFile);
    }

    if (file) {
      nsCOMPtr<nsIDOMFile> domFile = new nsDOMFile(file, GetOwnerDoc());
      files.AppendObject(domFile);
    } else {
      continue; // Not much we can do if the file doesn't exist
    }

  }

  SetFiles(files);

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::MozIsTextField(PRBool aExcludePassword, PRBool* aResult)
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
    SetValueInternal(aValue, PR_TRUE, PR_TRUE);
  }
  return NS_OK;
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
nsHTMLInputElement::GetPlaceholderNode()
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    return state->GetPlaceholderNode();
  }
  return nsnull;
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::UpdatePlaceholderText(PRBool aNotify)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->UpdatePlaceholderText(aNotify);
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::SetPlaceholderClass(PRBool aVisible, PRBool aNotify)
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->SetPlaceholderClass(aVisible, aNotify);
  }
}

void
nsHTMLInputElement::GetDisplayFileName(nsAString& aValue) const
{
  if (GetOwnerDoc()->IsStaticDocument()) {
    aValue = mStaticDocFileList;
    return;
  }

  aValue.Truncate();
  for (PRUint32 i = 0; i < (PRUint32)mFiles.Count(); ++i) {
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
nsHTMLInputElement::SetFiles(const nsCOMArray<nsIDOMFile>& aFiles)
{
  mFiles.Clear();
  mFiles.AppendObjects(aFiles);

  // No need to flush here, if there's no frame at this point we
  // don't need to force creation of one just to tell it about this
  // new value.  We just want the display to update as needed.
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_FALSE);
  if (formControlFrame) {
    nsAutoString readableValue;
    GetDisplayFileName(readableValue);
    formControlFrame->SetFormProperty(nsGkAtoms::value, readableValue);
  }

  UpdateFileList();

  SetValueChanged(PR_TRUE);
  UpdateAllValidityStates(PR_TRUE);
}

const nsCOMArray<nsIDOMFile>&
nsHTMLInputElement::GetFiles()
{
  return mFiles;
}

nsresult
nsHTMLInputElement::UpdateFileList()
{
  if (mFileList) {
    mFileList->Clear();

    const nsCOMArray<nsIDOMFile>& files = GetFiles();
    for (PRUint32 i = 0; i < (PRUint32)files.Count(); ++i) {
      if (!mFileList->Append(files[i])) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult
nsHTMLInputElement::SetValueInternal(const nsAString& aValue,
                                     PRBool aUserInput,
                                     PRBool aSetValueChanged)
{
  NS_PRECONDITION(mType != NS_FORM_INPUT_FILE,
                  "Don't call SetValueInternal for file inputs");

  if (mType == NS_FORM_INPUT_FILE) {
    return NS_ERROR_UNEXPECTED;
  }

  if (IsSingleLineTextControl(PR_FALSE)) {
    // At the moment, only single line text control have to sanitize their value
    // Because we have to create a new string for that, we should prevent doing
    // it if it's useless.
    nsAutoString value(aValue);
    SanitizeValue(value);

    if (aSetValueChanged) {
      SetValueChanged(PR_TRUE);
    }
    mInputData.mState->SetValue(value, aUserInput);

    if (PlaceholderApplies() &&
        HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)) {
      nsIDocument* doc = GetCurrentDoc();
      if (doc) {
        mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
        doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_MOZ_PLACEHOLDER);
      }
    }

    return NS_OK;
  }

  // If the value of a hidden input was changed, we mark it changed so that we
  // will know we need to save / restore the value.  Yes, we are overloading
  // the meaning of ValueChanged just a teensy bit to save a measly byte of
  // storage space in nsHTMLInputElement.  Yes, you are free to make a new flag,
  // NEED_TO_SAVE_VALUE, at such time as mBitField becomes a 16-bit value.
  if (mType == NS_FORM_INPUT_HIDDEN) {
    SetValueChanged(PR_TRUE);
  }

  // Treat value == defaultValue for other input elements.
  return nsGenericHTMLFormElement::SetAttr(kNameSpaceID_None,
                                           nsGkAtoms::value, aValue,
                                           PR_TRUE);
}

NS_IMETHODIMP
nsHTMLInputElement::SetValueChanged(PRBool aValueChanged)
{
  SET_BOOLBIT(mBitField, BF_VALUE_CHANGED, aValueChanged);
  if (!aValueChanged) {
    if (!IsSingleLineTextControl(PR_FALSE)) {
      FreeData();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsHTMLInputElement::GetChecked(PRBool* aChecked)
{
  *aChecked = GetChecked();
  return NS_OK;
}

void
nsHTMLInputElement::SetCheckedChanged(PRBool aCheckedChanged)
{
  DoSetCheckedChanged(aCheckedChanged, PR_TRUE);
}

void
nsHTMLInputElement::DoSetCheckedChanged(PRBool aCheckedChanged,
                                        PRBool aNotify)
{
  if (mType == NS_FORM_INPUT_RADIO) {
    if (GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED) != aCheckedChanged) {
      nsCOMPtr<nsIRadioVisitor> visitor;
      NS_GetRadioSetCheckedChangedVisitor(aCheckedChanged,
                                          getter_AddRefs(visitor));
      VisitGroup(visitor, aNotify);
    }
  } else {
    SetCheckedChangedInternal(aCheckedChanged);
  }
}

void
nsHTMLInputElement::SetCheckedChangedInternal(PRBool aCheckedChanged)
{
  SET_BOOLBIT(mBitField, BF_CHECKED_CHANGED, aCheckedChanged);
}


PRBool
nsHTMLInputElement::GetCheckedChanged()
{
  return GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED);
}

NS_IMETHODIMP
nsHTMLInputElement::SetChecked(PRBool aChecked)
{
  return DoSetChecked(aChecked, PR_TRUE, PR_TRUE);
}

nsresult
nsHTMLInputElement::DoSetChecked(PRBool aChecked, PRBool aNotify,
                                 PRBool aSetValueChanged)
{
  nsresult rv = NS_OK;

  // If the user or JS attempts to set checked, whether it actually changes the
  // value or not, we say the value was changed so that defaultValue don't
  // affect it no more.
  if (aSetValueChanged) {
    DoSetCheckedChanged(PR_TRUE, aNotify);
  }

  //
  // Don't do anything if we're not changing whether it's checked (it would
  // screw up state actually, especially when you are setting radio button to
  // false)
  //
  if (GetChecked() == aChecked) {
    return NS_OK;
  }

  //
  // Set checked
  //
  if (mType == NS_FORM_INPUT_RADIO) {
    //
    // For radio button, we need to do some extra fun stuff
    //
    if (aChecked) {
      rv = RadioSetChecked(aNotify);
    } else {
      nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
      if (container) {
        nsAutoString name;
        if (GetNameIfExists(name)) {
          container->SetCurrentRadioButton(name, nsnull);
        }
      }
      // SetCheckedInternal is going to ask all radios to update their
      // validity state. We have to be sure the radio group container knows
      // the currently selected radio.
      SetCheckedInternal(PR_FALSE, aNotify);
    }
  } else {
    SetCheckedInternal(aChecked, aNotify);
  }

  return rv;
}

nsresult
nsHTMLInputElement::RadioSetChecked(PRBool aNotify)
{
  nsresult rv = NS_OK;

  //
  // Find the selected radio button so we can deselect it
  //
  nsCOMPtr<nsIDOMHTMLInputElement> currentlySelected = GetSelectedRadioButton();

  //
  // Deselect the currently selected radio button
  //
  if (currentlySelected) {
    // Pass PR_TRUE for the aNotify parameter since the currently selected
    // button is already in the document.
    static_cast<nsHTMLInputElement*>
               (static_cast<nsIDOMHTMLInputElement*>(currentlySelected))->SetCheckedInternal(PR_FALSE, PR_TRUE);
  }

  //
  // Let the group know that we are now the One True Radio Button
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  nsAutoString name;
  if (container && GetNameIfExists(name)) {
    rv = container->SetCurrentRadioButton(name, this);
  }

  // SetCheckedInternal is going to ask all radios to update their
  // validity state. We have to be sure the radio group container knows
  // the currently selected radio.
  if (NS_SUCCEEDED(rv)) {
    SetCheckedInternal(PR_TRUE, aNotify);
  }

  return rv;
}

/* virtual */ already_AddRefed<nsIRadioGroupContainer>
nsHTMLInputElement::GetRadioGroupContainer()
{
  nsIRadioGroupContainer* retval = nsnull;
  if (mForm) {
    CallQueryInterface(mForm, &retval);
  } else {
    nsIDocument* currentDoc = GetCurrentDoc();
    if (currentDoc) {
      CallQueryInterface(currentDoc, &retval);
    }
  }
  return retval;
}

already_AddRefed<nsIDOMHTMLInputElement>
nsHTMLInputElement::GetSelectedRadioButton()
{
  nsIDOMHTMLInputElement* selected;
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();

  if (!container) {
    return nsnull;
  }

  nsAutoString name;
  if (!GetNameIfExists(name)) {
    return nsnull;
  }

  container->GetCurrentRadioButton(name, &selected);
  return selected;
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
    nsCOMPtr<nsIContent> submitContent(do_QueryInterface(submitControl));
    NS_ASSERTION(submitContent, "Form control not implementing nsIContent?!");
    // Fire the button's onclick handler and let the button handle
    // submitting the form.
    nsMouseEvent event(PR_TRUE, NS_MOUSE_CLICK, nsnull, nsMouseEvent::eReal);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(submitContent, &event, &status);
  } else if (mForm->HasSingleTextControl() &&
             (mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate) ||
              mForm->CheckValidFormSubmission())) {
    // TODO: removing this code and have the submit event sent by the form,
    // bug 592124.
    // If there's only one text control, just submit the form
    // Hold strong ref across the event
    nsRefPtr<nsHTMLFormElement> form(mForm);
    nsFormEvent event(PR_TRUE, NS_FORM_SUBMIT);
    nsEventStatus status  = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(mForm, &event, &status);
  }

  return NS_OK;
}

void
nsHTMLInputElement::SetCheckedInternal(PRBool aChecked, PRBool aNotify)
{
  //
  // Set the value
  //
  SET_BOOLBIT(mBitField, BF_CHECKED, aChecked);

  //
  // Notify the frame
  //
  if (mType == NS_FORM_INPUT_CHECKBOX || mType == NS_FORM_INPUT_RADIO) {
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
    }
  }

  // Notify the document that the CSS :checked pseudoclass for this element
  // has changed state.
  if (aNotify) {
    nsIDocument* document = GetCurrentDoc();
    if (document) {
      mozAutoDocUpdate upd(document, UPDATE_CONTENT_STATE, aNotify);
      document->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_CHECKED);
    }
  }

  if (mType == NS_FORM_INPUT_CHECKBOX) {
    UpdateAllValidityStates(aNotify);
  }

  if (mType == NS_FORM_INPUT_RADIO) {
    // OnValueChanged is going to be called for all radios in the radio group.
    nsCOMPtr<nsIRadioVisitor> visitor =
      NS_GetRadioUpdateValueMissingVisitor(aNotify);
    VisitGroup(visitor, aNotify);
  }
}


void
nsHTMLInputElement::FireOnChange()
{
  //
  // Since the value is changing, send out an onchange event (bug 23571)
  //
  nsEventStatus status = nsEventStatus_eIgnore;
  nsEvent event(PR_TRUE, NS_FORM_CHANGE);
  nsRefPtr<nsPresContext> presContext = GetPresContext();
  nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), presContext,
                              &event, nsnull, &status);
}

NS_IMETHODIMP
nsHTMLInputElement::Blur()
{
  return nsGenericHTMLElement::Blur();
}

NS_IMETHODIMP
nsHTMLInputElement::Focus()
{
  if (mType == NS_FORM_INPUT_FILE) {
    // for file inputs, focus the button instead
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      nsIFrame* childFrame = frame->GetFirstChild(nsnull);
      while (childFrame) {
        // see if the child is a button control
        nsCOMPtr<nsIFormControl> formCtrl =
          do_QueryInterface(childFrame->GetContent());
        if (formCtrl && formCtrl->GetType() == NS_FORM_INPUT_BUTTON) {
          nsCOMPtr<nsIDOMElement> element(do_QueryInterface(formCtrl));
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm && element)
            fm->SetFocus(element, 0);
          break;
        }

        childFrame = childFrame->GetNextSibling();
      }
    }

    return NS_OK;
  }

  return nsGenericHTMLElement::Focus();
}

NS_IMETHODIMP
nsHTMLInputElement::Select()
{
  if (!IsSingleLineTextControl(PR_FALSE)) {
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

PRBool
nsHTMLInputElement::DispatchSelectEvent(nsPresContext* aPresContext)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  // If already handling select event, don't dispatch a second.
  if (!GET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT)) {
    nsEvent event(nsContentUtils::IsCallerChrome(), NS_FORM_SELECTED);

    SET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT, PR_TRUE);
    nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                aPresContext, &event, nsnull, &status);
    SET_BOOLBIT(mBitField, BF_HANDLING_SELECT_EVENT, PR_FALSE);
  }

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  return (status == nsEventStatus_eIgnore);
}
    
void
nsHTMLInputElement::SelectAll(nsPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }
}

NS_IMETHODIMP
nsHTMLInputElement::Click()
{
  nsresult rv = NS_OK;

  if (GET_BOOLBIT(mBitField, BF_HANDLING_CLICK)) // Fixes crash as in bug 41599
      return rv;                      // --heikki@netscape.com

  // first see if we are disabled or not. If disabled then do nothing.
  nsAutoString disabled;
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    return NS_OK;
  }

  // see what type of input we are.  Only click button, checkbox, radio,
  // reset, submit, & image
  if (mType == NS_FORM_INPUT_BUTTON   ||
      mType == NS_FORM_INPUT_CHECKBOX ||
      mType == NS_FORM_INPUT_RADIO    ||
      mType == NS_FORM_INPUT_RESET    ||
      mType == NS_FORM_INPUT_SUBMIT   ||
      mType == NS_FORM_INPUT_IMAGE    ||
      mType == NS_FORM_INPUT_FILE) {

    // Strong in case the event kills it
    nsCOMPtr<nsIDocument> doc = GetCurrentDoc();
    if (!doc) {
      return rv;
    }

    nsCOMPtr<nsIPresShell> shell = doc->GetShell();
    nsRefPtr<nsPresContext> context = nsnull;
    if (shell) {
      context = shell->GetPresContext();
    }

    if (!context) {
      doc->FlushPendingNotifications(Flush_Frames);
      shell = doc->GetShell();
      if (shell) {
        context = shell->GetPresContext();
      }
    }

    if (context) {
      // Click() is never called from native code, but it may be
      // called from chrome JS. Mark this event trusted if Click()
      // is called from chrome code.
      nsMouseEvent event(nsContentUtils::IsCallerChrome(),
                         NS_MOUSE_CLICK, nsnull, nsMouseEvent::eReal);
      event.inputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_UNKNOWN;
      nsEventStatus status = nsEventStatus_eIgnore;

      SET_BOOLBIT(mBitField, BF_HANDLING_CLICK, PR_TRUE);
      if (mType == NS_FORM_INPUT_FILE){
        FireAsyncClickHandler();
      }
      nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this), context,
                                  &event, nsnull, &status);

      SET_BOOLBIT(mBitField, BF_HANDLING_CLICK, PR_FALSE);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLInputElement::FireAsyncClickHandler()
{
  nsCOMPtr<nsIRunnable> event = new AsyncClickHandler(this);
  return NS_DispatchToMainThread(event);
}

PRBool
nsHTMLInputElement::NeedToInitializeEditorForEvent(nsEventChainPreVisitor& aVisitor) const
{
  // We only need to initialize the editor for single line input controls because they
  // are lazily initialized.  We don't need to initialize the control for
  // certain types of events, because we know that those events are safe to be
  // handled without the editor being initialized.  These events include:
  // mousein/move/out, and DOM mutation events.
  if (IsSingleLineTextControl(PR_FALSE) &&
      aVisitor.mEvent->eventStructType != NS_MUTATION_EVENT) {

    switch (aVisitor.mEvent->message) {
    case NS_MOUSE_MOVE:
    case NS_MOUSE_ENTER:
    case NS_MOUSE_EXIT:
    case NS_MOUSE_ENTER_SYNTH:
    case NS_MOUSE_EXIT_SYNTH:
      return PR_FALSE;
      break;
    }
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsHTMLInputElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = PR_FALSE;
  PRBool disabled;
  nsresult rv = GetDisabled(&disabled);
  NS_ENSURE_SUCCESS(rv, rv);
  if (disabled) {
    return NS_OK;
  }
  
  // For some reason or another we also need to check if the style shows us
  // as disabled.
  {
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      const nsStyleUserInterface* uiStyle = frame->GetStyleUserInterface();

      if (uiStyle->mUserInput == NS_STYLE_USER_INPUT_NONE ||
          uiStyle->mUserInput == NS_STYLE_USER_INPUT_DISABLED) {
        return NS_OK;
      }
    }
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
  PRBool outerActivateEvent =
    (NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) ||
     (aVisitor.mEvent->message == NS_UI_ACTIVATE &&
      !GET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE)));

  if (outerActivateEvent) {
    aVisitor.mItemFlags |= NS_OUTER_ACTIVATE_EVENT;
  }

  PRBool originalCheckedValue = PR_FALSE;

  if (outerActivateEvent) {
    SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_FALSE);

    switch(mType) {
      case NS_FORM_INPUT_CHECKBOX:
        {
          if (GET_BOOLBIT(mBitField, BF_INDETERMINATE)) {
            // indeterminate is always set to FALSE when the checkbox is toggled
            SetIndeterminateInternal(PR_FALSE, PR_FALSE);
            aVisitor.mItemFlags |= NS_ORIGINAL_INDETERMINATE_VALUE;
          }

          GetChecked(&originalCheckedValue);
          DoSetChecked(!originalCheckedValue, PR_TRUE, PR_TRUE);
          SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_TRUE);
        }
        break;

      case NS_FORM_INPUT_RADIO:
        {
          nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton = GetSelectedRadioButton();
          aVisitor.mItemData = selectedRadioButton;

          originalCheckedValue = GetChecked();
          if (!originalCheckedValue) {
            DoSetChecked(PR_TRUE, PR_TRUE, PR_TRUE);
            SET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED, PR_TRUE);
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
    } //switch
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
  if (IsSingleLineTextControl(PR_FALSE) &&
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

static PRBool
SelectTextFieldOnFocus()
{
  if (!gSelectTextFieldOnFocus) {
    nsCOMPtr<nsILookAndFeel> lookNFeel(do_GetService(kLookAndFeelCID));
    if (lookNFeel) {
      PRInt32 selectTextfieldsOnKeyFocus = -1;
      lookNFeel->GetMetric(nsILookAndFeel::eMetric_SelectTextfieldsOnKeyFocus,
                           selectTextfieldsOnKeyFocus);
      gSelectTextFieldOnFocus = selectTextfieldsOnKeyFocus != 0 ? 1 : -1;
    }
    else {
      gSelectTextFieldOnFocus = -1;
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

  if (PlaceholderApplies() &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      // TODO: checking if the value is empty could be a good idea but we do not
      // have a simple way to do that, see bug 585100
      (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
       aVisitor.mEvent->message == NS_BLUR_CONTENT)) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_MOZ_PLACEHOLDER);
    }
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
  PRBool outerActivateEvent = !!(aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT);
  PRBool originalCheckedValue =
    !!(aVisitor.mItemFlags & NS_ORIGINAL_CHECKED_VALUE);
  PRBool noContentDispatch = !!(aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH);
  PRUint8 oldType = NS_CONTROL_TYPE(aVisitor.mItemFlags);
  // Ideally we would make the default action for click and space just dispatch
  // DOMActivate, and the default action for DOMActivate flip the checkbox/
  // radio state and fire onchange.  However, for backwards compatibility, we
  // need to flip the state before firing click, and we need to fire click
  // when space is pressed.  So, we just nest the firing of DOMActivate inside
  // the click event handling, and allow cancellation of DOMActivate to cancel
  // the click.
  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault &&
      !IsSingleLineTextControl(PR_TRUE) &&
      NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent)) {
    nsUIEvent actEvent(NS_IS_TRUSTED_EVENT(aVisitor.mEvent), NS_UI_ACTIVATE, 1);

    nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
    if (shell) {
      nsEventStatus status = nsEventStatus_eIgnore;
      SET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE, PR_TRUE);
      rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
      SET_BOOLBIT(mBitField, BF_IN_INTERNAL_ACTIVATE, PR_FALSE);

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
    } //switch
  }

  // Reset the flag for other content besides this text field
  aVisitor.mEvent->flags |=
    noContentDispatch ? NS_EVENT_FLAG_NO_CONTENT_DISPATCH : NS_EVENT_FLAG_NONE;

  // now check to see if the event was "cancelled"
  if (GET_BOOLBIT(mBitField, BF_CHECKED_IS_TOGGLED) && outerActivateEvent) {
    if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
      // if it was cancelled and a radio button, then set the old
      // selected btn to TRUE. if it is a checkbox then set it to its
      // original value
      nsCOMPtr<nsIDOMHTMLInputElement> selectedRadioButton =
        do_QueryInterface(aVisitor.mItemData);
      if (selectedRadioButton) {
        selectedRadioButton->SetChecked(PR_TRUE);
        // If this one is no longer a radio button we must reset it back to
        // false to cancel the action.  See how the web of hack grows?
        if (mType != NS_FORM_INPUT_RADIO) {
          DoSetChecked(PR_FALSE, PR_TRUE, PR_TRUE);
        }
      } else if (oldType == NS_FORM_INPUT_CHECKBOX) {
        PRBool originalIndeterminateValue =
          !!(aVisitor.mItemFlags & NS_ORIGINAL_INDETERMINATE_VALUE);
        SetIndeterminateInternal(originalIndeterminateValue, PR_FALSE);
        DoSetChecked(originalCheckedValue, PR_TRUE, PR_TRUE);
      }
    } else {
      FireOnChange();
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
          if (fm && IsSingleLineTextControl(PR_FALSE) &&
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
                event.inputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_KEYBOARD;
                nsEventStatus status = nsEventStatus_eIgnore;

                nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                            aVisitor.mPresContext, &event,
                                            nsnull, &status);
              } // case
            } // switch
          }
          if (aVisitor.mEvent->message == NS_KEY_PRESS &&
              mType == NS_FORM_INPUT_RADIO && !keyEvent->isAlt &&
              !keyEvent->isControl && !keyEvent->isMeta) {
            PRBool isMovingBack = PR_FALSE;
            switch (keyEvent->keyCode) {
              case NS_VK_UP: 
              case NS_VK_LEFT:
                isMovingBack = PR_TRUE;
              case NS_VK_DOWN:
              case NS_VK_RIGHT:
              // Arrow key pressed, focus+select prev/next radio button
              nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
              if (container) {
                nsAutoString name;
                if (GetNameIfExists(name)) {
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
                      event.inputSource = nsIDOMNSMouseEvent::MOZ_SOURCE_KEYBOARD;
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
          }

          /*
           * If this is input type=text, and the user hit enter, fire onChange
           * and submit the form (if we are in one)
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
              (mType == NS_FORM_INPUT_TEXT ||
               mType == NS_FORM_INPUT_EMAIL ||
               mType == NS_FORM_INPUT_SEARCH ||
               mType == NS_FORM_INPUT_PASSWORD ||
               mType == NS_FORM_INPUT_TEL ||
               mType == NS_FORM_INPUT_URL ||
               mType == NS_FORM_INPUT_FILE)) {

            PRBool isButton = PR_FALSE;
            // If this is an enter on the button of a file input, don't submit
            // -- that's supposed to put up the filepicker
            if (mType == NS_FORM_INPUT_FILE) {
              nsCOMPtr<nsIContent> maybeButton =
                do_QueryInterface(aVisitor.mEvent->originalTarget);
              if (maybeButton) {
                isButton = maybeButton->AttrValueIs(kNameSpaceID_None,
                                                    nsGkAtoms::type,
                                                    nsGkAtoms::button,
                                                    eCaseMatters);
              }
            }

            if (!isButton) {
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
            nsFormEvent event(PR_TRUE, (mType == NS_FORM_INPUT_RESET) ?
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
      (NS_FAILED(LoadImage(uri, PR_FALSE, PR_TRUE)) ||
       !LoadingEnabled())) {
    CancelImageRequests(PR_TRUE);
  }
}

nsresult
nsHTMLInputElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                               nsIContent* aBindingParent,
                               PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mType == NS_FORM_INPUT_IMAGE) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
      ClearBrokenState();
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

  return rv;
}

void
nsHTMLInputElement::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // If we have a form and are unbound from it,
  // nsGenericHTMLFormElement::UnbindFromTree() will unset the form and
  // that takes care of form's WillRemove so we just have to take care
  // of the case where we're removing from the document and we don't
  // have a form
  if (!mForm && mType == NS_FORM_INPUT_RADIO) {
    WillRemoveFromRadioGroup(PR_FALSE);
  }

  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);

  // GetCurrentDoc is returning nsnull so we can update the value
  // missing validity state to reflect we are no longer into a doc.
  UpdateValueMissingValidityState();
}

void
nsHTMLInputElement::HandleTypeChange(PRUint8 aNewType)
{
  ValueModeType aOldValueMode = GetValueMode();
  nsAutoString aOldValue;
  GetValue(aOldValue);

  // Only single line text inputs have a text editor state.
  PRBool isNewTypeSingleLine =
    IsSingleLineTextControlInternal(PR_FALSE, aNewType);
  PRBool isCurrentTypeSingleLine =
    IsSingleLineTextControl(PR_FALSE);
  if (isNewTypeSingleLine && !isCurrentTypeSingleLine) {
    FreeData();
    mInputData.mState = new nsTextEditorState(this);
    NS_ADDREF(mInputData.mState);
  } else if (isCurrentTypeSingleLine && !isNewTypeSingleLine) {
    FreeData();
  }

  mType = aNewType;

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
        SetAttr(kNameSpaceID_None, nsGkAtoms::value, aOldValue, PR_TRUE);
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
        SetValueInternal(value, PR_FALSE, PR_FALSE);
      }
      break;
    case VALUE_MODE_FILENAME:
    default:
      // We don't care about the value.
      // There is no value sanitizing algorithm for elements in this mode.
      break;
  }

  // Do not notify, it will be done after if needed.
  UpdateAllValidityStates(PR_FALSE);
}

void
nsHTMLInputElement::SanitizeValue(nsAString& aValue)
{
  switch (mType) {
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_EMAIL:
      {
        PRUnichar crlf[] = { PRUnichar('\r'), PRUnichar('\n'), 0 };
        aValue.StripChars(crlf);
      }
      break;
    case NS_FORM_INPUT_URL:
      {
        PRUnichar crlf[] = { PRUnichar('\r'), PRUnichar('\n'), 0 };
        aValue.StripChars(crlf);

        aValue = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(aValue);
      }
      break;
  }
}

PRBool
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
      PRBool success = aResult.ParseEnumValue(aValue, kInputTypeTable, PR_FALSE);
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
          ClearFiles();
        }

        HandleTypeChange(newType);
      }

      return success;
    }
    if (aAttribute == nsGkAtoms::width) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE);
    }
    if (aAttribute == nsGkAtoms::height) {
      return aResult.ParseSpecialIntValue(aValue, PR_TRUE);
    }
    if (aAttribute == nsGkAtoms::maxlength) {
      return aResult.ParseNonNegativeIntValue(aValue);
    }
    if (aAttribute == nsGkAtoms::size) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::border) {
      return aResult.ParseIntWithBounds(aValue, 0);
    }
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::formmethod) {
      return aResult.ParseEnumValue(aValue, kFormMethodTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::formenctype) {
      return aResult.ParseEnumValue(aValue, kFormEnctypeTable, PR_FALSE);
    }
    if (aAttribute == nsGkAtoms::autocomplete) {
      return aResult.ParseEnumValue(aValue, kInputAutocompleteTable, PR_FALSE);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      // We have to call |ParseImageAttribute| unconditionally since we
      // don't know if we're going to have a type="image" attribute yet,
      // (or could have it set dynamically in the future).  See bug
      // 214077.
      return PR_TRUE;
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
             IsSingleLineTextControl(PR_FALSE)) {
    NS_UpdateHint(retval, NS_STYLE_HINT_REFLOW);
  }
  return retval;
}

NS_IMETHODIMP_(PRBool)
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

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
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
  if (IsSingleLineTextControl(PR_FALSE))
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
                                      PRInt32 aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionStart(PRInt32* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);
  
  PRInt32 selEnd;
  return GetSelectionRange(aSelectionStart, &selEnd);
}

NS_IMETHODIMP
nsHTMLInputElement::SetSelectionStart(PRInt32 aSelectionStart)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->SetSelectionStart(aSelectionStart);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetSelectionEnd(PRInt32* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);
  
  PRInt32 selStart;
  return GetSelectionRange(&selStart, aSelectionEnd);
}


NS_IMETHODIMP
nsHTMLInputElement::SetSelectionEnd(PRInt32 aSelectionEnd)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->SetSelectionEnd(aSelectionEnd);
  }

  return rv;
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
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame)
      rv = textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return rv;
}

NS_IMETHODIMP
nsHTMLInputElement::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate(0);
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(PR_TRUE);

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
    event->InitEvent(aEventType, PR_TRUE, PR_TRUE);

    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event));
    if (privateEvent) {
      privateEvent->SetTrusted(PR_TRUE);
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
  return SetValueInternal(resetVal, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP
nsHTMLInputElement::Reset()
{
  // We should be able to reset all dirty flags regardless of the type.
  SetCheckedChanged(PR_FALSE);
  SetValueChanged(PR_FALSE);

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      return SetDefaultValueAsValue();
    case VALUE_MODE_DEFAULT_ON:
      PRBool resetVal;
      GetDefaultChecked(&resetVal);
      return DoSetChecked(resetVal, PR_TRUE, PR_FALSE);
    case VALUE_MODE_FILENAME:
      ClearFiles();
      return NS_OK;
    case VALUE_MODE_DEFAULT:
    default:
      return NS_OK;
  }
}

NS_IMETHODIMP
nsHTMLInputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
{
  nsresult rv = NS_OK;

  // Disabled elements don't submit
  // For type=reset, and type=button, we just never submit, period.
  // For type=image and type=button, we only submit if we were the button
  // pressed
  // For type=radio and type=checkbox, we only submit if checked=true
  PRBool disabled;
  rv = GetDisabled(&disabled);
  if (disabled || mType == NS_FORM_INPUT_RESET ||
      mType == NS_FORM_INPUT_BUTTON ||
      ((mType == NS_FORM_INPUT_SUBMIT || mType == NS_FORM_INPUT_IMAGE) &&
       aFormSubmission->GetOriginatingElement() != this) ||
      ((mType == NS_FORM_INPUT_RADIO || mType == NS_FORM_INPUT_CHECKBOX) &&
       !GetChecked())) {
    return NS_OK;
  }

  // Get the name
  nsAutoString name;
  PRBool nameThere = GetNameIfExists(name);

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
  if (!nameThere) {
    return NS_OK;
  }

  // Get the value
  nsAutoString value;
  rv = GetValue(value);
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

    for (PRUint32 i = 0; i < (PRUint32)files.Count(); ++i) {
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
    rv = aFormSubmission->AddNameValuePair(name,
                                           NS_ConvertASCIItoUTF16(charset));
  }
  else if (IsSingleLineTextControl(PR_TRUE) &&
           name.EqualsLiteral("isindex") &&
           aFormSubmission->SupportsIsindexSubmission()) {
    rv = aFormSubmission->AddIsindex(value);
  }
  else {
    rv = aFormSubmission->AddNameValuePair(name, value);
  }

  return rv;
}


NS_IMETHODIMP
nsHTMLInputElement::SaveState()
{
  nsresult rv = NS_OK;

  nsRefPtr<nsHTMLInputElementState> inputState = nsnull;

  switch (mType) {
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_RADIO:
      {
        PRBool checked = GetChecked();
        PRBool defaultChecked = PR_FALSE;
        GetDefaultChecked(&defaultChecked);
        // Only save if checked != defaultChecked (bug 62713)
        // (always save if it's a radio button so that the checked
        // state of all radio buttons is restored)
        if (mType == NS_FORM_INPUT_RADIO || checked != defaultChecked) {
          inputState = new nsHTMLInputElementState();
          if (!inputState) {
            return NS_ERROR_OUT_OF_MEMORY;
          }

          inputState->SetChecked(checked);
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
        if (GET_BOOLBIT(mBitField, BF_VALUE_CHANGED)) {
          inputState = new nsHTMLInputElementState();
          if (!inputState) {
            return NS_ERROR_OUT_OF_MEMORY;
          }

          nsAutoString value;
          GetValue(value);
          rv = nsLinebreakConverter::ConvertStringLineBreaks(
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
          if (!inputState) {
            return NS_ERROR_OUT_OF_MEMORY;
          }

          inputState->SetFiles(mFiles);
        }
        break;
      }
  }
  
  nsPresState* state = nsnull;
  if (inputState) {
    rv = GetPrimaryPresState(this, &state);
    if (state) {
      state->SetStateProperty(inputState);
    }
  }

  if (GET_BOOLBIT(mBitField, BF_DISABLED_CHANGED)) {
    rv |= GetPrimaryPresState(this, &state);
    if (state) {
      PRBool disabled;
      GetDisabled(&disabled);
      state->SetDisabled(disabled);
    }
  }

  return rv;
}

void
nsHTMLInputElement::DoneCreatingElement()
{
  SET_BOOLBIT(mBitField, BF_PARSER_CREATING, PR_FALSE);

  //
  // Restore state as needed.  Note that disabled state applies to all control
  // types.
  //
  PRBool restoredCheckedState =
      GET_BOOLBIT(mBitField, BF_INHIBIT_RESTORATION) ?
      PR_FALSE :
      RestoreFormControlState(this, this);

  //
  // If restore does not occur, we initialize .checked using the CHECKED
  // property.
  //
  if (!restoredCheckedState &&
      GET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED)) {
    PRBool resetVal;
    GetDefaultChecked(&resetVal);
    DoSetChecked(resetVal, PR_FALSE, PR_TRUE);
    DoSetCheckedChanged(PR_FALSE, PR_FALSE);
  }

  SET_BOOLBIT(mBitField, BF_SHOULD_INIT_CHECKED, PR_FALSE);
}

PRInt32
nsHTMLInputElement::IntrinsicState() const
{
  // If you add states here, and they're type-dependent, you need to add them
  // to the type case in AfterSetAttr.
  
  PRInt32 state = nsGenericHTMLFormElement::IntrinsicState();
  if (mType == NS_FORM_INPUT_CHECKBOX || mType == NS_FORM_INPUT_RADIO) {
    // Check current checked state (:checked)
    if (GET_BOOLBIT(mBitField, BF_CHECKED)) {
      state |= NS_EVENT_STATE_CHECKED;
    }

    // Check current indeterminate state (:indeterminate)
    if (mType == NS_FORM_INPUT_CHECKBOX && GET_BOOLBIT(mBitField, BF_INDETERMINATE)) {
      state |= NS_EVENT_STATE_INDETERMINATE;
    }

    // Check whether we are the default checked element (:default)
    // The call is to an interface function, which makes it non-const, so we
    // use a nasty hack :(
    PRBool defaultState = PR_FALSE;
    const_cast<nsHTMLInputElement*>(this)->GetDefaultChecked(&defaultState);
    if (defaultState) {
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
    state |= IsValid() ? NS_EVENT_STATE_VALID : NS_EVENT_STATE_INVALID;
  }

  if (PlaceholderApplies() && HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      !nsContentUtils::IsFocusedContent((nsIContent*)(this))) {
    // TODO: we really need a GetValue(...) const method, see bug 585097
    nsTextEditorState* edState = GetEditorState();
    nsAutoString value;

    if (edState) {
      edState->GetValue(value, PR_TRUE);
    } else {
      GetAttr(kNameSpaceID_None, nsGkAtoms::value, value);
    }

    if (value.IsEmpty()) {
      state |= NS_EVENT_STATE_MOZ_PLACEHOLDER;
    }
  }

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  return state;
}

PRBool
nsHTMLInputElement::RestoreState(nsPresState* aState)
{
  PRBool restoredCheckedState = PR_FALSE;

  nsCOMPtr<nsHTMLInputElementState> inputState
    (do_QueryInterface(aState->GetStateProperty()));

  if (inputState) {
    switch (mType) {
      case NS_FORM_INPUT_CHECKBOX:
      case NS_FORM_INPUT_RADIO:
        {
          if (inputState->IsCheckedSet()) {
            restoredCheckedState = PR_TRUE;
            DoSetChecked(inputState->GetChecked(), PR_TRUE, PR_TRUE);
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
          SetValueInternal(inputState->GetValue(), PR_FALSE, PR_TRUE);
          break;
        }
      case NS_FORM_INPUT_FILE:
        {
          const nsCOMArray<nsIDOMFile>& files = inputState->GetFiles();
          SetFiles(files);
          break;
        }
    }
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return restoredCheckedState;
}

PRBool
nsHTMLInputElement::AllowDrop()
{
  // Allow drop on anything other than file inputs.

  return mType != NS_FORM_INPUT_FILE;
}

/*
 * Radio group stuff
 */

void
nsHTMLInputElement::AddedToRadioGroup(PRBool aNotify)
{
  // Make sure not to notify if we're still being created by the parser
  aNotify = aNotify && !GET_BOOLBIT(mBitField, BF_PARSER_CREATING);

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
  if (GetChecked()) {
    //
    // If it is checked, call "RadioSetChecked" to perform the selection/
    // deselection ritual.  This has the side effect of repainting the
    // radio button, but as adding a checked radio button into the group
    // should not be that common an occurrence, I think we can live with
    // that.
    //
    RadioSetChecked(aNotify);
  }

  //
  // For integrity purposes, we have to ensure that "checkedChanged" is
  // the same for this new element as for all the others in the group
  //
  PRBool checkedChanged = GET_BOOLBIT(mBitField, BF_CHECKED_CHANGED);
  nsCOMPtr<nsIRadioVisitor> visitor;
  nsresult rv = NS_GetRadioGetCheckedChangedVisitor(&checkedChanged, this,
                                           getter_AddRefs(visitor));
  if (NS_FAILED(rv)) { return; }
  
  VisitGroup(visitor, aNotify);
  SetCheckedChangedInternal(checkedChanged);
  
  //
  // Add the radio to the radio group container.
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    if (GetNameIfExists(name)) {
      container->AddToRadioGroup(name, static_cast<nsIFormControl*>(this));
    }
  }
}

void
nsHTMLInputElement::WillRemoveFromRadioGroup(PRBool aNotify)
{
  //
  // If the input element is not in a form and
  // not in a document, we just need to return.
  //
  if (!mForm && !(IsInDoc() && GetParent())) {
    return;
  }

  //
  // If this button was checked, we need to notify the group that there is no
  // longer a selected radio button
  //
  nsAutoString name;
  PRBool gotName = PR_FALSE;
  if (GetChecked()) {
    if (!gotName) {
      if (!GetNameIfExists(name)) {
        // If the name doesn't exist, nothing is going to happen anyway
        return;
      }
      gotName = PR_TRUE;
    }

    nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
    if (container) {
      container->SetCurrentRadioButton(name, nsnull);
    }

    // Removing a checked radio from the group can change the validity state.
    // Let's ask other radio to update their value missing validity state.
    nsCOMPtr<nsIRadioVisitor> visitor =
      NS_GetRadioUpdateValueMissingVisitor(aNotify);
    VisitGroup(visitor, PR_FALSE);
  }
  
  //
  // Remove this radio from its group in the container
  //
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    if (!gotName) {
      if (!GetNameIfExists(name)) {
        // If the name doesn't exist, nothing is going to happen anyway
        return;
      }
      gotName = PR_TRUE;
    }
    container->RemoveFromRadioGroup(name,
                                    static_cast<nsIFormControl*>(this));
  }
}

PRBool
nsHTMLInputElement::IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return PR_TRUE;
  }

  if (HasAttr(kNameSpaceID_None, nsGkAtoms::disabled)) {
    *aIsFocusable = PR_FALSE;
    return PR_TRUE;
  }

  if (IsSingleLineTextControl(PR_FALSE)) {
    *aIsFocusable = PR_TRUE;
    return PR_FALSE;
  }

#ifdef XP_MACOSX
  const PRBool defaultFocusable = !aWithMouse || nsFocusManager::sMouseFocusesFormControl;
#else
  const PRBool defaultFocusable = PR_TRUE;
#endif

  if (mType == NS_FORM_INPUT_FILE) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    *aIsFocusable = defaultFocusable;
    return PR_TRUE;
  }

  if (mType == NS_FORM_INPUT_HIDDEN) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }
    *aIsFocusable = PR_FALSE;
    return PR_FALSE;
  }

  if (!aTabIndex) {
    // The other controls are all focusable
    *aIsFocusable = defaultFocusable;
    return PR_FALSE;
  }

  if (mType != NS_FORM_INPUT_RADIO) {
    *aIsFocusable = defaultFocusable;
    return PR_FALSE;
  }

  if (GetChecked()) {
    // Selected radio buttons are tabbable
    *aIsFocusable = defaultFocusable;
    return PR_FALSE;
  }

  // Current radio button is not selected.
  // But make it tabbable if nothing in group is selected.
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  nsAutoString name;
  if (!container || !GetNameIfExists(name)) {
    *aIsFocusable = defaultFocusable;
    return PR_FALSE;
  }

  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio;
  container->GetCurrentRadioButton(name, getter_AddRefs(currentRadio));
  if (currentRadio) {
    *aTabIndex = -1;
  }
  *aIsFocusable = defaultFocusable;
  return PR_FALSE;
}

nsresult
nsHTMLInputElement::VisitGroup(nsIRadioVisitor* aVisitor, PRBool aFlushContent)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    if (GetNameIfExists(name)) {
      rv = container->WalkRadioGroup(name, aVisitor, aFlushContent);
    } else {
      PRBool stop;
      aVisitor->Visit(this, &stop);
    }
  } else {
    PRBool stop;
    aVisitor->Visit(this, &stop);
  }
  return rv;
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

PRBool
nsHTMLInputElement::IsMutable() const
{
  return !HasAttr(kNameSpaceID_None, nsGkAtoms::disabled) &&
         GetCurrentDoc() &&
         !(DoesReadOnlyApply() &&
           HasAttr(kNameSpaceID_None, nsGkAtoms::readonly));
}

PRBool
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
      return PR_FALSE;
#ifdef DEBUG
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      return PR_TRUE;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesReadOnlyApply()");
      return PR_TRUE;
#else // DEBUG
    default:
      return PR_TRUE;
#endif // DEBUG
  }
}

PRBool
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
      return PR_FALSE;
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
      return PR_TRUE;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesRequiredApply()");
      return PR_TRUE;
#else // DEBUG
    default:
      return PR_TRUE;
#endif // DEBUG
  }
}

PRBool
nsHTMLInputElement::DoesPatternApply() const
{
  return IsSingleLineTextControl(PR_FALSE);
}

// nsIConstraintValidation

NS_IMETHODIMP
nsHTMLInputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  nsIDocument* doc = GetCurrentDoc();
  if (doc) {
    MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
    doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_INVALID |
                                            NS_EVENT_STATE_VALID);
  }

  return NS_OK;
}

PRBool
nsHTMLInputElement::IsTooLong()
{
  if (!GET_BOOLBIT(mBitField, BF_VALUE_CHANGED)) {
    return PR_FALSE;
  }

  PRInt32 maxLength = -1;
  PRInt32 textLength = -1;

  GetMaxLength(&maxLength);
  GetTextLength(&textLength);

  return maxLength >= 0 && textLength > maxLength;
}

PRBool
nsHTMLInputElement::IsValueMissing()
{
  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) ||
      !DoesRequiredApply()) {
    return PR_FALSE;
  }

  if (GetValueMode() == VALUE_MODE_VALUE) {
    if (!IsMutable()) {
      return PR_FALSE;
    }

    nsAutoString value;
    NS_ENSURE_SUCCESS(GetValue(value), PR_FALSE);

    return value.IsEmpty();
  }

  switch (mType)
  {
    case NS_FORM_INPUT_CHECKBOX:
      return !GetChecked();
    case NS_FORM_INPUT_RADIO:
      {
        nsCOMPtr<nsIDOMHTMLInputElement> selected = GetSelectedRadioButton();
        return !selected;
      }
    case NS_FORM_INPUT_FILE:
      {
        const nsCOMArray<nsIDOMFile>& files = GetFiles();
        return !files.Count();
      }
    default:
      return PR_FALSE;
  }
}

PRBool
nsHTMLInputElement::HasTypeMismatch()
{
  if (mType != NS_FORM_INPUT_EMAIL && mType != NS_FORM_INPUT_URL) {
    return PR_FALSE;
  }

  nsAutoString value;
  NS_ENSURE_SUCCESS(GetValue(value), PR_FALSE);

  if (value.IsEmpty()) {
    return PR_FALSE;
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

  return PR_FALSE;
}

PRBool
nsHTMLInputElement::HasPatternMismatch()
{
  nsAutoString pattern;
  if (!DoesPatternApply() ||
      !GetAttr(kNameSpaceID_None, nsGkAtoms::pattern, pattern)) {
    return PR_FALSE;
  }

  nsAutoString value;
  NS_ENSURE_SUCCESS(GetValue(value), PR_FALSE);

  if (value.IsEmpty()) {
    return PR_FALSE;
  }

  nsIDocument* doc = GetOwnerDoc();
  if (!doc) {
    return PR_FALSE;
  }

  return !IsPatternMatching(value, pattern, doc);
}

void
nsHTMLInputElement::UpdateTooLongValidityState()
{
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
}

void
nsHTMLInputElement::UpdateValueMissingValidityState()
{
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
nsHTMLInputElement::UpdateAllValidityStates(PRBool aNotify)
{
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();
  UpdateTypeMismatchValidityState();
  UpdatePatternMismatchValidityState();

  if (aNotify) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull,
                                NS_EVENT_STATE_VALID | NS_EVENT_STATE_INVALID);
    }
  }
}

void
nsHTMLInputElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(mType == NS_FORM_INPUT_HIDDEN ||
                                    mType == NS_FORM_INPUT_BUTTON ||
                                    mType == NS_FORM_INPUT_RESET ||
                                    HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) ||
                                    HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
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

      const PRUnichar* params[] = { strTextLength.get(), strMaxLength.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "ElementSuffersFromBeingTooLong",
                                                 params, 2, message);
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
          key.Assign("FileElementSuffersFromBeingMissing");
          break;
        case NS_FORM_INPUT_CHECKBOX:
          key.Assign("CheckboxElementSuffersFromBeingMissing");
          break;
        case NS_FORM_INPUT_RADIO:
          key.Assign("RadioElementSuffersFromBeingMissing");
          break;
        default:
          key.Assign("TextElementSuffersFromBeingMissing");
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
        key.AssignLiteral("ElementSuffersFromInvalidEmail");
      } else if (mType == NS_FORM_INPUT_URL) {
        key.AssignLiteral("ElementSuffersFromInvalidURL");
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
                                                "ElementSuffersFromPatternMismatch",
                                                message);
      } else {
        const PRUnichar* params[] = { title.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "ElementSuffersFromPatternMismatchWithTitle",
                                                   params, 1, message);
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
PRBool
nsHTMLInputElement::IsValidEmailAddressList(const nsAString& aValue)
{
  nsCharSeparatedTokenizerTemplate<nsContentUtils::IsHTMLWhitespace>
    tokenizer(aValue, ',');

  while (tokenizer.hasMoreTokens()) {
    if (!IsValidEmailAddress(tokenizer.nextToken())) {
      return PR_FALSE;
    }
  }

  return !tokenizer.lastTokenEndedWithSeparator();
}

//static
PRBool
nsHTMLInputElement::IsValidEmailAddress(const nsAString& aValue)
{
  PRUint32 i = 0;
  PRUint32 length = aValue.Length();

  // If the email address is empty, begins with a '@' or ends with a '.',
  // we know it's invalid.
  if (length == 0 || aValue[0] == '@' || aValue[length-1] == '.') {
    return PR_FALSE;
  }

  // Parsing the username.
  for (; i < length && aValue[i] != '@'; ++i) {
    PRUnichar c = aValue[i];

    // The username characters have to be in this list to be valid.
    if (!(nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) ||
          c == '.' || c == '!' || c == '#' || c == '$' || c == '%' ||
          c == '&' || c == '\''|| c == '*' || c == '+' || c == '-' ||
          c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
          c == '`' || c == '{' || c == '|' || c == '}' || c == '~' )) {
      return PR_FALSE;
    }
  }

  // There is no domain name (or it's one-character long),
  // that's not a valid email address.
  if (++i >= length) {
    return PR_FALSE;
  }

  // The domain name can't begin with a dot.
  if (aValue[i] == '.') {
    return PR_FALSE;
  }

  // The domain name must have at least one dot which can't follow another dot,
  // can't be the first nor the last domain name character.
  PRBool dotFound = PR_FALSE;

  // Parsing the domain name.
  for (; i < length; ++i) {
    PRUnichar c = aValue[i];

    if (c == '.') {
      dotFound = PR_TRUE;
      // A dot can't follow a dot.
      if (aValue[i-1] == '.') {
        return PR_FALSE;
      }
    } else if (!(nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) ||
                 c == '-')) {
      // The domain characters have to be in this list to be valid.
      return PR_FALSE;
    }
  }

  return dotFound;
}

//static
PRBool
nsHTMLInputElement::IsPatternMatching(nsAString& aValue, nsAString& aPattern,
                                      nsIDocument* aDocument)
{
  NS_ASSERTION(aDocument, "aDocument should be a valid pointer (not null)");
  NS_ENSURE_TRUE(aDocument->GetScriptGlobalObject(), PR_TRUE);

  JSContext* ctx = (JSContext*) aDocument->GetScriptGlobalObject()->
                                  GetContext()->GetNativeContext();
  NS_ENSURE_TRUE(ctx, PR_TRUE);

  JSAutoRequest ar(ctx);

  // The pattern has to match the entire value.
  aPattern.Insert(NS_LITERAL_STRING("^(?:"), 0);
  aPattern.Append(NS_LITERAL_STRING(")$"));

  JSObject* re = JS_NewUCRegExpObjectNoStatics(ctx, reinterpret_cast<jschar*>
                                                 (aPattern.BeginWriting()),
                                                aPattern.Length(), 0);
  NS_ENSURE_TRUE(re, PR_TRUE);

  jsval rval = JSVAL_NULL;
  size_t idx = 0;
  JSBool res;

  res = JS_ExecuteRegExpNoStatics(ctx, re, reinterpret_cast<jschar*>
                                    (aValue.BeginWriting()),
                                  aValue.Length(), &idx, JS_TRUE, &rval);

  return res == JS_FALSE || rval != JSVAL_NULL;
}

//
// Visitor classes
//
//
// CLASS nsRadioVisitor
//
// (this is the superclass of the others)
//
class nsRadioVisitor : public nsIRadioVisitor {
public:
  nsRadioVisitor() { }
  virtual ~nsRadioVisitor() { }

  NS_DECL_ISUPPORTS

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop) = 0;
};

NS_IMPL_ISUPPORTS1(nsRadioVisitor, nsIRadioVisitor)


//
// CLASS nsRadioSetCheckedChangedVisitor
//
class nsRadioSetCheckedChangedVisitor : public nsRadioVisitor {
public:
  nsRadioSetCheckedChangedVisitor(PRBool aCheckedChanged) :
    nsRadioVisitor(), mCheckedChanged(aCheckedChanged)
    { }

  virtual ~nsRadioSetCheckedChangedVisitor() { }

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop)
  {
    nsRefPtr<nsHTMLInputElement> radio =
      static_cast<nsHTMLInputElement*>(aRadio);
    NS_ASSERTION(radio, "Visit() passed a null button!");
    radio->SetCheckedChangedInternal(mCheckedChanged);
    return NS_OK;
  }

protected:
  PRPackedBool mCheckedChanged;
};

//
// CLASS nsRadioGetCheckedChangedVisitor
//
class nsRadioGetCheckedChangedVisitor : public nsRadioVisitor {
public:
  nsRadioGetCheckedChangedVisitor(PRBool* aCheckedChanged,
                                  nsIFormControl* aExcludeElement) :
    nsRadioVisitor(),
    mCheckedChanged(aCheckedChanged),
    mExcludeElement(aExcludeElement)
    { }

  virtual ~nsRadioGetCheckedChangedVisitor() { }

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop)
  {
    if (aRadio == mExcludeElement) {
      return NS_OK;
    }
    nsRefPtr<nsHTMLInputElement> radio =
      static_cast<nsHTMLInputElement*>(aRadio);
    NS_ASSERTION(radio, "Visit() passed a null button!");
    *mCheckedChanged = radio->GetCheckedChanged();
    *aStop = PR_TRUE;
    return NS_OK;
  }

protected:
  PRBool* mCheckedChanged;
  nsIFormControl* mExcludeElement;
};

class nsRadioUpdateValueMissingVisitor : public nsRadioVisitor {
public:
  nsRadioUpdateValueMissingVisitor(PRBool aNotify)
    : nsRadioVisitor()
    , mNotify(aNotify)
    { }

  virtual ~nsRadioUpdateValueMissingVisitor() { };

  NS_IMETHOD Visit(nsIFormControl* aRadio, PRBool* aStop)
  {
    /**
     * The simplest way to update the value missing validity state is to do a
     * global update of the validity state by simulationg a value change.
     * OnValueChanged() is declared into nsITextControlElement. That may sound
     * to be a weird way to update the validity states for radio controls but
     * they are also implementing nsITextControlElement interface.
     *
     * When OnValueChanged() is called on a radio control, it will check if any
     * radio in the group is checked. If none, the required radio will be
     * suffering from being missing.
     */
    nsCOMPtr<nsITextControlElement> textCtl(do_QueryInterface(aRadio));
    NS_ASSERTION(textCtl, "Visit() passed a null or non-radio pointer");
    textCtl->OnValueChanged(mNotify);
    return NS_OK;
  }

protected:
  PRBool mNotify;
};

nsresult
NS_GetRadioSetCheckedChangedVisitor(PRBool aCheckedChanged,
                                    nsIRadioVisitor** aVisitor)
{
  //
  // These are static so that we don't have to keep creating new visitors for
  // such an ordinary process all the time.  There are only two possibilities
  // for this visitor: set to true, and set to false.
  //
  static nsIRadioVisitor* sVisitorTrue = nsnull;
  static nsIRadioVisitor* sVisitorFalse = nsnull;

  //
  // Get the visitor that sets them to true
  //
  if (aCheckedChanged) {
    if (!sVisitorTrue) {
      sVisitorTrue = new nsRadioSetCheckedChangedVisitor(PR_TRUE);
      if (!sVisitorTrue) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ADDREF(sVisitorTrue);
      nsresult rv =
        nsContentUtils::ReleasePtrOnShutdown((nsISupports**)&sVisitorTrue);
      if (NS_FAILED(rv)) {
        NS_RELEASE(sVisitorTrue);
        return rv;
      }
    }
    *aVisitor = sVisitorTrue;
  }
  //
  // Get the visitor that sets them to false
  //
  else {
    if (!sVisitorFalse) {
      sVisitorFalse = new nsRadioSetCheckedChangedVisitor(PR_FALSE);
      if (!sVisitorFalse) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ADDREF(sVisitorFalse);
      nsresult rv =
        nsContentUtils::ReleasePtrOnShutdown((nsISupports**)&sVisitorFalse);
      if (NS_FAILED(rv)) {
        NS_RELEASE(sVisitorFalse);
        return rv;
      }
    }
    *aVisitor = sVisitorFalse;
  }

  NS_ADDREF(*aVisitor);
  return NS_OK;
}

nsresult
NS_GetRadioGetCheckedChangedVisitor(PRBool* aCheckedChanged,
                                    nsIFormControl* aExcludeElement,
                                    nsIRadioVisitor** aVisitor)
{
  *aVisitor = new nsRadioGetCheckedChangedVisitor(aCheckedChanged,
                                                  aExcludeElement);
  if (!*aVisitor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  NS_ADDREF(*aVisitor);

  return NS_OK;
}

/*
 * This method is a factory: it lets callers to create an instance of
 * nsRadioUpdateValueMissing without the class declaration and definition.
 *
 * TODO:
 * Do we really need factories for radio visitors? Or at least, we should move
 * that somewhere else because it feels like it's here only because the radio
 * visitor classes are defined after most of nsHTMLInputElement code.
 * See bug 586298
 */
nsIRadioVisitor*
NS_GetRadioUpdateValueMissingVisitor(PRBool aNotify)
{
  return new nsRadioUpdateValueMissingVisitor(aNotify);
}

NS_IMETHODIMP_(PRBool)
nsHTMLInputElement::IsSingleLineTextControl() const
{
  return IsSingleLineTextControl(PR_FALSE);
}

NS_IMETHODIMP_(PRBool)
nsHTMLInputElement::IsTextArea() const
{
  return PR_FALSE;
}

NS_IMETHODIMP_(PRBool)
nsHTMLInputElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return PR_TRUE;
}

NS_IMETHODIMP_(PRBool)
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
    SanitizeValue(aValue);
  }
}

NS_IMETHODIMP_(PRBool)
nsHTMLInputElement::ValueChanged() const
{
  return GET_BOOLBIT(mBitField, BF_VALUE_CHANGED);
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::GetTextEditorValue(nsAString& aValue,
                                       PRBool aIgnoreWrap) const
{
  nsTextEditorState *state = GetEditorState();
  if (state) {
    state->GetValue(aValue, aIgnoreWrap);
  }
}

NS_IMETHODIMP_(void)
nsHTMLInputElement::SetTextEditorValue(const nsAString& aValue,
                                       PRBool aUserInput)
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
nsHTMLInputElement::OnValueChanged(PRBool aNotify)
{
  UpdateAllValidityStates(aNotify);

  // :-moz-placeholder pseudo-class may change when the value changes.
  // However, we don't want to waste cycles if the state doesn't apply.
  if (aNotify && PlaceholderApplies()
      && HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)
      && !nsContentUtils::IsFocusedContent((nsIContent*)(this))) {
    nsIDocument* doc = GetCurrentDoc();
    if (doc) {
      MOZ_AUTO_DOC_UPDATE(doc, UPDATE_CONTENT_STATE, PR_TRUE);
      doc->ContentStatesChanged(this, nsnull, NS_EVENT_STATE_MOZ_PLACEHOLDER);
    }
  }
}

