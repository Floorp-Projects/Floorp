/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLInputElementBinding.h"
#include "nsAsyncDOMEvent.h"
#include "nsAttrValueInlines.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIRadioVisitor.h"
#include "nsIPhonetic.h"

#include "nsIControllers.h"
#include "nsIStringBundle.h"
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
#include "nsRangeFrame.h"
#include "nsEventStates.h"
#include "nsIServiceManager.h"
#include "nsError.h"
#include "nsIEditor.h"
#include "nsGUIEvent.h"
#include "nsIIOService.h"
#include "nsDocument.h"
#include "nsAttrValueOrString.h"

#include "nsPresState.h"
#include "nsIDOMEvent.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLCollection.h"
#include "nsLinebreakConverter.h" //to strip out carriage returns
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsEventDispatcher.h"
#include "nsLayoutUtils.h"

#include "nsIDOMMutationEvent.h"
#include "nsMutationEvent.h"
#include "nsEventListenerManager.h"

#include "nsRuleData.h"
#include <algorithm>

// input type=radio
#include "nsIRadioGroupContainer.h"

// input type=file
#include "nsIFile.h"
#include "nsNetUtil.h"
#include "nsDOMFile.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIContentPrefService.h"
#include "nsIMIMEService.h"
#include "nsIObserverService.h"
#include "nsIPopupWindowManager.h"
#include "nsGlobalWindow.h"

// input type=image
#include "nsImageLoadingContent.h"
#include "imgRequestProxy.h"

#include "mozAutoDocUpdate.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "mozilla/dom/DirectionalityUtils.h"
#include "nsRadioVisitor.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/Util.h" // DebugOnly
#include "mozilla/Preferences.h"
#include "mozilla/MathAlgorithms.h"

#include "nsIIDNService.h"

#include <limits>

#include "nsIColorPicker.h"

// input type=date
#include "js/Date.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Input)

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

// This must come outside of any namespace, or else it won't overload with the
// double based version in nsMathUtils.h
inline NS_HIDDEN_(mozilla::Decimal)
NS_floorModulo(mozilla::Decimal x, mozilla::Decimal y)
{
  return (x - y * (x / y).floor());
}

namespace mozilla {
namespace dom {

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
static int32_t gSelectTextFieldOnFocus;
UploadLastDir* HTMLInputElement::gUploadLastDir;

static const nsAttrValue::EnumTable kInputTypeTable[] = {
  { "button", NS_FORM_INPUT_BUTTON },
  { "checkbox", NS_FORM_INPUT_CHECKBOX },
  { "color", NS_FORM_INPUT_COLOR },
  { "date", NS_FORM_INPUT_DATE },
  { "email", NS_FORM_INPUT_EMAIL },
  { "file", NS_FORM_INPUT_FILE },
  { "hidden", NS_FORM_INPUT_HIDDEN },
  { "reset", NS_FORM_INPUT_RESET },
  { "image", NS_FORM_INPUT_IMAGE },
  { "number", NS_FORM_INPUT_NUMBER },
  { "password", NS_FORM_INPUT_PASSWORD },
  { "radio", NS_FORM_INPUT_RADIO },
  { "range", NS_FORM_INPUT_RANGE },
  { "search", NS_FORM_INPUT_SEARCH },
  { "submit", NS_FORM_INPUT_SUBMIT },
  { "tel", NS_FORM_INPUT_TEL },
  { "text", NS_FORM_INPUT_TEXT },
  { "time", NS_FORM_INPUT_TIME },
  { "url", NS_FORM_INPUT_URL },
  { 0 }
};

// Default type is 'text'.
static const nsAttrValue::EnumTable* kInputDefaultType = &kInputTypeTable[16];

static const uint8_t NS_INPUT_AUTOCOMPLETE_OFF     = 0;
static const uint8_t NS_INPUT_AUTOCOMPLETE_ON      = 1;
static const uint8_t NS_INPUT_AUTOCOMPLETE_DEFAULT = 2;

static const nsAttrValue::EnumTable kInputAutocompleteTable[] = {
  { "", NS_INPUT_AUTOCOMPLETE_DEFAULT },
  { "on", NS_INPUT_AUTOCOMPLETE_ON },
  { "off", NS_INPUT_AUTOCOMPLETE_OFF },
  { 0 }
};

// Default autocomplete value is "".
static const nsAttrValue::EnumTable* kInputDefaultAutocomplete = &kInputAutocompleteTable[0];

static const uint8_t NS_INPUT_INPUTMODE_AUTO              = 0;
static const uint8_t NS_INPUT_INPUTMODE_NUMERIC           = 1;
static const uint8_t NS_INPUT_INPUTMODE_DIGIT             = 2;
static const uint8_t NS_INPUT_INPUTMODE_UPPERCASE         = 3;
static const uint8_t NS_INPUT_INPUTMODE_LOWERCASE         = 4;
static const uint8_t NS_INPUT_INPUTMODE_TITLECASE         = 5;
static const uint8_t NS_INPUT_INPUTMODE_AUTOCAPITALIZED   = 6;

static const nsAttrValue::EnumTable kInputInputmodeTable[] = {
  { "auto", NS_INPUT_INPUTMODE_AUTO },
  { "numeric", NS_INPUT_INPUTMODE_NUMERIC },
  { "digit", NS_INPUT_INPUTMODE_DIGIT },
  { "uppercase", NS_INPUT_INPUTMODE_UPPERCASE },
  { "lowercase", NS_INPUT_INPUTMODE_LOWERCASE },
  { "titlecase", NS_INPUT_INPUTMODE_TITLECASE },
  { "autocapitalized", NS_INPUT_INPUTMODE_AUTOCAPITALIZED },
  { 0 }
};

// Default inputmode value is "auto".
static const nsAttrValue::EnumTable* kInputDefaultInputmode = &kInputInputmodeTable[0];

const Decimal HTMLInputElement::kStepScaleFactorDate = 86400000;
const Decimal HTMLInputElement::kStepScaleFactorNumberRange = 1;
const Decimal HTMLInputElement::kStepScaleFactorTime = 1000;
const Decimal HTMLInputElement::kDefaultStepBase = 0;
const Decimal HTMLInputElement::kDefaultStep = 1;
const Decimal HTMLInputElement::kDefaultStepTime = 60;
const Decimal HTMLInputElement::kStepAny = 0;

#define NS_INPUT_ELEMENT_STATE_IID                 \
{ /* dc3b3d14-23e2-4479-b513-7b369343e3a0 */       \
  0xdc3b3d14,                                      \
  0x23e2,                                          \
  0x4479,                                          \
  {0xb5, 0x13, 0x7b, 0x36, 0x93, 0x43, 0xe3, 0xa0} \
}

class HTMLInputElementState MOZ_FINAL : public nsISupports
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

    void SetValue(const nsAString& aValue) {
      mValue = aValue;
    }

    const nsCOMArray<nsIDOMFile>& GetFiles() {
      return mFiles;
    }

    void SetFiles(const nsCOMArray<nsIDOMFile>& aFiles) {
      mFiles.Clear();
      mFiles.AppendObjects(aFiles);
    }

    HTMLInputElementState()
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

NS_IMPL_ISUPPORTS1(HTMLInputElementState, HTMLInputElementState)
NS_DEFINE_STATIC_IID_ACCESSOR(HTMLInputElementState, NS_INPUT_ELEMENT_STATE_IID)

HTMLInputElement::nsFilePickerShownCallback::nsFilePickerShownCallback(
  HTMLInputElement* aInput, nsIFilePicker* aFilePicker, bool aMulti)
  : mFilePicker(aFilePicker)
  , mInput(aInput)
  , mMulti(aMulti)
{
}

NS_IMPL_ISUPPORTS1(UploadLastDir::ContentPrefCallback, nsIContentPrefCallback2)

NS_IMETHODIMP
UploadLastDir::ContentPrefCallback::HandleCompletion(uint16_t aReason)
{
  nsCOMPtr<nsIFile> localFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
  NS_ENSURE_STATE(localFile);

  if (aReason == nsIContentPrefCallback2::COMPLETE_ERROR ||
      !mResult) {
    // Default to "desktop" directory for each platform
    nsCOMPtr<nsIFile> homeDir;
    NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(homeDir));
    localFile = do_QueryInterface(homeDir);
  } else {
    nsAutoString prefStr;
    nsCOMPtr<nsIVariant> pref;
    mResult->GetValue(getter_AddRefs(pref));
    pref->GetAsAString(prefStr);
    localFile->InitWithPath(prefStr);
  }

  mFilePicker->SetDisplayDirectory(localFile);
  mFilePicker->Open(mFpCallback);
  return NS_OK;
}

NS_IMETHODIMP
UploadLastDir::ContentPrefCallback::HandleResult(nsIContentPref* pref)
{
  mResult = pref;
  return NS_OK;
}

NS_IMETHODIMP
UploadLastDir::ContentPrefCallback::HandleError(nsresult error)
{
  // HandleCompletion is always called (even with HandleError was called),
  // so we don't need to do anything special here.
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::nsFilePickerShownCallback::Done(int16_t aResult)
{
  if (aResult == nsIFilePicker::returnCancel) {
    return NS_OK;
  }

  // Collect new selected filenames
  nsCOMArray<nsIDOMFile> newFiles;
  if (mMulti) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    nsresult rv = mFilePicker->GetDomfiles(getter_AddRefs(iter));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!iter) {
      return NS_OK;
    }

    nsCOMPtr<nsISupports> tmp;
    bool prefSaved = false;
    bool loop = true;

    while (NS_SUCCEEDED(iter->HasMoreElements(&loop)) && loop) {
      iter->GetNext(getter_AddRefs(tmp));
      nsCOMPtr<nsIDOMFile> domFile = do_QueryInterface(tmp);
      MOZ_ASSERT(domFile);

      newFiles.AppendObject(domFile);

      if (!prefSaved) {
        // Store the last used directory using the content pref service
        HTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(
          mInput->OwnerDoc(), domFile);
        prefSaved = true;
      }
    }
  }
  else {
    nsCOMPtr<nsIDOMFile> domFile;
    nsresult rv = mFilePicker->GetDomfile(getter_AddRefs(domFile));
    NS_ENSURE_SUCCESS(rv, rv);
    if (domFile) {
      newFiles.AppendObject(domFile);

      // Store the last used directory using the content pref service
      HTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(
        mInput->OwnerDoc(), domFile);
    }
  }

  if (!newFiles.Count()) {
    return NS_OK;
  }

  // The text control frame (if there is one) isn't going to send a change
  // event because it will think this is done by a script.
  // So, we can safely send one by ourself.
  mInput->SetFiles(newFiles, true);
  return nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                                              NS_LITERAL_STRING("change"), true,
                                              false);
}

NS_IMPL_ISUPPORTS1(HTMLInputElement::nsFilePickerShownCallback,
                   nsIFilePickerShownCallback)

class nsColorPickerShownCallback MOZ_FINAL
  : public nsIColorPickerShownCallback
{
public:
  nsColorPickerShownCallback(HTMLInputElement* aInput,
                             nsIColorPicker* aColorPicker)
    : mInput(aInput)
    , mColorPicker(aColorPicker)
    , mValueChanged(false)
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Update(const nsAString& aColor) MOZ_OVERRIDE;
  NS_IMETHOD Done(const nsAString& aColor) MOZ_OVERRIDE;

private:
  /**
   * Updates the internals of the object using aColor as the new value.
   * If aTrustedUpdate is true, it will consider that aColor is a new value.
   * Otherwise, it will check that aColor is different from the current value.
   */
  nsresult UpdateInternal(const nsAString& aColor, bool aTrustedUpdate);

  nsRefPtr<HTMLInputElement> mInput;
  nsCOMPtr<nsIColorPicker>   mColorPicker;
  bool                       mValueChanged;
};

nsresult
nsColorPickerShownCallback::UpdateInternal(const nsAString& aColor,
                                           bool aTrustedUpdate)
{
  bool valueChanged = false;

  nsAutoString oldValue;
  if (aTrustedUpdate) {
    valueChanged = true;
  } else {
    mInput->GetValue(oldValue);
  }

  mInput->SetValue(aColor);

  if (!aTrustedUpdate) {
    nsAutoString newValue;
    mInput->GetValue(newValue);
    if (!oldValue.Equals(newValue)) {
      valueChanged = true;
    }
  }

  if (valueChanged) {
    mValueChanged = true;
    return nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                                                static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                                                NS_LITERAL_STRING("input"), true,
                                                false);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsColorPickerShownCallback::Update(const nsAString& aColor)
{
  return UpdateInternal(aColor, true);
}

NS_IMETHODIMP
nsColorPickerShownCallback::Done(const nsAString& aColor)
{
  /**
   * When Done() is called, we might be at the end of a serie of Update() calls
   * in which case mValueChanged is set to true and a change event will have to
   * be fired but we might also be in a one shot Done() call situation in which
   * case we should fire a change event iif the value actually changed.
   * UpdateInternal(bool) is taking care of that logic for us.
   */
  nsresult rv = NS_OK;

  if (!aColor.IsEmpty()) {
    UpdateInternal(aColor, false);
  }

  if (mValueChanged) {
    rv = nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                                              NS_LITERAL_STRING("change"), true,
                                              false);
  }

  return rv;
}

NS_IMPL_ISUPPORTS1(nsColorPickerShownCallback, nsIColorPickerShownCallback)

HTMLInputElement::AsyncClickHandler::AsyncClickHandler(HTMLInputElement* aInput)
  : mInput(aInput)
{
  nsPIDOMWindow* win = aInput->OwnerDoc()->GetWindow();
  if (win) {
    mPopupControlState = win->GetPopupControlState();
  }
}

NS_IMETHODIMP
HTMLInputElement::AsyncClickHandler::Run()
{
  if (mInput->GetType() == NS_FORM_INPUT_FILE) {
    return InitFilePicker();
  } else if (mInput->GetType() == NS_FORM_INPUT_COLOR) {
    return InitColorPicker();
  }
  return NS_ERROR_FAILURE;
}

nsresult
HTMLInputElement::AsyncClickHandler::InitColorPicker()
{
  // Get parent nsPIDOMWindow object.
  nsCOMPtr<nsIDocument> doc = mInput->OwnerDoc();

  nsCOMPtr<nsPIDOMWindow> win = doc->GetWindow();
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

    uint32_t permission;
    pm->TestPermission(doc->NodePrincipal(), &permission);
    if (permission == nsIPopupWindowManager::DENY_POPUP) {
      nsGlobalWindow::FirePopupBlockedEvent(doc, win, nullptr, EmptyString(), EmptyString());
      return NS_OK;
    }
  }

  // Get Loc title
  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "ColorPicker", title);

  nsCOMPtr<nsIColorPicker> colorPicker = do_CreateInstance("@mozilla.org/colorpicker;1");
  if (!colorPicker) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString initialValue;
  mInput->GetValueInternal(initialValue);
  nsresult rv = colorPicker->Init(win, title, initialValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIColorPickerShownCallback> callback =
    new nsColorPickerShownCallback(mInput, colorPicker);

  return colorPicker->Open(callback);
}

nsresult
HTMLInputElement::AsyncClickHandler::InitFilePicker()
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

    uint32_t permission;
    pm->TestPermission(doc->NodePrincipal(), &permission);
    if (permission == nsIPopupWindowManager::DENY_POPUP) {
      nsGlobalWindow::FirePopupBlockedEvent(doc, win, nullptr, EmptyString(), EmptyString());
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
                                  ? static_cast<int16_t>(nsIFilePicker::modeOpenMultiple)
                                  : static_cast<int16_t>(nsIFilePicker::modeOpen));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mInput->HasAttr(kNameSpaceID_None, nsGkAtoms::accept)) {
    mInput->SetFilePickerFiltersFromAccept(filePicker);
  } else {
    filePicker->AppendFilters(nsIFilePicker::filterAll);
  }

  // Set default directry and filename
  nsAutoString defaultName;

  const nsCOMArray<nsIDOMFile>& oldFiles = mInput->GetFilesInternal();

  nsCOMPtr<nsIFilePickerShownCallback> callback =
    new HTMLInputElement::nsFilePickerShownCallback(mInput, filePicker, multi);

  if (oldFiles.Count()) {
    nsString path;

    oldFiles[0]->GetMozFullPathInternal(path);

    nsCOMPtr<nsIFile> localFile;
    rv = NS_NewLocalFile(path, false, getter_AddRefs(localFile));

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIFile> parentFile;
      rv = localFile->GetParent(getter_AddRefs(parentFile));
      if (NS_SUCCEEDED(rv)) {
        filePicker->SetDisplayDirectory(parentFile);
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

    return filePicker->Open(callback);
  }

  HTMLInputElement::gUploadLastDir->FetchDirectoryAndDisplayPicker(doc, filePicker, callback);
  return NS_OK;

}

#define CPS_PREF_NAME NS_LITERAL_STRING("browser.upload.lastDir")

NS_IMPL_ISUPPORTS2(UploadLastDir, nsIObserver, nsISupportsWeakReference)

void
HTMLInputElement::InitUploadLastDir() {
  gUploadLastDir = new UploadLastDir();
  NS_ADDREF(gUploadLastDir);

  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService && gUploadLastDir) {
    observerService->AddObserver(gUploadLastDir, "browser:purge-session-history", true);
  }
}

void
HTMLInputElement::DestroyUploadLastDir() {
  NS_IF_RELEASE(gUploadLastDir);
}

nsresult
UploadLastDir::FetchDirectoryAndDisplayPicker(nsIDocument* aDoc,
                                              nsIFilePicker* aFilePicker,
                                              nsIFilePickerShownCallback* aFpCallback)
{
  NS_PRECONDITION(aDoc, "aDoc is null");
  NS_PRECONDITION(aFilePicker, "aFilePicker is null");
  NS_PRECONDITION(aFpCallback, "aFpCallback is null");

  nsIURI* docURI = aDoc->GetDocumentURI();
  NS_PRECONDITION(docURI, "docURI is null");

  nsCOMPtr<nsISupports> container = aDoc->GetContainer();
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(container);

  nsCOMPtr<nsIContentPrefCallback2> prefCallback = 
    new UploadLastDir::ContentPrefCallback(aFilePicker, aFpCallback);

  // Attempt to get the CPS, if it's not present we'll fallback to use the Desktop folder
  nsCOMPtr<nsIContentPrefService2> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  if (!contentPrefService) {
    prefCallback->HandleCompletion(nsIContentPrefCallback2::COMPLETE_ERROR);
    return NS_OK;
  }

  nsAutoCString cstrSpec;
  docURI->GetSpec(cstrSpec);
  NS_ConvertUTF8toUTF16 spec(cstrSpec);

  contentPrefService->GetByDomainAndName(spec, CPS_PREF_NAME, loadContext, prefCallback);
  return NS_OK;
}

nsresult
UploadLastDir::StoreLastUsedDirectory(nsIDocument* aDoc, nsIDOMFile* aDomFile)
{
  NS_PRECONDITION(aDoc, "aDoc is null");
  NS_PRECONDITION(aDomFile, "aDomFile is null");

  nsString path;
  nsresult rv = aDomFile->GetMozFullPathInternal(path);
  if (NS_FAILED(rv) || path.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIFile> localFile;
  rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(path), true,
                             getter_AddRefs(localFile));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> docURI = aDoc->GetDocumentURI();
  NS_PRECONDITION(docURI, "docURI is null");

  nsCOMPtr<nsIFile> parentFile;
  localFile->GetParent(getter_AddRefs(parentFile));
  if (!parentFile) {
    return NS_OK;
  }

  // Attempt to get the CPS, if it's not present we'll just return
  nsCOMPtr<nsIContentPrefService2> contentPrefService =
    do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
  if (!contentPrefService)
    return NS_ERROR_NOT_AVAILABLE;

  nsAutoCString cstrSpec;
  docURI->GetSpec(cstrSpec);
  NS_ConvertUTF8toUTF16 spec(cstrSpec);

  // Find the parent of aFile, and store it
  nsString unicodePath;
  parentFile->GetPath(unicodePath);
  if (unicodePath.IsEmpty()) // nothing to do
    return NS_OK;
  nsCOMPtr<nsIWritableVariant> prefValue = do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!prefValue)
    return NS_ERROR_OUT_OF_MEMORY;
  prefValue->SetAsAString(unicodePath);

  nsCOMPtr<nsISupports> container = aDoc->GetContainer();
  nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(container);
  return contentPrefService->Set(spec, CPS_PREF_NAME, prefValue, loadContext, nullptr);
}

NS_IMETHODIMP
UploadLastDir::Observe(nsISupports* aSubject, char const* aTopic, PRUnichar const* aData)
{
  if (strcmp(aTopic, "browser:purge-session-history") == 0) {
    nsCOMPtr<nsIContentPrefService2> contentPrefService =
      do_GetService(NS_CONTENT_PREF_SERVICE_CONTRACTID);
    if (contentPrefService)
      contentPrefService->RemoveByName(CPS_PREF_NAME, nullptr, nullptr);
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

HTMLInputElement::HTMLInputElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                   FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo)
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
  , mHasRange(false)
  , mIsDraggingRange(false)
{
  // We are in a type=text so we now we currenty need a nsTextEditorState.
  mInputData.mState = new nsTextEditorState(this);

  if (!gUploadLastDir)
    HTMLInputElement::InitUploadLastDir();

  // Set up our default state.  By default we're enabled (since we're
  // a control type that can be disabled but not actually disabled
  // right now), optional, and valid.  We are NOT readwrite by default
  // until someone calls UpdateEditableState on us, apparently!  Also
  // by default we don't have to show validity UI and so forth.
  AddStatesSilently(NS_EVENT_STATE_ENABLED |
                    NS_EVENT_STATE_OPTIONAL |
                    NS_EVENT_STATE_VALID);
}

HTMLInputElement::~HTMLInputElement()
{
  if (mFileList) {
    mFileList->Disconnect();
  }
  DestroyImageLoadingContent();
  FreeData();
}

void
HTMLInputElement::FreeData()
{
  if (!IsSingleLineTextControl(false)) {
    nsMemory::Free(mInputData.mValue);
    mInputData.mValue = nullptr;
  } else {
    UnbindFromFrame(nullptr);
    delete mInputData.mState;
    mInputData.mState = nullptr;
  }
}

nsTextEditorState*
HTMLInputElement::GetEditorState() const
{
  if (!IsSingleLineTextControl(false)) {
    return nullptr;
  }

  MOZ_ASSERT(mInputData.mState, "Single line text controls need to have a state"
                                " associated with them");

  return mInputData.mState;
}


// nsISupports

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLInputElement,
                                                  nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  if (tmp->IsSingleLineTextControl(false)) {
    tmp->mInputData.mState->Traverse(cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFiles)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLInputElement,
                                                nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFiles)
  if (tmp->mFileList) {
    tmp->mFileList->Disconnect();
    tmp->mFileList = nullptr;
  }
  if (tmp->IsSingleLineTextControl(false)) {
    tmp->mInputData.mState->Unlink();
  }
  //XXX should unlink more?
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLInputElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLInputElement, Element)

// QueryInterface implementation for HTMLInputElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLInputElement)
  NS_HTML_CONTENT_INTERFACES(nsGenericHTMLFormElementWithState)
  NS_INTERFACE_TABLE_INHERITED8(HTMLInputElement,
                                nsIDOMHTMLInputElement,
                                nsITextControlElement,
                                nsIPhonetic,
                                imgINotificationObserver,
                                nsIImageLoadingContent,
                                imgIOnloadBlocker,
                                nsIDOMNSEditableElement,
                                nsIConstraintValidation)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE
NS_ELEMENT_INTERFACE_MAP_END

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLInputElement)

// nsIDOMNode

nsresult
HTMLInputElement::Clone(nsINodeInfo* aNodeInfo, nsINode** aResult) const
{
  *aResult = nullptr;

  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  nsRefPtr<HTMLInputElement> it =
    new HTMLInputElement(ni.forget(), NOT_FROM_PARSER);

  nsresult rv = const_cast<HTMLInputElement*>(this)->CopyInnerTo(it);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      if (mValueChanged) {
        // We don't have our default value anymore.  Set our value on
        // the clone.
        nsAutoString value;
        GetValueInternal(value);
        // SetValueInternal handles setting the VALUE_CHANGED bit for us
        it->SetValueInternal(value, false, true);
      }
      break;
    case VALUE_MODE_FILENAME:
      if (it->OwnerDoc()->IsStaticDocument()) {
        // We're going to be used in print preview.  Since the doc is static
        // we can just grab the pretty string and use it as wallpaper
        GetDisplayFileName(it->mStaticDocFileList);
      } else {
        it->mFiles.Clear();
        it->mFiles.AppendObjects(mFiles);
      }
      break;
    case VALUE_MODE_DEFAULT_ON:
      if (mCheckedChanged) {
        // We no longer have our original checked state.  Set our
        // checked state on the clone.
        it->DoSetChecked(mChecked, false, true);
      }
      break;
    case VALUE_MODE_DEFAULT:
      if (mType == NS_FORM_INPUT_IMAGE && it->OwnerDoc()->IsStaticDocument()) {
        CreateStaticImageClone(it);
      }
      break;
  }

  it.forget(aResult);
  return NS_OK;
}

nsresult
HTMLInputElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
    } else if (aName == nsGkAtoms::dir &&
               AttrValueIs(kNameSpaceID_None, nsGkAtoms::dir,
                           nsGkAtoms::_auto, eIgnoreCase)) {
      SetDirectionIfAuto(false, aNotify);
    }
  }

  return nsGenericHTMLFormElementWithState::BeforeSetAttr(aNameSpaceID, aName,
                                                          aValue, aNotify);
}

nsresult
HTMLInputElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
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
    } else if (aName == nsGkAtoms::max) {
      UpdateHasRange();
      UpdateRangeOverflowValidityState();
      if (mType == NS_FORM_INPUT_RANGE) {
        // The value may need to change when @max changes since the value may
        // have been invalid and can now change to a valid value, or vice
        // versa. For example, consider:
        // <input type=range value=-1 max=1 step=3>. The valid range is 0 to 1
        // while the nearest valid steps are -1 and 2 (the max value having
        // prevented there being a valid step in range). Changing @max to/from
        // 1 and a number greater than on equal to 3 should change whether we
        // have a step mismatch or not.
        // The value may also need to change between a value that results in
        // a step mismatch and a value that results in overflow. For example,
        // if @max in the example above were to change from 1 to -1.
        nsAutoString value;
        GetValue(value);
        SetValueInternal(value, false, false);
        MOZ_ASSERT(!GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                   "HTML5 spec does not allow this");
      }
    } else if (aName == nsGkAtoms::min) {
      UpdateHasRange();
      UpdateRangeUnderflowValidityState();
      UpdateStepMismatchValidityState();
      if (mType == NS_FORM_INPUT_RANGE) {
        // See @max comment
        nsAutoString value;
        GetValue(value);
        SetValueInternal(value, false, false);
        MOZ_ASSERT(!GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                   "HTML5 spec does not allow this");
      }
    } else if (aName == nsGkAtoms::step) {
      UpdateStepMismatchValidityState();
      if (mType == NS_FORM_INPUT_RANGE) {
        // See @max comment
        nsAutoString value;
        GetValue(value);
        SetValueInternal(value, false, false);
        MOZ_ASSERT(!GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                   "HTML5 spec does not allow this");
      }
    } else if (aName == nsGkAtoms::dir &&
               aValue && aValue->Equals(nsGkAtoms::_auto, eIgnoreCase)) {
      SetDirectionIfAuto(true, aNotify);
    }

    UpdateState(aNotify);
  }

  return nsGenericHTMLFormElementWithState::AfterSetAttr(aNameSpaceID, aName,
                                                         aValue, aNotify);
}

// nsIDOMHTMLInputElement

NS_IMETHODIMP
HTMLInputElement::GetForm(nsIDOMHTMLFormElement** aForm)
{
  return nsGenericHTMLFormElementWithState::GetForm(aForm);
}

NS_IMPL_STRING_ATTR(HTMLInputElement, DefaultValue, value)
NS_IMPL_BOOL_ATTR(HTMLInputElement, DefaultChecked, checked)
NS_IMPL_STRING_ATTR(HTMLInputElement, Accept, accept)
NS_IMPL_STRING_ATTR(HTMLInputElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLInputElement, Alt, alt)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLInputElement, Autocomplete, autocomplete,
                                kInputDefaultAutocomplete->tag)
NS_IMPL_BOOL_ATTR(HTMLInputElement, Autofocus, autofocus)
//NS_IMPL_BOOL_ATTR(HTMLInputElement, Checked, checked)
NS_IMPL_BOOL_ATTR(HTMLInputElement, Disabled, disabled)
NS_IMPL_STRING_ATTR(HTMLInputElement, Max, max)
NS_IMPL_STRING_ATTR(HTMLInputElement, Min, min)
NS_IMPL_ACTION_ATTR(HTMLInputElement, FormAction, formaction)
NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES(HTMLInputElement, FormEnctype, formenctype,
                                                 "", kFormDefaultEnctype->tag)
NS_IMPL_ENUM_ATTR_DEFAULT_MISSING_INVALID_VALUES(HTMLInputElement, FormMethod, formmethod,
                                                 "", kFormDefaultMethod->tag)
NS_IMPL_BOOL_ATTR(HTMLInputElement, FormNoValidate, formnovalidate)
NS_IMPL_STRING_ATTR(HTMLInputElement, FormTarget, formtarget)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLInputElement, InputMode, inputmode,
                                kInputDefaultInputmode->tag)
NS_IMPL_BOOL_ATTR(HTMLInputElement, Multiple, multiple)
NS_IMPL_NON_NEGATIVE_INT_ATTR(HTMLInputElement, MaxLength, maxlength)
NS_IMPL_STRING_ATTR(HTMLInputElement, Name, name)
NS_IMPL_BOOL_ATTR(HTMLInputElement, ReadOnly, readonly)
NS_IMPL_BOOL_ATTR(HTMLInputElement, Required, required)
NS_IMPL_URI_ATTR(HTMLInputElement, Src, src)
NS_IMPL_STRING_ATTR(HTMLInputElement, Step, step)
NS_IMPL_STRING_ATTR(HTMLInputElement, UseMap, usemap)
//NS_IMPL_STRING_ATTR(HTMLInputElement, Value, value)
NS_IMPL_UINT_ATTR_NON_ZERO_DEFAULT_VALUE(HTMLInputElement, Size, size, DEFAULT_COLS)
NS_IMPL_STRING_ATTR(HTMLInputElement, Pattern, pattern)
NS_IMPL_STRING_ATTR(HTMLInputElement, Placeholder, placeholder)
NS_IMPL_ENUM_ATTR_DEFAULT_VALUE(HTMLInputElement, Type, type,
                                kInputDefaultType->tag)

int32_t
HTMLInputElement::TabIndexDefault()
{
  return 0;
}

uint32_t
HTMLInputElement::Height()
{
  return GetWidthHeightForImage(mCurrentRequest).height;
}

NS_IMETHODIMP
HTMLInputElement::GetHeight(uint32_t* aHeight)
{
  *aHeight = Height();
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::SetHeight(uint32_t aHeight)
{
  ErrorResult rv;
  SetHeight(aHeight, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLInputElement::GetIndeterminate(bool* aValue)
{
  *aValue = Indeterminate();
  return NS_OK;
}

void
HTMLInputElement::SetIndeterminateInternal(bool aValue,
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
}

NS_IMETHODIMP
HTMLInputElement::SetIndeterminate(bool aValue)
{
  SetIndeterminateInternal(aValue, true);
  return NS_OK;
}

uint32_t
HTMLInputElement::Width()
{
  return GetWidthHeightForImage(mCurrentRequest).width;
}

NS_IMETHODIMP
HTMLInputElement::GetWidth(uint32_t* aWidth)
{
  *aWidth = Width();
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::SetWidth(uint32_t aWidth)
{
  ErrorResult rv;
  SetWidth(aWidth, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLInputElement::GetValue(nsAString& aValue)
{
  GetValueInternal(aValue);

  // Don't return non-sanitized value for types that are experimental on mobile.
  if (IsExperimentalMobileType(mType)) {
    SanitizeValue(aValue);
  }

  return NS_OK;
}

nsresult
HTMLInputElement::GetValueInternal(nsAString& aValue) const
{
  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      if (IsSingleLineTextControl(false)) {
        mInputData.mState->GetValue(aValue, true);
      } else {
        aValue.Assign(mInputData.mValue);
      }
      return NS_OK;

    case VALUE_MODE_FILENAME:
      if (nsContentUtils::IsCallerChrome()) {
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
HTMLInputElement::IsValueEmpty() const
{
  nsAutoString value;
  GetValueInternal(value);

  return value.IsEmpty();
}

static Decimal StringToDecimal(nsAString& aValue)
{
  if (!IsASCII(aValue)) {
    return Decimal::nan();
  }
  NS_LossyConvertUTF16toASCII asciiString(aValue);
  std::string stdString = asciiString.get();
  return Decimal::fromString(stdString);
}

bool
HTMLInputElement::ConvertStringToNumber(nsAString& aValue,
                                        Decimal& aResultValue) const
{
  MOZ_ASSERT(DoesValueAsNumberApply(),
             "ConvertStringToNumber only applies if .valueAsNumber applies");

  switch (mType) {
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
      {
        aResultValue = StringToDecimal(aValue);
        if (!aResultValue.isFinite()) {
          return false;
        }
        return true;
      }
    case NS_FORM_INPUT_DATE:
      {
        uint32_t year, month, day;
        if (!GetValueAsDate(aValue, &year, &month, &day)) {
          return false;
        }

        double date = JS::MakeDate(year, month - 1, day);
        if (IsNaN(date)) {
          return false;
        }

        aResultValue = Decimal::fromDouble(date);
        return true;
      }
    case NS_FORM_INPUT_TIME:
      uint32_t milliseconds;
      if (!ParseTime(aValue, &milliseconds)) {
        return false;
      }

      aResultValue = int32_t(milliseconds);
      return true;
    default:
      MOZ_ASSERT(false, "Unrecognized input type");
      return false;
  }
}

Decimal
HTMLInputElement::GetValueAsDecimal() const
{
  Decimal decimalValue;
  nsAutoString stringValue;

  GetValueInternal(stringValue);

  return !ConvertStringToNumber(stringValue, decimalValue) ? Decimal::nan()
                                                           : decimalValue;
}

void
HTMLInputElement::SetValue(const nsAString& aValue, ErrorResult& aRv)
{
  // check security.  Note that setting the value to the empty string is always
  // OK and gives pages a way to clear a file input if necessary.
  if (mType == NS_FORM_INPUT_FILE) {
    if (!aValue.IsEmpty()) {
      if (!nsContentUtils::IsCallerChrome()) {
        // setting the value of a "FILE" input widget requires
        // chrome privilege
        aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
        return;
      }
      Sequence<nsString> list;
      list.AppendElement(aValue);
      MozSetFileNameArray(list);
      return;
    }
    else {
      ClearFiles(true);
    }
  }
  else {
    if (MayFireChangeOnBlur()) {
      // If the value has been set by a script, we basically want to keep the
      // current change event state. If the element is ready to fire a change
      // event, we should keep it that way. Otherwise, we should make sure the
      // element will not fire any event because of the script interaction.
      //
      // NOTE: this is currently quite expensive work (too much string
      // manipulation). We should probably optimize that.
      nsAutoString currentValue;
      GetValueInternal(currentValue);

      SetValueInternal(aValue, false, true);

      if (mType == NS_FORM_INPUT_RANGE) {
        nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
        if (frame) {
          frame->UpdateForValueChange();
        }
      }

      if (mFocusedValue.Equals(currentValue)) {
        GetValueInternal(mFocusedValue);
      }
    } else {
      SetValueInternal(aValue, false, true);
    }
  }
}

NS_IMETHODIMP
HTMLInputElement::SetValue(const nsAString& aValue)
{
  ErrorResult rv;
  SetValue(aValue, rv);
  return rv.ErrorCode();
}

nsGenericHTMLElement*
HTMLInputElement::GetList() const
{
  nsAutoString dataListId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::list, dataListId);
  if (dataListId.IsEmpty()) {
    return nullptr;
  }

  nsIDocument* doc = GetCurrentDoc();
  if (!doc) {
    return nullptr;
  }

  Element* element = doc->GetElementById(dataListId);
  if (!element || !element->IsHTML(nsGkAtoms::datalist)) {
    return nullptr;
  }

  return static_cast<nsGenericHTMLElement*>(element);
}

NS_IMETHODIMP
HTMLInputElement::GetList(nsIDOMHTMLElement** aValue)
{
  *aValue = nullptr;

  nsRefPtr<nsGenericHTMLElement> element = GetList();
  if (!element) {
    return NS_OK;
  }

  CallQueryInterface(element, aValue);
  return NS_OK;
}

void
HTMLInputElement::SetValue(Decimal aValue)
{
  MOZ_ASSERT(!aValue.isInfinity(), "aValue must not be Infinity!");

  if (aValue.isNaN()) {
    SetValue(EmptyString());
    return;
  }

  nsAutoString value;
  ConvertNumberToString(aValue, value);
  SetValue(value);
}

bool
HTMLInputElement::ConvertNumberToString(Decimal aValue,
                                        nsAString& aResultString) const
{
  MOZ_ASSERT(DoesValueAsNumberApply(),
             "ConvertNumberToString is only implemented for types implementing .valueAsNumber");
  MOZ_ASSERT(aValue.isFinite(),
             "aValue must be a valid non-Infinite number.");

  aResultString.Truncate();

  switch (mType) {
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
      {
        char buf[32];
        bool ok = aValue.toString(buf, ArrayLength(buf));
        aResultString.AssignASCII(buf);
        MOZ_ASSERT(ok, "buf not big enough");
        return ok;
      }
    case NS_FORM_INPUT_DATE:
      {
        // The specs (and our JS APIs) require |aValue| to be truncated.
        aValue = aValue.floor();

        double year = JS::YearFromTime(aValue.toDouble());
        double month = JS::MonthFromTime(aValue.toDouble());
        double day = JS::DayFromTime(aValue.toDouble());

        if (IsNaN(year) || IsNaN(month) || IsNaN(day)) {
          return false;
        }

        aResultString.AppendPrintf("%04.0f-%02.0f-%02.0f", year,
                                   month + 1, day);

        return true;
      }
    case NS_FORM_INPUT_TIME:
      {
        // Per spec, we need to truncate |aValue| and we should only represent
        // times inside a day [00:00, 24:00[, which means that we should do a
        // modulo on |aValue| using the number of milliseconds in a day (86400000).
        uint32_t value = NS_floorModulo(aValue.floor(), 86400000).toDouble();

        uint16_t milliseconds = value % 1000;
        value /= 1000;

        uint8_t seconds = value % 60;
        value /= 60;

        uint8_t minutes = value % 60;
        value /= 60;

        uint8_t hours = value;

        if (milliseconds != 0) {
          aResultString.AppendPrintf("%02d:%02d:%02d.%03d",
                                     hours, minutes, seconds, milliseconds);
        } else if (seconds != 0) {
          aResultString.AppendPrintf("%02d:%02d:%02d",
                                     hours, minutes, seconds);
        } else {
          aResultString.AppendPrintf("%02d:%02d", hours, minutes);
        }

        return true;
      }
    default:
      MOZ_ASSERT(false, "Unrecognized input type");
      return false;
  }
}


Nullable<Date>
HTMLInputElement::GetValueAsDate(ErrorResult& aRv)
{
  if (mType != NS_FORM_INPUT_DATE && mType != NS_FORM_INPUT_TIME) {
    return Nullable<Date>();
  }

  switch (mType) {
    case NS_FORM_INPUT_DATE:
    {
      uint32_t year, month, day;
      nsAutoString value;
      GetValueInternal(value);
      if (!GetValueAsDate(value, &year, &month, &day)) {
        return Nullable<Date>();
      }

      return Nullable<Date>(Date(JS::MakeDate(year, month - 1, day)));
    }
    case NS_FORM_INPUT_TIME:
    {
      uint32_t millisecond;
      nsAutoString value;
      GetValueInternal(value);
      if (!ParseTime(value, &millisecond)) {
        return Nullable<Date>();
      }

      return Nullable<Date>(Date(millisecond));
    }
  }

  MOZ_ASSERT(false, "Unrecognized input type");
  aRv.Throw(NS_ERROR_UNEXPECTED);
  return Nullable<Date>();
}

void
HTMLInputElement::SetValueAsDate(Nullable<Date> aDate, ErrorResult& aRv)
{
  if (mType != NS_FORM_INPUT_DATE && mType != NS_FORM_INPUT_TIME) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aDate.IsNull() || aDate.Value().IsUndefined()) {
    aRv = SetValue(EmptyString());
    return;
  }

  SetValue(Decimal::fromDouble(aDate.Value().TimeStamp()));
}

NS_IMETHODIMP
HTMLInputElement::GetValueAsNumber(double* aValueAsNumber)
{
  *aValueAsNumber = ValueAsNumber();
  return NS_OK;
}

void
HTMLInputElement::SetValueAsNumber(double aValueAsNumber, ErrorResult& aRv)
{
  // TODO: return TypeError when HTMLInputElement is converted to WebIDL, see
  // bug 825197.
  if (IsInfinite(aValueAsNumber)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }

  if (!DoesValueAsNumberApply()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  SetValue(Decimal::fromDouble(aValueAsNumber));
}

NS_IMETHODIMP
HTMLInputElement::SetValueAsNumber(double aValueAsNumber)
{
  ErrorResult rv;
  SetValueAsNumber(aValueAsNumber, rv);
  return rv.ErrorCode();
}

Decimal
HTMLInputElement::GetMinimum() const
{
  MOZ_ASSERT(DoesValueAsNumberApply(),
             "GetMinimum() should only be used for types that allow .valueAsNumber");

  // Only type=range has a default minimum
  Decimal defaultMinimum =
    mType == NS_FORM_INPUT_RANGE ? 0 : Decimal::nan();

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::min)) {
    return defaultMinimum;
  }

  nsAutoString minStr;
  GetAttr(kNameSpaceID_None, nsGkAtoms::min, minStr);

  Decimal min;
  return ConvertStringToNumber(minStr, min) ? min : defaultMinimum;
}

Decimal
HTMLInputElement::GetMaximum() const
{
  MOZ_ASSERT(DoesValueAsNumberApply(),
             "GetMaximum() should only be used for types that allow .valueAsNumber");

  // Only type=range has a default maximum
  Decimal defaultMaximum =
    mType == NS_FORM_INPUT_RANGE ? 100 : Decimal::nan();

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::max)) {
    return defaultMaximum;
  }

  nsAutoString maxStr;
  GetAttr(kNameSpaceID_None, nsGkAtoms::max, maxStr);

  Decimal max;
  return ConvertStringToNumber(maxStr, max) ? max : defaultMaximum;
}

Decimal
HTMLInputElement::GetStepBase() const
{
  MOZ_ASSERT(mType == NS_FORM_INPUT_NUMBER ||
             mType == NS_FORM_INPUT_DATE ||
             mType == NS_FORM_INPUT_TIME ||
             mType == NS_FORM_INPUT_RANGE,
             "Check that kDefaultStepBase is correct for this new type");

  Decimal stepBase;

  // Do NOT use GetMinimum here - the spec says to use "the min content
  // attribute", not "the minimum".
  nsAutoString minStr;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::min, minStr) &&
      ConvertStringToNumber(minStr, stepBase)) {
    return stepBase;
  }

  // If @min is not a double, we should use @value.
  nsAutoString valueStr;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::value, valueStr) &&
      ConvertStringToNumber(valueStr, stepBase)) {
    return stepBase;
  }

  return kDefaultStepBase;
}

nsresult
HTMLInputElement::ApplyStep(int32_t aStep)
{
  if (!DoStepDownStepUpApply()) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  Decimal step = GetStep();
  if (step == kStepAny) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  Decimal value = GetValueAsDecimal();
  if (value.isNaN()) {
    return NS_OK;
  }

  Decimal minimum = GetMinimum();

  Decimal maximum = GetMaximum();
  if (!maximum.isNaN()) {
    // "max - (max - stepBase) % step" is the nearest valid value to max.
    maximum = maximum - NS_floorModulo(maximum - GetStepBase(), step);
  }

  // Cases where we are clearly going in the wrong way.
  // We don't use ValidityState because we can be higher than the maximal
  // allowed value and still not suffer from range overflow in the case of
  // of the value specified in @max isn't in the step.
  if ((value <= minimum && aStep < 0) ||
      (value >= maximum && aStep > 0)) {
    return NS_OK;
  }

  if (GetValidityState(VALIDITY_STATE_STEP_MISMATCH) &&
      value != minimum && value != maximum) {
    if (aStep > 0) {
      value -= NS_floorModulo(value - GetStepBase(), step);
    } else if (aStep < 0) {
      value -= NS_floorModulo(value - GetStepBase(), step);
      value += step;
    }
  }

  value += step * aStep;

  // For date inputs, the value can hold a string that is not a day. We do not
  // want to round it, as it might result in a step mismatch. Instead we want to
  // clamp to the next valid value.
  if (mType == NS_FORM_INPUT_DATE &&
      NS_floorModulo(value - GetStepBase(), GetStepScaleFactor()) != 0) {
    MOZ_ASSERT(GetStep() > 0);
    Decimal validStep = EuclidLCM<Decimal>(GetStep().floor(),
                                           GetStepScaleFactor().floor());
    if (aStep > 0) {
      value -= NS_floorModulo(value - GetStepBase(), validStep);
      value += validStep;
    } else if (aStep < 0) {
      value -= NS_floorModulo(value - GetStepBase(), validStep);
    }
  }

  // When stepUp() is called and the value is below minimum, we should clamp on
  // minimum unless stepUp() moves us higher than minimum.
  if (GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW) && aStep > 0 &&
      value <= minimum) {
    MOZ_ASSERT(!minimum.isNaN(), "Can't be NaN if we are here");
    value = minimum;
  // Same goes for stepDown() and maximum.
  } else if (GetValidityState(VALIDITY_STATE_RANGE_OVERFLOW) && aStep < 0 &&
             value >= maximum) {
    MOZ_ASSERT(!maximum.isNaN(), "Can't be NaN if we are here");
    value = maximum;
  // If we go down, we want to clamp on min.
  } else if (aStep < 0 && minimum == minimum) {
    value = std::max(value, minimum);
  // If we go up, we want to clamp on max.
  } else if (aStep > 0 && maximum == maximum) {
    value = std::min(value, maximum);
  }

  SetValue(value);

  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::StepDown(int32_t n, uint8_t optional_argc)
{
  return ApplyStep(optional_argc ? -n : -1);
}

NS_IMETHODIMP
HTMLInputElement::StepUp(int32_t n, uint8_t optional_argc)
{
  return ApplyStep(optional_argc ? n : 1);
}

void
HTMLInputElement::MozGetFileNameArray(nsTArray< nsString >& aArray)
{
  for (int32_t i = 0; i < mFiles.Count(); i++) {
    nsString str;
    mFiles[i]->GetMozFullPathInternal(str);
    aArray.AppendElement(str);
  }
}


NS_IMETHODIMP
HTMLInputElement::MozGetFileNameArray(uint32_t* aLength, PRUnichar*** aFileNames)
{
  if (!nsContentUtils::IsCallerChrome()) {
    // Since this function returns full paths it's important that normal pages
    // can't call it.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsTArray<nsString> array;
  MozGetFileNameArray(array);

  *aLength = array.Length();
  PRUnichar** ret =
    static_cast<PRUnichar**>(NS_Alloc(*aLength * sizeof(PRUnichar*)));
  if (!ret) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  for (uint32_t i = 0; i < *aLength; ++i) {
    ret[i] = NS_strdup(array[i].get());
  }

  *aFileNames = ret;

  return NS_OK;
}

void
HTMLInputElement::MozSetFileNameArray(const Sequence< nsString >& aFileNames)
{
  nsCOMArray<nsIDOMFile> files;
  for (uint32_t i = 0; i < aFileNames.Length(); ++i) {
    nsCOMPtr<nsIFile> file;

    if (StringBeginsWith(aFileNames[i], NS_LITERAL_STRING("file:"),
                         nsASCIICaseInsensitiveStringComparator())) {
      // Converts the URL string into the corresponding nsIFile if possible
      // A local file will be created if the URL string begins with file://
      NS_GetFileFromURLSpec(NS_ConvertUTF16toUTF8(aFileNames[i]),
                            getter_AddRefs(file));
    }

    if (!file) {
      // this is no "file://", try as local file
      NS_NewLocalFile(aFileNames[i], false, getter_AddRefs(file));
    }

    if (file) {
      nsCOMPtr<nsIDOMFile> domFile = new nsDOMFileFile(file);
      files.AppendObject(domFile);
    } else {
      continue; // Not much we can do if the file doesn't exist
    }

  }

  SetFiles(files, true);
}

NS_IMETHODIMP
HTMLInputElement::MozSetFileNameArray(const PRUnichar** aFileNames, uint32_t aLength)
{
  if (!nsContentUtils::IsCallerChrome()) {
    // setting the value of a "FILE" input widget requires chrome privilege
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  Sequence<nsString> list;
  for (uint32_t i = 0; i < aLength; ++i) {
    list.AppendElement(nsDependentString(aFileNames[i]));
  }

  MozSetFileNameArray(list);
  return NS_OK;
}

bool
HTMLInputElement::MozIsTextField(bool aExcludePassword)
{
  // TODO: temporary until bug 635240 and 773205 are fixed.
  if (IsExperimentalMobileType(mType)) {
    return false;
  }

  return IsSingleLineTextControl(aExcludePassword);
}

NS_IMETHODIMP
HTMLInputElement::MozIsTextField(bool aExcludePassword, bool* aResult)
{
  *aResult = MozIsTextField(aExcludePassword);
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::SetUserInput(const nsAString& aValue)
{
  if (!nsContentUtils::IsCallerChrome()) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  if (mType == NS_FORM_INPUT_FILE)
  {
    Sequence<nsString> list;
    list.AppendElement(aValue);
    MozSetFileNameArray(list);
    return NS_OK;
  } else {
    SetValueInternal(aValue, true, true);
  }

  return nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(this),
                                              NS_LITERAL_STRING("input"), true,
                                              true);
}

nsIEditor*
HTMLInputElement::GetEditor()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->GetEditor();
  }
  return nullptr;
}

NS_IMETHODIMP_(nsIEditor*)
HTMLInputElement::GetTextEditor()
{
  return GetEditor();
}

NS_IMETHODIMP_(nsISelectionController*)
HTMLInputElement::GetSelectionController()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->GetSelectionController();
  }
  return nullptr;
}

nsFrameSelection*
HTMLInputElement::GetConstFrameSelection()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->GetConstFrameSelection();
  }
  return nullptr;
}

NS_IMETHODIMP
HTMLInputElement::BindToFrame(nsTextControlFrame* aFrame)
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->BindToFrame(aFrame);
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void)
HTMLInputElement::UnbindFromFrame(nsTextControlFrame* aFrame)
{
  nsTextEditorState* state = GetEditorState();
  if (state && aFrame) {
    state->UnbindFromFrame(aFrame);
  }
}

NS_IMETHODIMP
HTMLInputElement::CreateEditor()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->PrepareEditor();
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(nsIContent*)
HTMLInputElement::GetRootEditorNode()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->GetRootNode();
  }
  return nullptr;
}

NS_IMETHODIMP_(nsIContent*)
HTMLInputElement::CreatePlaceholderNode()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    NS_ENSURE_SUCCESS(state->CreatePlaceholderNode(), nullptr);
    return state->GetPlaceholderNode();
  }
  return nullptr;
}

NS_IMETHODIMP_(nsIContent*)
HTMLInputElement::GetPlaceholderNode()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    return state->GetPlaceholderNode();
  }
  return nullptr;
}

NS_IMETHODIMP_(void)
HTMLInputElement::UpdatePlaceholderVisibility(bool aNotify)
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    state->UpdatePlaceholderVisibility(aNotify);
  }
}

NS_IMETHODIMP_(bool)
HTMLInputElement::GetPlaceholderVisibility()
{
  nsTextEditorState* state = GetEditorState();
  if (!state) {
    return false;
  }

  return state->GetPlaceholderVisibility();
}

void
HTMLInputElement::GetDisplayFileName(nsAString& aValue) const
{
  if (OwnerDoc()->IsStaticDocument()) {
    aValue = mStaticDocFileList;
    return;
  }

  if (mFiles.Count() == 1) {
    mFiles[0]->GetName(aValue);
    return;
  }

  nsXPIDLString value;

  if (mFiles.Count() == 0) {
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFilesSelected", value);
    } else {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFileSelected", value);
    }
  } else {
    nsString count;
    count.AppendInt(mFiles.Count());

    const PRUnichar* params[] = { count.get() };
    nsContentUtils::FormatLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                          "XFilesSelected", params, value);
  }

  aValue = value;
}

void
HTMLInputElement::SetFiles(const nsCOMArray<nsIDOMFile>& aFiles,
                           bool aSetValueChanged)
{
  mFiles.Clear();
  mFiles.AppendObjects(aFiles);

  AfterSetFiles(aSetValueChanged);
}

void
HTMLInputElement::SetFiles(nsIDOMFileList* aFiles,
                           bool aSetValueChanged)
{
  mFiles.Clear();

  if (aFiles) {
    uint32_t listLength;
    aFiles->GetLength(&listLength);
    for (uint32_t i = 0; i < listLength; i++) {
      nsCOMPtr<nsIDOMFile> file;
      aFiles->Item(i, getter_AddRefs(file));
      mFiles.AppendObject(file);
    }
  }

  AfterSetFiles(aSetValueChanged);
}

void
HTMLInputElement::AfterSetFiles(bool aSetValueChanged)
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

void
HTMLInputElement::FireChangeEventIfNeeded()
{
  nsString value;
  GetValueInternal(value);

  if (!MayFireChangeOnBlur() || mFocusedValue.Equals(value)) {
    return;
  }

  // Dispatch the change event.
  mFocusedValue = value;
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                       static_cast<nsIContent*>(this),
                                       NS_LITERAL_STRING("change"), true,
                                       false);
}

nsDOMFileList*
HTMLInputElement::GetFiles()
{
  if (mType != NS_FORM_INPUT_FILE) {
    return nullptr;
  }

  if (!mFileList) {
    mFileList = new nsDOMFileList(static_cast<nsIContent*>(this));
    UpdateFileList();
  }

  return mFileList;
}

nsresult
HTMLInputElement::UpdateFileList()
{
  if (mFileList) {
    mFileList->Clear();

    const nsCOMArray<nsIDOMFile>& files = GetFilesInternal();
    for (int32_t i = 0; i < files.Count(); ++i) {
      if (!mFileList->Append(files[i])) {
        return NS_ERROR_FAILURE;
      }
    }
  }

  return NS_OK;
}

nsresult
HTMLInputElement::SetValueInternal(const nsAString& aValue,
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

      if (IsSingleLineTextControl(false)) {
        mInputData.mState->SetValue(value, aUserInput, aSetValueChanged);
      } else {
        mInputData.mValue = ToNewUnicode(value);
        if (aSetValueChanged) {
          SetValueChanged(true);
        }
        OnValueChanged(!mParserCreating);
      }

      return NS_OK;
    }

    case VALUE_MODE_DEFAULT:
    case VALUE_MODE_DEFAULT_ON:
      // If the value of a hidden input was changed, we mark it changed so that we
      // will know we need to save / restore the value.  Yes, we are overloading
      // the meaning of ValueChanged just a teensy bit to save a measly byte of
      // storage space in HTMLInputElement.  Yes, you are free to make a new flag,
      // NEED_TO_SAVE_VALUE, at such time as mBitField becomes a 16-bit value.
      if (mType == NS_FORM_INPUT_HIDDEN) {
        SetValueChanged(true);
      }

      // Treat value == defaultValue for other input elements.
      return nsGenericHTMLFormElementWithState::SetAttr(kNameSpaceID_None,
                                                        nsGkAtoms::value, aValue,
                                                        true);

    case VALUE_MODE_FILENAME:
      return NS_ERROR_UNEXPECTED;
  }

  // This return statement is required for some compilers.
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::SetValueChanged(bool aValueChanged)
{
  bool valueChangedBefore = mValueChanged;

  mValueChanged = aValueChanged;

  if (valueChangedBefore != aValueChanged) {
    UpdateState(true);
  }

  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::GetChecked(bool* aChecked)
{
  *aChecked = Checked();
  return NS_OK;
}

void
HTMLInputElement::SetCheckedChanged(bool aCheckedChanged)
{
  DoSetCheckedChanged(aCheckedChanged, true);
}

void
HTMLInputElement::DoSetCheckedChanged(bool aCheckedChanged,
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
HTMLInputElement::SetCheckedChangedInternal(bool aCheckedChanged)
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
HTMLInputElement::SetChecked(bool aChecked)
{
  DoSetChecked(aChecked, true, true);
  return NS_OK;
}

void
HTMLInputElement::DoSetChecked(bool aChecked, bool aNotify,
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
    return;
  }

  // Set checked
  if (mType != NS_FORM_INPUT_RADIO) {
    SetCheckedInternal(aChecked, aNotify);
    return;
  }

  // For radio button, we need to do some extra fun stuff
  if (aChecked) {
    RadioSetChecked(aNotify);
    return;
  }

  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    container->SetCurrentRadioButton(name, nullptr);
  }
  // SetCheckedInternal is going to ask all radios to update their
  // validity state. We have to be sure the radio group container knows
  // the currently selected radio.
  SetCheckedInternal(false, aNotify);
}

void
HTMLInputElement::RadioSetChecked(bool aNotify)
{
  // Find the selected radio button so we can deselect it
  nsCOMPtr<nsIDOMHTMLInputElement> currentlySelected = GetSelectedRadioButton();

  // Deselect the currently selected radio button
  if (currentlySelected) {
    // Pass true for the aNotify parameter since the currently selected
    // button is already in the document.
    static_cast<HTMLInputElement*>(currentlySelected.get())
      ->SetCheckedInternal(false, true);
  }

  // Let the group know that we are now the One True Radio Button
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (container) {
    nsAutoString name;
    GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
    container->SetCurrentRadioButton(name, this);
  }

  // SetCheckedInternal is going to ask all radios to update their
  // validity state.
  SetCheckedInternal(true, aNotify);
}

nsIRadioGroupContainer*
HTMLInputElement::GetRadioGroupContainer() const
{
  NS_ASSERTION(mType == NS_FORM_INPUT_RADIO,
               "GetRadioGroupContainer should only be called when type='radio'");

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  if (name.IsEmpty()) {
    return nullptr;
  }

  if (mForm) {
    return mForm;
  }

  return static_cast<nsDocument*>(GetCurrentDoc());
}

already_AddRefed<nsIDOMHTMLInputElement>
HTMLInputElement::GetSelectedRadioButton()
{
  nsIRadioGroupContainer* container = GetRadioGroupContainer();
  if (!container) {
    return nullptr;
  }

  nsAutoString name;
  GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  nsCOMPtr<nsIDOMHTMLInputElement> selected = container->GetCurrentRadioButton(name);
  return selected.forget();
}

nsresult
HTMLInputElement::MaybeSubmitForm(nsPresContext* aPresContext)
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
    nsMouseEvent event(true, NS_MOUSE_CLICK, nullptr, nsMouseEvent::eReal);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(submitContent, &event, &status);
  } else if (mForm->HasSingleTextControl() &&
             (mForm->HasAttr(kNameSpaceID_None, nsGkAtoms::novalidate) ||
              mForm->CheckValidFormSubmission())) {
    // TODO: removing this code and have the submit event sent by the form,
    // bug 592124.
    // If there's only one text control, just submit the form
    // Hold strong ref across the event
    nsRefPtr<mozilla::dom::HTMLFormElement> form = mForm;
    nsFormEvent event(true, NS_FORM_SUBMIT);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(mForm, &event, &status);
  }

  return NS_OK;
}

void
HTMLInputElement::SetCheckedInternal(bool aChecked, bool aNotify)
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

void
HTMLInputElement::Focus(ErrorResult& aError)
{
  if (mType != NS_FORM_INPUT_FILE) {
    nsGenericHTMLElement::Focus(aError);
    return;
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
      if (formCtrl && formCtrl->GetType() == NS_FORM_BUTTON_BUTTON) {
        nsCOMPtr<nsIDOMElement> element = do_QueryInterface(formCtrl);
        nsIFocusManager* fm = nsFocusManager::GetFocusManager();
        if (fm && element) {
          fm->SetFocus(element, 0);
        }
        break;
      }
    }
  }

  return;
}

NS_IMETHODIMP
HTMLInputElement::Select()
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
    if (SameCOMIdentity(static_cast<nsIDOMNode*>(this), focusedElement)) {
      // Now Select all the text!
      SelectAll(presContext);
    }
  }

  return NS_OK;
}

bool
HTMLInputElement::DispatchSelectEvent(nsPresContext* aPresContext)
{
  nsEventStatus status = nsEventStatus_eIgnore;

  // If already handling select event, don't dispatch a second.
  if (!mHandlingSelectEvent) {
    nsEvent event(nsContentUtils::IsCallerChrome(), NS_FORM_SELECTED);

    mHandlingSelectEvent = true;
    nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                aPresContext, &event, nullptr, &status);
    mHandlingSelectEvent = false;
  }

  // If the DOM event was not canceled (e.g. by a JS event handler
  // returning false)
  return (status == nsEventStatus_eIgnore);
}

void
HTMLInputElement::SelectAll(nsPresContext* aPresContext)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    formControlFrame->SetFormProperty(nsGkAtoms::select, EmptyString());
  }
}

NS_IMETHODIMP
HTMLInputElement::FireAsyncClickHandler()
{
  nsCOMPtr<nsIRunnable> event = new AsyncClickHandler(this);
  return NS_DispatchToMainThread(event);
}

bool
HTMLInputElement::NeedToInitializeEditorForEvent(nsEventChainPreVisitor& aVisitor) const
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

bool
HTMLInputElement::IsDisabledForEvents(uint32_t aMessage)
{
  return IsElementDisabledForEvents(aMessage, GetPrimaryFrame());
}

nsresult
HTMLInputElement::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->message)) {
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
        if (mForm) {
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

  // If mNoContentDispatch is true we will not allow content to handle
  // this event.  But to allow middle mouse button paste to work we must allow
  // middle clicks to go to text fields anyway.
  if (aVisitor.mEvent->mFlags.mNoContentDispatch) {
    aVisitor.mItemFlags |= NS_NO_CONTENT_DISPATCH;
  }
  if (IsSingleLineTextControl(false) &&
      aVisitor.mEvent->message == NS_MOUSE_CLICK &&
      aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT &&
      static_cast<nsMouseEvent*>(aVisitor.mEvent)->button ==
        nsMouseEvent::eMiddleButton) {
    aVisitor.mEvent->mFlags.mNoContentDispatch = false;
  }

  // We must cache type because mType may change during JS event (bug 2369)
  aVisitor.mItemFlags |= mType;

  // Fire onchange (if necessary), before we do the blur, bug 357684.
  if (aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    // Experimental mobile types rely on the system UI to prevent users to not
    // set invalid values but we have to be extra-careful. Especially if the
    // option has been enabled on desktop.
    if (IsExperimentalMobileType(mType)) {
      nsAutoString aValue;
      GetValueInternal(aValue);
      SetValueInternal(aValue, false, false);
    }
    FireChangeEventIfNeeded();
  }

  if (mType == NS_FORM_INPUT_RANGE &&
      (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
       aVisitor.mEvent->message == NS_BLUR_CONTENT)) {
    // Just as nsGenericHTMLFormElementWithState::PreHandleEvent calls
    // nsIFormControlFrame::SetFocus, we handle focus here.
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
    }
  }

  return nsGenericHTMLFormElementWithState::PreHandleEvent(aVisitor);
}

void
HTMLInputElement::StartRangeThumbDrag(nsGUIEvent* aEvent)
{
  mIsDraggingRange = true;
  mRangeThumbDragStartValue = GetValueAsDecimal();
  nsIPresShell::SetCapturingContent(this, CAPTURE_IGNOREALLOWED |
                                          CAPTURE_RETARGETTOELEMENT);
  nsRangeFrame* rangeFrame = do_QueryFrame(GetPrimaryFrame());
  SetValueOfRangeForUserEvent(rangeFrame->GetValueAtEventPoint(aEvent));
}

void
HTMLInputElement::FinishRangeThumbDrag(nsGUIEvent* aEvent)
{
  MOZ_ASSERT(mIsDraggingRange);

  if (nsIPresShell::GetCapturingContent() == this) {
    nsIPresShell::SetCapturingContent(nullptr, 0); // cancel capture
  }
  if (aEvent) {
    nsRangeFrame* rangeFrame = do_QueryFrame(GetPrimaryFrame());
    SetValueOfRangeForUserEvent(rangeFrame->GetValueAtEventPoint(aEvent));
  }
  mIsDraggingRange = false;
  FireChangeEventIfNeeded();
}

void
HTMLInputElement::CancelRangeThumbDrag(bool aIsForUserEvent)
{
  MOZ_ASSERT(mIsDraggingRange);

  mIsDraggingRange = false;
  if (nsIPresShell::GetCapturingContent() == this) {
    nsIPresShell::SetCapturingContent(nullptr, 0); // cancel capture
  }
  if (aIsForUserEvent) {
    SetValueOfRangeForUserEvent(mRangeThumbDragStartValue);
  } else {
    // Don't dispatch an 'input' event - at least not using
    // DispatchTrustedEvent.
    // TODO: decide what we should do here - bug 851782.
    nsAutoString val;
    ConvertNumberToString(mRangeThumbDragStartValue, val);
    SetValueInternal(val, true, true);
    nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
    if (frame) {
      frame->UpdateForValueChange();
    }
    nsRefPtr<nsAsyncDOMEvent> event =
      new nsAsyncDOMEvent(this, NS_LITERAL_STRING("input"), true, false);
    event->RunDOMEventWhenSafe();
  }
}

void
HTMLInputElement::SetValueOfRangeForUserEvent(Decimal aValue)
{
  MOZ_ASSERT(aValue.isFinite());

  nsAutoString val;
  ConvertNumberToString(aValue, val);
  SetValueInternal(val, true, true);
  nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
  if (frame) {
    frame->UpdateForValueChange();
  }
  nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                       static_cast<nsIDOMHTMLInputElement*>(this),
                                       NS_LITERAL_STRING("input"), true,
                                       false);
}

static bool
SelectTextFieldOnFocus()
{
  if (!gSelectTextFieldOnFocus) {
    int32_t selectTextfieldsOnKeyFocus = -1;
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

static bool
IsLTR(Element* aElement)
{
  nsIFrame* frame = aElement->GetPrimaryFrame();
  if (frame) {
    return frame->StyleVisibility()->mDirection == NS_STYLE_DIRECTION_LTR;
  }
  // at least for HTML, directionality is exclusively LTR or RTL
  return aElement->GetDirectionality() == eDir_LTR;
}

bool
HTMLInputElement::ShouldPreventDOMActivateDispatch(EventTarget* aOriginalTarget)
{
  /*
   * For the moment, there is only one situation where we actually want to
   * prevent firing a DOMActivate event:
   *  - we are a <input type='file'> that just got a click event,
   *  - the event was targeted to our button which should have sent a
   *    DOMActivate event.
   */

  if (mType != NS_FORM_INPUT_FILE) {
    return false;
  }

  nsCOMPtr<nsIContent> target = do_QueryInterface(aOriginalTarget);
  if (!target) {
    return false;
  }

  return target->GetParent() == this &&
         target->IsRootOfNativeAnonymousSubtree() &&
         target->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                             nsGkAtoms::button, eCaseMatters);
}

void
HTMLInputElement::MaybeFireAsyncClickHandler(nsEventChainPostVisitor& aVisitor)
{
  // Open a file picker when we receive a click on a <input type='file'>, or
  // open a color picker when we receive a click on a <input type='color'>.
  // A click is handled in the following cases:
  // - preventDefault() has not been called (or something similar);
  // - it's the left mouse button.
  // We do not prevent non-trusted click because authors can already use
  // .click(). However, the file picker will follow the rules of popup-blocking.
  if ((mType == NS_FORM_INPUT_FILE || mType == NS_FORM_INPUT_COLOR) &&
      NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) &&
      !aVisitor.mEvent->mFlags.mDefaultPrevented) {
    FireAsyncClickHandler();
  }
}

nsresult
HTMLInputElement::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  if (!aVisitor.mPresContext) {
    // Hack alert! In order to open file picker even in case the element isn't
    // in document, fire click handler even without PresContext.
    MaybeFireAsyncClickHandler(aVisitor);
    return NS_OK;
  }

  if (aVisitor.mEvent->message == NS_FOCUS_CONTENT ||
      aVisitor.mEvent->message == NS_BLUR_CONTENT) {
    if (aVisitor.mEvent->message == NS_FOCUS_CONTENT &&
        MayFireChangeOnBlur()) {
      GetValueInternal(mFocusedValue);
    }

    if (mIsDraggingRange &&
        aVisitor.mEvent->message == NS_BLUR_CONTENT) {
      FinishRangeThumbDrag();
    }

    UpdateValidityUIBits(aVisitor.mEvent->message == NS_FOCUS_CONTENT);

    UpdateState(true);
  }

  nsresult rv = NS_OK;
  bool outerActivateEvent = !!(aVisitor.mItemFlags & NS_OUTER_ACTIVATE_EVENT);
  bool originalCheckedValue =
    !!(aVisitor.mItemFlags & NS_ORIGINAL_CHECKED_VALUE);
  bool noContentDispatch = !!(aVisitor.mItemFlags & NS_NO_CONTENT_DISPATCH);
  uint8_t oldType = NS_CONTROL_TYPE(aVisitor.mItemFlags);

  // Ideally we would make the default action for click and space just dispatch
  // DOMActivate, and the default action for DOMActivate flip the checkbox/
  // radio state and fire onchange.  However, for backwards compatibility, we
  // need to flip the state before firing click, and we need to fire click
  // when space is pressed.  So, we just nest the firing of DOMActivate inside
  // the click event handling, and allow cancellation of DOMActivate to cancel
  // the click.
  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault &&
      !IsSingleLineTextControl(true) &&
      NS_IS_MOUSE_LEFT_CLICK(aVisitor.mEvent) &&
      !ShouldPreventDOMActivateDispatch(aVisitor.mEvent->originalTarget)) {
    nsUIEvent actEvent(aVisitor.mEvent->mFlags.mIsTrusted, NS_UI_ACTIVATE, 1);

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
        if (mForm) {
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
  aVisitor.mEvent->mFlags.mNoContentDispatch = noContentDispatch;

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
        if (previous) {
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
              !(static_cast<nsFocusEvent*>(aVisitor.mEvent))->fromRaise &&
              SelectTextFieldOnFocus()) {
            nsIDocument* document = GetCurrentDoc();
            if (document) {
              uint32_t lastFocusMethod;
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
          nsKeyEvent* keyEvent = (nsKeyEvent*)aVisitor.mEvent;

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
                nsMouseEvent event(aVisitor.mEvent->mFlags.mIsTrusted,
                                   NS_MOUSE_CLICK, nullptr, nsMouseEvent::eReal);
                event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
                nsEventStatus status = nsEventStatus_eIgnore;

                nsEventDispatcher::Dispatch(static_cast<nsIContent*>(this),
                                            aVisitor.mPresContext, &event,
                                            nullptr, &status);
                aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
              } // case
            } // switch
          }
          if (aVisitor.mEvent->message == NS_KEY_PRESS &&
              mType == NS_FORM_INPUT_RADIO && !keyEvent->IsAlt() &&
              !keyEvent->IsControl() && !keyEvent->IsMeta()) {
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
                    nsMouseEvent event(aVisitor.mEvent->mFlags.mIsTrusted,
                                       NS_MOUSE_CLICK, nullptr,
                                       nsMouseEvent::eReal);
                    event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_KEYBOARD;
                    rv = nsEventDispatcher::Dispatch(radioContent,
                                                     aVisitor.mPresContext,
                                                     &event, nullptr, &status);
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
               (IsSingleLineTextControl(false, mType) ||
                IsExperimentalMobileType(mType))) {
            FireChangeEventIfNeeded();
            rv = MaybeSubmitForm(aVisitor.mPresContext);
            NS_ENSURE_SUCCESS(rv, rv);
          }

          if (aVisitor.mEvent->message == NS_KEY_PRESS &&
              mType == NS_FORM_INPUT_RANGE && !keyEvent->IsAlt() &&
              !keyEvent->IsControl() && !keyEvent->IsMeta() &&
              (keyEvent->keyCode == NS_VK_LEFT ||
               keyEvent->keyCode == NS_VK_RIGHT ||
               keyEvent->keyCode == NS_VK_UP ||
               keyEvent->keyCode == NS_VK_DOWN ||
               keyEvent->keyCode == NS_VK_PAGE_UP ||
               keyEvent->keyCode == NS_VK_PAGE_DOWN ||
               keyEvent->keyCode == NS_VK_HOME ||
               keyEvent->keyCode == NS_VK_END)) {
            Decimal minimum = GetMinimum();
            Decimal maximum = GetMaximum();
            MOZ_ASSERT(minimum.isFinite() && maximum.isFinite());
            if (minimum < maximum) { // else the value is locked to the minimum
              Decimal value = GetValueAsDecimal();
              Decimal step = GetStep();
              if (step == kStepAny) {
                step = GetDefaultStep();
              }
              MOZ_ASSERT(value.isFinite() && step.isFinite());
              Decimal newValue;
              switch (keyEvent->keyCode) {
                case  NS_VK_LEFT:
                  newValue = value + (IsLTR(this) ? -step : step);
                  break;
                case  NS_VK_RIGHT:
                  newValue = value + (IsLTR(this) ? step : -step);
                  break;
                case  NS_VK_UP:
                  // Even for horizontal range, "up" means "increase"
                  newValue = value + step;
                  break;
                case  NS_VK_DOWN:
                  // Even for horizontal range, "down" means "decrease"
                  newValue = value - step;
                  break;
                case  NS_VK_HOME:
                  newValue = minimum;
                  break;
                case  NS_VK_END:
                  newValue = maximum;
                  break;
                case  NS_VK_PAGE_UP:
                  // For PgUp/PgDn we jump 10% of the total range, unless step
                  // requires us to jump more.
                  newValue = value + std::max(step, (maximum - minimum) / 10);
                  break;
                case  NS_VK_PAGE_DOWN:
                  newValue = value - std::max(step, (maximum - minimum) / 10);
                  break;
              }
              SetValueOfRangeForUserEvent(newValue);
              aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
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
              nsRefPtr<mozilla::dom::HTMLFormElement> form(mForm);
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

  if (NS_SUCCEEDED(rv) && mType == NS_FORM_INPUT_RANGE) {
    PostHandleEventForRangeThumb(aVisitor);
  }

  MaybeFireAsyncClickHandler(aVisitor);

  return rv;
}

void
HTMLInputElement::PostHandleEventForRangeThumb(nsEventChainPostVisitor& aVisitor)
{
  MOZ_ASSERT(mType == NS_FORM_INPUT_RANGE);

  if (nsEventStatus_eConsumeNoDefault == aVisitor.mEventStatus ||
      !(aVisitor.mEvent->eventStructType == NS_MOUSE_EVENT ||
        aVisitor.mEvent->eventStructType == NS_TOUCH_EVENT ||
        aVisitor.mEvent->eventStructType == NS_KEY_EVENT)) {
    return;
  }

  nsRangeFrame* rangeFrame = do_QueryFrame(GetPrimaryFrame());
  if (!rangeFrame && mIsDraggingRange) {
    CancelRangeThumbDrag();
    return;
  }

  switch (aVisitor.mEvent->message)
  {
    case NS_MOUSE_BUTTON_DOWN:
    case NS_TOUCH_START: {
      if (mIsDraggingRange) {
        break;
      }
      if (nsIPresShell::GetCapturingContent()) {
        break; // don't start drag if someone else is already capturing
      }
      nsInputEvent* inputEvent = static_cast<nsInputEvent*>(aVisitor.mEvent);
      if (inputEvent->IsShift() || inputEvent->IsControl() ||
          inputEvent->IsAlt() || inputEvent->IsMeta() ||
          inputEvent->IsAltGraph() || inputEvent->IsFn() ||
          inputEvent->IsOS()) {
        break; // ignore
      }
      if (aVisitor.mEvent->message == NS_MOUSE_BUTTON_DOWN) {
        nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(aVisitor.mEvent);
        if (mouseEvent->buttons == nsMouseEvent::eLeftButtonFlag) {
          StartRangeThumbDrag(inputEvent);
        } else if (mIsDraggingRange) {
          CancelRangeThumbDrag();
        }
      } else {
        nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(aVisitor.mEvent);
        if (touchEvent->touches.Length() == 1) {
          StartRangeThumbDrag(inputEvent);
        } else if (mIsDraggingRange) {
          CancelRangeThumbDrag();
        }
      }
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
    } break;

    case NS_MOUSE_MOVE:
    case NS_TOUCH_MOVE:
      if (!mIsDraggingRange) {
        break;
      }
      if (nsIPresShell::GetCapturingContent() != this) {
        // Someone else grabbed capture.
        CancelRangeThumbDrag();
        break;
      }
      SetValueOfRangeForUserEvent(rangeFrame->GetValueAtEventPoint(
                                    static_cast<nsInputEvent*>(aVisitor.mEvent)));
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
      break;

    case NS_MOUSE_BUTTON_UP:
    case NS_TOUCH_END:
      if (!mIsDraggingRange) {
        break;
      }
      // We don't check to see whether we are the capturing content here and
      // call CancelRangeThumbDrag() if that is the case. We just finish off
      // the drag and set our final value (unless someone has called
      // preventDefault() and prevents us getting here).
      FinishRangeThumbDrag(static_cast<nsInputEvent*>(aVisitor.mEvent));
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
      break;

    case NS_KEY_PRESS:
      if (mIsDraggingRange &&
          static_cast<nsKeyEvent*>(aVisitor.mEvent)->keyCode == NS_VK_ESCAPE) {
        CancelRangeThumbDrag();
      }
      break;

    case NS_TOUCH_CANCEL:
      if (mIsDraggingRange) {
        CancelRangeThumbDrag();
      }
      break;
  }
}

void
HTMLInputElement::MaybeLoadImage()
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
HTMLInputElement::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                             nsIContent* aBindingParent,
                             bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElementWithState::BindToTree(aDocument, aParent,
                                                              aBindingParent,
                                                              aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  nsImageLoadingContent::BindToTree(aDocument, aParent, aBindingParent,
                                    aCompileEventHandlers);

  if (mType == NS_FORM_INPUT_IMAGE) {
    // Our base URI may have changed; claim that our URI changed, and the
    // nsImageLoadingContent will decide whether a new image load is warranted.
    if (HasAttr(kNameSpaceID_None, nsGkAtoms::src)) {
      // FIXME: Bug 660963 it would be nice if we could just have
      // ClearBrokenState update our state and do it fast...
      ClearBrokenState();
      RemoveStatesSilently(NS_EVENT_STATE_BROKEN);
      nsContentUtils::AddScriptRunner(
        NS_NewRunnableMethod(this, &HTMLInputElement::MaybeLoadImage));
    }
  }

  // Add radio to document if we don't have a form already (if we do it's
  // already been added into that group)
  if (aDocument && !mForm && mType == NS_FORM_INPUT_RADIO) {
    AddedToRadioGroup();
  }

  // Set direction based on value if dir=auto
  SetDirectionIfAuto(HasDirAuto(), false);

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
HTMLInputElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  // If we have a form and are unbound from it,
  // nsGenericHTMLFormElementWithState::UnbindFromTree() will unset the form and
  // that takes care of form's WillRemove so we just have to take care
  // of the case where we're removing from the document and we don't
  // have a form
  if (!mForm && mType == NS_FORM_INPUT_RADIO) {
    WillRemoveFromRadioGroup();
  }

  nsImageLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLFormElementWithState::UnbindFromTree(aDeep, aNullParent);

  // GetCurrentDoc is returning nullptr so we can update the value
  // missing validity state to reflect we are no longer into a doc.
  UpdateValueMissingValidityState();
  // We might be no longer disabled because of parent chain changed.
  UpdateBarredFromConstraintValidation();

  // And now make sure our state is up to date
  UpdateState(false);
}

void
HTMLInputElement::HandleTypeChange(uint8_t aNewType)
{
  if (mType == NS_FORM_INPUT_RANGE && mIsDraggingRange) {
    CancelRangeThumbDrag(false);
  }

  ValueModeType aOldValueMode = GetValueMode();
  uint8_t oldType = mType;
  nsAutoString aOldValue;

  if (aOldValueMode == VALUE_MODE_VALUE) {
    GetValue(aOldValue);
  }

  // We already have a copy of the value, lets free it and changes the type.
  FreeData();
  mType = aNewType;

  if (IsSingleLineTextControl()) {
    mInputData.mState = new nsTextEditorState(this);
  }

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
          value = aOldValue;
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

  // Updating mFocusedValue in consequence:
  // If the new type fires a change event on blur, but the previous type
  // doesn't, we should set mFocusedValue to the current value.
  // Otherwise, if the new type doesn't fire a change event on blur, but the
  // previous type does, we should clear out mFocusedValue.
  if (MayFireChangeOnBlur(mType) && !MayFireChangeOnBlur(oldType)) {
    GetValueInternal(mFocusedValue);
  } else if (!IsSingleLineTextControl(mType, false) &&
             IsSingleLineTextControl(oldType, false)) {
    mFocusedValue.Truncate();
  }

  UpdateHasRange();

  // Do not notify, it will be done after if needed.
  UpdateAllValidityStates(false);
}

void
HTMLInputElement::SanitizeValue(nsAString& aValue)
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
    case NS_FORM_INPUT_NUMBER:
      {
        nsresult ec;
        double val = PromiseFlatString(aValue).ToDouble(&ec);
        if (NS_FAILED(ec) || !IsFinite(val)) {
          aValue.Truncate();
        }
      }
      break;
    case NS_FORM_INPUT_RANGE:
      {
        Decimal minimum = GetMinimum();
        Decimal maximum = GetMaximum();
        MOZ_ASSERT(minimum.isFinite() && maximum.isFinite(),
                   "type=range should have a default maximum/minimum");

        // We use this to avoid modifying the string unnecessarily, since that
        // may introduce rounding. This is set to true only if the value we
        // parse out from aValue needs to be sanitized.
        bool needSanitization = false;

        Decimal value;
        bool ok = ConvertStringToNumber(aValue, value);
        if (!ok) {
          needSanitization = true;
          // Set value to midway between minimum and maximum.
          value = maximum <= minimum ? minimum : minimum + (maximum - minimum)/2;
        } else if (value < minimum || maximum < minimum) {
          needSanitization = true;
          value = minimum;
        } else if (value > maximum) {
          needSanitization = true;
          value = maximum;
        }

        Decimal step = GetStep();
        if (step != kStepAny) {
          Decimal stepBase = GetStepBase();
          // There could be rounding issues below when dealing with fractional
          // numbers, but let's ignore that until ECMAScript supplies us with a
          // decimal number type.
          Decimal deltaToStep = NS_floorModulo(value - stepBase, step);
          if (deltaToStep != 0) {
            // "suffering from a step mismatch"
            // Round the element's value to the nearest number for which the
            // element would not suffer from a step mismatch, and which is
            // greater than or equal to the minimum, and, if the maximum is not
            // less than the minimum, which is less than or equal to the
            // maximum, if there is a number that matches these constraints:
            MOZ_ASSERT(deltaToStep > 0, "stepBelow/stepAbove will be wrong");
            Decimal stepBelow = value - deltaToStep;
            Decimal stepAbove = value - deltaToStep + step;
            Decimal halfStep = step / 2;
            bool stepAboveIsClosest = (stepAbove - value) <= halfStep;
            bool stepAboveInRange = stepAbove >= minimum &&
                                    stepAbove <= maximum;
            bool stepBelowInRange = stepBelow >= minimum &&
                                    stepBelow <= maximum;

            if ((stepAboveIsClosest || !stepBelowInRange) && stepAboveInRange) {
              needSanitization = true;
              value = stepAbove;
            } else if ((!stepAboveIsClosest || !stepAboveInRange) && stepBelowInRange) {
              needSanitization = true;
              value = stepBelow;
            }
          }
        }

        if (needSanitization) {
          char buf[32];
          DebugOnly<bool> ok = value.toString(buf, ArrayLength(buf));
          aValue.AssignASCII(buf);
          MOZ_ASSERT(ok, "buf not big enough");
        }
      }
      break;
    case NS_FORM_INPUT_DATE:
      {
        if (!aValue.IsEmpty() && !IsValidDate(aValue)) {
          aValue.Truncate();
        }
      }
      break;
    case NS_FORM_INPUT_TIME:
      {
        if (!aValue.IsEmpty() && !IsValidTime(aValue)) {
          aValue.Truncate();
        }
      }
      break;
    case NS_FORM_INPUT_COLOR:
      {
        if (IsValidSimpleColor(aValue)) {
          ToLowerCase(aValue);
        } else {
          // Set default (black) color, if aValue wasn't parsed correctly.
          aValue.AssignLiteral("#000000");
        }
      }
      break;
  }
}

bool HTMLInputElement::IsValidSimpleColor(const nsAString& aValue) const
{
  if (aValue.Length() != 7 || aValue.First() != '#') {
    return false;
  }

  for (int i = 1; i < 7; ++i) {
    if (!nsCRT::IsAsciiDigit(aValue[i]) &&
        !(aValue[i] >= 'a' && aValue[i] <= 'f') &&
        !(aValue[i] >= 'A' && aValue[i] <= 'F')) {
      return false;
    }
  }
  return true;
}

bool
HTMLInputElement::IsValidDate(const nsAString& aValue) const
{
  uint32_t year, month, day;
  return GetValueAsDate(aValue, &year, &month, &day);
}

bool
HTMLInputElement::GetValueAsDate(const nsAString& aValue,
                                 uint32_t* aYear,
                                 uint32_t* aMonth,
                                 uint32_t* aDay) const
{

/*
 * Parse the year, month, day values out a date string formatted as 'yyyy-mm-dd'.
 * -The year must be 4 or more digits long, and year > 0
 * -The month must be exactly 2 digits long, and 01 <= month <= 12
 * -The day must be exactly 2 digit long, and 01 <= day <= maxday
 *  Where maxday is the number of days in the month 'month' and year 'year'
 */

  if (aValue.Length() < 10) {
    return false;
  }

  uint32_t endOfYearOffset = 0;
  for (; NS_IsAsciiDigit(aValue[endOfYearOffset]); ++endOfYearOffset);

  // The year must be at least 4 digits long.
  if (aValue[endOfYearOffset] != '-' || endOfYearOffset < 4) {
    return false;
  }

  // Now, we know where is the next '-' and what should be the size of the
  // string.
  if (aValue[endOfYearOffset + 3] != '-' ||
      aValue.Length() != 10 + (endOfYearOffset - 4)) {
    return false;
  }

  nsresult ec;
  *aYear = PromiseFlatString(StringHead(aValue, endOfYearOffset)).ToInteger(&ec);
  if (NS_FAILED(ec) || *aYear == 0) {
    return false;
  }

  if (!DigitSubStringToNumber(aValue, endOfYearOffset + 1, 2, aMonth) ||
      *aMonth < 1 || *aMonth > 12) {
    return false;
  }

  return DigitSubStringToNumber(aValue, endOfYearOffset + 4, 2, aDay) &&
         *aDay > 0 && *aDay <= NumberOfDaysInMonth(*aMonth, *aYear);
}

uint32_t
HTMLInputElement::NumberOfDaysInMonth(uint32_t aMonth, uint32_t aYear) const
{
/*
 * Returns the number of days in a month.
 * Months that are |longMonths| always have 31 days.
 * Months that are not |longMonths| have 30 days except February (month 2).
 * February has 29 days during leap years which are years that are divisible by 400.
 * or divisible by 100 and 4. February has 28 days otherwise.
 */

  static const bool longMonths[] = { true, false, true, false, true, false,
                                     true, true, false, true, false, true };
  MOZ_ASSERT(aMonth <= 12 && aMonth > 0);

  if (longMonths[aMonth-1]) {
    return 31;
  }

  if (aMonth != 2) {
    return 30;
  }

  return (aYear % 400 == 0 || (aYear % 100 != 0 && aYear % 4 == 0))
          ? 29 : 28;
}

/* static */ bool
HTMLInputElement::DigitSubStringToNumber(const nsAString& aStr,
                                         uint32_t aStart, uint32_t aLen,
                                         uint32_t* aRetVal)
{
  MOZ_ASSERT(aStr.Length() > (aStart + aLen - 1));

  for (uint32_t offset = 0; offset < aLen; ++offset) {
    if (!NS_IsAsciiDigit(aStr[aStart + offset])) {
      return false;
    }
  }

  nsresult ec;
  *aRetVal = static_cast<uint32_t>(PromiseFlatString(Substring(aStr, aStart, aLen)).ToInteger(&ec));

  return NS_SUCCEEDED(ec);
}

bool
HTMLInputElement::IsValidTime(const nsAString& aValue) const
{
  return ParseTime(aValue, nullptr);
}

/* static */ bool
HTMLInputElement::ParseTime(const nsAString& aValue, uint32_t* aResult)
{
  /* The string must have the following parts:
   * - HOURS: two digits, value being in [0, 23];
   * - Colon (:);
   * - MINUTES: two digits, value being in [0, 59];
   * - Optional:
   *   - Colon (:);
   *   - SECONDS: two digits, value being in [0, 59];
   *   - Optional:
   *     - DOT (.);
   *     - FRACTIONAL SECONDS: one to three digits, no value range.
   */

  // The following format is the shorter one allowed: "HH:MM".
  if (aValue.Length() < 5) {
    return false;
  }

  uint32_t hours;
  if (!DigitSubStringToNumber(aValue, 0, 2, &hours) || hours > 23) {
    return false;
  }

  // Hours/minutes separator.
  if (aValue[2] != ':') {
    return false;
  }

  uint32_t minutes;
  if (!DigitSubStringToNumber(aValue, 3, 2, &minutes) || minutes > 59) {
    return false;
  }

  if (aValue.Length() == 5) {
    if (aResult) {
      *aResult = ((hours * 60) + minutes) * 60000;
    }
    return true;
  }

  // The following format is the next shorter one: "HH:MM:SS".
  if (aValue.Length() < 8 || aValue[5] != ':') {
    return false;
  }

  uint32_t seconds;
  if (!DigitSubStringToNumber(aValue, 6, 2, &seconds) || seconds > 59) {
    return false;
  }

  if (aValue.Length() == 8) {
    if (aResult) {
      *aResult = (((hours * 60) + minutes) * 60 + seconds) * 1000;
    }
    return true;
  }

  // The string must follow this format now: "HH:MM:SS.{s,ss,sss}".
  // There can be 1 to 3 digits for the fractions of seconds.
  if (aValue.Length() == 9 || aValue.Length() > 12 || aValue[8] != '.') {
    return false;
  }

  uint32_t fractionsSeconds;
  if (!DigitSubStringToNumber(aValue, 9, aValue.Length() - 9, &fractionsSeconds)) {
    return false;
  }

  if (aResult) {
    *aResult = (((hours * 60) + minutes) * 60 + seconds) * 1000 +
               // NOTE: there is 10.0 instead of 10 and static_cast<int> because
               // some old [and stupid] compilers can't just do the right thing.
               fractionsSeconds * pow(10.0, static_cast<int>(3 - (aValue.Length() - 9)));
  }

  return true;
}

bool
HTMLInputElement::ParseAttribute(int32_t aNamespaceID,
                                 nsIAtom* aAttribute,
                                 const nsAString& aValue,
                                 nsAttrValue& aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::type) {
      // XXX ARG!! This is major evilness. ParseAttribute
      // shouldn't set members. Override SetAttr instead
      int32_t newType;
      bool success = aResult.ParseEnumValue(aValue, kInputTypeTable, false);
      if (success) {
        newType = aResult.GetEnumValue();
        if ((IsExperimentalMobileType(newType) &&
             !Preferences::GetBool("dom.experimental_forms", false)) ||
            (newType == NS_FORM_INPUT_RANGE &&
             !Preferences::GetBool("dom.experimental_forms_range", false)) ||
            (newType == NS_FORM_INPUT_COLOR &&
             !Preferences::GetBool("dom.forms.color", false))) {
          newType = kInputDefaultType->value;
          aResult.SetTo(newType, &aValue);
        }
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
    if (aAttribute == nsGkAtoms::inputmode) {
      return aResult.ParseEnumValue(aValue, kInputInputmodeTable, false);
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
    nsGenericHTMLFormElementWithState::MapImageBorderAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElementWithState::MapImageMarginAttributeInto(aAttributes, aData);
    nsGenericHTMLFormElementWithState::MapImageSizeAttributesInto(aAttributes, aData);
    // Images treat align as "float"
    nsGenericHTMLFormElementWithState::MapImageAlignAttributeInto(aAttributes, aData);
  }

  nsGenericHTMLFormElementWithState::MapCommonAttributesInto(aAttributes, aData);
}

nsChangeHint
HTMLInputElement::GetAttributeChangeHint(const nsIAtom* aAttribute,
                                         int32_t aModType) const
{
  nsChangeHint retval =
    nsGenericHTMLFormElementWithState::GetAttributeChangeHint(aAttribute, aModType);
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
HTMLInputElement::IsAttributeMapped(const nsIAtom* aAttribute) const
{
  static const MappedAttributeEntry attributes[] = {
    { &nsGkAtoms::align },
    { &nsGkAtoms::type },
    { nullptr },
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
HTMLInputElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}


// Controllers Methods

nsIControllers*
HTMLInputElement::GetControllers(ErrorResult& aRv)
{
  //XXX: what about type "file"?
  if (IsSingleLineTextControl(false))
  {
    if (!mControllers)
    {
      nsresult rv;
      mControllers = do_CreateInstance(kXULControllersCID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return nullptr;
      }

      nsCOMPtr<nsIController>
        controller(do_CreateInstance("@mozilla.org/editor/editorcontroller;1",
                                     &rv));
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return nullptr;
      }

      mControllers->AppendController(controller);

      controller = do_CreateInstance("@mozilla.org/editor/editingcontroller;1",
                                     &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return nullptr;
      }

      mControllers->AppendController(controller);
    }
  }

  return mControllers;
}

NS_IMETHODIMP
HTMLInputElement::GetControllers(nsIControllers** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  ErrorResult rv;
  nsRefPtr<nsIControllers> controller = GetControllers(rv);
  controller.forget(aResult);
  return rv.ErrorCode();
}

int32_t
HTMLInputElement::GetTextLength(ErrorResult& aRv)
{
  nsAutoString val;
  GetValue(val);
  return val.Length();
}

NS_IMETHODIMP
HTMLInputElement::GetTextLength(int32_t* aTextLength)
{
  ErrorResult rv;
  *aTextLength = GetTextLength(rv);
  return rv.ErrorCode();
}

void
HTMLInputElement::SetSelectionRange(int32_t aSelectionStart,
                                    int32_t aSelectionEnd,
                                    const Optional<nsAString>& aDirection,
                                    ErrorResult& aRv)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      // Default to forward, even if not specified.
      // Note that we don't currently support directionless selections, so
      // "none" is treated like "forward".
      nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eForward;
      if (aDirection.WasPassed() && aDirection.Value().EqualsLiteral("backward")) {
        dir = nsITextControlFrame::eBackward;
      }

      aRv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd, dir);
      if (!aRv.Failed()) {
        aRv = textControlFrame->ScrollSelectionIntoView();
      }
    }
  }
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionRange(int32_t aSelectionStart,
                                    int32_t aSelectionEnd,
                                    const nsAString& aDirection)
{
  ErrorResult rv;
  Optional<nsAString> direction;
  direction = &aDirection;

  SetSelectionRange(aSelectionStart, aSelectionEnd, direction, rv);
  return rv.ErrorCode();
}

int32_t
HTMLInputElement::GetSelectionStart(ErrorResult& aRv)
{
  int32_t selEnd, selStart;
  aRv = GetSelectionRange(&selStart, &selEnd);

  if (aRv.Failed()) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      aRv = NS_OK;
      return state->GetSelectionProperties().mStart;
    }
  }

  return selStart;
}

NS_IMETHODIMP
HTMLInputElement::GetSelectionStart(int32_t* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);

  ErrorResult rv;
  *aSelectionStart = GetSelectionStart(rv);
  return rv.ErrorCode();
}

void
HTMLInputElement::SetSelectionStart(int32_t aSelectionStart, ErrorResult& aRv)
{
  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().mStart = aSelectionStart;
    return;
  }

  nsAutoString direction;
  aRv = GetSelectionDirection(direction);
  if (aRv.Failed()) {
    return;
  }

  int32_t start, end;
  aRv = GetSelectionRange(&start, &end);
  if (aRv.Failed()) {
    return;
  }

  start = aSelectionStart;
  if (end < start) {
    end = start;
  }

  aRv = SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionStart(int32_t aSelectionStart)
{
  ErrorResult rv;
  SetSelectionStart(aSelectionStart, rv);
  return rv.ErrorCode();
}

int32_t
HTMLInputElement::GetSelectionEnd(ErrorResult& aRv)
{
  int32_t selStart, selEnd;
  aRv = GetSelectionRange(&selStart, &selEnd);

  if (aRv.Failed()) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      aRv = NS_OK;
      return state->GetSelectionProperties().mEnd;
    }
  }

  return selEnd;
}

NS_IMETHODIMP
HTMLInputElement::GetSelectionEnd(int32_t* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);

  ErrorResult rv;
  *aSelectionEnd = GetSelectionEnd(rv);
  return rv.ErrorCode();
}

void
HTMLInputElement::SetSelectionEnd(int32_t aSelectionEnd, ErrorResult& aRv)
{
  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().mEnd = aSelectionEnd;
    return;
  }

  nsAutoString direction;
  aRv = GetSelectionDirection(direction);
  if (aRv.Failed()) {
    return;
  }

  int32_t start, end;
  aRv = GetSelectionRange(&start, &end);
  if (aRv.Failed()) {
    return;
  }

  end = aSelectionEnd;
  if (start > end) {
    start = end;
  }

  aRv = SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionEnd(int32_t aSelectionEnd)
{
  ErrorResult rv;
  SetSelectionEnd(aSelectionEnd, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLInputElement::GetFiles(nsIDOMFileList** aFileList)
{
  nsRefPtr<nsDOMFileList> list = GetFiles();
  list.forget(aFileList);
  return NS_OK;
}

nsresult
HTMLInputElement::GetSelectionRange(int32_t* aSelectionStart,
                                    int32_t* aSelectionEnd)
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

void
HTMLInputElement::GetSelectionDirection(nsAString& aDirection, ErrorResult& aRv)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);

  if (formControlFrame) {
    nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
    if (textControlFrame) {
      nsITextControlFrame::SelectionDirection dir;
      rv = textControlFrame->GetSelectionRange(nullptr, nullptr, &dir);
      if (NS_SUCCEEDED(rv)) {
        DirectionToName(dir, aDirection);
      }
    }
  }

  if (NS_FAILED(rv)) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      DirectionToName(state->GetSelectionProperties().mDirection, aDirection);
      return;
    }
  }

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

NS_IMETHODIMP
HTMLInputElement::GetSelectionDirection(nsAString& aDirection)
{
  ErrorResult rv;
  GetSelectionDirection(aDirection, rv);
  return rv.ErrorCode();
}

void
HTMLInputElement::SetSelectionDirection(const nsAString& aDirection, ErrorResult& aRv)
{
  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    state->GetSelectionProperties().mDirection = dir;
    return;
  }

  int32_t start, end;
  aRv = GetSelectionRange(&start, &end);
  if (!aRv.Failed()) {
    aRv = SetSelectionRange(start, end, aDirection);
  }
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionDirection(const nsAString& aDirection)
{
  ErrorResult rv;
  SetSelectionDirection(aDirection, rv);
  return rv.ErrorCode();
}

NS_IMETHODIMP
HTMLInputElement::GetPhonetic(nsAString& aPhonetic)
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
  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aTarget);
  if (NS_SUCCEEDED(nsEventDispatcher::CreateEvent(element, aPresContext, nullptr,
                                                  NS_LITERAL_STRING("Events"),
                                                  getter_AddRefs(event)))) {
    event->InitEvent(aEventType, true, true);
    event->SetTrusted(true);

    nsEventDispatcher::DispatchDOMEvent(aTarget, nullptr, event, aPresContext, nullptr);
  }

  return NS_OK;
}
#endif

nsresult
HTMLInputElement::SetDefaultValueAsValue()
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

void
HTMLInputElement::SetDirectionIfAuto(bool aAuto, bool aNotify)
{
  if (aAuto) {
    SetHasDirAuto();
    if (IsSingleLineTextControl(true)) {
      nsAutoString value;
      GetValue(value);
      SetDirectionalityFromValue(this, value, aNotify);
    }
  } else {
    ClearHasDirAuto();
  }
}

NS_IMETHODIMP
HTMLInputElement::Reset()
{
  // We should be able to reset all dirty flags regardless of the type.
  SetCheckedChanged(false);
  SetValueChanged(false);

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      return SetDefaultValueAsValue();
    case VALUE_MODE_DEFAULT_ON:
      DoSetChecked(DefaultChecked(), true, false);
      return NS_OK;
    case VALUE_MODE_FILENAME:
      ClearFiles(false);
      return NS_OK;
    case VALUE_MODE_DEFAULT:
    default:
      return NS_OK;
  }
}

NS_IMETHODIMP
HTMLInputElement::SubmitNamesValues(nsFormSubmission* aFormSubmission)
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
    int32_t x, y;
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

    const nsCOMArray<nsIDOMFile>& files = GetFilesInternal();

    for (int32_t i = 0; i < files.Count(); ++i) {
      aFormSubmission->AddNameFilePair(name, files[i], NullString());
    }

    if (files.Count() == 0) {
      // If no file was selected, pretend we had an empty file with an
      // empty filename.
      aFormSubmission->AddNameFilePair(name, nullptr, NullString());

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
HTMLInputElement::SaveState()
{
  nsRefPtr<HTMLInputElementState> inputState;
  switch (GetValueMode()) {
    case VALUE_MODE_DEFAULT_ON:
      if (mCheckedChanged) {
        inputState = new HTMLInputElementState();
        inputState->SetChecked(mChecked);
      }
      break;
    case VALUE_MODE_FILENAME:
      if (mFiles.Count()) {
        inputState = new HTMLInputElementState();
        inputState->SetFiles(mFiles);
      }
      break;
    case VALUE_MODE_VALUE:
    case VALUE_MODE_DEFAULT:
      // VALUE_MODE_DEFAULT shouldn't have their value saved except 'hidden',
      // mType shouldn't be NS_FORM_INPUT_PASSWORD and value should have changed.
      if ((GetValueMode() == VALUE_MODE_DEFAULT &&
           mType != NS_FORM_INPUT_HIDDEN) ||
          mType == NS_FORM_INPUT_PASSWORD || !mValueChanged) {
        break;
      }

      inputState = new HTMLInputElementState();
      nsAutoString value;
      GetValue(value);
      DebugOnly<nsresult> rv =
        nsLinebreakConverter::ConvertStringLineBreaks(
             value,
             nsLinebreakConverter::eLinebreakPlatform,
             nsLinebreakConverter::eLinebreakContent);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Converting linebreaks failed!");
      inputState->SetValue(value);
      break;
  }

  if (inputState) {
    nsPresState* state = GetPrimaryPresState();
    if (state) {
      state->SetStateProperty(inputState);
    }
  }

  if (mDisabledChanged) {
    nsPresState* state = GetPrimaryPresState();
    if (state) {
      // We do not want to save the real disabled state but the disabled
      // attribute.
      state->SetDisabled(HasAttr(kNameSpaceID_None, nsGkAtoms::disabled));
    }
  }

  return NS_OK;
}

void
HTMLInputElement::DoneCreatingElement()
{
  mParserCreating = false;

  //
  // Restore state as needed.  Note that disabled state applies to all control
  // types.
  //
  bool restoredCheckedState =
    !mInhibitRestoration && NS_SUCCEEDED(GenerateStateKey()) && RestoreFormControlState();

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
HTMLInputElement::IntrinsicState() const
{
  // If you add states here, and they're type-dependent, you need to add them
  // to the type case in AfterSetAttr.

  nsEventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();
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

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  // :in-range and :out-of-range only apply if the element currently has a range.
  if (mHasRange) {
    state |= (GetValidityState(VALIDITY_STATE_RANGE_OVERFLOW) ||
              GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW))
               ? NS_EVENT_STATE_OUTOFRANGE
               : NS_EVENT_STATE_INRANGE;
  }

  return state;
}

bool
HTMLInputElement::RestoreState(nsPresState* aState)
{
  bool restoredCheckedState = false;

  nsCOMPtr<HTMLInputElementState> inputState
    (do_QueryInterface(aState->GetStateProperty()));

  if (inputState) {
    switch (GetValueMode()) {
      case VALUE_MODE_DEFAULT_ON:
        if (inputState->IsCheckedSet()) {
          restoredCheckedState = true;
          DoSetChecked(inputState->GetChecked(), true, true);
        }
        break;
      case VALUE_MODE_FILENAME:
        {
          const nsCOMArray<nsIDOMFile>& files = inputState->GetFiles();
          SetFiles(files, true);
        }
        break;
      case VALUE_MODE_VALUE:
      case VALUE_MODE_DEFAULT:
        if (GetValueMode() == VALUE_MODE_DEFAULT &&
            mType != NS_FORM_INPUT_HIDDEN) {
          break;
        }

        SetValueInternal(inputState->GetValue(), false, true);
        break;
        break;
    }
  }

  if (aState->IsDisabledSet()) {
    SetDisabled(aState->GetDisabled());
  }

  return restoredCheckedState;
}

bool
HTMLInputElement::AllowDrop()
{
  // Allow drop on anything other than file inputs.

  return mType != NS_FORM_INPUT_FILE;
}

/*
 * Radio group stuff
 */

void
HTMLInputElement::AddedToRadioGroup()
{
  // If the element is neither in a form nor a document, there is no group so we
  // should just stop here.
  if (!mForm && !IsInDoc()) {
    return;
  }

  // Make sure not to notify if we're still being created by the parser
  bool notify = !mParserCreating;

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
HTMLInputElement::WillRemoveFromRadioGroup()
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
    container->SetCurrentRadioButton(name, nullptr);
  }

  // Remove this radio from its group in the container.
  // We need to call UpdateValueMissingValidityStateForRadio before to make sure
  // the group validity is updated (with this element being ignored).
  UpdateValueMissingValidityStateForRadio(true);
  container->RemoveFromRadioGroup(name, static_cast<nsIFormControl*>(this));
}

bool
HTMLInputElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable, int32_t* aTabIndex)
{
  if (nsGenericHTMLFormElementWithState::IsHTMLFocusable(aWithMouse, aIsFocusable,
      aTabIndex))
  {
    return true;
  }

  if (IsDisabled()) {
    *aIsFocusable = false;
    return true;
  }

  if (IsSingleLineTextControl(false) ||
      mType == NS_FORM_INPUT_RANGE) {
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

  nsCOMPtr<nsIDOMHTMLInputElement> currentRadio = container->GetCurrentRadioButton(name);
  if (currentRadio) {
    *aTabIndex = -1;
  }
  *aIsFocusable = defaultFocusable;
  return false;
}

nsresult
HTMLInputElement::VisitGroup(nsIRadioVisitor* aVisitor, bool aFlushContent)
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

HTMLInputElement::ValueModeType
HTMLInputElement::GetValueMode() const
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
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
    case NS_FORM_INPUT_COLOR:
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
HTMLInputElement::IsMutable() const
{
  return !IsDisabled() && GetCurrentDoc() &&
         !(DoesReadOnlyApply() &&
           HasAttr(kNameSpaceID_None, nsGkAtoms::readonly));
}

bool
HTMLInputElement::DoesReadOnlyApply() const
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
    case NS_FORM_INPUT_RANGE:
    case NS_FORM_INPUT_COLOR:
      return false;
#ifdef DEBUG
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
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
HTMLInputElement::DoesRequiredApply() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_RANGE:
    case NS_FORM_INPUT_COLOR:
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
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
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
HTMLInputElement::PlaceholderApplies() const
{
  if (mType == NS_FORM_INPUT_DATE ||
      mType == NS_FORM_INPUT_TIME) {
    return false;
  }

  return IsSingleLineTextControl(false);
}

bool
HTMLInputElement::DoesPatternApply() const
{
  // TODO: temporary until bug 635240 and bug 773205 are fixed.
  if (IsExperimentalMobileType(mType)) {
    return false;
  }

  return IsSingleLineTextControl(false);
}

bool
HTMLInputElement::DoesMinMaxApply() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
    case NS_FORM_INPUT_RANGE:
    // TODO:
    // All date/time types.
      return true;
#ifdef DEBUG
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_FILE:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
    case NS_FORM_INPUT_COLOR:
      return false;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesRequiredApply()");
      return false;
#else // DEBUG
    default:
      return false;
#endif // DEBUG
  }
}

Decimal
HTMLInputElement::GetStep() const
{
  MOZ_ASSERT(DoesStepApply(), "GetStep() can only be called if @step applies");

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::step)) {
    return GetDefaultStep() * GetStepScaleFactor();
  }

  nsAutoString stepStr;
  GetAttr(kNameSpaceID_None, nsGkAtoms::step, stepStr);

  if (stepStr.LowerCaseEqualsLiteral("any")) {
    // The element can't suffer from step mismatch if there is no step.
    return kStepAny;
  }

  Decimal step = StringToDecimal(stepStr);
  if (!step.isFinite() || step <= 0) {
    step = GetDefaultStep();
  }

  return step * GetStepScaleFactor();
}

// nsIConstraintValidation

NS_IMETHODIMP
HTMLInputElement::SetCustomValidity(const nsAString& aError)
{
  nsIConstraintValidation::SetCustomValidity(aError);

  UpdateState(true);

  return NS_OK;
}

bool
HTMLInputElement::IsTooLong()
{
  if (!MaxLengthApplies() ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength) ||
      !mValueChanged) {
    return false;
  }

  int32_t maxLength = MaxLength();

  // Maxlength of -1 means parsing error.
  if (maxLength == -1) {
    return false;
  }

  int32_t textLength = -1;
  GetTextLength(&textLength);

  return textLength > maxLength;
}

bool
HTMLInputElement::IsValueMissing() const
{
  // Should use UpdateValueMissingValidityStateForRadio() for type radio.
  MOZ_ASSERT(mType != NS_FORM_INPUT_RADIO);

  if (!HasAttr(kNameSpaceID_None, nsGkAtoms::required) ||
      !DoesRequiredApply()) {
    return false;
  }

  if (!IsMutable()) {
    return false;
  }

  switch (GetValueMode()) {
    case VALUE_MODE_VALUE:
      return IsValueEmpty();
    case VALUE_MODE_FILENAME:
    {
      const nsCOMArray<nsIDOMFile>& files = GetFilesInternal();
      return !files.Count();
    }
    case VALUE_MODE_DEFAULT_ON:
      // This should not be used for type radio.
      // See the MOZ_ASSERT at the beginning of the method.
      return !mChecked;
    case VALUE_MODE_DEFAULT:
    default:
      return false;
  }
}

bool
HTMLInputElement::HasTypeMismatch() const
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

    return !NS_SUCCEEDED(ioService->NewURI(NS_ConvertUTF16toUTF8(value), nullptr,
                                           nullptr, getter_AddRefs(uri)));
  }

  return false;
}

bool
HTMLInputElement::HasPatternMismatch() const
{
  if (!DoesPatternApply() ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::pattern)) {
    return false;
  }

  nsAutoString pattern;
  GetAttr(kNameSpaceID_None, nsGkAtoms::pattern, pattern);

  nsAutoString value;
  NS_ENSURE_SUCCESS(GetValueInternal(value), false);

  if (value.IsEmpty()) {
    return false;
  }

  nsIDocument* doc = OwnerDoc();

  return !nsContentUtils::IsPatternMatching(value, pattern, doc);
}

bool
HTMLInputElement::IsRangeOverflow() const
{
  if (!DoesMinMaxApply()) {
    return false;
  }

  Decimal maximum = GetMaximum();
  if (maximum.isNaN()) {
    return false;
  }

  Decimal value = GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value > maximum;
}

bool
HTMLInputElement::IsRangeUnderflow() const
{
  if (!DoesMinMaxApply()) {
    return false;
  }

  Decimal minimum = GetMinimum();
  if (minimum.isNaN()) {
    return false;
  }

  Decimal value = GetValueAsDecimal();
  if (value.isNaN()) {
    return false;
  }

  return value < minimum;
}

bool
HTMLInputElement::HasStepMismatch() const
{
  if (!DoesStepApply()) {
    return false;
  }

  Decimal value = GetValueAsDecimal();
  if (value.isNaN()) {
    // The element can't suffer from step mismatch if it's value isn't a number.
    return false;
  }

  Decimal step = GetStep();
  if (step == kStepAny) {
    return false;
  }

  // Value has to be an integral multiple of step.
  return NS_floorModulo(value - GetStepBase(), step) != 0;
}

void
HTMLInputElement::UpdateTooLongValidityState()
{
  // TODO: this code will be re-enabled with bug 613016 and bug 613019.
#if 0
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
#endif
}

void
HTMLInputElement::UpdateValueMissingValidityStateForRadio(bool aIgnoreSelf)
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
    SetValidityState(VALIDITY_STATE_VALUE_MISSING,
                     IsMutable() && required && !selected);
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

  valueMissing = IsMutable() && required && !selected;

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
HTMLInputElement::UpdateValueMissingValidityState()
{
  if (mType == NS_FORM_INPUT_RADIO) {
    UpdateValueMissingValidityStateForRadio(false);
    return;
  }

  SetValidityState(VALIDITY_STATE_VALUE_MISSING, IsValueMissing());
}

void
HTMLInputElement::UpdateTypeMismatchValidityState()
{
    SetValidityState(VALIDITY_STATE_TYPE_MISMATCH, HasTypeMismatch());
}

void
HTMLInputElement::UpdatePatternMismatchValidityState()
{
  SetValidityState(VALIDITY_STATE_PATTERN_MISMATCH, HasPatternMismatch());
}

void
HTMLInputElement::UpdateRangeOverflowValidityState()
{
  SetValidityState(VALIDITY_STATE_RANGE_OVERFLOW, IsRangeOverflow());
}

void
HTMLInputElement::UpdateRangeUnderflowValidityState()
{
  SetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW, IsRangeUnderflow());
}

void
HTMLInputElement::UpdateStepMismatchValidityState()
{
  SetValidityState(VALIDITY_STATE_STEP_MISMATCH, HasStepMismatch());
}

void
HTMLInputElement::UpdateAllValidityStates(bool aNotify)
{
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateValueMissingValidityState();
  UpdateTypeMismatchValidityState();
  UpdatePatternMismatchValidityState();
  UpdateRangeOverflowValidityState();
  UpdateRangeUnderflowValidityState();
  UpdateStepMismatchValidityState();

  if (validBefore != IsValid()) {
    UpdateState(aNotify);
  }
}

void
HTMLInputElement::UpdateBarredFromConstraintValidation()
{
  SetBarredFromConstraintValidation(mType == NS_FORM_INPUT_HIDDEN ||
                                    mType == NS_FORM_INPUT_BUTTON ||
                                    mType == NS_FORM_INPUT_RESET ||
                                    mType == NS_FORM_INPUT_SUBMIT ||
                                    mType == NS_FORM_INPUT_IMAGE ||
                                    HasAttr(kNameSpaceID_None, nsGkAtoms::readonly) ||
                                    IsDisabled());
}

void
HTMLInputElement::GetValidationMessage(nsAString& aValidationMessage,
                                       ErrorResult& aRv)
{
  aRv = GetValidationMessage(aValidationMessage);
}

nsresult
HTMLInputElement::GetValidationMessage(nsAString& aValidationMessage,
                                       ValidityStateType aType)
{
  nsresult rv = NS_OK;

  switch (aType)
  {
    case VALIDITY_STATE_TOO_LONG:
    {
      nsXPIDLString message;
      int32_t maxLength = MaxLength();
      int32_t textLength = -1;
      nsAutoString strMaxLength;
      nsAutoString strTextLength;

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
      nsAutoCString key;
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
      nsAutoCString key;
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
    case VALIDITY_STATE_RANGE_OVERFLOW:
    {
      nsXPIDLString message;

      nsAutoString maxStr;
      if (mType == NS_FORM_INPUT_NUMBER ||
          mType == NS_FORM_INPUT_RANGE) {
        //We want to show the value as parsed when it's a number
        Decimal maximum = GetMaximum();
        MOZ_ASSERT(!maximum.isNaN());

        char buf[32];
        DebugOnly<bool> ok = maximum.toString(buf, ArrayLength(buf));
        maxStr.AssignASCII(buf);
        MOZ_ASSERT(ok, "buf not big enough");
      } else if (mType == NS_FORM_INPUT_DATE || mType == NS_FORM_INPUT_TIME) {
        GetAttr(kNameSpaceID_None, nsGkAtoms::max, maxStr);
      } else {
        NS_NOTREACHED("Unexpected input type");
      }

      const PRUnichar* params[] = { maxStr.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "FormValidationRangeOverflow",
                                                 params, message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_RANGE_UNDERFLOW:
    {
      nsXPIDLString message;

      nsAutoString minStr;
      if (mType == NS_FORM_INPUT_NUMBER ||
          mType == NS_FORM_INPUT_RANGE) {
        Decimal minimum = GetMinimum();
        MOZ_ASSERT(!minimum.isNaN());

        char buf[32];
        DebugOnly<bool> ok = minimum.toString(buf, ArrayLength(buf));
        minStr.AssignASCII(buf);
        MOZ_ASSERT(ok, "buf not big enough");
      } else if (mType == NS_FORM_INPUT_DATE || mType == NS_FORM_INPUT_TIME) {
        GetAttr(kNameSpaceID_None, nsGkAtoms::min, minStr);
      } else {
        NS_NOTREACHED("Unexpected input type");
      }

      const PRUnichar* params[] = { minStr.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "FormValidationRangeUnderflow",
                                                 params, message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_STEP_MISMATCH:
    {
      nsXPIDLString message;

      Decimal value = GetValueAsDecimal();
      MOZ_ASSERT(!value.isNaN());

      Decimal step = GetStep();
      MOZ_ASSERT(step != kStepAny && step > 0);

      // In case this is a date and the step is not an integer, we don't want to
      // display the dates corresponding to the truncated timestamps of valueLow
      // and valueHigh because they might suffer from a step mismatch as well.
      // Instead we want the timestamps to correspond to a rounded day. That is,
      // we want a multiple of the step scale factor (1 day) as well as of step.
      if (mType == NS_FORM_INPUT_DATE) {
        step = EuclidLCM<Decimal>(step.floor(),
                                  GetStepScaleFactor().floor());
      }

      Decimal stepBase = GetStepBase();

      Decimal valueLow = value - NS_floorModulo(value - stepBase, step);
      Decimal valueHigh = value + step - NS_floorModulo(value - stepBase, step);

      Decimal maximum = GetMaximum();

      if (maximum.isNaN() || valueHigh <= maximum) {
        nsAutoString valueLowStr, valueHighStr;
        ConvertNumberToString(valueLow, valueLowStr);
        ConvertNumberToString(valueHigh, valueHighStr);

        if (valueLowStr.Equals(valueHighStr)) {
          const PRUnichar* params[] = { valueLowStr.get() };
          rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                     "FormValidationStepMismatchOneValue",
                                                     params, message);
        } else {
          const PRUnichar* params[] = { valueLowStr.get(), valueHighStr.get() };
          rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                     "FormValidationStepMismatch",
                                                     params, message);
        }
      } else {
        nsAutoString valueLowStr;
        ConvertNumberToString(valueLow, valueLowStr);

        const PRUnichar* params[] = { valueLowStr.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationStepMismatchOneValue",
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
HTMLInputElement::IsValidEmailAddressList(const nsAString& aValue)
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
HTMLInputElement::IsValidEmailAddress(const nsAString& aValue)
{
  nsAutoCString value = NS_ConvertUTF16toUTF8(aValue);
  uint32_t i = 0;
  uint32_t length = value.Length();

  // Puny-encode the string if needed before running the validation algorithm.
  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (idnSrv) {
    bool ace;
    if (NS_SUCCEEDED(idnSrv->IsACE(value, &ace)) && !ace) {
      nsAutoCString punyCodedValue;
      if (NS_SUCCEEDED(idnSrv->ConvertUTF8toACE(value, punyCodedValue))) {
        value = punyCodedValue;
        length = value.Length();
      }
    }
  } else {
    NS_ERROR("nsIIDNService isn't present!");
  }

  // If the email address is empty, begins with an '@'
  // or ends with a '.' or '-', we know it's invalid.
  if (length == 0 || value[0] == '@' || value[length-1] == '.' ||
      value[length-1] == '-') {
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

  // If there is no domain name, that's not a valid email address.
  if (++i >= length) {
    return false;
  }

  // The domain name can't begin with a dot or a dash.
  if (value[i] == '.' || value[i] == '-') {
    return false;
  }

  // Parsing the domain name.
  for (; i < length; ++i) {
    PRUnichar c = value[i];

    if (c == '.') {
      // A dot can't follow a dot or a dash.
      if (value[i-1] == '.' || value[i-1] == '-') {
        return false;
      }
    } else if (c == '-'){
      // A dash can't follow a dot.
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
HTMLInputElement::IsSingleLineTextControl() const
{
  return IsSingleLineTextControl(false);
}

NS_IMETHODIMP_(bool)
HTMLInputElement::IsTextArea() const
{
  return false;
}

NS_IMETHODIMP_(bool)
HTMLInputElement::IsPlainTextControl() const
{
  // need to check our HTML attribute and/or CSS.
  return true;
}

NS_IMETHODIMP_(bool)
HTMLInputElement::IsPasswordTextControl() const
{
  return mType == NS_FORM_INPUT_PASSWORD;
}

NS_IMETHODIMP_(int32_t)
HTMLInputElement::GetCols()
{
  // Else we know (assume) it is an input with size attr
  const nsAttrValue* attr = GetParsedAttr(nsGkAtoms::size);
  if (attr && attr->Type() == nsAttrValue::eInteger) {
    int32_t cols = attr->GetIntegerValue();
    if (cols > 0) {
      return cols;
    }
  }

  return DEFAULT_COLS;
}

NS_IMETHODIMP_(int32_t)
HTMLInputElement::GetWrapCols()
{
  return -1; // only textarea's can have wrap cols
}

NS_IMETHODIMP_(int32_t)
HTMLInputElement::GetRows()
{
  return DEFAULT_ROWS;
}

NS_IMETHODIMP_(void)
HTMLInputElement::GetDefaultValueFromContent(nsAString& aValue)
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
HTMLInputElement::ValueChanged() const
{
  return mValueChanged;
}

NS_IMETHODIMP_(void)
HTMLInputElement::GetTextEditorValue(nsAString& aValue,
                                     bool aIgnoreWrap) const
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    state->GetValue(aValue, aIgnoreWrap);
  }
}

NS_IMETHODIMP_(void)
HTMLInputElement::InitializeKeyboardEventListeners()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    state->InitializeKeyboardEventListeners();
  }
}

NS_IMETHODIMP_(void)
HTMLInputElement::OnValueChanged(bool aNotify)
{
  UpdateAllValidityStates(aNotify);

  if (HasDirAuto()) {
    SetDirectionIfAuto(true, aNotify);
  }
}

NS_IMETHODIMP_(bool)
HTMLInputElement::HasCachedSelection()
{
  bool isCached = false;
  nsTextEditorState* state = GetEditorState();
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
HTMLInputElement::FieldSetDisabledChanged(bool aNotify)
{
  UpdateValueMissingValidityState();
  UpdateBarredFromConstraintValidation();

  nsGenericHTMLFormElementWithState::FieldSetDisabledChanged(aNotify);
}

void
HTMLInputElement::SetFilePickerFiltersFromAccept(nsIFilePicker* filePicker)
{
  // We always add |filterAll|
  filePicker->AppendFilters(nsIFilePicker::filterAll);

  NS_ASSERTION(HasAttr(kNameSpaceID_None, nsGkAtoms::accept),
               "You should not call SetFilePickerFiltersFromAccept if the"
               " element has no accept attribute!");

  // Services to retrieve image/*, audio/*, video/* filters
  nsCOMPtr<nsIStringBundleService> stringService =
    mozilla::services::GetStringBundleService();
  if (!stringService) {
    return;
  }
  nsCOMPtr<nsIStringBundle> filterBundle;
  if (NS_FAILED(stringService->CreateBundle("chrome://global/content/filepicker.properties",
                                            getter_AddRefs(filterBundle)))) {
    return;
  }

  // Service to retrieve mime type information for mime types filters
  nsCOMPtr<nsIMIMEService> mimeService = do_GetService("@mozilla.org/mime;1");
  if (!mimeService) {
    return;
  }

  nsAutoString accept;
  GetAttr(kNameSpaceID_None, nsGkAtoms::accept, accept);

  HTMLSplitOnSpacesTokenizer tokenizer(accept, ',');

  nsTArray<nsFilePickerFilter> filters;
  nsString allExtensionsList;

  // Retrieve all filters
  while (tokenizer.hasMoreTokens()) {
    const nsDependentSubstring& token = tokenizer.nextToken();

    if (token.IsEmpty()) {
      continue;
    }

    int32_t filterMask = 0;
    nsString filterName;
    nsString extensionListStr;

    // First, check for image/audio/video filters...
    if (token.EqualsLiteral("image/*")) {
      filterMask = nsIFilePicker::filterImages;
      filterBundle->GetStringFromName(NS_LITERAL_STRING("imageFilter").get(),
                                      getter_Copies(extensionListStr));
    } else if (token.EqualsLiteral("audio/*")) {
      filterMask = nsIFilePicker::filterAudio;
      filterBundle->GetStringFromName(NS_LITERAL_STRING("audioFilter").get(),
                                      getter_Copies(extensionListStr));
    } else if (token.EqualsLiteral("video/*")) {
      filterMask = nsIFilePicker::filterVideo;
      filterBundle->GetStringFromName(NS_LITERAL_STRING("videoFilter").get(),
                                      getter_Copies(extensionListStr));
    } else {
      //... if no image/audio/video filter is found, check mime types filters
      nsCOMPtr<nsIMIMEInfo> mimeInfo;
      if (NS_FAILED(mimeService->GetFromTypeAndExtension(
                      NS_ConvertUTF16toUTF8(token),
                      EmptyCString(), // No extension
                      getter_AddRefs(mimeInfo))) ||
          !mimeInfo) {
        continue;
      }

      // Get mime type name
      nsCString mimeTypeName;
      mimeInfo->GetType(mimeTypeName);
      CopyUTF8toUTF16(mimeTypeName, filterName);

      // Get extension list
      nsCOMPtr<nsIUTF8StringEnumerator> extensions;
      mimeInfo->GetFileExtensions(getter_AddRefs(extensions));

      bool hasMore;
      while (NS_SUCCEEDED(extensions->HasMore(&hasMore)) && hasMore) {
        nsCString extension;
        if (NS_FAILED(extensions->GetNext(extension))) {
          continue;
        }
        if (!extensionListStr.IsEmpty()) {
          extensionListStr.AppendLiteral("; ");
        }
        extensionListStr += NS_LITERAL_STRING("*.") +
                            NS_ConvertUTF8toUTF16(extension);
      }
    }

    if (!filterMask && (extensionListStr.IsEmpty() || filterName.IsEmpty())) {
      // No valid filter found
      continue;
    }

    // If we arrived here, that means we have a valid filter: let's create it
    // and add it to our list, if no similar filter is already present
    nsFilePickerFilter filter;
    if (filterMask) {
      filter = nsFilePickerFilter(filterMask);
    } else {
      filter = nsFilePickerFilter(filterName, extensionListStr);
    }

    if (!filters.Contains(filter)) {
      if (!allExtensionsList.IsEmpty()) {
        allExtensionsList.AppendLiteral("; ");
      }
      allExtensionsList += extensionListStr;
      filters.AppendElement(filter);
    }
  }

  // Add "All Supported Types" filter
  if (filters.Length() > 1) {
    nsXPIDLString title;
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "AllSupportedTypes", title);
    filePicker->AppendFilter(title, allExtensionsList);
  }

  // Add each filter, and check if all filters are trusted
  bool allFilterAreTrusted = true;
  for (uint32_t i = 0; i < filters.Length(); ++i) {
    const nsFilePickerFilter& filter = filters[i];
    if (filter.mFilterMask) {
      filePicker->AppendFilters(filter.mFilterMask);
    } else {
      filePicker->AppendFilter(filter.mTitle, filter.mFilter);
    }
    allFilterAreTrusted &= filter.mIsTrusted;
  }

  // If all filters are trusted, select the first filter as default;
  // otherwise filterAll will remain the default filter
  if (filters.Length() >= 1 && allFilterAreTrusted) {
    // |filterAll| will always use index=0 so we need to set index=1 as the
    // current filter.
    filePicker->SetFilterIndex(1);
  }
}

int32_t
HTMLInputElement::GetFilterFromAccept()
{
  NS_ASSERTION(HasAttr(kNameSpaceID_None, nsGkAtoms::accept),
               "You should not call GetFileFiltersFromAccept if the element"
               " has no accept attribute!");

  int32_t filter = 0;
  nsAutoString accept;
  GetAttr(kNameSpaceID_None, nsGkAtoms::accept, accept);

  HTMLSplitOnSpacesTokenizer tokenizer(accept, ',');

  while (tokenizer.hasMoreTokens()) {
    const nsDependentSubstring token = tokenizer.nextToken();

    int32_t tokenFilter = 0;
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

Decimal
HTMLInputElement::GetStepScaleFactor() const
{
  MOZ_ASSERT(DoesStepApply());

  switch (mType) {
    case NS_FORM_INPUT_DATE:
      return kStepScaleFactorDate;
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
      return kStepScaleFactorNumberRange;
    case NS_FORM_INPUT_TIME:
      return kStepScaleFactorTime;
    default:
      MOZ_ASSERT(false, "Unrecognized input type");
      return Decimal::nan();
  }
}

Decimal
HTMLInputElement::GetDefaultStep() const
{
  MOZ_ASSERT(DoesStepApply());

  switch (mType) {
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
      return kDefaultStep;
    case NS_FORM_INPUT_TIME:
      return kDefaultStepTime;
    default:
      MOZ_ASSERT(false, "Unrecognized input type");
      return Decimal::nan();
  }
}

void
HTMLInputElement::UpdateValidityUIBits(bool aIsFocused)
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

void
HTMLInputElement::UpdateHasRange()
{
  /*
   * There is a range if min/max applies for the type and if the element
   * currently have a valid min or max.
   */

  mHasRange = false;

  if (!DoesMinMaxApply()) {
    return;
  }

  Decimal minimum = GetMinimum();
  if (!minimum.isNaN()) {
    mHasRange = true;
    return;
  }

  Decimal maximum = GetMaximum();
  if (!maximum.isNaN()) {
    mHasRange = true;
    return;
  }
}

JSObject*
HTMLInputElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return HTMLInputElementBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
