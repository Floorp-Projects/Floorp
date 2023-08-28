/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "HTMLSelectEventListener.h"

#include "nsListControlFrame.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/MouseEvent.h"
#include "mozilla/Casting.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/HTMLSelectElement.h"
#include "mozilla/dom/HTMLOptionElement.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/ClearOnShutdown.h"

using namespace mozilla;
using namespace mozilla::dom;

static bool IsOptionInteractivelySelectable(HTMLSelectElement& aSelect,
                                            HTMLOptionElement& aOption,
                                            bool aIsCombobox) {
  if (aSelect.IsOptionDisabled(&aOption)) {
    return false;
  }
  if (!aIsCombobox) {
    return aOption.GetPrimaryFrame();
  }
  // In dropdown mode no options have frames, but we can check whether they
  // are rendered / not in a display: none subtree.
  if (!aOption.HasServoData() || Servo_Element_IsDisplayNone(&aOption)) {
    return false;
  }
  // TODO(emilio): This is a bit silly and doesn't match the options that we
  // show / don't show in the dropdown, but matches the frame construction we
  // do for multiple selects. For backwards compat also don't allow selecting
  // options in a display: contents subtree interactively.
  // test_select_key_navigation_bug1498769.html tests for this and should
  // probably be changed (and this loop removed) or alternatively
  // SelectChild.jsm should be changed to match it.
  for (Element* el = &aOption; el && el != &aSelect;
       el = el->GetParentElement()) {
    if (Servo_Element_IsDisplayContents(el)) {
      return false;
    }
  }
  return true;
}

namespace mozilla {

static StaticAutoPtr<nsString> sIncrementalString;
static TimeStamp gLastKeyTime;
static uintptr_t sLastKeyListener = 0;
static constexpr int32_t kNothingSelected = -1;

static nsString& GetIncrementalString() {
  MOZ_ASSERT(sLastKeyListener != 0);
  if (!sIncrementalString) {
    sIncrementalString = new nsString();
    ClearOnShutdown(&sIncrementalString);
  }
  return *sIncrementalString;
}

class MOZ_RAII AutoIncrementalSearchHandler {
 public:
  explicit AutoIncrementalSearchHandler(HTMLSelectEventListener& aListener) {
    if (sLastKeyListener != uintptr_t(&aListener)) {
      sLastKeyListener = uintptr_t(&aListener);
      GetIncrementalString().Truncate();
      // To make it easier to handle time comparisons in the other methods,
      // initialize gLastKeyTime to a value in the past.
      gLastKeyTime = TimeStamp::Now() -
                     TimeDuration::FromMilliseconds(
                         StaticPrefs::ui_menu_incremental_search_timeout() * 2);
    }
  }
  ~AutoIncrementalSearchHandler() {
    if (!mResettingCancelled) {
      GetIncrementalString().Truncate();
    }
  }
  void CancelResetting() { mResettingCancelled = true; }

 private:
  bool mResettingCancelled = false;
};

NS_IMPL_ISUPPORTS(HTMLSelectEventListener, nsIMutationObserver,
                  nsIDOMEventListener)

HTMLSelectEventListener::~HTMLSelectEventListener() {
  if (sLastKeyListener == uintptr_t(this)) {
    sLastKeyListener = 0;
  }
}

nsListControlFrame* HTMLSelectEventListener::GetListControlFrame() const {
  if (mIsCombobox) {
    MOZ_ASSERT(!mElement->GetPrimaryFrame() ||
               !mElement->GetPrimaryFrame()->IsListControlFrame());
    return nullptr;
  }
  return do_QueryFrame(mElement->GetPrimaryFrame());
}

int32_t HTMLSelectEventListener::GetEndSelectionIndex() const {
  if (auto* lf = GetListControlFrame()) {
    return lf->GetEndSelectionIndex();
  }
  // Combobox selects only have one selected index, so the end and start is the
  // same.
  return mElement->SelectedIndex();
}

bool HTMLSelectEventListener::IsOptionInteractivelySelectable(
    uint32_t aIndex) const {
  HTMLOptionElement* option = mElement->Item(aIndex);
  return option &&
         ::IsOptionInteractivelySelectable(*mElement, *option, mIsCombobox);
}

//---------------------------------------------------------------------
// Ok, the entire idea of this routine is to move to the next item that
// is suppose to be selected. If the item is disabled then we search in
// the same direction looking for the next item to select. If we run off
// the end of the list then we start at the end of the list and search
// backwards until we get back to the original item or an enabled option
//
// aStartIndex - the index to start searching from
// aNewIndex - will get set to the new index if it finds one
// aNumOptions - the total number of options in the list
// aDoAdjustInc - the initial index increment / decrement
// aDoAdjustIncNext - the subsequent index increment/decrement used to search
//                    for the next enabled option
//
// the aDoAdjustInc could be a "1" for a single item or
// any number greater representing a page of items
//
void HTMLSelectEventListener::AdjustIndexForDisabledOpt(
    int32_t aStartIndex, int32_t& aNewIndex, int32_t aNumOptions,
    int32_t aDoAdjustInc, int32_t aDoAdjustIncNext) {
  // Cannot select anything if there is nothing to select
  if (aNumOptions == 0) {
    aNewIndex = kNothingSelected;
    return;
  }

  // means we reached the end of the list and now we are searching backwards
  bool doingReverse = false;
  // lowest index in the search range
  int32_t bottom = 0;
  // highest index in the search range
  int32_t top = aNumOptions;

  // Start off keyboard options at selectedIndex if nothing else is defaulted to
  //
  // XXX Perhaps this should happen for mouse too, to start off shift click
  // automatically in multiple ... to do this, we'd need to override
  // OnOptionSelected and set mStartSelectedIndex if nothing is selected.  Not
  // sure of the effects, though, so I'm not doing it just yet.
  int32_t startIndex = aStartIndex;
  if (startIndex < bottom) {
    startIndex = mElement->SelectedIndex();
  }
  int32_t newIndex = startIndex + aDoAdjustInc;

  // make sure we start off in the range
  if (newIndex < bottom) {
    newIndex = 0;
  } else if (newIndex >= top) {
    newIndex = aNumOptions - 1;
  }

  while (true) {
    // if the newIndex is selectable, we are golden, bail out
    if (IsOptionInteractivelySelectable(newIndex)) {
      break;
    }

    // it WAS disabled, so sart looking ahead for the next enabled option
    newIndex += aDoAdjustIncNext;

    // well, if we reach end reverse the search
    if (newIndex < bottom) {
      if (doingReverse) {
        return;  // if we are in reverse mode and reach the end bail out
      }
      // reset the newIndex to the end of the list we hit
      // reverse the incrementer
      // set the other end of the list to our original starting index
      newIndex = bottom;
      aDoAdjustIncNext = 1;
      doingReverse = true;
      top = startIndex;
    } else if (newIndex >= top) {
      if (doingReverse) {
        return;  // if we are in reverse mode and reach the end bail out
      }
      // reset the newIndex to the end of the list we hit
      // reverse the incrementer
      // set the other end of the list to our original starting index
      newIndex = top - 1;
      aDoAdjustIncNext = -1;
      doingReverse = true;
      bottom = startIndex;
    }
  }

  // Looks like we found one
  aNewIndex = newIndex;
}

NS_IMETHODIMP
HTMLSelectEventListener::HandleEvent(dom::Event* aEvent) {
  nsAutoString eventType;
  aEvent->GetType(eventType);
  if (eventType.EqualsLiteral("keydown")) {
    return KeyDown(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return KeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    if (aEvent->DefaultPrevented()) {
      return NS_OK;
    }
    return MouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    // Don't try to honor defaultPrevented here - it's not web compatible.
    // (bug 1194733)
    return MouseUp(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove")) {
    // I don't think we want to honor defaultPrevented on mousemove
    // in general, and it would only prevent highlighting here.
    return MouseMove(aEvent);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected eventType");
  return NS_OK;
}

void HTMLSelectEventListener::Attach() {
  mElement->AddSystemEventListener(u"keydown"_ns, this, false, false);
  mElement->AddSystemEventListener(u"keypress"_ns, this, false, false);
  mElement->AddSystemEventListener(u"mousedown"_ns, this, false, false);
  mElement->AddSystemEventListener(u"mouseup"_ns, this, false, false);
  mElement->AddSystemEventListener(u"mousemove"_ns, this, false, false);

  if (mIsCombobox) {
    mElement->AddMutationObserver(this);
  }
}

void HTMLSelectEventListener::Detach() {
  mElement->RemoveSystemEventListener(u"keydown"_ns, this, false);
  mElement->RemoveSystemEventListener(u"keypress"_ns, this, false);
  mElement->RemoveSystemEventListener(u"mousedown"_ns, this, false);
  mElement->RemoveSystemEventListener(u"mouseup"_ns, this, false);
  mElement->RemoveSystemEventListener(u"mousemove"_ns, this, false);

  if (mIsCombobox) {
    mElement->RemoveMutationObserver(this);
    if (mElement->OpenInParentProcess()) {
      nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
          "HTMLSelectEventListener::Detach", [element = mElement] {
            // Don't hide the dropdown if the element has another frame already,
            // this prevents closing dropdowns on reframe, see bug 1440506.
            //
            // FIXME(emilio): The flush is needed to deal with reframes started
            // from DOM node removal. But perhaps we can be a bit smarter here.
            if (!element->IsCombobox() ||
                !element->GetPrimaryFrame(FlushType::Frames)) {
              nsContentUtils::DispatchChromeEvent(
                  element->OwnerDoc(), element, u"mozhidedropdown"_ns,
                  CanBubble::eYes, Cancelable::eNo);
            }
          }));
    }
  }
}

const uint32_t kMaxDropdownRows = 20;  // matches the setting for 4.x browsers

int32_t HTMLSelectEventListener::ItemsPerPage() const {
  uint32_t size = [&] {
    if (mIsCombobox) {
      return kMaxDropdownRows;
    }
    if (auto* lf = GetListControlFrame()) {
      return lf->GetNumDisplayRows();
    }
    return mElement->Size();
  }();
  if (size <= 1) {
    return 1;
  }
  if (MOZ_UNLIKELY(size > INT32_MAX)) {
    return INT32_MAX - 1;
  }
  return AssertedCast<int32_t>(size - 1u);
}

void HTMLSelectEventListener::OptionValueMightHaveChanged(
    nsIContent* aMutatingNode) {
#ifdef ACCESSIBILITY
  if (nsAccessibilityService* acc = GetAccService()) {
    acc->ComboboxOptionMaybeChanged(mElement->OwnerDoc()->GetPresShell(),
                                    aMutatingNode);
  }
#endif
}

void HTMLSelectEventListener::AttributeChanged(dom::Element* aElement,
                                               int32_t aNameSpaceID,
                                               nsAtom* aAttribute,
                                               int32_t aModType,
                                               const nsAttrValue* aOldValue) {
  if (aElement->IsHTMLElement(nsGkAtoms::option) &&
      aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::label) {
    // A11y has its own mutation listener for this so no need to do
    // OptionValueMightHaveChanged().
    ComboboxMightHaveChanged();
  }
}

void HTMLSelectEventListener::CharacterDataChanged(
    nsIContent* aContent, const CharacterDataChangeInfo&) {
  if (nsContentUtils::IsInSameAnonymousTree(mElement, aContent)) {
    OptionValueMightHaveChanged(aContent);
    ComboboxMightHaveChanged();
  }
}

void HTMLSelectEventListener::ContentRemoved(nsIContent* aChild,
                                             nsIContent* aPreviousSibling) {
  if (nsContentUtils::IsInSameAnonymousTree(mElement, aChild)) {
    OptionValueMightHaveChanged(aChild);
    ComboboxMightHaveChanged();
  }
}

void HTMLSelectEventListener::ContentAppended(nsIContent* aFirstNewContent) {
  if (nsContentUtils::IsInSameAnonymousTree(mElement, aFirstNewContent)) {
    OptionValueMightHaveChanged(aFirstNewContent);
    ComboboxMightHaveChanged();
  }
}

void HTMLSelectEventListener::ContentInserted(nsIContent* aChild) {
  if (nsContentUtils::IsInSameAnonymousTree(mElement, aChild)) {
    OptionValueMightHaveChanged(aChild);
    ComboboxMightHaveChanged();
  }
}

void HTMLSelectEventListener::ComboboxMightHaveChanged() {
  if (nsIFrame* f = mElement->GetPrimaryFrame()) {
    PresShell* ps = f->PresShell();
    // nsComoboxControlFrame::Reflow updates the selected text. AddOption /
    // RemoveOption / etc takes care of keeping the displayed index up to date.
    ps->FrameNeedsReflow(f, IntrinsicDirty::FrameAncestorsAndDescendants,
                         NS_FRAME_IS_DIRTY);
#ifdef ACCESSIBILITY
    if (nsAccessibilityService* acc = GetAccService()) {
      acc->ScheduleAccessibilitySubtreeUpdate(ps, mElement);
    }
#endif
  }
}

void HTMLSelectEventListener::FireOnInputAndOnChange() {
  RefPtr<HTMLSelectElement> element = mElement;
  // Dispatch the input event.
  DebugOnly<nsresult> rvIgnored = nsContentUtils::DispatchInputEvent(element);
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rvIgnored),
                       "Failed to dispatch input event");

  // Dispatch the change event.
  nsContentUtils::DispatchTrustedEvent(element->OwnerDoc(), element,
                                       u"change"_ns, CanBubble::eYes,
                                       Cancelable::eNo);
}

static void FireDropDownEvent(HTMLSelectElement* aElement, bool aShow,
                              bool aIsSourceTouchEvent) {
  const auto eventName = [&] {
    if (aShow) {
      return aIsSourceTouchEvent ? u"mozshowdropdown-sourcetouch"_ns
                                 : u"mozshowdropdown"_ns;
    }
    return u"mozhidedropdown"_ns;
  }();
  nsContentUtils::DispatchChromeEvent(aElement->OwnerDoc(), aElement, eventName,
                                      CanBubble::eYes, Cancelable::eNo);
}

nsresult HTMLSelectEventListener::MouseDown(dom::Event* aMouseEvent) {
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  if (mElement->State().HasState(ElementState::DISABLED)) {
    return NS_OK;
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  const bool isLeftButton = mouseEvent->Button() == 0;
  if (!isLeftButton) {
    return NS_OK;
  }

  if (mIsCombobox) {
    uint16_t inputSource = mouseEvent->InputSource();
    if (mElement->OpenInParentProcess()) {
      nsCOMPtr<nsIContent> target = do_QueryInterface(aMouseEvent->GetTarget());
      if (target && target->IsHTMLElement(nsGkAtoms::option)) {
        return NS_OK;
      }
    }

    const bool isSourceTouchEvent =
        inputSource == MouseEvent_Binding::MOZ_SOURCE_TOUCH;
    FireDropDownEvent(mElement, !mElement->OpenInParentProcess(),
                      isSourceTouchEvent);
    return NS_OK;
  }

  if (nsListControlFrame* list = GetListControlFrame()) {
    mButtonDown = true;
    return list->HandleLeftButtonMouseDown(aMouseEvent);
  }
  return NS_OK;
}

nsresult HTMLSelectEventListener::MouseUp(dom::Event* aMouseEvent) {
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  mButtonDown = false;

  if (mElement->State().HasState(ElementState::DISABLED)) {
    return NS_OK;
  }

  if (nsListControlFrame* lf = GetListControlFrame()) {
    lf->CaptureMouseEvents(false);
  }

  // only allow selection with the left button
  // if a right button click is on the combobox itself
  // or on the select when in listbox mode, then let the click through
  const bool isLeftButton = mouseEvent->Button() == 0;
  if (!isLeftButton) {
    return NS_OK;
  }

  if (nsListControlFrame* lf = GetListControlFrame()) {
    return lf->HandleLeftButtonMouseUp(aMouseEvent);
  }

  return NS_OK;
}

nsresult HTMLSelectEventListener::MouseMove(dom::Event* aMouseEvent) {
  NS_ASSERTION(aMouseEvent != nullptr, "aMouseEvent is null.");

  MouseEvent* mouseEvent = aMouseEvent->AsMouseEvent();
  NS_ENSURE_TRUE(mouseEvent, NS_ERROR_FAILURE);

  if (!mButtonDown) {
    return NS_OK;
  }

  if (nsListControlFrame* lf = GetListControlFrame()) {
    return lf->DragMove(aMouseEvent);
  }

  return NS_OK;
}

nsresult HTMLSelectEventListener::KeyPress(dom::Event* aKeyEvent) {
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  if (mElement->State().HasState(ElementState::DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchHandler incrementalHandler(*this);

  const WidgetKeyboardEvent* keyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
             "DOM event must have WidgetKeyboardEvent for its internal event");

  // Select option with this as the first character
  // XXX Not I18N compliant

  // Don't do incremental search if the key event has already consumed.
  if (keyEvent->DefaultPrevented()) {
    return NS_OK;
  }

  if (keyEvent->IsAlt()) {
    return NS_OK;
  }

  // With some keyboard layout, space key causes non-ASCII space.
  // So, the check in keydown event handler isn't enough, we need to check it
  // again with keypress event.
  if (keyEvent->mCharCode != ' ') {
    mControlSelectMode = false;
  }

  const bool isControlOrMeta =
      keyEvent->IsControl()
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
      // Ignore Windows Logo key press in Win/Linux because it's not a usual
      // modifier for applications.  Here wants to check "Accel" like modifier.
      || keyEvent->IsMeta()
#endif
      ;
  if (isControlOrMeta && keyEvent->mCharCode != ' ') {
    return NS_OK;
  }

  // NOTE: If mKeyCode of keypress event is not 0, mCharCode is always 0.
  //       Therefore, all non-printable keys are not handled after this block.
  if (!keyEvent->mCharCode) {
    // Backspace key will delete the last char in the string.  Otherwise,
    // non-printable keypress should reset incremental search.
    if (keyEvent->mKeyCode == NS_VK_BACK) {
      incrementalHandler.CancelResetting();
      if (!GetIncrementalString().IsEmpty()) {
        GetIncrementalString().Truncate(GetIncrementalString().Length() - 1);
      }
      aKeyEvent->PreventDefault();
    } else {
      // XXX When a select element has focus, even if the key causes nothing,
      //     it might be better to call preventDefault() here because nobody
      //     should expect one of other elements including chrome handles the
      //     key event.
    }
    return NS_OK;
  }

  incrementalHandler.CancelResetting();

  // We ate the key if we got this far.
  aKeyEvent->PreventDefault();

  // XXX Why don't we check/modify timestamp first?

  // Incremental Search: if time elapsed is below
  // ui.menu.incremental_search.timeout, append this keystroke to the search
  // string we will use to find options and start searching at the current
  // keystroke.  Otherwise, Truncate the string if it's been a long time
  // since our last keypress.
  if ((keyEvent->mTimeStamp - gLastKeyTime).ToMilliseconds() >
      StaticPrefs::ui_menu_incremental_search_timeout()) {
    // If this is ' ' and we are at the beginning of the string, treat it as
    // "select this option" (bug 191543)
    if (keyEvent->mCharCode == ' ') {
      // Actually process the new index and let the selection code
      // do the scrolling for us
      PostHandleKeyEvent(GetEndSelectionIndex(), keyEvent->mCharCode,
                         keyEvent->IsShift(), isControlOrMeta);

      return NS_OK;
    }

    GetIncrementalString().Truncate();
  }

  gLastKeyTime = keyEvent->mTimeStamp;

  // Append this keystroke to the search string.
  char16_t uniChar = ToLowerCase(static_cast<char16_t>(keyEvent->mCharCode));
  GetIncrementalString().Append(uniChar);

  // See bug 188199, if all letters in incremental string are same, just try to
  // match the first one
  nsAutoString incrementalString(GetIncrementalString());
  uint32_t charIndex = 1, stringLength = incrementalString.Length();
  while (charIndex < stringLength &&
         incrementalString[charIndex] == incrementalString[charIndex - 1]) {
    charIndex++;
  }
  if (charIndex == stringLength) {
    incrementalString.Truncate(1);
    stringLength = 1;
  }

  // Determine where we're going to start reading the string
  // If we have multiple characters to look for, we start looking *at* the
  // current option.  If we have only one character to look for, we start
  // looking *after* the current option.
  // Exception: if there is no option selected to start at, we always start
  // *at* 0.
  int32_t startIndex = mElement->SelectedIndex();
  if (startIndex == kNothingSelected) {
    startIndex = 0;
  } else if (stringLength == 1) {
    startIndex++;
  }

  // now make sure there are options or we are wasting our time
  RefPtr<dom::HTMLOptionsCollection> options = mElement->Options();
  uint32_t numOptions = options->Length();

  for (uint32_t i = 0; i < numOptions; ++i) {
    uint32_t index = (i + startIndex) % numOptions;
    RefPtr<dom::HTMLOptionElement> optionElement = options->ItemAsOption(index);
    if (!optionElement || !::IsOptionInteractivelySelectable(
                              *mElement, *optionElement, mIsCombobox)) {
      continue;
    }

    nsAutoString text;
    optionElement->GetRenderedLabel(text);
    if (!StringBeginsWith(
            nsContentUtils::TrimWhitespace<
                nsContentUtils::IsHTMLWhitespaceOrNBSP>(text, false),
            incrementalString, nsCaseInsensitiveStringComparator)) {
      continue;
    }

    if (mIsCombobox) {
      if (optionElement->Selected()) {
        return NS_OK;
      }
      optionElement->SetSelected(true);
      FireOnInputAndOnChange();
      return NS_OK;
    }

    if (nsListControlFrame* lf = GetListControlFrame()) {
      bool wasChanged =
          lf->PerformSelection(index, keyEvent->IsShift(), isControlOrMeta);
      if (!wasChanged) {
        return NS_OK;
      }
      FireOnInputAndOnChange();
    }
    break;
  }

  return NS_OK;
}

nsresult HTMLSelectEventListener::KeyDown(dom::Event* aKeyEvent) {
  MOZ_ASSERT(aKeyEvent, "aKeyEvent is null.");

  if (mElement->State().HasState(ElementState::DISABLED)) {
    return NS_OK;
  }

  AutoIncrementalSearchHandler incrementalHandler(*this);

  if (aKeyEvent->DefaultPrevented()) {
    return NS_OK;
  }

  const WidgetKeyboardEvent* keyEvent =
      aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
  MOZ_ASSERT(keyEvent,
             "DOM event must have WidgetKeyboardEvent for its internal event");

  bool dropDownMenuOnUpDown;
  bool dropDownMenuOnSpace;
#ifdef XP_MACOSX
  dropDownMenuOnUpDown = mIsCombobox && !mElement->OpenInParentProcess();
  dropDownMenuOnSpace = mIsCombobox && !keyEvent->IsAlt() &&
                        !keyEvent->IsControl() && !keyEvent->IsMeta();
#else
  dropDownMenuOnUpDown = mIsCombobox && keyEvent->IsAlt();
  dropDownMenuOnSpace = mIsCombobox && !mElement->OpenInParentProcess();
#endif
  bool withinIncrementalSearchTime =
      (keyEvent->mTimeStamp - gLastKeyTime).ToMilliseconds() <=
      StaticPrefs::ui_menu_incremental_search_timeout();
  if ((dropDownMenuOnUpDown &&
       (keyEvent->mKeyCode == NS_VK_UP || keyEvent->mKeyCode == NS_VK_DOWN)) ||
      (dropDownMenuOnSpace && keyEvent->mKeyCode == NS_VK_SPACE &&
       !withinIncrementalSearchTime)) {
    FireDropDownEvent(mElement, !mElement->OpenInParentProcess(), false);
    aKeyEvent->PreventDefault();
    return NS_OK;
  }
  if (keyEvent->IsAlt()) {
    return NS_OK;
  }

  // We should not change the selection if the popup is "opened in the parent
  // process" (even when we're in single-process mode).
  const bool shouldSelect = !mIsCombobox || !mElement->OpenInParentProcess();

  // now make sure there are options or we are wasting our time
  RefPtr<dom::HTMLOptionsCollection> options = mElement->Options();
  uint32_t numOptions = options->Length();

  // this is the new index to set
  int32_t newIndex = kNothingSelected;

  bool isControlOrMeta =
      keyEvent->IsControl()
#if !defined(XP_WIN) && !defined(MOZ_WIDGET_GTK)
      // Ignore Windows Logo key press in Win/Linux because it's not a usual
      // modifier for applications.  Here wants to check "Accel" like modifier.
      || keyEvent->IsMeta()
#endif
      ;
  // Don't try to handle multiple-select pgUp/pgDown in single-select lists.
  if (isControlOrMeta && !mElement->Multiple() &&
      (keyEvent->mKeyCode == NS_VK_PAGE_UP ||
       keyEvent->mKeyCode == NS_VK_PAGE_DOWN)) {
    return NS_OK;
  }
  if (isControlOrMeta &&
      (keyEvent->mKeyCode == NS_VK_UP || keyEvent->mKeyCode == NS_VK_LEFT ||
       keyEvent->mKeyCode == NS_VK_DOWN || keyEvent->mKeyCode == NS_VK_RIGHT ||
       keyEvent->mKeyCode == NS_VK_HOME || keyEvent->mKeyCode == NS_VK_END)) {
    // Don't go into multiple-select mode unless this list can handle it.
    isControlOrMeta = mControlSelectMode = mElement->Multiple();
  } else if (keyEvent->mKeyCode != NS_VK_SPACE) {
    mControlSelectMode = false;
  }

  switch (keyEvent->mKeyCode) {
    case NS_VK_UP:
    case NS_VK_LEFT:
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(GetEndSelectionIndex(), newIndex,
                                  int32_t(numOptions), -1, -1);
      }
      break;
    case NS_VK_DOWN:
    case NS_VK_RIGHT:
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(GetEndSelectionIndex(), newIndex,
                                  int32_t(numOptions), 1, 1);
      }
      break;
    case NS_VK_RETURN:
      // If this is single select listbox, Enter key doesn't cause anything.
      if (!mElement->Multiple()) {
        return NS_OK;
      }

      newIndex = GetEndSelectionIndex();
      break;
    case NS_VK_PAGE_UP: {
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(GetEndSelectionIndex(), newIndex,
                                  int32_t(numOptions), -ItemsPerPage(), -1);
      }
      break;
    }
    case NS_VK_PAGE_DOWN: {
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(GetEndSelectionIndex(), newIndex,
                                  int32_t(numOptions), ItemsPerPage(), 1);
      }
      break;
    }
    case NS_VK_HOME:
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(0, newIndex, int32_t(numOptions), 0, 1);
      }
      break;
    case NS_VK_END:
      if (shouldSelect) {
        AdjustIndexForDisabledOpt(int32_t(numOptions) - 1, newIndex,
                                  int32_t(numOptions), 0, -1);
      }
      break;
    default:  // printable key will be handled by keypress event.
      incrementalHandler.CancelResetting();
      return NS_OK;
  }

  aKeyEvent->PreventDefault();

  // Actually process the new index and let the selection code
  // do the scrolling for us
  PostHandleKeyEvent(newIndex, 0, keyEvent->IsShift(), isControlOrMeta);
  return NS_OK;
}

HTMLOptionElement* HTMLSelectEventListener::GetCurrentOption() const {
  // The mEndSelectionIndex is what is currently being selected. Use
  // the selected index if this is kNothingSelected.
  int32_t endIndex = GetEndSelectionIndex();
  int32_t focusedIndex =
      endIndex == kNothingSelected ? mElement->SelectedIndex() : endIndex;
  if (focusedIndex != kNothingSelected) {
    return mElement->Item(AssertedCast<uint32_t>(focusedIndex));
  }

  // There is no selected option. Return the first non-disabled option, if any.
  return GetNonDisabledOptionFrom(0);
}

HTMLOptionElement* HTMLSelectEventListener::GetNonDisabledOptionFrom(
    int32_t aFromIndex, int32_t* aFoundIndex) const {
  const uint32_t length = mElement->Length();
  for (uint32_t i = std::max(aFromIndex, 0); i < length; ++i) {
    if (IsOptionInteractivelySelectable(i)) {
      if (aFoundIndex) {
        *aFoundIndex = i;
      }
      return mElement->Item(i);
    }
  }
  return nullptr;
}

void HTMLSelectEventListener::PostHandleKeyEvent(int32_t aNewIndex,
                                                 uint32_t aCharCode,
                                                 bool aIsShift,
                                                 bool aIsControlOrMeta) {
  if (aNewIndex == kNothingSelected) {
    int32_t endIndex = GetEndSelectionIndex();
    int32_t focusedIndex =
        endIndex == kNothingSelected ? mElement->SelectedIndex() : endIndex;
    if (focusedIndex != kNothingSelected) {
      return;
    }
    // No options are selected.  In this case the focus ring is on the first
    // non-disabled option (if any), so we should behave as if that's the option
    // the user acted on.
    if (!GetNonDisabledOptionFrom(0, &aNewIndex)) {
      return;
    }
  }

  if (mIsCombobox) {
    RefPtr<HTMLOptionElement> newOption = mElement->Item(aNewIndex);
    MOZ_ASSERT(newOption);
    if (newOption->Selected()) {
      return;
    }
    newOption->SetSelected(true);
    FireOnInputAndOnChange();
    return;
  }
  if (nsListControlFrame* lf = GetListControlFrame()) {
    lf->UpdateSelectionAfterKeyEvent(aNewIndex, aCharCode, aIsShift,
                                     aIsControlOrMeta, mControlSelectMode);
  }
}

}  // namespace mozilla
