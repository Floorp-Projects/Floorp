/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/HTMLInputElement.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/dom/Date.h"
#include "mozilla/dom/Directory.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/FileSystemUtils.h"
#include "mozilla/dom/GetFilesHelper.h"
#include "nsAttrValueInlines.h"
#include "nsCRTGlue.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsITextControlElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIRadioVisitor.h"
#include "nsIPhonetic.h"

#include "HTMLFormSubmissionConstants.h"
#include "mozilla/Telemetry.h"
#include "nsIControllers.h"
#include "nsIStringBundle.h"
#include "nsFocusManager.h"
#include "nsColorControlFrame.h"
#include "nsNumberControlFrame.h"
#include "nsPIDOMWindow.h"
#include "nsRepeatService.h"
#include "nsContentCID.h"
#include "nsIComponentManager.h"
#include "nsIDOMHTMLFormElement.h"
#include "mozilla/dom/ProgressEvent.h"
#include "nsGkAtoms.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsMappedAttributes.h"
#include "nsIFormControl.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIFormControlFrame.h"
#include "nsITextControlFrame.h"
#include "nsIFrame.h"
#include "nsRangeFrame.h"
#include "nsIServiceManager.h"
#include "nsError.h"
#include "nsIEditor.h"
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
#include "nsLayoutUtils.h"
#include "nsVariant.h"

#include "nsIDOMMutationEvent.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventStates.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

#include "nsRuleData.h"
#include <algorithm>

// input type=radio
#include "nsIRadioGroupContainer.h"

// input type=file
#include "mozilla/dom/FileSystemEntry.h"
#include "mozilla/dom/FileSystem.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FileList.h"
#include "nsIFile.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
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
#include "nsTextEditorState.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/Preferences.h"
#include "mozilla/MathAlgorithms.h"

#include "nsIIDNService.h"

#include <limits>

#include "nsIColorPicker.h"
#include "nsIDatePicker.h"
#include "nsIStringEnumerator.h"
#include "HTMLSplitOnSpacesTokenizer.h"
#include "nsIController.h"
#include "nsIMIMEInfo.h"
#include "nsFrameSelection.h"

#include "nsIConsoleService.h"

// input type=date
#include "js/Date.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Input)

// XXX align=left, hspace, vspace, border? other nav4 attrs

static NS_DEFINE_CID(kXULControllersCID,  NS_XULCONTROLLERS_CID);

// This must come outside of any namespace, or else it won't overload with the
// double based version in nsMathUtils.h
inline mozilla::Decimal
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
  { "month", NS_FORM_INPUT_MONTH },
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
  { "week", NS_FORM_INPUT_WEEK },
  { 0 }
};

// Default type is 'text'.
static const nsAttrValue::EnumTable* kInputDefaultType = &kInputTypeTable[17];

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

const Decimal HTMLInputElement::kStepScaleFactorDate = Decimal(86400000);
const Decimal HTMLInputElement::kStepScaleFactorNumberRange = Decimal(1);
const Decimal HTMLInputElement::kStepScaleFactorTime = Decimal(1000);
const Decimal HTMLInputElement::kStepScaleFactorMonth = Decimal(1);
const Decimal HTMLInputElement::kDefaultStepBase = Decimal(0);
const Decimal HTMLInputElement::kDefaultStep = Decimal(1);
const Decimal HTMLInputElement::kDefaultStepTime = Decimal(60);
const Decimal HTMLInputElement::kStepAny = Decimal(0);

const double HTMLInputElement::kMaximumYear = 275760;
const double HTMLInputElement::kMinimumYear = 1;

#define NS_INPUT_ELEMENT_STATE_IID                 \
{ /* dc3b3d14-23e2-4479-b513-7b369343e3a0 */       \
  0xdc3b3d14,                                      \
  0x23e2,                                          \
  0x4479,                                          \
  {0xb5, 0x13, 0x7b, 0x36, 0x93, 0x43, 0xe3, 0xa0} \
}

#define PROGRESS_STR "progress"
static const uint32_t kProgressEventInterval = 50; // ms

// An helper class for the dispatching of the 'change' event.
// This class is used when the FilePicker finished its task (or when files and
// directories are set by some chrome/test only method).
// The task of this class is to postpone the dispatching of 'change' and 'input'
// events at the end of the exploration of the directories.
class DispatchChangeEventCallback final : public GetFilesCallback
{
public:
  explicit DispatchChangeEventCallback(HTMLInputElement* aInputElement)
    : mInputElement(aInputElement)
  {
    MOZ_ASSERT(aInputElement);
  }

  virtual void
  Callback(nsresult aStatus, const Sequence<RefPtr<File>>& aFiles) override
  {
    nsTArray<OwningFileOrDirectory> array;
    for (uint32_t i = 0; i < aFiles.Length(); ++i) {
      OwningFileOrDirectory* element = array.AppendElement();
      element->SetAsFile() = aFiles[i];
    }

    mInputElement->SetFilesOrDirectories(array, true);
    NS_WARN_IF(NS_FAILED(DispatchEvents()));
  }

  nsresult
  DispatchEvents()
  {
    nsresult rv = NS_OK;
    rv = nsContentUtils::DispatchTrustedEvent(mInputElement->OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(mInputElement.get()),
                                              NS_LITERAL_STRING("input"), true,
                                              false);
    NS_WARN_IF(NS_FAILED(rv));

    rv = nsContentUtils::DispatchTrustedEvent(mInputElement->OwnerDoc(),
                                              static_cast<nsIDOMHTMLInputElement*>(mInputElement.get()),
                                              NS_LITERAL_STRING("change"), true,
                                              false);

    return rv;
  }

private:
  RefPtr<HTMLInputElement> mInputElement;
};

class HTMLInputElementState final : public nsISupports
{
  public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_INPUT_ELEMENT_STATE_IID)
    NS_DECL_ISUPPORTS

    bool IsCheckedSet()
    {
      return mCheckedSet;
    }

    bool GetChecked()
    {
      return mChecked;
    }

    void SetChecked(bool aChecked)
    {
      mChecked = aChecked;
      mCheckedSet = true;
    }

    const nsString& GetValue()
    {
      return mValue;
    }

    void SetValue(const nsAString& aValue)
    {
      mValue = aValue;
    }

    void
    GetFilesOrDirectories(nsPIDOMWindowInner* aWindow,
                          nsTArray<OwningFileOrDirectory>& aResult) const
    {
      for (uint32_t i = 0; i < mBlobImplsOrDirectoryPaths.Length(); ++i) {
        if (mBlobImplsOrDirectoryPaths[i].mType == BlobImplOrDirectoryPath::eBlobImpl) {
          RefPtr<File> file =
            File::Create(aWindow,
                         mBlobImplsOrDirectoryPaths[i].mBlobImpl);
          MOZ_ASSERT(file);

          OwningFileOrDirectory* element = aResult.AppendElement();
          element->SetAsFile() = file;
        } else {
          MOZ_ASSERT(mBlobImplsOrDirectoryPaths[i].mType == BlobImplOrDirectoryPath::eDirectoryPath);

          nsCOMPtr<nsIFile> file;
          NS_ConvertUTF16toUTF8 path(mBlobImplsOrDirectoryPaths[i].mDirectoryPath);
          nsresult rv = NS_NewNativeLocalFile(path, true, getter_AddRefs(file));
          if (NS_WARN_IF(NS_FAILED(rv))) {
            continue;
          }

          RefPtr<Directory> directory = Directory::Create(aWindow, file);
          MOZ_ASSERT(directory);

          OwningFileOrDirectory* element = aResult.AppendElement();
          element->SetAsDirectory() = directory;
        }
      }
    }

    void SetFilesOrDirectories(const nsTArray<OwningFileOrDirectory>& aArray)
    {
      mBlobImplsOrDirectoryPaths.Clear();
      for (uint32_t i = 0; i < aArray.Length(); ++i) {
        if (aArray[i].IsFile()) {
          BlobImplOrDirectoryPath* data = mBlobImplsOrDirectoryPaths.AppendElement();

          RefPtr<File> file = aArray[i].GetAsFile();

          nsAutoString name;
          file->GetName(name);

          nsAutoString path;
          path.AssignLiteral(FILESYSTEM_DOM_PATH_SEPARATOR_LITERAL);
          path.Append(name);

          file->SetPath(path);

          data->mBlobImpl = file->Impl();
          data->mType = BlobImplOrDirectoryPath::eBlobImpl;
        } else {
          MOZ_ASSERT(aArray[i].IsDirectory());
          nsAutoString fullPath;
          nsresult rv = aArray[i].GetAsDirectory()->GetFullRealPath(fullPath);
          if (NS_WARN_IF(NS_FAILED(rv))) {
            continue;
          }

          BlobImplOrDirectoryPath* data =
            mBlobImplsOrDirectoryPaths.AppendElement();

          data->mDirectoryPath = fullPath;
          data->mType = BlobImplOrDirectoryPath::eDirectoryPath;
        }
      }
    }

    HTMLInputElementState()
      : mValue()
      , mChecked(false)
      , mCheckedSet(false)
    {}

  protected:
    ~HTMLInputElementState() {}

    nsString mValue;

    struct BlobImplOrDirectoryPath
    {
      RefPtr<BlobImpl> mBlobImpl;
      nsString mDirectoryPath;

      enum {
        eBlobImpl,
        eDirectoryPath
      } mType;
    };

    nsTArray<BlobImplOrDirectoryPath> mBlobImplsOrDirectoryPaths;

    bool mChecked;
    bool mCheckedSet;
};

NS_DEFINE_STATIC_IID_ACCESSOR(HTMLInputElementState, NS_INPUT_ELEMENT_STATE_IID)

NS_IMPL_ISUPPORTS(HTMLInputElementState, HTMLInputElementState)

HTMLInputElement::nsFilePickerShownCallback::nsFilePickerShownCallback(
  HTMLInputElement* aInput, nsIFilePicker* aFilePicker)
  : mFilePicker(aFilePicker)
  , mInput(aInput)
{
}

NS_IMPL_ISUPPORTS(UploadLastDir::ContentPrefCallback, nsIContentPrefCallback2)

NS_IMETHODIMP
UploadLastDir::ContentPrefCallback::HandleCompletion(uint16_t aReason)
{
  nsCOMPtr<nsIFile> localFile;
  nsAutoString prefStr;

  if (aReason == nsIContentPrefCallback2::COMPLETE_ERROR || !mResult) {
    prefStr = Preferences::GetString("dom.input.fallbackUploadDir");
    if (prefStr.IsEmpty()) {
      // If no custom directory was set through the pref, default to
      // "desktop" directory for each platform.
      NS_GetSpecialDirectory(NS_OS_DESKTOP_DIR, getter_AddRefs(localFile));
    }
  }

  if (!localFile) {
    if (prefStr.IsEmpty() && mResult) {
      nsCOMPtr<nsIVariant> pref;
      mResult->GetValue(getter_AddRefs(pref));
      pref->GetAsAString(prefStr);
    }
    localFile = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID);
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

namespace {

/**
 * This may return nullptr if the DOM File's implementation of
 * File::mozFullPathInternal does not successfully return a non-empty
 * string that is a valid path. This can happen on Firefox OS, for example,
 * where the file picker can create Blobs.
 */
static already_AddRefed<nsIFile>
DOMFileOrDirectoryToLocalFile(const OwningFileOrDirectory& aData)
{
  nsString path;

  if (aData.IsFile()) {
    ErrorResult rv;
    aData.GetAsFile()->GetMozFullPathInternal(path, rv);
    if (rv.Failed() || path.IsEmpty()) {
      rv.SuppressException();
      return nullptr;
    }
  } else {
    MOZ_ASSERT(aData.IsDirectory());
    aData.GetAsDirectory()->GetFullRealPath(path);
  }

  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NS_NewNativeLocalFile(NS_ConvertUTF16toUTF8(path), true,
                                      getter_AddRefs(localFile));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return localFile.forget();
}

void
GetDOMFileOrDirectoryName(const OwningFileOrDirectory& aData,
                          nsAString& aName)
{
  if (aData.IsFile()) {
    aData.GetAsFile()->GetName(aName);
  } else {
    MOZ_ASSERT(aData.IsDirectory());
    ErrorResult rv;
    aData.GetAsDirectory()->GetName(aName, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }
  }
}

void
GetDOMFileOrDirectoryPath(const OwningFileOrDirectory& aData,
                          nsAString& aPath,
                          ErrorResult& aRv)
{
  if (aData.IsFile()) {
    aData.GetAsFile()->GetMozFullPathInternal(aPath, aRv);
  } else {
    MOZ_ASSERT(aData.IsDirectory());
    aData.GetAsDirectory()->GetFullRealPath(aPath);
  }
}

} // namespace

/* static */
bool
HTMLInputElement::ValueAsDateEnabled(JSContext* cx, JSObject* obj)
{
  return Preferences::GetBool("dom.experimental_forms", false) ||
    Preferences::GetBool("dom.forms.datepicker", false) ||
    Preferences::GetBool("dom.forms.datetime", false);
}

NS_IMETHODIMP
HTMLInputElement::nsFilePickerShownCallback::Done(int16_t aResult)
{
  mInput->PickerClosed();

  if (aResult == nsIFilePicker::returnCancel) {
    return NS_OK;
  }

  int16_t mode;
  mFilePicker->GetMode(&mode);

  // Collect new selected filenames
  nsTArray<OwningFileOrDirectory> newFilesOrDirectories;
  if (mode == static_cast<int16_t>(nsIFilePicker::modeOpenMultiple)) {
    nsCOMPtr<nsISimpleEnumerator> iter;
    nsresult rv =
      mFilePicker->GetDomFileOrDirectoryEnumerator(getter_AddRefs(iter));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!iter) {
      return NS_OK;
    }

    nsCOMPtr<nsISupports> tmp;
    bool hasMore = true;

    while (NS_SUCCEEDED(iter->HasMoreElements(&hasMore)) && hasMore) {
      iter->GetNext(getter_AddRefs(tmp));
      nsCOMPtr<nsIDOMBlob> domBlob = do_QueryInterface(tmp);
      MOZ_ASSERT(domBlob,
                 "Null file object from FilePicker's file enumerator?");
      if (!domBlob) {
        continue;
      }

      OwningFileOrDirectory* element = newFilesOrDirectories.AppendElement();
      element->SetAsFile() = static_cast<File*>(domBlob.get());
    }
  } else {
    MOZ_ASSERT(mode == static_cast<int16_t>(nsIFilePicker::modeOpen) ||
               mode == static_cast<int16_t>(nsIFilePicker::modeGetFolder));
    nsCOMPtr<nsISupports> tmp;
    nsresult rv = mFilePicker->GetDomFileOrDirectory(getter_AddRefs(tmp));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDOMBlob> blob = do_QueryInterface(tmp);
    if (blob) {
      RefPtr<File> file = static_cast<Blob*>(blob.get())->ToFile();
      MOZ_ASSERT(file);

      OwningFileOrDirectory* element = newFilesOrDirectories.AppendElement();
      element->SetAsFile() = file;
    } else if (tmp) {
      RefPtr<Directory> directory = static_cast<Directory*>(tmp.get());
      OwningFileOrDirectory* element = newFilesOrDirectories.AppendElement();
      element->SetAsDirectory() = directory;
    }
  }

  if (newFilesOrDirectories.IsEmpty()) {
    return NS_OK;
  }

  // Store the last used directory using the content pref service:
  nsCOMPtr<nsIFile> file =
    DOMFileOrDirectoryToLocalFile(newFilesOrDirectories[0]);

  if (file) {
    nsCOMPtr<nsIFile> lastUsedDir;
    file->GetParent(getter_AddRefs(lastUsedDir));
    HTMLInputElement::gUploadLastDir->StoreLastUsedDirectory(
      mInput->OwnerDoc(), lastUsedDir);
  }

  // The text control frame (if there is one) isn't going to send a change
  // event because it will think this is done by a script.
  // So, we can safely send one by ourself.
  mInput->SetFilesOrDirectories(newFilesOrDirectories, true);

  RefPtr<DispatchChangeEventCallback> dispatchChangeEventCallback =
    new DispatchChangeEventCallback(mInput);

  if (Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) &&
      mInput->HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory)) {
    ErrorResult error;
    GetFilesHelper* helper = mInput->GetOrCreateGetFilesHelper(true, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.StealNSResult();
    }

    helper->AddCallback(dispatchChangeEventCallback);
    return NS_OK;
  }

  return dispatchChangeEventCallback->DispatchEvents();
}

NS_IMPL_ISUPPORTS(HTMLInputElement::nsFilePickerShownCallback,
                  nsIFilePickerShownCallback)

class nsColorPickerShownCallback final
  : public nsIColorPickerShownCallback
{
  ~nsColorPickerShownCallback() {}

public:
  nsColorPickerShownCallback(HTMLInputElement* aInput,
                             nsIColorPicker* aColorPicker)
    : mInput(aInput)
    , mColorPicker(aColorPicker)
    , mValueChanged(false)
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Update(const nsAString& aColor) override;
  NS_IMETHOD Done(const nsAString& aColor) override;

private:
  /**
   * Updates the internals of the object using aColor as the new value.
   * If aTrustedUpdate is true, it will consider that aColor is a new value.
   * Otherwise, it will check that aColor is different from the current value.
   */
  nsresult UpdateInternal(const nsAString& aColor, bool aTrustedUpdate);

  RefPtr<HTMLInputElement> mInput;
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

  mInput->PickerClosed();

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

NS_IMPL_ISUPPORTS(nsColorPickerShownCallback, nsIColorPickerShownCallback)

class DatePickerShownCallback final : public nsIDatePickerShownCallback
{
  ~DatePickerShownCallback() {}
public:
  DatePickerShownCallback(HTMLInputElement* aInput,
                          nsIDatePicker* aDatePicker)
    : mInput(aInput)
    , mDatePicker(aDatePicker)
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Done(const nsAString& aDate) override;
  NS_IMETHOD Cancel() override;

private:
  RefPtr<HTMLInputElement> mInput;
  nsCOMPtr<nsIDatePicker> mDatePicker;
};

NS_IMETHODIMP
DatePickerShownCallback::Cancel()
{
  mInput->PickerClosed();
  return NS_OK;
}

NS_IMETHODIMP
DatePickerShownCallback::Done(const nsAString& aDate)
{
  nsAutoString oldValue;

  mInput->PickerClosed();
  mInput->GetValue(oldValue);

  if(!oldValue.Equals(aDate)){
    mInput->SetValue(aDate);
    nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                            static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                            NS_LITERAL_STRING("input"), true,
                            false);
    return nsContentUtils::DispatchTrustedEvent(mInput->OwnerDoc(),
                            static_cast<nsIDOMHTMLInputElement*>(mInput.get()),
                            NS_LITERAL_STRING("change"), true,
                            false);
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(DatePickerShownCallback, nsIDatePickerShownCallback)


bool
HTMLInputElement::IsPopupBlocked() const
{
  nsCOMPtr<nsPIDOMWindowOuter> win = OwnerDoc()->GetWindow();
  MOZ_ASSERT(win, "window should not be null");
  if (!win) {
    return true;
  }

  // Check if page is allowed to open the popup
  if (win->GetPopupControlState() <= openControlled) {
    return false;
  }

  nsCOMPtr<nsIPopupWindowManager> pm = do_GetService(NS_POPUPWINDOWMANAGER_CONTRACTID);
  if (!pm) {
    return true;
  }

  uint32_t permission;
  pm->TestPermission(OwnerDoc()->NodePrincipal(), &permission);
  return permission == nsIPopupWindowManager::DENY_POPUP;
}

nsresult
HTMLInputElement::InitDatePicker()
{
  if (!Preferences::GetBool("dom.forms.datepicker", false)) {
    return NS_OK;
  }

  if (mPickerRunning) {
    NS_WARNING("Just one nsIDatePicker is allowed");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = OwnerDoc();

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  if (IsPopupBlocked()) {
    win->FirePopupBlockedEvent(doc, nullptr, EmptyString(), EmptyString());
    return NS_OK;
  }

  // Get Loc title
  nsXPIDLString title;
  nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                     "DatePicker", title);

  nsresult rv;
  nsCOMPtr<nsIDatePicker> datePicker = do_CreateInstance("@mozilla.org/datepicker;1", &rv);
  if (!datePicker) {
    return rv;
  }

  nsAutoString initialValue;
  GetValueInternal(initialValue);
  rv = datePicker->Init(win, title, initialValue);

  nsCOMPtr<nsIDatePickerShownCallback> callback =
    new DatePickerShownCallback(this, datePicker);

  rv = datePicker->Open(callback);
  if (NS_SUCCEEDED(rv)) {
    mPickerRunning = true;
  }

  return rv;
}

nsresult
HTMLInputElement::InitColorPicker()
{
  if (mPickerRunning) {
    NS_WARNING("Just one nsIColorPicker is allowed");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocument> doc = OwnerDoc();

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  if (IsPopupBlocked()) {
    win->FirePopupBlockedEvent(doc, nullptr, EmptyString(), EmptyString());
    return NS_OK;
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
  GetValueInternal(initialValue);
  nsresult rv = colorPicker->Init(win, title, initialValue);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIColorPickerShownCallback> callback =
    new nsColorPickerShownCallback(this, colorPicker);

  rv = colorPicker->Open(callback);
  if (NS_SUCCEEDED(rv)) {
    mPickerRunning = true;
  }

  return rv;
}

nsresult
HTMLInputElement::InitFilePicker(FilePickerType aType)
{
  if (mPickerRunning) {
    NS_WARNING("Just one nsIFilePicker is allowed");
    return NS_ERROR_FAILURE;
  }

  // Get parent nsPIDOMWindow object.
  nsCOMPtr<nsIDocument> doc = OwnerDoc();

  nsCOMPtr<nsPIDOMWindowOuter> win = doc->GetWindow();
  if (!win) {
    return NS_ERROR_FAILURE;
  }

  if (IsPopupBlocked()) {
    win->FirePopupBlockedEvent(doc, nullptr, EmptyString(), EmptyString());
    return NS_OK;
  }

  // Get Loc title
  nsXPIDLString title;
  nsXPIDLString okButtonLabel;
  if (aType == FILE_PICKER_DIRECTORY) {
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "DirectoryUpload", title);

    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "DirectoryPickerOkButtonLabel",
                                       okButtonLabel);
  } else {
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "FileUpload", title);
  }

  nsCOMPtr<nsIFilePicker> filePicker = do_CreateInstance("@mozilla.org/filepicker;1");
  if (!filePicker)
    return NS_ERROR_FAILURE;

  int16_t mode;

  if (aType == FILE_PICKER_DIRECTORY) {
    mode = static_cast<int16_t>(nsIFilePicker::modeGetFolder);
  } else if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
    mode = static_cast<int16_t>(nsIFilePicker::modeOpenMultiple);
  } else {
    mode = static_cast<int16_t>(nsIFilePicker::modeOpen);
  }

  nsresult rv = filePicker->Init(win, title, mode);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!okButtonLabel.IsEmpty()) {
    filePicker->SetOkButtonLabel(okButtonLabel);
  }

  // Native directory pickers ignore file type filters, so we don't spend
  // cycles adding them for FILE_PICKER_DIRECTORY.
  if (HasAttr(kNameSpaceID_None, nsGkAtoms::accept) &&
      aType != FILE_PICKER_DIRECTORY) {
    SetFilePickerFiltersFromAccept(filePicker);
  } else {
    filePicker->AppendFilters(nsIFilePicker::filterAll);
  }

  // Set default directry and filename
  nsAutoString defaultName;

  const nsTArray<OwningFileOrDirectory>& oldFiles =
    GetFilesOrDirectoriesInternal();

  nsCOMPtr<nsIFilePickerShownCallback> callback =
    new HTMLInputElement::nsFilePickerShownCallback(this, filePicker);

  if (!oldFiles.IsEmpty() &&
      aType != FILE_PICKER_DIRECTORY) {
    nsString path;

    nsCOMPtr<nsIFile> localFile = DOMFileOrDirectoryToLocalFile(oldFiles[0]);
    if (localFile) {
      nsCOMPtr<nsIFile> parentFile;
      nsresult rv = localFile->GetParent(getter_AddRefs(parentFile));
      if (NS_SUCCEEDED(rv)) {
        filePicker->SetDisplayDirectory(parentFile);
      }
    }

    // Unfortunately nsIFilePicker doesn't allow multiple files to be
    // default-selected, so only select something by default if exactly
    // one file was selected before.
    if (oldFiles.Length() == 1) {
      nsAutoString leafName;
      GetDOMFileOrDirectoryName(oldFiles[0], leafName);

      if (!leafName.IsEmpty()) {
        filePicker->SetDefaultString(leafName);
      }
    }

    rv = filePicker->Open(callback);
    if (NS_SUCCEEDED(rv)) {
      mPickerRunning = true;
    }

    return rv;
  }

  HTMLInputElement::gUploadLastDir->FetchDirectoryAndDisplayPicker(doc, filePicker, callback);
  mPickerRunning = true;
  return NS_OK;
}

#define CPS_PREF_NAME NS_LITERAL_STRING("browser.upload.lastDir")

NS_IMPL_ISUPPORTS(UploadLastDir, nsIObserver, nsISupportsWeakReference)

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

  nsCOMPtr<nsILoadContext> loadContext = aDoc->GetLoadContext();
  nsCOMPtr<nsIContentPrefCallback2> prefCallback =
    new UploadLastDir::ContentPrefCallback(aFilePicker, aFpCallback);

#ifdef MOZ_B2G
  if (XRE_IsContentProcess()) {
    prefCallback->HandleCompletion(nsIContentPrefCallback2::COMPLETE_ERROR);
    return NS_OK;
  }
#endif

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
UploadLastDir::StoreLastUsedDirectory(nsIDocument* aDoc, nsIFile* aDir)
{
  NS_PRECONDITION(aDoc, "aDoc is null");
  if (!aDir) {
    return NS_OK;
  }

#ifdef MOZ_B2G
  if (XRE_IsContentProcess()) {
    return NS_OK;
  }
#endif

  nsCOMPtr<nsIURI> docURI = aDoc->GetDocumentURI();
  NS_PRECONDITION(docURI, "docURI is null");

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
  aDir->GetPath(unicodePath);
  if (unicodePath.IsEmpty()) // nothing to do
    return NS_OK;
  RefPtr<nsVariantCC> prefValue = new nsVariantCC();
  prefValue->SetAsAString(unicodePath);

  // Use the document's current load context to ensure that the content pref
  // service doesn't persistently store this directory for this domain if the
  // user is using private browsing:
  nsCOMPtr<nsILoadContext> loadContext = aDoc->GetLoadContext();
  return contentPrefService->Set(spec, CPS_PREF_NAME, prefValue, loadContext, nullptr);
}

NS_IMETHODIMP
UploadLastDir::Observe(nsISupports* aSubject, char const* aTopic, char16_t const* aData)
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

HTMLInputElement::HTMLInputElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                   FromParser aFromParser)
  : nsGenericHTMLFormElementWithState(aNodeInfo)
  , mType(kInputDefaultType->value)
  , mAutocompleteAttrState(nsContentUtils::eAutocompleteAttrState_Unknown)
  , mDisabledChanged(false)
  , mValueChanged(false)
  , mLastValueChangeWasInteractive(false)
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
  , mNumberControlSpinnerIsSpinning(false)
  , mNumberControlSpinnerSpinsUp(false)
  , mPickerRunning(false)
  , mSelectionCached(true)
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
  UpdateApzAwareFlag();
}

HTMLInputElement::~HTMLInputElement()
{
  if (mNumberControlSpinnerIsSpinning) {
    StopNumberControlSpinnerSpin(eDisallowDispatchingEvents);
  }
  DestroyImageLoadingContent();
  FreeData();
}

void
HTMLInputElement::FreeData()
{
  if (!IsSingleLineTextControl(false)) {
    free(mInputData.mValue);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLInputElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLInputElement,
                                                  nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mControllers)
  if (tmp->IsSingleLineTextControl(false)) {
    tmp->mInputData.mState->Traverse(cb);
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFilesOrDirectories)

  if (tmp->mGetFilesRecursiveHelper) {
    tmp->mGetFilesRecursiveHelper->Traverse(cb);
  }

  if (tmp->mGetFilesNonRecursiveHelper) {
    tmp->mGetFilesNonRecursiveHelper->Traverse(cb);
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFileList)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEntries)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLInputElement,
                                                nsGenericHTMLFormElementWithState)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mControllers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFilesOrDirectories)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFileList)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mEntries)
  if (tmp->IsSingleLineTextControl(false)) {
    tmp->mInputData.mState->Unlink();
  }

  tmp->ClearGetFilesHelpers();

  //XXX should unlink more?
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLInputElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLInputElement, Element)

// QueryInterface implementation for HTMLInputElement
NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLInputElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLInputElement,
                               nsIDOMHTMLInputElement,
                               nsITextControlElement,
                               nsIPhonetic,
                               imgINotificationObserver,
                               nsIImageLoadingContent,
                               imgIOnloadBlocker,
                               nsIDOMNSEditableElement,
                               nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElementWithState)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION_EXCEPT_SETCUSTOMVALIDITY(HTMLInputElement)

// nsIDOMNode

nsresult
HTMLInputElement::Clone(mozilla::dom::NodeInfo* aNodeInfo, nsINode** aResult) const
{
  *aResult = nullptr;

  already_AddRefed<mozilla::dom::NodeInfo> ni = RefPtr<mozilla::dom::NodeInfo>(aNodeInfo).forget();
  RefPtr<HTMLInputElement> it = new HTMLInputElement(ni, NOT_FROM_PARSER);

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
        rv = it->SetValueInternal(value, nsTextEditorState::eSetValue_Notify);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      break;
    case VALUE_MODE_FILENAME:
      if (it->OwnerDoc()->IsStaticDocument()) {
        // We're going to be used in print preview.  Since the doc is static
        // we can just grab the pretty string and use it as wallpaper
        GetDisplayFileName(it->mStaticDocFileList);
      } else {
        it->ClearGetFilesHelpers();
        it->mFilesOrDirectories.Clear();
        it->mFilesOrDirectories.AppendElements(mFilesOrDirectories);
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

  it->mLastValueChangeWasInteractive = mLastValueChangeWasInteractive;
  it.forget(aResult);
  return NS_OK;
}

nsresult
HTMLInputElement::BeforeSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                nsAttrValueOrString* aValue,
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
        LoadImage(aValue->String(), true, aNotify, eImageLoadType_Normal);
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
    } else if (mType == NS_FORM_INPUT_RADIO && aName == nsGkAtoms::required) {
      nsCOMPtr<nsIRadioGroupContainer> container = GetRadioGroupContainer();

      if (container &&
          ((aValue && !HasAttr(aNameSpaceID, aName)) ||
           (!aValue && HasAttr(aNameSpaceID, aName)))) {
        nsAutoString name;
        GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
        container->RadioRequiredWillChange(name, !!aValue);
      }
    }

    if (aName == nsGkAtoms::webkitdirectory) {
      Telemetry::Accumulate(Telemetry::WEBKIT_DIRECTORY_USED, true);
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
          LoadImage(src, false, aNotify, eImageLoadType_Normal);
        }
      }

      if (mType == NS_FORM_INPUT_PASSWORD && IsInComposedDoc()) {
        AsyncEventDispatcher* dispatcher =
          new AsyncEventDispatcher(this,
                                   NS_LITERAL_STRING("DOMInputPasswordAdded"),
                                   true,
                                   true);
        dispatcher->PostDOMEvent();
      }
    }

    if (aName == nsGkAtoms::required || aName == nsGkAtoms::disabled ||
        aName == nsGkAtoms::readonly) {
      UpdateValueMissingValidityState();

      // This *has* to be called *after* validity has changed.
      if (aName == nsGkAtoms::readonly || aName == nsGkAtoms::disabled) {
        UpdateBarredFromConstraintValidation();
      }
    } else if (MinOrMaxLengthApplies() && aName == nsGkAtoms::maxlength) {
      UpdateTooLongValidityState();
    } else if (MinOrMaxLengthApplies() && aName == nsGkAtoms::minlength) {
      UpdateTooShortValidityState();
    } else if (aName == nsGkAtoms::pattern && !mParserCreating) {
      UpdatePatternMismatchValidityState();
    } else if (aName == nsGkAtoms::multiple) {
      UpdateTypeMismatchValidityState();
    } else if (aName == nsGkAtoms::max) {
      UpdateHasRange();
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
        nsresult rv =
          SetValueInternal(value, nsTextEditorState::eSetValue_Internal);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // Validity state must be updated *after* the SetValueInternal call above
      // or else the following assert will not be valid.
      // We don't assert the state of underflow during parsing since
      // DoneCreatingElement sanitizes.
      UpdateRangeOverflowValidityState();
      MOZ_ASSERT(mParserCreating ||
                 mType != NS_FORM_INPUT_RANGE ||
                 !GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                 "HTML5 spec does not allow underflow for type=range");
    } else if (aName == nsGkAtoms::min) {
      UpdateHasRange();
      if (mType == NS_FORM_INPUT_RANGE) {
        // See @max comment
        nsAutoString value;
        GetValue(value);
        nsresult rv =
          SetValueInternal(value, nsTextEditorState::eSetValue_Internal);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // See corresponding @max comment
      UpdateRangeUnderflowValidityState();
      UpdateStepMismatchValidityState();
      MOZ_ASSERT(mParserCreating ||
                 mType != NS_FORM_INPUT_RANGE ||
                 !GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                 "HTML5 spec does not allow underflow for type=range");
    } else if (aName == nsGkAtoms::step) {
      if (mType == NS_FORM_INPUT_RANGE) {
        // See @max comment
        nsAutoString value;
        GetValue(value);
        nsresult rv =
          SetValueInternal(value, nsTextEditorState::eSetValue_Internal);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      // See corresponding @max comment
      UpdateStepMismatchValidityState();
      MOZ_ASSERT(mParserCreating ||
                 mType != NS_FORM_INPUT_RANGE ||
                 !GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW),
                 "HTML5 spec does not allow underflow for type=range");
    } else if (aName == nsGkAtoms::dir &&
               aValue && aValue->Equals(nsGkAtoms::_auto, eIgnoreCase)) {
      SetDirectionIfAuto(true, aNotify);
    } else if (aName == nsGkAtoms::lang) {
      if (mType == NS_FORM_INPUT_NUMBER) {
        // Update the value that is displayed to the user to the new locale:
        nsAutoString value;
        GetValueInternal(value);
        nsNumberControlFrame* numberControlFrame =
          do_QueryFrame(GetPrimaryFrame());
        if (numberControlFrame) {
          numberControlFrame->SetValueOfAnonTextControl(value);
        }
      }
    } else if (aName == nsGkAtoms::autocomplete) {
      // Clear the cached @autocomplete attribute state.
      mAutocompleteAttrState = nsContentUtils::eAutocompleteAttrState_Unknown;
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
NS_IMPL_NON_NEGATIVE_INT_ATTR(HTMLInputElement, MinLength, minlength)
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

NS_IMETHODIMP
HTMLInputElement::GetAutocomplete(nsAString& aValue)
{
  if (!DoesAutocompleteApply()) {
    return NS_OK;
  }

  aValue.Truncate(0);
  const nsAttrValue* attributeVal = GetParsedAttr(nsGkAtoms::autocomplete);

  mAutocompleteAttrState =
    nsContentUtils::SerializeAutocompleteAttribute(attributeVal, aValue,
                                                   mAutocompleteAttrState);
  return NS_OK;
}

NS_IMETHODIMP
HTMLInputElement::SetAutocomplete(const nsAString& aValue)
{
  return SetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete, nullptr, aValue, true);
}

void
HTMLInputElement::GetAutocompleteInfo(Nullable<AutocompleteInfo>& aInfo)
{
  if (!DoesAutocompleteApply()) {
    aInfo.SetNull();
    return;
  }

  const nsAttrValue* attributeVal = GetParsedAttr(nsGkAtoms::autocomplete);
  mAutocompleteAttrState =
    nsContentUtils::SerializeAutocompleteAttribute(attributeVal, aInfo.SetValue(),
                                                   mAutocompleteAttrState);
}

int32_t
HTMLInputElement::TabIndexDefault()
{
  return 0;
}

uint32_t
HTMLInputElement::Height()
{
  if (mType != NS_FORM_INPUT_IMAGE) {
    return 0;
  }
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
  return rv.StealNSResult();
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
  if (mType != NS_FORM_INPUT_IMAGE) {
    return 0;
  }
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
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLInputElement::GetValue(nsAString& aValue)
{
  nsresult rv = GetValueInternal(aValue);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Don't return non-sanitized value for types that are experimental on mobile
  // or datetime types
  if (IsExperimentalMobileType(mType) || IsDateTimeInputType(mType)) {
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
      } else if (!aValue.Assign(mInputData.mValue, fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      return NS_OK;

    case VALUE_MODE_FILENAME:
      if (nsContentUtils::LegacyIsCallerChromeOrNativeCode()) {
        aValue.Assign(mFirstFilePath);
      } else {
        // Just return the leaf name
        if (mFilesOrDirectories.IsEmpty()) {
          aValue.Truncate();
        } else {
          GetDOMFileOrDirectoryName(mFilesOrDirectories[0], aValue);
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

void
HTMLInputElement::ClearFiles(bool aSetValueChanged)
{
  nsTArray<OwningFileOrDirectory> data;
  SetFilesOrDirectories(data, aSetValueChanged);
}

int32_t
HTMLInputElement::MonthsSinceJan1970(uint32_t aYear, uint32_t aMonth) const
{
  return (aYear - 1970) * 12 + aMonth - 1;
}

/* static */ Decimal
HTMLInputElement::StringToDecimal(const nsAString& aValue)
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
        if (!ParseDate(aValue, &year, &month, &day)) {
          return false;
        }

        JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, day));
        if (!time.isValid()) {
          return false;
        }

        aResultValue = Decimal::fromDouble(time.toDouble());
        return true;
      }
    case NS_FORM_INPUT_TIME:
      uint32_t milliseconds;
      if (!ParseTime(aValue, &milliseconds)) {
        return false;
      }

      aResultValue = Decimal(int32_t(milliseconds));
      return true;
    case NS_FORM_INPUT_MONTH:
      {
        uint32_t year, month;
        if (!ParseMonth(aValue, &year, &month)) {
          return false;
        }

        // Maximum valid month is 275760-09.
        if (year < kMinimumYear || year > kMaximumYear) {
          return false;
        }

        if (year == kMaximumYear && month > 9) {
          return false;
        }

        int32_t months = MonthsSinceJan1970(year, month);
        aResultValue = Decimal(int32_t(months));
        return true;
      }
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
HTMLInputElement::GetValue(nsAString& aValue, ErrorResult& aRv)
{
  nsresult rv = GetValue(aValue);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
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
      if (!list.AppendElement(aValue, fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }

      MozSetFileNameArray(list, aRv);
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
      GetValue(currentValue);

      nsresult rv =
        SetValueInternal(aValue, nsTextEditorState::eSetValue_ByContent |
                                 nsTextEditorState::eSetValue_Notify);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
      }

      if (mFocusedValue.Equals(currentValue)) {
        GetValue(mFocusedValue);
      }
    } else {
      nsresult rv =
        SetValueInternal(aValue, nsTextEditorState::eSetValue_ByContent |
                                 nsTextEditorState::eSetValue_Notify);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
      }
    }
  }
}

NS_IMETHODIMP
HTMLInputElement::SetValue(const nsAString& aValue)
{
  ErrorResult rv;
  SetValue(aValue, rv);
  return rv.StealNSResult();
}

nsGenericHTMLElement*
HTMLInputElement::GetList() const
{
  nsAutoString dataListId;
  GetAttr(kNameSpaceID_None, nsGkAtoms::list, dataListId);
  if (dataListId.IsEmpty()) {
    return nullptr;
  }

  //XXXsmaug How should this all work in case input element is in Shadow DOM.
  nsIDocument* doc = GetUncomposedDoc();
  if (!doc) {
    return nullptr;
  }

  Element* element = doc->GetElementById(dataListId);
  if (!element || !element->IsHTMLElement(nsGkAtoms::datalist)) {
    return nullptr;
  }

  return static_cast<nsGenericHTMLElement*>(element);
}

NS_IMETHODIMP
HTMLInputElement::GetList(nsIDOMHTMLElement** aValue)
{
  *aValue = nullptr;

  RefPtr<nsGenericHTMLElement> element = GetList();
  if (!element) {
    return NS_OK;
  }

  element.forget(aValue);
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
        uint32_t value = NS_floorModulo(aValue.floor(), Decimal(86400000)).toDouble();

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
    case NS_FORM_INPUT_MONTH:
      {
        aValue = aValue.floor();

        double month = NS_floorModulo(aValue, Decimal(12)).toDouble();
        month = (month < 0 ? month + 12 : month);

        double year = 1970 + (aValue.toDouble() - month) / 12;

        // Maximum valid month is 275760-09.
        if (year < kMinimumYear || year > kMaximumYear) {
          return false;
        }

        if (year == kMaximumYear && month > 8) {
          return false;
        }

        aResultString.AppendPrintf("%04.0f-%02.0f", year, month + 1);
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
  // TODO: this is temporary until bug 888316 is fixed.
  if (!IsDateTimeInputType(mType) || mType == NS_FORM_INPUT_WEEK) {
    return Nullable<Date>();
  }

  switch (mType) {
    case NS_FORM_INPUT_DATE:
    {
      uint32_t year, month, day;
      nsAutoString value;
      GetValueInternal(value);
      if (!ParseDate(value, &year, &month, &day)) {
        return Nullable<Date>();
      }

      JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, day));
      return Nullable<Date>(Date(time));
    }
    case NS_FORM_INPUT_TIME:
    {
      uint32_t millisecond;
      nsAutoString value;
      GetValueInternal(value);
      if (!ParseTime(value, &millisecond)) {
        return Nullable<Date>();
      }

      JS::ClippedTime time = JS::TimeClip(millisecond);
      MOZ_ASSERT(time.toDouble() == millisecond,
                 "HTML times are restricted to the day after the epoch and "
                 "never clip");
      return Nullable<Date>(Date(time));
    }
    case NS_FORM_INPUT_MONTH:
    {
      uint32_t year, month;
      nsAutoString value;
      GetValueInternal(value);
      if (!ParseMonth(value, &year, &month)) {
        return Nullable<Date>();
      }

      JS::ClippedTime time = JS::TimeClip(JS::MakeDate(year, month - 1, 1));
      return Nullable<Date>(Date(time));
    }
  }

  MOZ_ASSERT(false, "Unrecognized input type");
  aRv.Throw(NS_ERROR_UNEXPECTED);
  return Nullable<Date>();
}

void
HTMLInputElement::SetValueAsDate(Nullable<Date> aDate, ErrorResult& aRv)
{
  // TODO: this is temporary until bug 888316 is fixed.
  if (!IsDateTimeInputType(mType) || mType == NS_FORM_INPUT_WEEK) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aDate.IsNull() || aDate.Value().IsUndefined()) {
    aRv = SetValue(EmptyString());
    return;
  }

  double milliseconds = aDate.Value().TimeStamp().toDouble();

  if (mType != NS_FORM_INPUT_MONTH) {
    SetValue(Decimal::fromDouble(milliseconds));
    return;
  }

  // type=month expects the value to be number of months.
  double year = JS::YearFromTime(milliseconds);
  double month = JS::MonthFromTime(milliseconds);

  if (IsNaN(year) || IsNaN(month)) {
    SetValue(EmptyString());
    return;
  }

  int32_t months = MonthsSinceJan1970(year, month + 1);
  SetValue(Decimal(int32_t(months)));
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
  return rv.StealNSResult();
}

Decimal
HTMLInputElement::GetMinimum() const
{
  MOZ_ASSERT(DoesValueAsNumberApply(),
             "GetMinimum() should only be used for types that allow .valueAsNumber");

  // Only type=range has a default minimum
  Decimal defaultMinimum =
    mType == NS_FORM_INPUT_RANGE ? Decimal(0) : Decimal::nan();

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
    mType == NS_FORM_INPUT_RANGE ? Decimal(100) : Decimal::nan();

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
             mType == NS_FORM_INPUT_MONTH ||
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
HTMLInputElement::GetValueIfStepped(int32_t aStep,
                                    StepCallerType aCallerType,
                                    Decimal* aNextStep)
{
  if (!DoStepDownStepUpApply()) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  Decimal stepBase = GetStepBase();
  Decimal step = GetStep();
  if (step == kStepAny) {
    if (aCallerType != CALLED_FOR_USER_EVENT) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    // Allow the spin buttons and up/down arrow keys to do something sensible:
    step = GetDefaultStep();
  }

  Decimal minimum = GetMinimum();
  Decimal maximum = GetMaximum();

  if (!maximum.isNaN()) {
    // "max - (max - stepBase) % step" is the nearest valid value to max.
    maximum = maximum - NS_floorModulo(maximum - stepBase, step);
    if (!minimum.isNaN()) {
      if (minimum > maximum) {
        // Either the minimum was greater than the maximum prior to our
        // adjustment to align maximum on a step, or else (if we adjusted
        // maximum) there is no valid step between minimum and the unadjusted
        // maximum.
        return NS_OK;
      }
    }
  }

  Decimal value = GetValueAsDecimal();
  bool valueWasNaN = false;
  if (value.isNaN()) {
    value = Decimal(0);
    valueWasNaN = true;
  }
  Decimal valueBeforeStepping = value;

  Decimal deltaFromStep = NS_floorModulo(value - stepBase, step);

  if (deltaFromStep != Decimal(0)) {
    if (aStep > 0) {
      value += step - deltaFromStep;      // partial step
      value += step * Decimal(aStep - 1); // then remaining steps
    } else if (aStep < 0) {
      value -= deltaFromStep;             // partial step
      value += step * Decimal(aStep + 1); // then remaining steps
    }
  } else {
    value += step * Decimal(aStep);
  }

  if (value < minimum) {
    value = minimum;
    deltaFromStep = NS_floorModulo(value - stepBase, step);
    if (deltaFromStep != Decimal(0)) {
      value += step - deltaFromStep;
    }
  }
  if (value > maximum) {
    value = maximum;
    deltaFromStep = NS_floorModulo(value - stepBase, step);
    if (deltaFromStep != Decimal(0)) {
      value -= deltaFromStep;
    }
  }

  if (!valueWasNaN && // value="", resulting in us using "0"
      ((aStep > 0 && value < valueBeforeStepping) ||
       (aStep < 0 && value > valueBeforeStepping))) {
    // We don't want step-up to effectively step down, or step-down to
    // effectively step up, so return;
    return NS_OK;
  }

  *aNextStep = value;
  return NS_OK;
}

nsresult
HTMLInputElement::ApplyStep(int32_t aStep)
{
  Decimal nextStep = Decimal::nan(); // unchanged if value will not change

  nsresult rv = GetValueIfStepped(aStep, CALLED_FOR_SCRIPT, &nextStep);

  if (NS_SUCCEEDED(rv) && nextStep.isFinite()) {
    SetValue(nextStep);
  }

  return rv;
}

/* static */
bool
HTMLInputElement::IsExperimentalMobileType(uint8_t aType)
{
  return (aType == NS_FORM_INPUT_DATE &&
    !Preferences::GetBool("dom.forms.datetime", false) &&
    !Preferences::GetBool("dom.forms.datepicker", false)) ||
    (aType == NS_FORM_INPUT_TIME &&
     !Preferences::GetBool("dom.forms.datetime", false));
}

bool
HTMLInputElement::IsDateTimeInputType(uint8_t aType)
{
  return aType == NS_FORM_INPUT_DATE ||
         aType == NS_FORM_INPUT_TIME ||
         aType == NS_FORM_INPUT_MONTH ||
         aType == NS_FORM_INPUT_WEEK;
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
HTMLInputElement::FlushFrames()
{
  if (GetComposedDoc()) {
    GetComposedDoc()->FlushPendingNotifications(Flush_Frames);
  }
}

void
HTMLInputElement::MozGetFileNameArray(nsTArray<nsString>& aArray,
                                      ErrorResult& aRv)
{
  for (uint32_t i = 0; i < mFilesOrDirectories.Length(); i++) {
    nsAutoString str;
    GetDOMFileOrDirectoryPath(mFilesOrDirectories[i], str, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }

    aArray.AppendElement(str);
  }
}


NS_IMETHODIMP
HTMLInputElement::MozGetFileNameArray(uint32_t* aLength, char16_t*** aFileNames)
{
  if (!nsContentUtils::IsCallerChrome()) {
    // Since this function returns full paths it's important that normal pages
    // can't call it.
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  ErrorResult rv;
  nsTArray<nsString> array;
  MozGetFileNameArray(array, rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  *aLength = array.Length();
  char16_t** ret =
    static_cast<char16_t**>(moz_xmalloc(*aLength * sizeof(char16_t*)));
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
HTMLInputElement::MozSetFileArray(const Sequence<OwningNonNull<File>>& aFiles)
{
  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  if (!global) {
    return;
  }

  nsTArray<OwningFileOrDirectory> files;
  for (uint32_t i = 0; i < aFiles.Length(); ++i) {
    RefPtr<File> file = File::Create(global, aFiles[i].get()->Impl());
    MOZ_ASSERT(file);

    OwningFileOrDirectory* element = files.AppendElement();
    element->SetAsFile() = file;
  }

  SetFilesOrDirectories(files, true);
}

void
HTMLInputElement::MozSetFileNameArray(const Sequence<nsString>& aFileNames,
                                      ErrorResult& aRv)
{
  if (XRE_IsContentProcess()) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }

  nsTArray<OwningFileOrDirectory> files;
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

    if (!file) {
      continue; // Not much we can do if the file doesn't exist
    }

    nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
    if (!global) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    RefPtr<File> domFile = File::CreateFromFile(global, file);

    OwningFileOrDirectory* element = files.AppendElement();
    element->SetAsFile() = domFile;
  }

  SetFilesOrDirectories(files, true);
}

NS_IMETHODIMP
HTMLInputElement::MozSetFileNameArray(const char16_t** aFileNames,
                                      uint32_t aLength)
{
  if (!nsContentUtils::IsCallerChrome()) {
    // setting the value of a "FILE" input widget requires chrome privilege
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  Sequence<nsString> list;
  nsString* names = list.AppendElements(aLength, fallible);
  if (!names) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  for (uint32_t i = 0; i < aLength; ++i) {
    const char16_t* filename = aFileNames[i];
    names[i].Rebind(filename, nsCharTraits<char16_t>::length(filename));
  }

  ErrorResult rv;
  MozSetFileNameArray(list, rv);
  return rv.StealNSResult();
}

void
HTMLInputElement::MozSetDirectory(const nsAString& aDirectoryPath,
                                  ErrorResult& aRv)
{
  nsCOMPtr<nsIFile> file;
  NS_ConvertUTF16toUTF8 path(aDirectoryPath);
  aRv = NS_NewNativeLocalFile(path, true, getter_AddRefs(file));
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  nsPIDOMWindowInner* window = OwnerDoc()->GetInnerWindow();
  if (NS_WARN_IF(!window)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<Directory> directory = Directory::Create(window, file);
  MOZ_ASSERT(directory);

  nsTArray<OwningFileOrDirectory> array;
  OwningFileOrDirectory* element = array.AppendElement();
  element->SetAsDirectory() = directory;

  SetFilesOrDirectories(array, true);
}

bool
HTMLInputElement::MozIsTextField(bool aExcludePassword)
{
  // TODO: temporary until bug 888320 is fixed.
  if (IsExperimentalMobileType(mType) || IsDateTimeInputType(mType)) {
    return false;
  }

  return IsSingleLineTextControl(aExcludePassword);
}

HTMLInputElement*
HTMLInputElement::GetOwnerNumberControl()
{
  if (IsInNativeAnonymousSubtree() &&
      mType == NS_FORM_INPUT_TEXT &&
      GetParent() && GetParent()->GetParent()) {
    HTMLInputElement* grandparent =
      HTMLInputElement::FromContentOrNull(GetParent()->GetParent());
    if (grandparent && grandparent->mType == NS_FORM_INPUT_NUMBER) {
      return grandparent;
    }
  }
  return nullptr;
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
  if (mType == NS_FORM_INPUT_FILE)
  {
    Sequence<nsString> list;
    if (!list.AppendElement(aValue, fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    ErrorResult rv;
    MozSetFileNameArray(list, rv);
    return rv.StealNSResult();
  } else {
    nsresult rv =
      SetValueInternal(aValue, nsTextEditorState::eSetValue_BySetUserInput |
                               nsTextEditorState::eSetValue_Notify);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                       static_cast<nsIDOMHTMLInputElement*>(this),
                                       NS_LITERAL_STRING("input"), true,
                                       true);

  // If this element is not currently focused, it won't receive a change event for this
  // update through the normal channels. So fire a change event immediately, instead.
  if (!ShouldBlur(this)) {
    FireChangeEventIfNeeded();
  }

  return NS_OK;
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

NS_IMETHODIMP_(Element*)
HTMLInputElement::CreatePlaceholderNode()
{
  nsTextEditorState* state = GetEditorState();
  if (state) {
    NS_ENSURE_SUCCESS(state->CreatePlaceholderNode(), nullptr);
    return state->GetPlaceholderNode();
  }
  return nullptr;
}

NS_IMETHODIMP_(Element*)
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

  if (mFilesOrDirectories.Length() == 1) {
    GetDOMFileOrDirectoryName(mFilesOrDirectories[0], aValue);
    return;
  }

  nsXPIDLString value;

  if (mFilesOrDirectories.IsEmpty()) {
    if ((Preferences::GetBool("dom.input.dirpicker", false) && Allowdirs()) ||
        (Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) &&
         HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory))) {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoDirSelected", value);
    } else if (HasAttr(kNameSpaceID_None, nsGkAtoms::multiple)) {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFilesSelected", value);
    } else {
      nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                         "NoFileSelected", value);
    }
  } else {
    nsString count;
    count.AppendInt(int(mFilesOrDirectories.Length()));

    const char16_t* params[] = { count.get() };
    nsContentUtils::FormatLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                          "XFilesSelected", params, value);
  }

  aValue = value;
}

void
HTMLInputElement::SetFilesOrDirectories(const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories,
                                        bool aSetValueChanged)
{
  ClearGetFilesHelpers();

  if (Preferences::GetBool("dom.webkitBlink.filesystem.enabled", false)) {
    HTMLInputElementBinding::ClearCachedWebkitEntriesValue(this);
    mEntries.Clear();
  }

  mFilesOrDirectories.Clear();
  mFilesOrDirectories.AppendElements(aFilesOrDirectories);

  AfterSetFilesOrDirectories(aSetValueChanged);
}

void
HTMLInputElement::SetFiles(nsIDOMFileList* aFiles,
                           bool aSetValueChanged)
{
  RefPtr<FileList> files = static_cast<FileList*>(aFiles);
  mFilesOrDirectories.Clear();
  ClearGetFilesHelpers();

  if (Preferences::GetBool("dom.webkitBlink.filesystem.enabled", false)) {
    HTMLInputElementBinding::ClearCachedWebkitEntriesValue(this);
    mEntries.Clear();
  }

  if (aFiles) {
    uint32_t listLength;
    aFiles->GetLength(&listLength);
    for (uint32_t i = 0; i < listLength; i++) {
      OwningFileOrDirectory* element = mFilesOrDirectories.AppendElement();
      element->SetAsFile() = files->Item(i);
    }
  }

  AfterSetFilesOrDirectories(aSetValueChanged);
}

// This method is used for testing only.
void
HTMLInputElement::MozSetDndFilesAndDirectories(const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories)
{
  SetFilesOrDirectories(aFilesOrDirectories, true);

  if (Preferences::GetBool("dom.webkitBlink.filesystem.enabled", false)) {
    UpdateEntries(aFilesOrDirectories);
  }

  RefPtr<DispatchChangeEventCallback> dispatchChangeEventCallback =
    new DispatchChangeEventCallback(this);

  if (Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory)) {
    ErrorResult rv;
    GetFilesHelper* helper = GetOrCreateGetFilesHelper(true /* recursionFlag */,
                                                       rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
      return;
    }

    helper->AddCallback(dispatchChangeEventCallback);
  } else {
    dispatchChangeEventCallback->DispatchEvents();
  }
}

void
HTMLInputElement::AfterSetFilesOrDirectories(bool aSetValueChanged)
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

  // Grab the full path here for any chrome callers who access our .value via a
  // CPOW. This path won't be called from a CPOW meaning the potential sync IPC
  // call under GetMozFullPath won't be rejected for not being urgent.
  // XXX Protected by the ifndef because the blob code doesn't allow us to send
  // this message in b2g.
  if (mFilesOrDirectories.IsEmpty()) {
    mFirstFilePath.Truncate();
  } else {
    ErrorResult rv;
    GetDOMFileOrDirectoryPath(mFilesOrDirectories[0], mFirstFilePath, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }
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
  nsAutoString value;
  GetValue(value);

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

FileList*
HTMLInputElement::GetFiles()
{
  if (mType != NS_FORM_INPUT_FILE) {
    return nullptr;
  }

  if (Preferences::GetBool("dom.input.dirpicker", false) && Allowdirs() &&
      (!Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) ||
       !HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory))) {
    return nullptr;
  }

  if (!mFileList) {
    mFileList = new FileList(static_cast<nsIContent*>(this));
    UpdateFileList();
  }

  return mFileList;
}

/* static */ void
HTMLInputElement::HandleNumberControlSpin(void* aData)
{
  HTMLInputElement* input = static_cast<HTMLInputElement*>(aData);

  NS_ASSERTION(input->mNumberControlSpinnerIsSpinning,
               "Should have called nsRepeatService::Stop()");

  nsNumberControlFrame* numberControlFrame =
    do_QueryFrame(input->GetPrimaryFrame());
  if (input->mType != NS_FORM_INPUT_NUMBER || !numberControlFrame) {
    // Type has changed (and possibly our frame type hasn't been updated yet)
    // or else we've lost our frame. Either way, stop the timer and don't do
    // anything else.
    input->StopNumberControlSpinnerSpin();
  } else {
    input->StepNumberControlForUserEvent(input->mNumberControlSpinnerSpinsUp ? 1 : -1);
  }
}

void
HTMLInputElement::UpdateFileList()
{
  if (mFileList) {
    mFileList->Clear();

    const nsTArray<OwningFileOrDirectory>& array =
      GetFilesOrDirectoriesInternal();

    for (uint32_t i = 0; i < array.Length(); ++i) {
      if (array[i].IsFile()) {
        mFileList->Append(array[i].GetAsFile());
      }
    }
  }
}

nsresult
HTMLInputElement::SetValueInternal(const nsAString& aValue, uint32_t aFlags)
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
      // else DoneCreatingElement calls us again once mParserCreating is false

      bool setValueChanged = !!(aFlags & nsTextEditorState::eSetValue_Notify);
      if (setValueChanged) {
        SetValueChanged(true);
      }

      if (IsSingleLineTextControl(false)) {
        if (!mInputData.mState->SetValue(value, aFlags)) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        if (mType == NS_FORM_INPUT_EMAIL) {
          UpdateAllValidityStates(mParserCreating);
        }
      } else {
        free(mInputData.mValue);
        mInputData.mValue = ToNewUnicode(value);
        if (setValueChanged) {
          SetValueChanged(true);
        }
        if (mType == NS_FORM_INPUT_NUMBER) {
          // This has to happen before OnValueChanged is called because that
          // method needs the new value of our frame's anon text control.
          nsNumberControlFrame* numberControlFrame =
            do_QueryFrame(GetPrimaryFrame());
          if (numberControlFrame) {
            numberControlFrame->SetValueOfAnonTextControl(value);
          }
        } else if (mType == NS_FORM_INPUT_RANGE) {
          nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
          if (frame) {
            frame->UpdateForValueChange();
          }
        }
        if (!mParserCreating) {
          OnValueChanged(/* aNotify = */ true,
                         /* aWasInteractiveUserChange = */ false);
        }
        // else DoneCreatingElement calls us again once mParserCreating is false
      }

      if (mType == NS_FORM_INPUT_COLOR) {
        // Update color frame, to reflect color changes
        nsColorControlFrame* colorControlFrame = do_QueryFrame(GetPrimaryFrame());
        if (colorControlFrame) {
          colorControlFrame->UpdateColor();
        }
      }

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

  //XXXsmaug It isn't clear how this should work in Shadow DOM.
  return static_cast<nsDocument*>(GetUncomposedDoc());
}

already_AddRefed<nsIDOMHTMLInputElement>
HTMLInputElement::GetSelectedRadioButton() const
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
    WidgetMouseEvent event(true, eMouseClick, nullptr, WidgetMouseEvent::eReal);
    nsEventStatus status = nsEventStatus_eIgnore;
    shell->HandleDOMEventWithTarget(submitContent, &event, &status);
  } else if (!mForm->ImplicitSubmissionIsDisabled() &&
             mForm->SubmissionCanProceed(nullptr)) {
    // TODO: removing this code and have the submit event sent by the form,
    // bug 592124.
    // If there's only one text control, just submit the form
    // Hold strong ref across the event
    RefPtr<mozilla::dom::HTMLFormElement> form = mForm;
    InternalFormEvent event(true, eFormSubmit);
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

  // Notify all radios in the group that value has changed, this is to let
  // radios to have the chance to update its states, e.g., :indeterminate.
  if (mType == NS_FORM_INPUT_RADIO) {
    nsCOMPtr<nsIRadioVisitor> visitor = new nsRadioUpdateStateVisitor(this);
    VisitGroup(visitor, aNotify);
  }
}

void
HTMLInputElement::Blur(ErrorResult& aError)
{
  if (mType == NS_FORM_INPUT_NUMBER) {
    // Blur our anonymous text control, if we have one. (DOM 'change' event
    // firing and other things depend on this.)
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame) {
      HTMLInputElement* textControl = numberControlFrame->GetAnonTextControl();
      if (textControl) {
        textControl->Blur(aError);
        return;
      }
    }
  }
  nsGenericHTMLElement::Blur(aError);
}

void
HTMLInputElement::Focus(ErrorResult& aError)
{
  if (mType == NS_FORM_INPUT_NUMBER) {
    // Focus our anonymous text control, if we have one.
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame) {
      HTMLInputElement* textControl = numberControlFrame->GetAnonTextControl();
      if (textControl) {
        textControl->Focus(aError);
        return;
      }
    }
  }

  if (mType != NS_FORM_INPUT_FILE) {
    nsGenericHTMLElement::Focus(aError);
    return;
  }

  // For file inputs, focus the first button instead. In the case of there
  // being two buttons (when the picker is a directory picker) the user can
  // tab to the next one.
  nsIFrame* frame = GetPrimaryFrame();
  if (frame) {
    for (nsIFrame* childFrame : frame->PrincipalChildList()) {
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

#if defined(XP_WIN) || defined(XP_LINUX)
bool
HTMLInputElement::IsNodeApzAwareInternal() const
{
  // Tell APZC we may handle mouse wheel event and do preventDefault when input
  // type is number.
  return (mType == NS_FORM_INPUT_NUMBER) || (mType == NS_FORM_INPUT_RANGE) ||
         nsINode::IsNodeApzAwareInternal();
}
#endif

bool
HTMLInputElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return mType != NS_FORM_INPUT_HIDDEN ||
         nsGenericHTMLFormElementWithState::IsInteractiveHTMLContent(aIgnoreTabindex);
}

NS_IMETHODIMP
HTMLInputElement::Select()
{
  if (mType == NS_FORM_INPUT_NUMBER) {
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame) {
      return numberControlFrame->HandleSelectCall();
    }
    return NS_OK;
  }

  if (!IsSingleLineTextControl(false)) {
    return NS_OK;
  }

  // XXX Bug?  We have to give the input focus before contents can be
  // selected

  FocusTristate state = FocusState();
  if (state == eUnfocusable) {
    return NS_OK;
  }

  nsTextEditorState* tes = GetEditorState();
  if (tes) {
    nsFrameSelection* fs = tes->GetConstFrameSelection();
    if (fs && fs->MouseDownRecorded()) {
      // This means that we're being called while the frame selection has a mouse
      // down event recorded to adjust the caret during the mouse up event.
      // We are probably called from the focus event handler.  We should override
      // the delayed caret data in this case to ensure that this select() call
      // takes effect.
      fs->SetDelayedCaretData(nullptr);
    }
  }

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();

  RefPtr<nsPresContext> presContext = GetPresContext(eForComposedDoc);
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
    WidgetEvent event(nsContentUtils::LegacyIsCallerChromeOrNativeCode(), eFormSelect);

    mHandlingSelectEvent = true;
    EventDispatcher::Dispatch(static_cast<nsIContent*>(this),
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

bool
HTMLInputElement::NeedToInitializeEditorForEvent(
                    EventChainPreVisitor& aVisitor) const
{
  // We only need to initialize the editor for single line input controls because they
  // are lazily initialized.  We don't need to initialize the control for
  // certain types of events, because we know that those events are safe to be
  // handled without the editor being initialized.  These events include:
  // mousein/move/out, overflow/underflow, and DOM mutation events.
  if (!IsSingleLineTextControl(false) ||
      aVisitor.mEvent->mClass == eMutationEventClass) {
    return false;
  }

  switch (aVisitor.mEvent->mMessage) {
  case eMouseMove:
  case eMouseEnterIntoWidget:
  case eMouseExitFromWidget:
  case eMouseOver:
  case eMouseOut:
  case eScrollPortUnderflow:
  case eScrollPortOverflow:
    return false;
  default:
    return true;
  }
}

bool
HTMLInputElement::IsDisabledForEvents(EventMessage aMessage)
{
  return IsElementDisabledForEvents(aMessage, GetPrimaryFrame());
}

nsresult
HTMLInputElement::PreHandleEvent(EventChainPreVisitor& aVisitor)
{
  // Do not process any DOM events if the element is disabled
  aVisitor.mCanHandle = false;
  if (IsDisabledForEvents(aVisitor.mEvent->mMessage)) {
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
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  bool outerActivateEvent =
    ((mouseEvent && mouseEvent->IsLeftClickEvent()) ||
     (aVisitor.mEvent->mMessage == eLegacyDOMActivate && !mInInternalActivate));

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
      aVisitor.mEvent->mMessage == eMouseClick &&
      aVisitor.mEvent->AsMouseEvent()->button ==
        WidgetMouseEvent::eMiddleButton) {
    aVisitor.mEvent->mFlags.mNoContentDispatch = false;
  }

  // We must cache type because mType may change during JS event (bug 2369)
  aVisitor.mItemFlags |= mType;

  if (aVisitor.mEvent->mMessage == eFocus &&
      aVisitor.mEvent->IsTrusted() &&
      MayFireChangeOnBlur() &&
      // StartRangeThumbDrag already set mFocusedValue on 'mousedown' before
      // we get the 'focus' event.
      !mIsDraggingRange) {
    GetValue(mFocusedValue);
  }

  // Fire onchange (if necessary), before we do the blur, bug 357684.
  if (aVisitor.mEvent->mMessage == eBlur) {
    // Experimental mobile types rely on the system UI to prevent users to not
    // set invalid values but we have to be extra-careful. Especially if the
    // option has been enabled on desktop.
    if (IsExperimentalMobileType(mType) || IsDateTimeInputType(mType)) {
      nsAutoString aValue;
      GetValueInternal(aValue);
      nsresult rv =
        SetValueInternal(aValue, nsTextEditorState::eSetValue_Internal);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    FireChangeEventIfNeeded();
  }

  if (mType == NS_FORM_INPUT_RANGE &&
      (aVisitor.mEvent->mMessage == eFocus ||
       aVisitor.mEvent->mMessage == eBlur)) {
    // Just as nsGenericHTMLFormElementWithState::PreHandleEvent calls
    // nsIFormControlFrame::SetFocus, we handle focus here.
    nsIFrame* frame = GetPrimaryFrame();
    if (frame) {
      frame->InvalidateFrameSubtree();
    }
  }

  if (mType == NS_FORM_INPUT_NUMBER && aVisitor.mEvent->IsTrusted()) {
    if (mNumberControlSpinnerIsSpinning) {
      // If the timer is running the user has depressed the mouse on one of the
      // spin buttons. If the mouse exits the button we either want to reverse
      // the direction of spin if it has moved over the other button, or else
      // we want to end the spin. We do this here (rather than in
      // PostHandleEvent) because we don't want to let content preventDefault()
      // the end of the spin.
      if (aVisitor.mEvent->mMessage == eMouseMove) {
        // Be aggressive about stopping the spin:
        bool stopSpin = true;
        nsNumberControlFrame* numberControlFrame =
          do_QueryFrame(GetPrimaryFrame());
        if (numberControlFrame) {
          bool oldNumberControlSpinTimerSpinsUpValue =
                 mNumberControlSpinnerSpinsUp;
          switch (numberControlFrame->GetSpinButtonForPointerEvent(
                    aVisitor.mEvent->AsMouseEvent())) {
          case nsNumberControlFrame::eSpinButtonUp:
            mNumberControlSpinnerSpinsUp = true;
            stopSpin = false;
            break;
          case nsNumberControlFrame::eSpinButtonDown:
            mNumberControlSpinnerSpinsUp = false;
            stopSpin = false;
            break;
          }
          if (mNumberControlSpinnerSpinsUp !=
                oldNumberControlSpinTimerSpinsUpValue) {
            nsNumberControlFrame* numberControlFrame =
              do_QueryFrame(GetPrimaryFrame());
            if (numberControlFrame) {
              numberControlFrame->SpinnerStateChanged();
            }
          }
        }
        if (stopSpin) {
          StopNumberControlSpinnerSpin();
        }
      } else if (aVisitor.mEvent->mMessage == eMouseUp) {
        StopNumberControlSpinnerSpin();
      }
    }
    if (aVisitor.mEvent->mMessage == eFocus ||
        aVisitor.mEvent->mMessage == eBlur) {
      if (aVisitor.mEvent->mMessage == eFocus) {
        // Tell our frame it's getting focus so that it can make sure focus
        // is moved to our anonymous text control.
        nsNumberControlFrame* numberControlFrame =
          do_QueryFrame(GetPrimaryFrame());
        if (numberControlFrame) {
          // This could kill the frame!
          numberControlFrame->HandleFocusEvent(aVisitor.mEvent);
        }
      }
      nsIFrame* frame = GetPrimaryFrame();
      if (frame && frame->IsThemed()) {
        // Our frame's nested <input type=text> will be invalidated when it
        // loses focus, but since we are also native themed we need to make
        // sure that our entire area is repainted since any focus highlight
        // from the theme should be removed from us (the repainting of the
        // sub-area occupied by the anon text control is not enough to do
        // that).
        frame->InvalidateFrame();
      }
    }
  }

  nsresult rv = nsGenericHTMLFormElementWithState::PreHandleEvent(aVisitor);

  // We do this after calling the base class' PreHandleEvent so that
  // nsIContent::PreHandleEvent doesn't reset any change we make to mCanHandle.
  if (mType == NS_FORM_INPUT_NUMBER &&
      aVisitor.mEvent->IsTrusted()  &&
      aVisitor.mEvent->mOriginalTarget != this) {
    // <input type=number> has an anonymous <input type=text> descendant. If
    // 'input' or 'change' events are fired at that text control then we need
    // to do some special handling here.
    HTMLInputElement* textControl = nullptr;
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame) {
      textControl = numberControlFrame->GetAnonTextControl();
    }
    if (textControl && aVisitor.mEvent->mOriginalTarget == textControl) {
      if (aVisitor.mEvent->mMessage == eEditorInput) {
        // Propogate the anon text control's new value to our HTMLInputElement:
        nsAutoString value;
        numberControlFrame->GetValueOfAnonTextControl(value);
        numberControlFrame->HandlingInputEvent(true);
        nsWeakFrame weakNumberControlFrame(numberControlFrame);
        rv = SetValueInternal(value,
                              nsTextEditorState::eSetValue_BySetUserInput |
                              nsTextEditorState::eSetValue_Notify);
        NS_ENSURE_SUCCESS(rv, rv);
        if (weakNumberControlFrame.IsAlive()) {
          numberControlFrame->HandlingInputEvent(false);
        }
      }
      else if (aVisitor.mEvent->mMessage == eFormChange) {
        // We cancel the DOM 'change' event that is fired for any change to our
        // anonymous text control since we fire our own 'change' events and
        // content shouldn't be seeing two 'change' events. Besides that we
        // (as a number) control have tighter restrictions on when our internal
        // value changes than our anon text control does, so in some cases
        // (if our text control's value doesn't parse as a number) we don't
        // want to fire a 'change' event at all.
        aVisitor.mCanHandle = false;
      }
    }
  }

  return rv;
}

void
HTMLInputElement::StartRangeThumbDrag(WidgetGUIEvent* aEvent)
{
  mIsDraggingRange = true;
  mRangeThumbDragStartValue = GetValueAsDecimal();
  // Don't use CAPTURE_RETARGETTOELEMENT, as that breaks pseudo-class styling
  // of the thumb.
  nsIPresShell::SetCapturingContent(this, CAPTURE_IGNOREALLOWED);
  nsRangeFrame* rangeFrame = do_QueryFrame(GetPrimaryFrame());

  // Before we change the value, record the current value so that we'll
  // correctly send a 'change' event if appropriate. We need to do this here
  // because the 'focus' event is handled after the 'mousedown' event that
  // we're being called for (i.e. too late to update mFocusedValue, since we'll
  // have changed it by then).
  GetValue(mFocusedValue);

  SetValueOfRangeForUserEvent(rangeFrame->GetValueAtEventPoint(aEvent));
}

void
HTMLInputElement::FinishRangeThumbDrag(WidgetGUIEvent* aEvent)
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
    // TODO: What should we do if SetValueInternal fails?  (The allocation
    // is small, so we should be fine here.)
    SetValueInternal(val, nsTextEditorState::eSetValue_BySetUserInput |
                          nsTextEditorState::eSetValue_Notify);
    nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
    if (frame) {
      frame->UpdateForValueChange();
    }
    RefPtr<AsyncEventDispatcher> asyncDispatcher =
      new AsyncEventDispatcher(this, NS_LITERAL_STRING("input"), true, false);
    asyncDispatcher->RunDOMEventWhenSafe();
  }
}

void
HTMLInputElement::SetValueOfRangeForUserEvent(Decimal aValue)
{
  MOZ_ASSERT(aValue.isFinite());

  Decimal oldValue = GetValueAsDecimal();

  nsAutoString val;
  ConvertNumberToString(aValue, val);
  // TODO: What should we do if SetValueInternal fails?  (The allocation
  // is small, so we should be fine here.)
  SetValueInternal(val, nsTextEditorState::eSetValue_BySetUserInput |
                        nsTextEditorState::eSetValue_Notify);
  nsRangeFrame* frame = do_QueryFrame(GetPrimaryFrame());
  if (frame) {
    frame->UpdateForValueChange();
  }

  if (GetValueAsDecimal() != oldValue) {
    nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                         static_cast<nsIDOMHTMLInputElement*>(this),
                                         NS_LITERAL_STRING("input"), true,
                                         false);
  }
}

void
HTMLInputElement::StartNumberControlSpinnerSpin()
{
  MOZ_ASSERT(!mNumberControlSpinnerIsSpinning);

  mNumberControlSpinnerIsSpinning = true;

  nsRepeatService::GetInstance()->Start(HandleNumberControlSpin, this);

  // Capture the mouse so that we can tell if the pointer moves from one
  // spin button to the other, or to some other element:
  nsIPresShell::SetCapturingContent(this, CAPTURE_IGNOREALLOWED);

  nsNumberControlFrame* numberControlFrame =
    do_QueryFrame(GetPrimaryFrame());
  if (numberControlFrame) {
    numberControlFrame->SpinnerStateChanged();
  }
}

void
HTMLInputElement::StopNumberControlSpinnerSpin(SpinnerStopState aState)
{
  if (mNumberControlSpinnerIsSpinning) {
    if (nsIPresShell::GetCapturingContent() == this) {
      nsIPresShell::SetCapturingContent(nullptr, 0); // cancel capture
    }

    nsRepeatService::GetInstance()->Stop(HandleNumberControlSpin, this);

    mNumberControlSpinnerIsSpinning = false;

    if (aState == eAllowDispatchingEvents) {
      FireChangeEventIfNeeded();
    }

    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame) {
      MOZ_ASSERT(aState == eAllowDispatchingEvents,
                 "Shouldn't have primary frame for the element when we're not "
                 "allowed to dispatch events to it anymore.");
      numberControlFrame->SpinnerStateChanged();
    }
  }
}

void
HTMLInputElement::StepNumberControlForUserEvent(int32_t aDirection)
{
  // We can't use GetValidityState here because the validity state is not set
  // if the user hasn't previously taken an action to set or change the value,
  // according to the specs.
  if (HasBadInput()) {
    // If the user has typed a value into the control and inadvertently made a
    // mistake (e.g. put a thousand separator at the wrong point) we do not
    // want to wipe out what they typed if they try to increment/decrement the
    // value. Better is to highlight the value as being invalid so that they
    // can correct what they typed.
    // We only do this if there actually is a value typed in by/displayed to
    // the user. (IsValid() can return false if the 'required' attribute is
    // set and the value is the empty string.)
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame &&
        !numberControlFrame->AnonTextControlIsEmpty()) {
      // We pass 'true' for UpdateValidityUIBits' aIsFocused argument
      // regardless because we need the UI to update _now_ or the user will
      // wonder why the step behavior isn't functioning.
      UpdateValidityUIBits(true);
      UpdateState(true);
      return;
    }
  }

  Decimal newValue = Decimal::nan(); // unchanged if value will not change

  nsresult rv = GetValueIfStepped(aDirection, CALLED_FOR_USER_EVENT, &newValue);

  if (NS_FAILED(rv) || !newValue.isFinite()) {
    return; // value should not or will not change
  }

  nsAutoString newVal;
  ConvertNumberToString(newValue, newVal);
  // TODO: What should we do if SetValueInternal fails?  (The allocation
  // is small, so we should be fine here.)
  SetValueInternal(newVal, nsTextEditorState::eSetValue_BySetUserInput |
                           nsTextEditorState::eSetValue_Notify);

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

nsresult
HTMLInputElement::MaybeInitPickers(EventChainPostVisitor& aVisitor)
{
  // Open a file picker when we receive a click on a <input type='file'>, or
  // open a color picker when we receive a click on a <input type='color'>.
  // A click is handled in the following cases:
  // - preventDefault() has not been called (or something similar);
  // - it's the left mouse button.
  // We do not prevent non-trusted click because authors can already use
  // .click(). However, the pickers will follow the rules of popup-blocking.
  if (aVisitor.mEvent->DefaultPrevented()) {
    return NS_OK;
  }
  WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
  if (!(mouseEvent && mouseEvent->IsLeftClickEvent())) {
    return NS_OK;
  }
  if (mType == NS_FORM_INPUT_FILE) {
    // If the user clicked on the "Choose folder..." button we open the
    // directory picker, else we open the file picker.
    FilePickerType type = FILE_PICKER_FILE;
    nsCOMPtr<nsIContent> target =
      do_QueryInterface(aVisitor.mEvent->mOriginalTarget);
    if (target &&
        target->FindFirstNonChromeOnlyAccessContent() == this &&
        ((Preferences::GetBool("dom.input.dirpicker", false) && Allowdirs()) ||
         (Preferences::GetBool("dom.webkitBlink.dirPicker.enabled", false) &&
          HasAttr(kNameSpaceID_None, nsGkAtoms::webkitdirectory)))) {
      type = FILE_PICKER_DIRECTORY;
    }
    return InitFilePicker(type);
  }
  if (mType == NS_FORM_INPUT_COLOR) {
    return InitColorPicker();
  }
  if (mType == NS_FORM_INPUT_DATE) {
    return InitDatePicker();
  }

  return NS_OK;
}

nsresult
HTMLInputElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  if (!aVisitor.mPresContext) {
    // Hack alert! In order to open file picker even in case the element isn't
    // in document, try to init picker even without PresContext.
    return MaybeInitPickers(aVisitor);
  }

  if (aVisitor.mEvent->mMessage == eFocus ||
      aVisitor.mEvent->mMessage == eBlur) {
    if (aVisitor.mEvent->mMessage == eBlur) {
      if (mIsDraggingRange) {
        FinishRangeThumbDrag();
      } else if (mNumberControlSpinnerIsSpinning) {
        StopNumberControlSpinnerSpin();
      }
    }

    UpdateValidityUIBits(aVisitor.mEvent->mMessage == eFocus);

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
      mType != NS_FORM_INPUT_NUMBER) {
    WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
    if (mouseEvent && mouseEvent->IsLeftClickEvent() &&
        !ShouldPreventDOMActivateDispatch(aVisitor.mEvent->mOriginalTarget)) {
      // DOMActive event should be trusted since the activation is actually
      // occurred even if the cause is an untrusted click event.
      InternalUIEvent actEvent(true, eLegacyDOMActivate, mouseEvent);
      actEvent.mDetail = 1;

      nsCOMPtr<nsIPresShell> shell = aVisitor.mPresContext->GetPresShell();
      if (shell) {
        nsEventStatus status = nsEventStatus_eIgnore;
        mInInternalActivate = true;
        rv = shell->HandleDOMEventWithTarget(this, &actEvent, &status);
        mInInternalActivate = false;

        // If activate is cancelled, we must do the same as when click is
        // cancelled (revert the checkbox to its original value).
        if (status == nsEventStatus_eConsumeNoDefault) {
          aVisitor.mEventStatus = status;
        }
      }
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
      default:
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
      // Fire input event and then change event.
      nsContentUtils::DispatchTrustedEvent(OwnerDoc(),
                                           static_cast<nsIDOMHTMLInputElement*>(this),
                                           NS_LITERAL_STRING("input"), true,
                                           false);

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
    WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
    if (mType ==  NS_FORM_INPUT_NUMBER &&
        keyEvent && keyEvent->mMessage == eKeyPress &&
        aVisitor.mEvent->IsTrusted() &&
        (keyEvent->mKeyCode == NS_VK_UP || keyEvent->mKeyCode == NS_VK_DOWN) &&
        !(keyEvent->IsShift() || keyEvent->IsControl() ||
          keyEvent->IsAlt() || keyEvent->IsMeta() ||
          keyEvent->IsAltGraph() || keyEvent->IsFn() ||
          keyEvent->IsOS())) {
      // We handle the up/down arrow keys specially for <input type=number>.
      // On some platforms the editor for the nested text control will
      // process these keys to send the cursor to the start/end of the text
      // control and as a result aVisitor.mEventStatus will already have been
      // set to nsEventStatus_eConsumeNoDefault. However, we know that
      // whenever the up/down arrow keys cause the value of the number
      // control to change the string in the text control will change, and
      // the cursor will be moved to the end of the text control, overwriting
      // the editor's handling of up/down keypress events. For that reason we
      // just ignore aVisitor.mEventStatus here and go ahead and handle the
      // event to increase/decrease the value of the number control.
      if (!aVisitor.mEvent->DefaultPreventedByContent() && IsMutable()) {
        StepNumberControlForUserEvent(keyEvent->mKeyCode == NS_VK_UP ? 1 : -1);
        FireChangeEventIfNeeded();
        aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
      }
    } else if (nsEventStatus_eIgnore == aVisitor.mEventStatus) {
      switch (aVisitor.mEvent->mMessage) {
        case eFocus: {
          // see if we should select the contents of the textbox. This happens
          // for text and password fields when the field was focused by the
          // keyboard or a navigation, the platform allows it, and it wasn't
          // just because we raised a window.
          nsIFocusManager* fm = nsFocusManager::GetFocusManager();
          if (fm && IsSingleLineTextControl(false) &&
              !aVisitor.mEvent->AsFocusEvent()->mFromRaise &&
              SelectTextFieldOnFocus()) {
            nsIDocument* document = GetComposedDoc();
            if (document) {
              uint32_t lastFocusMethod;
              fm->GetLastFocusMethod(document->GetWindow(), &lastFocusMethod);
              if (lastFocusMethod &
                  (nsIFocusManager::FLAG_BYKEY | nsIFocusManager::FLAG_BYMOVEFOCUS)) {
                RefPtr<nsPresContext> presContext =
                  GetPresContext(eForComposedDoc);
                if (DispatchSelectEvent(presContext)) {
                  SelectAll(presContext);
                }
              }
            }
          }
          break;
        }

        case eKeyPress:
        case eKeyUp:
        {
          // For backwards compat, trigger checks/radios/buttons with
          // space or enter (bug 25300)
          WidgetKeyboardEvent* keyEvent = aVisitor.mEvent->AsKeyboardEvent();
          if ((aVisitor.mEvent->mMessage == eKeyPress &&
               keyEvent->mKeyCode == NS_VK_RETURN) ||
              (aVisitor.mEvent->mMessage == eKeyUp &&
               keyEvent->mKeyCode == NS_VK_SPACE)) {
            switch(mType) {
              case NS_FORM_INPUT_CHECKBOX:
              case NS_FORM_INPUT_RADIO:
              {
                // Checkbox and Radio try to submit on Enter press
                if (keyEvent->mKeyCode != NS_VK_SPACE) {
                  MaybeSubmitForm(aVisitor.mPresContext);

                  break;  // If we are submitting, do not send click event
                }
                // else fall through and treat Space like click...
                MOZ_FALLTHROUGH;
              }
              case NS_FORM_INPUT_BUTTON:
              case NS_FORM_INPUT_RESET:
              case NS_FORM_INPUT_SUBMIT:
              case NS_FORM_INPUT_IMAGE: // Bug 34418
              case NS_FORM_INPUT_COLOR:
              {
                DispatchSimulatedClick(this, aVisitor.mEvent->IsTrusted(),
                                       aVisitor.mPresContext);
                aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
              } // case
            } // switch
          }
          if (aVisitor.mEvent->mMessage == eKeyPress &&
              mType == NS_FORM_INPUT_RADIO && !keyEvent->IsAlt() &&
              !keyEvent->IsControl() && !keyEvent->IsMeta()) {
            bool isMovingBack = false;
            switch (keyEvent->mKeyCode) {
              case NS_VK_UP:
              case NS_VK_LEFT:
                isMovingBack = true;
                MOZ_FALLTHROUGH;
              case NS_VK_DOWN:
              case NS_VK_RIGHT:
              // Arrow key pressed, focus+select prev/next radio button
              nsIRadioGroupContainer* container = GetRadioGroupContainer();
              if (container) {
                nsAutoString name;
                GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);
                RefPtr<HTMLInputElement> selectedRadioButton;
                container->GetNextRadioButton(name, isMovingBack, this,
                                              getter_AddRefs(selectedRadioButton));
                if (selectedRadioButton) {
                  rv = selectedRadioButton->Focus();
                  if (NS_SUCCEEDED(rv)) {
                    rv = DispatchSimulatedClick(selectedRadioButton,
                                                aVisitor.mEvent->IsTrusted(),
                                                aVisitor.mPresContext);
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

          if (aVisitor.mEvent->mMessage == eKeyPress &&
              keyEvent->mKeyCode == NS_VK_RETURN &&
               (IsSingleLineTextControl(false, mType) ||
                mType == NS_FORM_INPUT_NUMBER ||
                IsExperimentalMobileType(mType) ||
                IsDateTimeInputType(mType))) {
            FireChangeEventIfNeeded();
            rv = MaybeSubmitForm(aVisitor.mPresContext);
            NS_ENSURE_SUCCESS(rv, rv);
          }

          if (aVisitor.mEvent->mMessage == eKeyPress &&
              mType == NS_FORM_INPUT_RANGE && !keyEvent->IsAlt() &&
              !keyEvent->IsControl() && !keyEvent->IsMeta() &&
              (keyEvent->mKeyCode == NS_VK_LEFT ||
               keyEvent->mKeyCode == NS_VK_RIGHT ||
               keyEvent->mKeyCode == NS_VK_UP ||
               keyEvent->mKeyCode == NS_VK_DOWN ||
               keyEvent->mKeyCode == NS_VK_PAGE_UP ||
               keyEvent->mKeyCode == NS_VK_PAGE_DOWN ||
               keyEvent->mKeyCode == NS_VK_HOME ||
               keyEvent->mKeyCode == NS_VK_END)) {
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
              switch (keyEvent->mKeyCode) {
                case  NS_VK_LEFT:
                  newValue = value + (GetComputedDirectionality() == eDir_RTL
                                        ? step : -step);
                  break;
                case  NS_VK_RIGHT:
                  newValue = value + (GetComputedDirectionality() == eDir_RTL
                                        ? -step : step);
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
                  newValue = value + std::max(step, (maximum - minimum) / Decimal(10));
                  break;
                case  NS_VK_PAGE_DOWN:
                  newValue = value - std::max(step, (maximum - minimum) / Decimal(10));
                  break;
              }
              SetValueOfRangeForUserEvent(newValue);
              FireChangeEventIfNeeded();
              aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
            }
          }

        } break; // eKeyPress || eKeyUp

        case eMouseDown:
        case eMouseUp:
        case eMouseDoubleClick: {
          // cancel all of these events for buttons
          //XXXsmaug Why?
          WidgetMouseEvent* mouseEvent = aVisitor.mEvent->AsMouseEvent();
          if (mouseEvent->button == WidgetMouseEvent::eMiddleButton ||
              mouseEvent->button == WidgetMouseEvent::eRightButton) {
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
          if (mType == NS_FORM_INPUT_NUMBER && aVisitor.mEvent->IsTrusted()) {
            if (mouseEvent->button == WidgetMouseEvent::eLeftButton &&
                !(mouseEvent->IsShift() || mouseEvent->IsControl() ||
                  mouseEvent->IsAlt() || mouseEvent->IsMeta() ||
                  mouseEvent->IsAltGraph() || mouseEvent->IsFn() ||
                  mouseEvent->IsOS())) {
              nsNumberControlFrame* numberControlFrame =
                do_QueryFrame(GetPrimaryFrame());
              if (numberControlFrame) {
                if (aVisitor.mEvent->mMessage == eMouseDown &&
                    IsMutable()) {
                  switch (numberControlFrame->GetSpinButtonForPointerEvent(
                            aVisitor.mEvent->AsMouseEvent())) {
                  case nsNumberControlFrame::eSpinButtonUp:
                    StepNumberControlForUserEvent(1);
                    mNumberControlSpinnerSpinsUp = true;
                    StartNumberControlSpinnerSpin();
                    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
                    break;
                  case nsNumberControlFrame::eSpinButtonDown:
                    StepNumberControlForUserEvent(-1);
                    mNumberControlSpinnerSpinsUp = false;
                    StartNumberControlSpinnerSpin();
                    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
                    break;
                  }
                }
              }
            }
            if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
              // We didn't handle this to step up/down. Whatever this was, be
              // aggressive about stopping the spin. (And don't set
              // nsEventStatus_eConsumeNoDefault after doing so, since that
              // might prevent, say, the context menu from opening.)
              StopNumberControlSpinnerSpin();
            }
          }
          break;
        }
#if defined(XP_WIN) || defined(XP_LINUX)
        case eWheel: {
          // Handle wheel events as increasing / decreasing the input element's
          // value when it's focused and it's type is number or range.
          WidgetWheelEvent* wheelEvent = aVisitor.mEvent->AsWheelEvent();
          if (!aVisitor.mEvent->DefaultPrevented() &&
              aVisitor.mEvent->IsTrusted() && IsMutable() && wheelEvent &&
              wheelEvent->mDeltaY != 0 &&
              wheelEvent->mDeltaMode != nsIDOMWheelEvent::DOM_DELTA_PIXEL) {
            if (mType == NS_FORM_INPUT_NUMBER) {
              nsNumberControlFrame* numberControlFrame =
                do_QueryFrame(GetPrimaryFrame());
              if (numberControlFrame && numberControlFrame->IsFocused()) {
                StepNumberControlForUserEvent(wheelEvent->mDeltaY > 0 ? -1 : 1);
                FireChangeEventIfNeeded();
                aVisitor.mEvent->PreventDefault();
              }
            } else if (mType == NS_FORM_INPUT_RANGE &&
                       nsContentUtils::IsFocusedContent(this) &&
                       GetMinimum() < GetMaximum()) {
              Decimal value = GetValueAsDecimal();
              Decimal step = GetStep();
              if (step == kStepAny) {
                step = GetDefaultStep();
              }
              MOZ_ASSERT(value.isFinite() && step.isFinite());
              SetValueOfRangeForUserEvent(wheelEvent->mDeltaY < 0 ?
                                          value + step : value - step);
              FireChangeEventIfNeeded();
              aVisitor.mEvent->PreventDefault();
            }
          }
          break;
        }
#endif
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
            InternalFormEvent event(true,
              (mType == NS_FORM_INPUT_RESET) ? eFormReset : eFormSubmit);
            event.mOriginator = this;
            nsEventStatus status  = nsEventStatus_eIgnore;

            nsCOMPtr<nsIPresShell> presShell =
              aVisitor.mPresContext->GetPresShell();

            // If |nsIPresShell::Destroy| has been called due to
            // handling the event the pres context will return a null
            // pres shell.  See bug 125624.
            // TODO: removing this code and have the submit event sent by the
            // form, see bug 592124.
            if (presShell && (event.mMessage != eFormSubmit ||
                              mForm->SubmissionCanProceed(this))) {
              // Hold a strong ref while dispatching
              RefPtr<mozilla::dom::HTMLFormElement> form(mForm);
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

  return MaybeInitPickers(aVisitor);
}

void
HTMLInputElement::PostHandleEventForRangeThumb(EventChainPostVisitor& aVisitor)
{
  MOZ_ASSERT(mType == NS_FORM_INPUT_RANGE);

  if (nsEventStatus_eConsumeNoDefault == aVisitor.mEventStatus ||
      !(aVisitor.mEvent->mClass == eMouseEventClass ||
        aVisitor.mEvent->mClass == eTouchEventClass ||
        aVisitor.mEvent->mClass == eKeyboardEventClass)) {
    return;
  }

  nsRangeFrame* rangeFrame = do_QueryFrame(GetPrimaryFrame());
  if (!rangeFrame && mIsDraggingRange) {
    CancelRangeThumbDrag();
    return;
  }

  switch (aVisitor.mEvent->mMessage)
  {
    case eMouseDown:
    case eTouchStart: {
      if (mIsDraggingRange) {
        break;
      }
      if (nsIPresShell::GetCapturingContent()) {
        break; // don't start drag if someone else is already capturing
      }
      WidgetInputEvent* inputEvent = aVisitor.mEvent->AsInputEvent();
      if (inputEvent->IsShift() || inputEvent->IsControl() ||
          inputEvent->IsAlt() || inputEvent->IsMeta() ||
          inputEvent->IsAltGraph() || inputEvent->IsFn() ||
          inputEvent->IsOS()) {
        break; // ignore
      }
      if (aVisitor.mEvent->mMessage == eMouseDown) {
        if (aVisitor.mEvent->AsMouseEvent()->buttons ==
              WidgetMouseEvent::eLeftButtonFlag) {
          StartRangeThumbDrag(inputEvent);
        } else if (mIsDraggingRange) {
          CancelRangeThumbDrag();
        }
      } else {
        if (aVisitor.mEvent->AsTouchEvent()->mTouches.Length() == 1) {
          StartRangeThumbDrag(inputEvent);
        } else if (mIsDraggingRange) {
          CancelRangeThumbDrag();
        }
      }
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
    } break;

    case eMouseMove:
    case eTouchMove:
      if (!mIsDraggingRange) {
        break;
      }
      if (nsIPresShell::GetCapturingContent() != this) {
        // Someone else grabbed capture.
        CancelRangeThumbDrag();
        break;
      }
      SetValueOfRangeForUserEvent(
        rangeFrame->GetValueAtEventPoint(aVisitor.mEvent->AsInputEvent()));
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
      break;

    case eMouseUp:
    case eTouchEnd:
      if (!mIsDraggingRange) {
        break;
      }
      // We don't check to see whether we are the capturing content here and
      // call CancelRangeThumbDrag() if that is the case. We just finish off
      // the drag and set our final value (unless someone has called
      // preventDefault() and prevents us getting here).
      FinishRangeThumbDrag(aVisitor.mEvent->AsInputEvent());
      aVisitor.mEvent->mFlags.mMultipleActionsPrevented = true;
      break;

    case eKeyPress:
      if (mIsDraggingRange &&
          aVisitor.mEvent->AsKeyboardEvent()->mKeyCode == NS_VK_ESCAPE) {
        CancelRangeThumbDrag();
      }
      break;

    case eTouchCancel:
      if (mIsDraggingRange) {
        CancelRangeThumbDrag();
      }
      break;

    default:
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
      (NS_FAILED(LoadImage(uri, false, true, eImageLoadType_Normal)) ||
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
        NewRunnableMethod(this, &HTMLInputElement::MaybeLoadImage));
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

  if (mType == NS_FORM_INPUT_PASSWORD) {
    if (IsInComposedDoc()) {
      AsyncEventDispatcher* dispatcher =
        new AsyncEventDispatcher(this,
                                 NS_LITERAL_STRING("DOMInputPasswordAdded"),
                                 true,
                                 true);
      dispatcher->PostDOMEvent();
    }

#ifdef EARLY_BETA_OR_EARLIER
    Telemetry::Accumulate(Telemetry::PWMGR_PASSWORD_INPUT_IN_FORM, !!mForm);
#endif
  }

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

  nsTextEditorState::SelectionProperties sp;

  if (GetEditorState()) {
    sp = mInputData.mState->GetSelectionProperties();
  }

  // We already have a copy of the value, lets free it and changes the type.
  FreeData();
  mType = aNewType;

  if (IsSingleLineTextControl()) {

    mInputData.mState = new nsTextEditorState(this);
    if (!sp.IsDefault()) {
      mInputData.mState->SetSelectionProperties(sp);
    }
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
        // TODO: What should we do if SetValueInternal fails?  (The allocation
        // may potentially be big, but most likely we've failed to allocate
        // before the type change.)
        SetValueInternal(value, nsTextEditorState::eSetValue_Internal);
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
    GetValue(mFocusedValue);
  } else if (!IsSingleLineTextControl(false, mType) &&
             IsSingleLineTextControl(false, oldType)) {
    mFocusedValue.Truncate();
  }

  UpdateHasRange();

  // Do not notify, it will be done after if needed.
  UpdateAllValidityStates(false);

  UpdateApzAwareFlag();
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
        char16_t crlf[] = { char16_t('\r'), char16_t('\n'), 0 };
        aValue.StripChars(crlf);
      }
      break;
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_URL:
      {
        char16_t crlf[] = { char16_t('\r'), char16_t('\n'), 0 };
        aValue.StripChars(crlf);

        aValue = nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(aValue);
      }
      break;
    case NS_FORM_INPUT_NUMBER:
      {
        Decimal value;
        bool ok = ConvertStringToNumber(aValue, value);
        if (!ok) {
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
          value = maximum <= minimum ? minimum : minimum + (maximum - minimum)/Decimal(2);
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
          if (deltaToStep != Decimal(0)) {
            // "suffering from a step mismatch"
            // Round the element's value to the nearest number for which the
            // element would not suffer from a step mismatch, and which is
            // greater than or equal to the minimum, and, if the maximum is not
            // less than the minimum, which is less than or equal to the
            // maximum, if there is a number that matches these constraints:
            MOZ_ASSERT(deltaToStep > Decimal(0), "stepBelow/stepAbove will be wrong");
            Decimal stepBelow = value - deltaToStep;
            Decimal stepAbove = value - deltaToStep + step;
            Decimal halfStep = step / Decimal(2);
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
    case NS_FORM_INPUT_MONTH:
      {
        if (!aValue.IsEmpty() && !IsValidMonth(aValue)) {
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
HTMLInputElement::IsValidMonth(const nsAString& aValue) const
{
  uint32_t year, month;
  return ParseMonth(aValue, &year, &month);
}

bool
HTMLInputElement::IsValidDate(const nsAString& aValue) const
{
  uint32_t year, month, day;
  return ParseDate(aValue, &year, &month, &day);
}

bool HTMLInputElement::ParseYear(const nsAString& aValue, uint32_t* aYear) const
{
  if (aValue.Length() < 4) {
    return false;
  }

  return DigitSubStringToNumber(aValue, 0, aValue.Length(), aYear) &&
      *aYear > 0;
}

bool HTMLInputElement::ParseMonth(const nsAString& aValue,
                                  uint32_t* aYear,
                                  uint32_t* aMonth) const
{
  // Parse the year, month values out a string formatted as 'yyyy-mm'.
  if (aValue.Length() < 7) {
    return false;
  }

  uint32_t endOfYearOffset = aValue.Length() - 3;
  if (aValue[endOfYearOffset] != '-') {
    return false;
  }

  const nsAString& yearStr = Substring(aValue, 0, endOfYearOffset);
  if (!ParseYear(yearStr, aYear)) {
    return false;
  }

  return DigitSubStringToNumber(aValue, endOfYearOffset + 1, 2, aMonth) &&
         *aMonth > 0 && *aMonth <= 12;
}

bool HTMLInputElement::ParseDate(const nsAString& aValue,
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

  uint32_t endOfMonthOffset = aValue.Length() - 3;
  if (aValue[endOfMonthOffset] != '-') {
    return false;
  }

  const nsAString& yearMonthStr = Substring(aValue, 0, endOfMonthOffset);
  if (!ParseMonth(yearMonthStr, aYear, aMonth)) {
    return false;
  }

  return DigitSubStringToNumber(aValue, endOfMonthOffset + 1, 2, aDay) &&
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

static bool
IsDateTimeEnabled(int32_t aNewType)
{
  return (aNewType == NS_FORM_INPUT_DATE &&
          (Preferences::GetBool("dom.forms.datetime", false) ||
           Preferences::GetBool("dom.experimental_forms", false) ||
           Preferences::GetBool("dom.forms.datepicker", false))) ||
         (aNewType == NS_FORM_INPUT_TIME &&
          (Preferences::GetBool("dom.forms.datetime", false) ||
           Preferences::GetBool("dom.experimental_forms", false))) ||
         ((aNewType == NS_FORM_INPUT_MONTH ||
           aNewType == NS_FORM_INPUT_WEEK) &&
          Preferences::GetBool("dom.forms.datetime", false));
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
            (newType == NS_FORM_INPUT_NUMBER &&
             !Preferences::GetBool("dom.forms.number", false)) ||
            (newType == NS_FORM_INPUT_COLOR &&
             !Preferences::GetBool("dom.forms.color", false)) ||
            (IsDateTimeInputType(newType) && !IsDateTimeEnabled(newType))) {
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
    if (aAttribute == nsGkAtoms::minlength) {
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
      aResult.ParseAtomArray(aValue);
      return true;
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

void
HTMLInputElement::MapAttributesIntoRule(const nsMappedAttributes* aAttributes,
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
  if (aAttribute == nsGkAtoms::type ||
      // The presence or absence of the 'directory' attribute determines what
      // buttons we show for type=file.
      aAttribute == nsGkAtoms::allowdirs ||
      aAttribute == nsGkAtoms::webkitdirectory) {
    retval |= nsChangeHint_ReconstructFrame;
  } else if (mType == NS_FORM_INPUT_IMAGE &&
             (aAttribute == nsGkAtoms::alt ||
              aAttribute == nsGkAtoms::value)) {
    // We might need to rebuild our alt text.  Just go ahead and
    // reconstruct our frame.  This should be quite rare..
    retval |= nsChangeHint_ReconstructFrame;
  } else if (aAttribute == nsGkAtoms::value) {
    retval |= NS_STYLE_HINT_REFLOW;
  } else if (aAttribute == nsGkAtoms::size &&
             IsSingleLineTextControl(false)) {
    retval |= NS_STYLE_HINT_REFLOW;
  } else if (PlaceholderApplies() && aAttribute == nsGkAtoms::placeholder) {
    retval |= nsChangeHint_ReconstructFrame;
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


// Directory picking methods:

bool
HTMLInputElement::IsFilesAndDirectoriesSupported() const
{
  // This method is supposed to return true if a file and directory picker
  // supports the selection of both files and directories *at the same time*.
  // Only Mac currently supports that. We could implement it for Mac, but
  // currently we do not.
  return false;
}

void
HTMLInputElement::ChooseDirectory(ErrorResult& aRv)
{
  if (mType != NS_FORM_INPUT_FILE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  // Script can call this method directly, so even though we don't show the
  // "Pick Folder..." button on platforms that don't have a directory picker
  // we have to redirect to the file picker here.
  InitFilePicker(
#if defined(ANDROID) || defined(MOZ_B2G)
                 // No native directory picker - redirect to plain file picker
                 FILE_PICKER_FILE
#else
                 FILE_PICKER_DIRECTORY
#endif
                 );
}

already_AddRefed<Promise>
HTMLInputElement::GetFilesAndDirectories(ErrorResult& aRv)
{
  if (mType != NS_FORM_INPUT_FILE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  const nsTArray<OwningFileOrDirectory>& filesAndDirs =
    GetFilesOrDirectoriesInternal();

  Sequence<OwningFileOrDirectory> filesAndDirsSeq;

  if (!filesAndDirsSeq.SetLength(filesAndDirs.Length(),
                                 mozilla::fallible_t())) {
    p->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
    return p.forget();
  }

  for (uint32_t i = 0; i < filesAndDirs.Length(); ++i) {
    if (filesAndDirs[i].IsDirectory()) {
      RefPtr<Directory> directory = filesAndDirs[i].GetAsDirectory();

      // In future we could refactor SetFilePickerFiltersFromAccept to return a
      // semicolon separated list of file extensions and include that in the
      // filter string passed here.
      directory->SetContentFilters(NS_LITERAL_STRING("filter-out-sensitive"));
      filesAndDirsSeq[i].SetAsDirectory() = directory;
    } else {
      MOZ_ASSERT(filesAndDirs[i].IsFile());

      // This file was directly selected by the user, so don't filter it.
      filesAndDirsSeq[i].SetAsFile() = filesAndDirs[i].GetAsFile();
    }
  }

  p->MaybeResolve(filesAndDirsSeq);
  return p.forget();
}

already_AddRefed<Promise>
HTMLInputElement::GetFiles(bool aRecursiveFlag, ErrorResult& aRv)
{
  if (mType != NS_FORM_INPUT_FILE) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  GetFilesHelper* helper = GetOrCreateGetFilesHelper(aRecursiveFlag, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
   }
  MOZ_ASSERT(helper);

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  if (!global) {
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  helper->AddPromise(p);
  return p.forget();
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
  RefPtr<nsIControllers> controller = GetControllers(rv);
  controller.forget(aResult);
  return rv.StealNSResult();
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
  return rv.StealNSResult();
}

void
HTMLInputElement::SetSelectionRange(int32_t aSelectionStart,
                                    int32_t aSelectionEnd,
                                    const Optional<nsAString>& aDirection,
                                    ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsresult rv = SetSelectionRange(aSelectionStart, aSelectionEnd,
    aDirection.WasPassed() ? aDirection.Value() : NullString());

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionRange(int32_t aSelectionStart,
                                    int32_t aSelectionEnd,
                                    const nsAString& aDirection)
{
  nsresult rv = NS_OK;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
    // Default to forward, even if not specified.
    // Note that we don't currently support directionless selections, so
    // "none" is treated like "forward".
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eForward;
    if (!aDirection.IsEmpty() && aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }

    rv = textControlFrame->SetSelectionRange(aSelectionStart, aSelectionEnd, dir);
    if (NS_SUCCEEDED(rv)) {
      rv = textControlFrame->ScrollSelectionIntoView();
      RefPtr<AsyncEventDispatcher> asyncDispatcher =
        new AsyncEventDispatcher(this, NS_LITERAL_STRING("select"),
                                 true, false);
      asyncDispatcher->PostDOMEvent();
    }
  }

  return rv;
}

void
HTMLInputElement::SetRangeText(const nsAString& aReplacement, ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  int32_t start, end;
  aRv = GetSelectionRange(&start, &end);
  if (aRv.Failed()) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      start = state->GetSelectionProperties().GetStart();
      end = state->GetSelectionProperties().GetEnd();
      aRv = NS_OK;
    }
  }

  SetRangeText(aReplacement, start, end, mozilla::dom::SelectionMode::Preserve,
               aRv, start, end);
}

void
HTMLInputElement::SetRangeText(const nsAString& aReplacement, uint32_t aStart,
                               uint32_t aEnd, const SelectionMode& aSelectMode,
                               ErrorResult& aRv, int32_t aSelectionStart,
                               int32_t aSelectionEnd)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aStart > aEnd) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  nsAutoString value;
  GetValueInternal(value);
  uint32_t inputValueLength = value.Length();

  if (aStart > inputValueLength) {
    aStart = inputValueLength;
  }

  if (aEnd > inputValueLength) {
    aEnd = inputValueLength;
  }

  if (aSelectionStart == -1 && aSelectionEnd == -1) {
    aRv = GetSelectionRange(&aSelectionStart, &aSelectionEnd);
    if (aRv.Failed()) {
      nsTextEditorState* state = GetEditorState();
      if (state && state->IsSelectionCached()) {
        aSelectionStart = state->GetSelectionProperties().GetStart();
        aSelectionEnd = state->GetSelectionProperties().GetEnd();
        aRv = NS_OK;
      }
    }
  }

  if (aStart <= aEnd) {
    value.Replace(aStart, aEnd - aStart, aReplacement);
    nsresult rv =
      SetValueInternal(value, nsTextEditorState::eSetValue_ByContent);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  }

  uint32_t newEnd = aStart + aReplacement.Length();
  int32_t delta =  aReplacement.Length() - (aEnd - aStart);

  switch (aSelectMode) {
    case mozilla::dom::SelectionMode::Select:
    {
      aSelectionStart = aStart;
      aSelectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Start:
    {
      aSelectionStart = aSelectionEnd = aStart;
    }
    break;
    case mozilla::dom::SelectionMode::End:
    {
      aSelectionStart = aSelectionEnd = newEnd;
    }
    break;
    case mozilla::dom::SelectionMode::Preserve:
    {
      if ((uint32_t)aSelectionStart > aEnd) {
        aSelectionStart += delta;
      } else if ((uint32_t)aSelectionStart > aStart) {
        aSelectionStart = aStart;
      }

      if ((uint32_t)aSelectionEnd > aEnd) {
        aSelectionEnd += delta;
      } else if ((uint32_t)aSelectionEnd > aStart) {
        aSelectionEnd = newEnd;
      }
    }
    break;
    default:
      MOZ_CRASH("Unknown mode!");
  }

  Optional<nsAString> direction;
  SetSelectionRange(aSelectionStart, aSelectionEnd, direction, aRv);
}

Nullable<int32_t>
HTMLInputElement::GetSelectionStart(ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    return Nullable<int32_t>();
  }

  int32_t selStart;
  nsresult rv = GetSelectionStart(&selStart);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return Nullable<int32_t>(selStart);
}

NS_IMETHODIMP
HTMLInputElement::GetSelectionStart(int32_t* aSelectionStart)
{
  NS_ENSURE_ARG_POINTER(aSelectionStart);

  int32_t selEnd, selStart;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv)) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      *aSelectionStart = state->GetSelectionProperties().GetStart();
      return NS_OK;
    }
    return rv;
  }

  *aSelectionStart = selStart;
  return NS_OK;
}

void
HTMLInputElement::SetSelectionStart(const Nullable<int32_t>& aSelectionStart,
                                    ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  int32_t selStart = 0;
  if (!aSelectionStart.IsNull()) {
    selStart = aSelectionStart.Value();
  }

  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().SetStart(selStart);
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

  start = selStart;
  if (end < start) {
    end = start;
  }

  aRv = SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionStart(int32_t aSelectionStart)
{
  ErrorResult rv;
  Nullable<int32_t> selStart(aSelectionStart);
  SetSelectionStart(selStart, rv);
  return rv.StealNSResult();
}

Nullable<int32_t>
HTMLInputElement::GetSelectionEnd(ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    return Nullable<int32_t>();
  }

  int32_t selEnd;
  nsresult rv = GetSelectionEnd(&selEnd);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }

  return Nullable<int32_t>(selEnd);
}

NS_IMETHODIMP
HTMLInputElement::GetSelectionEnd(int32_t* aSelectionEnd)
{
  NS_ENSURE_ARG_POINTER(aSelectionEnd);

  int32_t selEnd, selStart;
  nsresult rv = GetSelectionRange(&selStart, &selEnd);

  if (NS_FAILED(rv)) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      *aSelectionEnd = state->GetSelectionProperties().GetEnd();
      return NS_OK;
    }
    return rv;
  }

  *aSelectionEnd = selEnd;
  return NS_OK;
}

void
HTMLInputElement::SetSelectionEnd(const Nullable<int32_t>& aSelectionEnd,
                                  ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  int32_t selEnd = 0;
  if (!aSelectionEnd.IsNull()) {
    selEnd = aSelectionEnd.Value();
  }

  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    state->GetSelectionProperties().SetEnd(selEnd);
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

  end = selEnd;
  if (start > end) {
    start = end;
  }

  aRv = SetSelectionRange(start, end, direction);
}

NS_IMETHODIMP
HTMLInputElement::SetSelectionEnd(int32_t aSelectionEnd)
{
  ErrorResult rv;
  Nullable<int32_t> selEnd(aSelectionEnd);
  SetSelectionEnd(selEnd, rv);
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLInputElement::GetFiles(nsIDOMFileList** aFileList)
{
  RefPtr<FileList> list = GetFiles();
  list.forget(aFileList);
  return NS_OK;
}

nsresult
HTMLInputElement::GetSelectionRange(int32_t* aSelectionStart,
                                    int32_t* aSelectionEnd)
{
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
    return textControlFrame->GetSelectionRange(aSelectionStart, aSelectionEnd);
  }

  return NS_ERROR_FAILURE;
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
  if (!SupportsTextSelection()) {
    aDirection.SetIsVoid(true);
    return;
  }

  nsresult rv = NS_ERROR_FAILURE;
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
    nsITextControlFrame::SelectionDirection dir;
    rv = textControlFrame->GetSelectionRange(nullptr, nullptr, &dir);
    if (NS_SUCCEEDED(rv)) {
      DirectionToName(dir, aDirection);
    }
  }

  if (NS_FAILED(rv)) {
    nsTextEditorState* state = GetEditorState();
    if (state && state->IsSelectionCached()) {
      DirectionToName(state->GetSelectionProperties().GetDirection(), aDirection);
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
  return rv.StealNSResult();
}

void
HTMLInputElement::SetSelectionDirection(const nsAString& aDirection, ErrorResult& aRv)
{
  if (!SupportsTextSelection()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  nsTextEditorState* state = GetEditorState();
  if (state && state->IsSelectionCached()) {
    nsITextControlFrame::SelectionDirection dir = nsITextControlFrame::eNone;
    if (aDirection.EqualsLiteral("forward")) {
      dir = nsITextControlFrame::eForward;
    } else if (aDirection.EqualsLiteral("backward")) {
      dir = nsITextControlFrame::eBackward;
    }
    state->GetSelectionProperties().SetDirection(dir);
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
  return rv.StealNSResult();
}

NS_IMETHODIMP
HTMLInputElement::GetPhonetic(nsAString& aPhonetic)
{
  aPhonetic.Truncate();
  nsIFormControlFrame* formControlFrame = GetFormControlFrame(true);
  nsITextControlFrame* textControlFrame = do_QueryFrame(formControlFrame);
  if (textControlFrame) {
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
  nsCOMPtr<mozilla::dom::Element> element = do_QueryInterface(aTarget);
  RefPtr<Event> event = NS_NewDOMEvent(element, aPresContext, nullptr);
  event->InitEvent(aEventType, true, true);
  event->SetTrusted(true);

  EventDispatcher::DispatchDOMEvent(aTarget, nullptr, event, aPresContext,
                                    nullptr);

  return NS_OK;
}
#endif

void
HTMLInputElement::UpdateApzAwareFlag()
{
#if defined(XP_WIN) || defined(XP_LINUX)
  if ((mType == NS_FORM_INPUT_NUMBER) || (mType == NS_FORM_INPUT_RANGE)) {
    SetMayBeApzAware();
  }
#endif
}

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
  return SetValueInternal(resetVal, nsTextEditorState::eSetValue_Internal);
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
  mLastValueChangeWasInteractive = false;

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
HTMLInputElement::SubmitNamesValues(HTMLFormSubmission* aFormSubmission)
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

    const nsTArray<OwningFileOrDirectory>& files =
      GetFilesOrDirectoriesInternal();

    if (files.IsEmpty()) {
      aFormSubmission->AddNameBlobOrNullPair(name, nullptr);
      return NS_OK;
    }

    for (uint32_t i = 0; i < files.Length(); ++i) {
      if (files[i].IsFile()) {
        aFormSubmission->AddNameBlobOrNullPair(name, files[i].GetAsFile());
      } else {
        MOZ_ASSERT(files[i].IsDirectory());
        aFormSubmission->AddNameDirectoryPair(name, files[i].GetAsDirectory());
      }
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
  RefPtr<HTMLInputElementState> inputState;
  switch (GetValueMode()) {
    case VALUE_MODE_DEFAULT_ON:
      if (mCheckedChanged) {
        inputState = new HTMLInputElementState();
        inputState->SetChecked(mChecked);
      }
      break;
    case VALUE_MODE_FILENAME:
      if (!mFilesOrDirectories.IsEmpty()) {
        inputState = new HTMLInputElementState();
        inputState->SetFilesOrDirectories(mFilesOrDirectories);
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
      nsresult rv = GetValue(value);
      if (NS_FAILED(rv)) {
        return rv;
      }

      if (!IsSingleLineTextControl(false)) {
        rv = nsLinebreakConverter::ConvertStringLineBreaks(
               value,
               nsLinebreakConverter::eLinebreakPlatform,
               nsLinebreakConverter::eLinebreakContent);

        if (NS_FAILED(rv)) {
          NS_ERROR("Converting linebreaks failed!");
          return rv;
        }
      }

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
    // TODO: What should we do if SetValueInternal fails?  (The allocation
    // may potentially be big, but most likely we've failed to allocate
    // before the type change.)
    SetValueInternal(aValue, nsTextEditorState::eSetValue_Internal);
  }

  mShouldInitChecked = false;
}

EventStates
HTMLInputElement::IntrinsicState() const
{
  // If you add states here, and they're type-dependent, you need to add them
  // to the type case in AfterSetAttr.

  EventStates state = nsGenericHTMLFormElementWithState::IntrinsicState();
  if (mType == NS_FORM_INPUT_CHECKBOX || mType == NS_FORM_INPUT_RADIO) {
    // Check current checked state (:checked)
    if (mChecked) {
      state |= NS_EVENT_STATE_CHECKED;
    }

    // Check current indeterminate state (:indeterminate)
    if (mType == NS_FORM_INPUT_CHECKBOX && mIndeterminate) {
      state |= NS_EVENT_STATE_INDETERMINATE;
    }

    if (mType == NS_FORM_INPUT_RADIO) {
      nsCOMPtr<nsIDOMHTMLInputElement> selected = GetSelectedRadioButton();
      bool indeterminate = !selected && !mChecked;

      if (indeterminate) {
        state |= NS_EVENT_STATE_INDETERMINATE;
      }
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

    // :in-range and :out-of-range only apply if the element currently has a range
    if (mHasRange) {
      state |= (GetValidityState(VALIDITY_STATE_RANGE_OVERFLOW) ||
                GetValidityState(VALIDITY_STATE_RANGE_UNDERFLOW))
                 ? NS_EVENT_STATE_OUTOFRANGE
                 : NS_EVENT_STATE_INRANGE;
    }
  }

  if (PlaceholderApplies() &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder) &&
      IsValueEmpty()) {
    state |= NS_EVENT_STATE_PLACEHOLDERSHOWN;
  }

  if (mForm && !mForm->GetValidity() && IsSubmitControl()) {
    state |= NS_EVENT_STATE_MOZ_SUBMITINVALID;
  }

  return state;
}

void
HTMLInputElement::AddStates(EventStates aStates)
{
  if (mType == NS_FORM_INPUT_TEXT) {
    EventStates focusStates(aStates & (NS_EVENT_STATE_FOCUS |
                                       NS_EVENT_STATE_FOCUSRING));
    if (!focusStates.IsEmpty()) {
      HTMLInputElement* ownerNumberControl = GetOwnerNumberControl();
      if (ownerNumberControl) {
        ownerNumberControl->AddStates(focusStates);
      }
    }
  }
  nsGenericHTMLFormElementWithState::AddStates(aStates);
}

void
HTMLInputElement::RemoveStates(EventStates aStates)
{
  if (mType == NS_FORM_INPUT_TEXT) {
    EventStates focusStates(aStates & (NS_EVENT_STATE_FOCUS |
                                       NS_EVENT_STATE_FOCUSRING));
    if (!focusStates.IsEmpty()) {
      HTMLInputElement* ownerNumberControl = GetOwnerNumberControl();
      if (ownerNumberControl) {
        ownerNumberControl->RemoveStates(focusStates);
      }
    }
  }
  nsGenericHTMLFormElementWithState::RemoveStates(aStates);
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
          nsPIDOMWindowInner* window = OwnerDoc()->GetInnerWindow();
          if (window) {
            nsTArray<OwningFileOrDirectory> array;
            inputState->GetFilesOrDirectories(window, array);

            SetFilesOrDirectories(array, true);
          }
        }
        break;
      case VALUE_MODE_VALUE:
      case VALUE_MODE_DEFAULT:
        if (GetValueMode() == VALUE_MODE_DEFAULT &&
            mType != NS_FORM_INPUT_HIDDEN) {
          break;
        }

        // TODO: What should we do if SetValueInternal fails?  (The allocation
        // may potentially be big, but most likely we've failed to allocate
        // before the type change.)
        SetValueInternal(inputState->GetValue(),
                         nsTextEditorState::eSetValue_Notify);
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
  if (!mForm && !IsInUncomposedDoc()) {
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

    nsCOMPtr<nsIRadioVisitor> visitor = new nsRadioUpdateStateVisitor(this);
    VisitGroup(visitor, true);
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

  if (mType == NS_FORM_INPUT_FILE ||
      mType == NS_FORM_INPUT_NUMBER) {
    if (aTabIndex) {
      // We only want our native anonymous child to be tabable to, not ourself.
      *aTabIndex = -1;
    }
    if (mType == NS_FORM_INPUT_NUMBER) {
      *aIsFocusable = true;
    } else {
      *aIsFocusable = defaultFocusable;
    }
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

  if (container->GetCurrentRadioButton(name)) {
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
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
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
  return !IsDisabled() &&
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
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
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
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
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
  if (IsDateTimeInputType(mType)) {
    return false;
  }

  return IsSingleLineTextControl(false);
}

bool
HTMLInputElement::DoesPatternApply() const
{
  // TODO: temporary until bug 773205 is fixed.
  if (IsExperimentalMobileType(mType) || IsDateTimeInputType(mType)) {
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
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
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

bool
HTMLInputElement::DoesAutocompleteApply() const
{
  switch (mType)
  {
    case NS_FORM_INPUT_HIDDEN:
    case NS_FORM_INPUT_TEXT:
    case NS_FORM_INPUT_SEARCH:
    case NS_FORM_INPUT_URL:
    case NS_FORM_INPUT_TEL:
    case NS_FORM_INPUT_EMAIL:
    case NS_FORM_INPUT_PASSWORD:
    case NS_FORM_INPUT_DATE:
    case NS_FORM_INPUT_TIME:
    case NS_FORM_INPUT_NUMBER:
    case NS_FORM_INPUT_RANGE:
    case NS_FORM_INPUT_COLOR:
    case NS_FORM_INPUT_MONTH:
    case NS_FORM_INPUT_WEEK:
      return true;
#ifdef DEBUG
    case NS_FORM_INPUT_RESET:
    case NS_FORM_INPUT_SUBMIT:
    case NS_FORM_INPUT_IMAGE:
    case NS_FORM_INPUT_BUTTON:
    case NS_FORM_INPUT_RADIO:
    case NS_FORM_INPUT_CHECKBOX:
    case NS_FORM_INPUT_FILE:
      return false;
    default:
      NS_NOTYETIMPLEMENTED("Unexpected input type in DoesAutocompleteApply()");
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
  if (!step.isFinite() || step <= Decimal(0)) {
    step = GetDefaultStep();
  }

  // For input type=date, we round the step value to have a rounded day.
  if (mType == NS_FORM_INPUT_DATE || mType == NS_FORM_INPUT_MONTH) {
    step = std::max(step.round(), Decimal(1));
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
  if (!mValueChanged ||
      !mLastValueChangeWasInteractive ||
      !MinOrMaxLengthApplies() ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::maxlength)) {
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
HTMLInputElement::IsTooShort()
{
  if (!mValueChanged ||
      !mLastValueChangeWasInteractive ||
      !MinOrMaxLengthApplies() ||
      !HasAttr(kNameSpaceID_None, nsGkAtoms::minlength)) {
    return false;
  }

  int32_t minLength = MinLength();

  // Minlength of -1 means parsing error.
  if (minLength == -1) {
    return false;
  }

  int32_t textLength = -1;
  GetTextLength(&textLength);

  return textLength && textLength < minLength;
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
      return GetFilesOrDirectoriesInternal().IsEmpty();

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
  // TODO: this is temporary until bug 888316 is fixed.
  if (!DoesMinMaxApply() || mType == NS_FORM_INPUT_WEEK) {
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
  // TODO: this is temporary until bug 888316 is fixed.
  if (!DoesMinMaxApply() || mType == NS_FORM_INPUT_WEEK) {
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
HTMLInputElement::HasStepMismatch(bool aUseZeroIfValueNaN) const
{
  if (!DoesStepApply()) {
    return false;
  }

  Decimal value = GetValueAsDecimal();
  if (value.isNaN()) {
    if (aUseZeroIfValueNaN) {
      value = Decimal(0);
    } else {
      // The element can't suffer from step mismatch if it's value isn't a number.
      return false;
    }
  }

  Decimal step = GetStep();
  if (step == kStepAny) {
    return false;
  }

  // Value has to be an integral multiple of step.
  return NS_floorModulo(value - GetStepBase(), step) != Decimal(0);
}

/**
 * Takes aEmail and attempts to convert everything after the first "@"
 * character (if anything) to punycode before returning the complete result via
 * the aEncodedEmail out-param. The aIndexOfAt out-param is set to the index of
 * the "@" character.
 *
 * If no "@" is found in aEmail, aEncodedEmail is simply set to aEmail and
 * the aIndexOfAt out-param is set to kNotFound.
 *
 * Returns true in all cases unless an attempt to punycode encode fails. If
 * false is returned, aEncodedEmail has not been set.
 *
 * This function exists because ConvertUTF8toACE() splits on ".", meaning that
 * for 'user.name@sld.tld' it would treat "name@sld" as a label. We want to
 * encode the domain part only.
 */
static bool PunycodeEncodeEmailAddress(const nsAString& aEmail,
                                       nsAutoCString& aEncodedEmail,
                                       uint32_t* aIndexOfAt)
{
  nsAutoCString value = NS_ConvertUTF16toUTF8(aEmail);
  *aIndexOfAt = (uint32_t)value.FindChar('@');

  if (*aIndexOfAt == (uint32_t)kNotFound ||
      *aIndexOfAt == value.Length() - 1) {
    aEncodedEmail = value;
    return true;
  }

  nsCOMPtr<nsIIDNService> idnSrv = do_GetService(NS_IDNSERVICE_CONTRACTID);
  if (!idnSrv) {
    NS_ERROR("nsIIDNService isn't present!");
    return false;
  }

  uint32_t indexOfDomain = *aIndexOfAt + 1;

  const nsDependentCSubstring domain = Substring(value, indexOfDomain);
  bool ace;
  if (NS_SUCCEEDED(idnSrv->IsACE(domain, &ace)) && !ace) {
    nsAutoCString domainACE;
    if (NS_FAILED(idnSrv->ConvertUTF8toACE(domain, domainACE))) {
      return false;
    }
    value.Replace(indexOfDomain, domain.Length(), domainACE);
  }

  aEncodedEmail = value;
  return true;
}

bool
HTMLInputElement::HasBadInput() const
{
  if (mType == NS_FORM_INPUT_NUMBER) {
    nsAutoString value;
    GetValueInternal(value);
    if (!value.IsEmpty()) {
      // The input can't be bad, otherwise it would have been sanitized to the
      // empty string.
      NS_ASSERTION(!GetValueAsDecimal().isNaN(), "Should have sanitized");
      return false;
    }
    nsNumberControlFrame* numberControlFrame =
      do_QueryFrame(GetPrimaryFrame());
    if (numberControlFrame &&
        !numberControlFrame->AnonTextControlIsEmpty()) {
      // The input the user entered failed to parse as a number.
      return true;
    }
    return false;
  }
  if (mType == NS_FORM_INPUT_EMAIL) {
    // With regards to suffering from bad input the spec says that only the
    // punycode conversion works, so we don't care whether the email address is
    // valid or not here. (If the email address is invalid then we will be
    // suffering from a type mismatch.)
    nsAutoString value;
    nsAutoCString unused;
    uint32_t unused2;
    NS_ENSURE_SUCCESS(GetValueInternal(value), false);
    HTMLSplitOnSpacesTokenizer tokenizer(value, ',');
    while (tokenizer.hasMoreTokens()) {
      if (!PunycodeEncodeEmailAddress(tokenizer.nextToken(), unused, &unused2)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

void
HTMLInputElement::UpdateTooLongValidityState()
{
  SetValidityState(VALIDITY_STATE_TOO_LONG, IsTooLong());
}

void
HTMLInputElement::UpdateTooShortValidityState()
{
  SetValidityState(VALIDITY_STATE_TOO_SHORT, IsTooShort());
}

void
HTMLInputElement::UpdateValueMissingValidityStateForRadio(bool aIgnoreSelf)
{
  bool notify = !mParserCreating;
  nsCOMPtr<nsIDOMHTMLInputElement> selection = GetSelectedRadioButton();

  aIgnoreSelf = aIgnoreSelf || !IsMutable();

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
HTMLInputElement::UpdateBadInputValidityState()
{
  SetValidityState(VALIDITY_STATE_BAD_INPUT, HasBadInput());
}

void
HTMLInputElement::UpdateAllValidityStates(bool aNotify)
{
  bool validBefore = IsValid();
  UpdateTooLongValidityState();
  UpdateTooShortValidityState();
  UpdateValueMissingValidityState();
  UpdateTypeMismatchValidityState();
  UpdatePatternMismatchValidityState();
  UpdateRangeOverflowValidityState();
  UpdateRangeUnderflowValidityState();
  UpdateStepMismatchValidityState();
  UpdateBadInputValidityState();

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

      const char16_t* params[] = { strMaxLength.get(), strTextLength.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "FormValidationTextTooLong",
                                                 params, message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_TOO_SHORT:
    {
      nsXPIDLString message;
      int32_t minLength = MinLength();
      int32_t textLength = -1;
      nsAutoString strMinLength;
      nsAutoString strTextLength;

      GetTextLength(&textLength);

      strMinLength.AppendInt(minLength);
      strTextLength.AppendInt(textLength);

      const char16_t* params[] = { strMinLength.get(), strTextLength.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "FormValidationTextTooShort",
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
          key.AssignLiteral("FormValidationFileMissing");
          break;
        case NS_FORM_INPUT_CHECKBOX:
          key.AssignLiteral("FormValidationCheckboxMissing");
          break;
        case NS_FORM_INPUT_RADIO:
          key.AssignLiteral("FormValidationRadioMissing");
          break;
        case NS_FORM_INPUT_NUMBER:
          key.AssignLiteral("FormValidationBadInputNumber");
          break;
        default:
          key.AssignLiteral("FormValidationValueMissing");
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
        const char16_t* params[] = { title.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationPatternMismatchWithTitle",
                                                   params, message);
      }
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_RANGE_OVERFLOW:
    {
      static const char kNumberOverTemplate[] = "FormValidationNumberRangeOverflow";
      static const char kDateOverTemplate[] = "FormValidationDateRangeOverflow";
      static const char kTimeOverTemplate[] = "FormValidationTimeRangeOverflow";

      const char* msgTemplate;
      nsXPIDLString message;

      nsAutoString maxStr;
      if (mType == NS_FORM_INPUT_NUMBER ||
          mType == NS_FORM_INPUT_RANGE) {
        msgTemplate = kNumberOverTemplate;

        //We want to show the value as parsed when it's a number
        Decimal maximum = GetMaximum();
        MOZ_ASSERT(!maximum.isNaN());

        char buf[32];
        DebugOnly<bool> ok = maximum.toString(buf, ArrayLength(buf));
        maxStr.AssignASCII(buf);
        MOZ_ASSERT(ok, "buf not big enough");
      } else if (IsDateTimeInputType(mType)) {
        msgTemplate = mType == NS_FORM_INPUT_TIME ? kTimeOverTemplate : kDateOverTemplate;
        GetAttr(kNameSpaceID_None, nsGkAtoms::max, maxStr);
      } else {
        msgTemplate = kNumberOverTemplate;
        NS_NOTREACHED("Unexpected input type");
      }

      const char16_t* params[] = { maxStr.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 msgTemplate,
                                                 params, message);
      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_RANGE_UNDERFLOW:
    {
      static const char kNumberUnderTemplate[] = "FormValidationNumberRangeUnderflow";
      static const char kDateUnderTemplate[] = "FormValidationDateRangeUnderflow";
      static const char kTimeUnderTemplate[] = "FormValidationTimeRangeUnderflow";

      const char* msgTemplate;
      nsXPIDLString message;

      nsAutoString minStr;
      if (mType == NS_FORM_INPUT_NUMBER ||
          mType == NS_FORM_INPUT_RANGE) {
        msgTemplate = kNumberUnderTemplate;

        Decimal minimum = GetMinimum();
        MOZ_ASSERT(!minimum.isNaN());

        char buf[32];
        DebugOnly<bool> ok = minimum.toString(buf, ArrayLength(buf));
        minStr.AssignASCII(buf);
        MOZ_ASSERT(ok, "buf not big enough");
      } else if (IsDateTimeInputType(mType)) {
        msgTemplate = mType == NS_FORM_INPUT_TIME ? kTimeUnderTemplate : kDateUnderTemplate;
        GetAttr(kNameSpaceID_None, nsGkAtoms::min, minStr);
      } else {
        msgTemplate = kNumberUnderTemplate;
        NS_NOTREACHED("Unexpected input type");
      }

      const char16_t* params[] = { minStr.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 msgTemplate,
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
      MOZ_ASSERT(step != kStepAny && step > Decimal(0));

      Decimal stepBase = GetStepBase();

      Decimal valueLow = value - NS_floorModulo(value - stepBase, step);
      Decimal valueHigh = value + step - NS_floorModulo(value - stepBase, step);

      Decimal maximum = GetMaximum();

      if (maximum.isNaN() || valueHigh <= maximum) {
        nsAutoString valueLowStr, valueHighStr;
        ConvertNumberToString(valueLow, valueLowStr);
        ConvertNumberToString(valueHigh, valueHighStr);

        if (valueLowStr.Equals(valueHighStr)) {
          const char16_t* params[] = { valueLowStr.get() };
          rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                     "FormValidationStepMismatchOneValue",
                                                     params, message);
        } else {
          const char16_t* params[] = { valueLowStr.get(), valueHighStr.get() };
          rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                     "FormValidationStepMismatch",
                                                     params, message);
        }
      } else {
        nsAutoString valueLowStr;
        ConvertNumberToString(valueLow, valueLowStr);

        const char16_t* params[] = { valueLowStr.get() };
        rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                   "FormValidationStepMismatchOneValue",
                                                   params, message);
      }

      aValidationMessage = message;
      break;
    }
    case VALIDITY_STATE_BAD_INPUT:
    {
      nsXPIDLString message;
      nsAutoCString key;
      if (mType == NS_FORM_INPUT_NUMBER) {
        key.AssignLiteral("FormValidationBadInputNumber");
      } else if (mType == NS_FORM_INPUT_EMAIL) {
        key.AssignLiteral("FormValidationInvalidEmail");
      } else {
        return NS_ERROR_UNEXPECTED;
      }
      rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                              key.get(), message);
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

  return !tokenizer.separatorAfterCurrentToken();
}

//static
bool
HTMLInputElement::IsValidEmailAddress(const nsAString& aValue)
{
  // Email addresses can't be empty and can't end with a '.' or '-'.
  if (aValue.IsEmpty() || aValue.Last() == '.' || aValue.Last() == '-') {
    return false;
  }

  uint32_t atPos;
  nsAutoCString value;
  if (!PunycodeEncodeEmailAddress(aValue, value, &atPos) ||
      atPos == (uint32_t)kNotFound || atPos == 0 || atPos == value.Length() - 1) {
    // Could not encode, or "@" was not found, or it was at the start or end
    // of the input - in all cases, not a valid email address.
    return false;
  }

  uint32_t length = value.Length();
  uint32_t i = 0;

  // Parsing the username.
  for (; i < atPos; ++i) {
    char16_t c = value[i];

    // The username characters have to be in this list to be valid.
    if (!(nsCRT::IsAsciiAlpha(c) || nsCRT::IsAsciiDigit(c) ||
          c == '.' || c == '!' || c == '#' || c == '$' || c == '%' ||
          c == '&' || c == '\''|| c == '*' || c == '+' || c == '-' ||
          c == '/' || c == '=' || c == '?' || c == '^' || c == '_' ||
          c == '`' || c == '{' || c == '|' || c == '}' || c == '~' )) {
      return false;
    }
  }

  // Skip the '@'.
  ++i;

  // The domain name can't begin with a dot or a dash.
  if (value[i] == '.' || value[i] == '-') {
    return false;
  }

  // Parsing the domain name.
  for (; i < length; ++i) {
    char16_t c = value[i];

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
  return 0; // only textarea's can have wrap cols
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
HTMLInputElement::OnValueChanged(bool aNotify, bool aWasInteractiveUserChange)
{
  mLastValueChangeWasInteractive = aWasInteractiveUserChange;

  UpdateAllValidityStates(aNotify);

  if (HasDirAuto()) {
    SetDirectionIfAuto(true, aNotify);
  }

  // :placeholder-shown pseudo-class may change when the value changes.
  // However, we don't want to waste cycles if the state doesn't apply.
  if (PlaceholderApplies() &&
      HasAttr(kNameSpaceID_None, nsGkAtoms::placeholder)) {
    UpdateState(aNotify);
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

  bool allMimeTypeFiltersAreValid = true;
  bool atLeastOneFileExtensionFilter = false;

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
      filterBundle->GetStringFromName(u"imageFilter",
                                      getter_Copies(extensionListStr));
    } else if (token.EqualsLiteral("audio/*")) {
      filterMask = nsIFilePicker::filterAudio;
      filterBundle->GetStringFromName(u"audioFilter",
                                      getter_Copies(extensionListStr));
    } else if (token.EqualsLiteral("video/*")) {
      filterMask = nsIFilePicker::filterVideo;
      filterBundle->GetStringFromName(u"videoFilter",
                                      getter_Copies(extensionListStr));
    } else if (token.First() == '.') {
      if (token.Contains(';') || token.Contains('*')) {
        // Ignore this filter as it contains reserved characters
        continue;
      }
      extensionListStr = NS_LITERAL_STRING("*") + token;
      filterName = extensionListStr;
      atLeastOneFileExtensionFilter = true;
    } else {
      //... if no image/audio/video filter is found, check mime types filters
      nsCOMPtr<nsIMIMEInfo> mimeInfo;
      if (NS_FAILED(mimeService->GetFromTypeAndExtension(
                      NS_ConvertUTF16toUTF8(token),
                      EmptyCString(), // No extension
                      getter_AddRefs(mimeInfo))) ||
          !mimeInfo) {
        allMimeTypeFiltersAreValid =  false;
        continue;
      }

      // Get a name for the filter: first try the description, then the mime type
      // name if there is no description
      mimeInfo->GetDescription(filterName);
      if (filterName.IsEmpty()) {
        nsCString mimeTypeName;
        mimeInfo->GetType(mimeTypeName);
        CopyUTF8toUTF16(mimeTypeName, filterName);
      }

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
      allMimeTypeFiltersAreValid = false;
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

  // Remove similar filters
  // Iterate over a copy, as we might modify the original filters list
  nsTArray<nsFilePickerFilter> filtersCopy;
  filtersCopy = filters;
  for (uint32_t i = 0; i < filtersCopy.Length(); ++i) {
    const nsFilePickerFilter& filterToCheck = filtersCopy[i];
    if (filterToCheck.mFilterMask) {
      continue;
    }
    for (uint32_t j = 0; j < filtersCopy.Length(); ++j) {
      if (i == j) {
        continue;
      }
      // Check if this filter's extension list is a substring of the other one.
      // e.g. if filters are "*.jpeg" and "*.jpeg; *.jpg" the first one should
      // be removed.
      // Add an extra "; " to be sure the check will work and avoid cases like
      // "*.xls" being a subtring of "*.xslx" while those are two differents
      // filters and none should be removed.
      if (FindInReadable(filterToCheck.mFilter + NS_LITERAL_STRING(";"),
                         filtersCopy[j].mFilter + NS_LITERAL_STRING(";"))) {
        // We already have a similar, less restrictive filter (i.e.
        // filterToCheck extensionList is just a subset of another filter
        // extension list): remove this one
        filters.RemoveElement(filterToCheck);
      }
    }
  }

  // Add "All Supported Types" filter
  if (filters.Length() > 1) {
    nsXPIDLString title;
    nsContentUtils::GetLocalizedString(nsContentUtils::eFORMS_PROPERTIES,
                                       "AllSupportedTypes", title);
    filePicker->AppendFilter(title, allExtensionsList);
  }

  // Add each filter
  for (uint32_t i = 0; i < filters.Length(); ++i) {
    const nsFilePickerFilter& filter = filters[i];
    if (filter.mFilterMask) {
      filePicker->AppendFilters(filter.mFilterMask);
    } else {
      filePicker->AppendFilter(filter.mTitle, filter.mFilter);
    }
  }

  if (filters.Length() >= 1 &&
      (allMimeTypeFiltersAreValid || atLeastOneFileExtensionFilter)) {
    // |filterAll| will always use index=0 so we need to set index=1 as the
    // current filter.
    filePicker->SetFilterIndex(1);
  }
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
    case NS_FORM_INPUT_MONTH:
      return kStepScaleFactorMonth;
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
    case NS_FORM_INPUT_MONTH:
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

  // TODO: this is temporary until bug 888316 is fixed.
  if (!DoesMinMaxApply() || mType == NS_FORM_INPUT_WEEK) {
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

void
HTMLInputElement::PickerClosed()
{
  mPickerRunning = false;
}

JSObject*
HTMLInputElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return HTMLInputElementBinding::Wrap(aCx, this, aGivenProto);
}

void
HTMLInputElement::ClearGetFilesHelpers()
{
  if (mGetFilesRecursiveHelper) {
    mGetFilesRecursiveHelper->Unlink();
    mGetFilesRecursiveHelper = nullptr;
  }

  if (mGetFilesNonRecursiveHelper) {
    mGetFilesNonRecursiveHelper->Unlink();
    mGetFilesNonRecursiveHelper = nullptr;
  }
}

GetFilesHelper*
HTMLInputElement::GetOrCreateGetFilesHelper(bool aRecursiveFlag,
                                            ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (aRecursiveFlag) {
    if (!mGetFilesRecursiveHelper) {
      mGetFilesRecursiveHelper =
       GetFilesHelper::Create(global,
                              GetFilesOrDirectoriesInternal(),
                              aRecursiveFlag, aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }
    }

    return mGetFilesRecursiveHelper;
  }

  if (!mGetFilesNonRecursiveHelper) {
    mGetFilesNonRecursiveHelper =
     GetFilesHelper::Create(global,
                            GetFilesOrDirectoriesInternal(),
                            aRecursiveFlag, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return mGetFilesNonRecursiveHelper;
}

void
HTMLInputElement::UpdateEntries(const nsTArray<OwningFileOrDirectory>& aFilesOrDirectories)
{
  MOZ_ASSERT(mEntries.IsEmpty());

  nsCOMPtr<nsIGlobalObject> global = OwnerDoc()->GetScopeObject();
  MOZ_ASSERT(global);

  RefPtr<FileSystem> fs = FileSystem::Create(global);
  if (NS_WARN_IF(!fs)) {
    return;
  }

  Sequence<RefPtr<FileSystemEntry>> entries;
  for (uint32_t i = 0; i < aFilesOrDirectories.Length(); ++i) {
    RefPtr<FileSystemEntry> entry =
      FileSystemEntry::Create(global, aFilesOrDirectories[i], fs);
    MOZ_ASSERT(entry);

    if (!entries.AppendElement(entry, fallible)) {
      return;
    }
  }

  // The root fileSystem is a DirectoryEntry object that contains only the
  // dropped fileEntry and directoryEntry objects.
  fs->CreateRoot(entries);

  mEntries.SwapElements(entries);
}

void
HTMLInputElement::GetWebkitEntries(nsTArray<RefPtr<FileSystemEntry>>& aSequence)
{
  Telemetry::Accumulate(Telemetry::BLINK_FILESYSTEM_USED, true);
  aSequence.AppendElements(mEntries);
}

} // namespace dom
} // namespace mozilla

#undef NS_ORIGINAL_CHECKED_VALUE
