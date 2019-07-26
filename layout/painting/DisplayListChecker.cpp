/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DisplayListChecker.h"

#include "nsDisplayList.h"
#include "mozilla/StaticPrefs_layout.h"

namespace mozilla {

class DisplayItemBlueprint;

// Stack node used during tree visits, to store the path to a display item.
struct DisplayItemBlueprintStack {
  const DisplayItemBlueprintStack* mPrevious;
  const DisplayItemBlueprint* mItem;
  // Output stack to aSs, with format "name#index > ... > name#index".
  // Returns true if anything was output, false if empty.
  bool Output(std::stringstream& aSs) const;
};

// Object representing a list of display items (either the top of the tree, or
// an item's children), with just enough information to compare with another
// tree and output useful debugging information.
class DisplayListBlueprint {
 public:
  DisplayListBlueprint(nsDisplayList* aList, const char* aName)
      : DisplayListBlueprint(aList, 0, aName) {}

  DisplayListBlueprint(nsDisplayList* aList, const char* aName,
                       unsigned& aIndex) {
    processChildren(aList, aName, aIndex);
  }

  // Find a display item with the given frame and per-frame key.
  // Returns empty string if not found.
  std::string Find(const nsIFrame* aFrame, uint32_t aPerFrameKey) const {
    const DisplayItemBlueprintStack stack{nullptr, nullptr};
    return Find(aFrame, aPerFrameKey, stack);
  }

  std::string Find(const nsIFrame* aFrame, uint32_t aPerFrameKey,
                   const DisplayItemBlueprintStack& aStack) const;

  // Compare this list with another one, output differences between the two
  // into aDiff.
  // Differences include: Display items from one tree for which a corresponding
  // item (same frame and per-frame key) cannot be found under corresponding
  // parent items.
  // Returns true if trees are similar, false if different.
  bool CompareList(const DisplayListBlueprint& aOther,
                   std::stringstream& aDiff) const {
    const DisplayItemBlueprintStack stack{nullptr, nullptr};
    const bool ab = CompareList(*this, aOther, aOther, aDiff, stack, stack);
    const bool ba =
        aOther.CompareList(aOther, *this, *this, aDiff, stack, stack);
    return ab && ba;
  }

  bool CompareList(const DisplayListBlueprint& aRoot,
                   const DisplayListBlueprint& aOther,
                   const DisplayListBlueprint& aOtherRoot,
                   std::stringstream& aDiff,
                   const DisplayItemBlueprintStack& aStack,
                   const DisplayItemBlueprintStack& aStackOther) const;

  // Output this tree to aSs.
  void Dump(std::stringstream& aSs) const { Dump(aSs, 0); }

  void Dump(std::stringstream& aSs, unsigned aDepth) const;

 private:
  // Only used by first constructor, to call the 2nd constructor with an index
  // variable on the stack.
  DisplayListBlueprint(nsDisplayList* aList, unsigned aIndex, const char* aName)
      : DisplayListBlueprint(aList, aName, aIndex) {}

  void processChildren(nsDisplayList* aList, const char* aName,
                       unsigned& aIndex);

  std::vector<DisplayItemBlueprint> mItems;
  const bool mVerifyOrder =
      StaticPrefs::layout_display_list_retain_verify_order();
};

// Object representing one display item, with just enough information to
// compare with another item and output useful debugging information.
class DisplayItemBlueprint {
 public:
  DisplayItemBlueprint(nsDisplayItem& aItem, const char* aName,
                       unsigned& aIndex)
      : mListName(aName),
        mIndex(++aIndex),
        mIndexString(WriteIndex(aName, aIndex)),
        mIndexStringFW(WriteIndexFW(aName, aIndex)),
        mDisplayItemPointer(WriteDisplayItemPointer(aItem)),
        mDescription(WriteDescription(aName, aIndex, aItem)),
        mFrame(aItem.HasDeletedFrame() ? nullptr : aItem.Frame()),
        mPerFrameKey(aItem.GetPerFrameKey()),
        mChildren(aItem.GetChildren(), aName, aIndex) {}

  // Compare this item with another one, based on frame and per-frame key.
  // Not recursive! I.e., children are not examined.
  bool CompareItem(const DisplayItemBlueprint& aOther,
                   std::stringstream& aDiff) const {
    return mFrame == aOther.mFrame && mPerFrameKey == aOther.mPerFrameKey;
  }

  void Dump(std::stringstream& aSs, unsigned aDepth) const;

  const char* mListName;
  const unsigned mIndex;
  const std::string mIndexString;
  const std::string mIndexStringFW;
  const std::string mDisplayItemPointer;
  const std::string mDescription;

  // For pointer comparison only, do not dereference!
  const nsIFrame* const mFrame;
  const uint32_t mPerFrameKey;

  const DisplayListBlueprint mChildren;

 private:
  static std::string WriteIndex(const char* aName, unsigned aIndex) {
    return nsPrintfCString("%s#%u", aName, aIndex).get();
  }

  static std::string WriteIndexFW(const char* aName, unsigned aIndex) {
    return nsPrintfCString("%s#%4u", aName, aIndex).get();
  }

  static std::string WriteDisplayItemPointer(nsDisplayItem& aItem) {
    return nsPrintfCString("0x%p", &aItem).get();
  }

  static std::string WriteDescription(const char* aName, unsigned aIndex,
                                      nsDisplayItem& aItem) {
    if (aItem.HasDeletedFrame()) {
      return nsPrintfCString("%s %s#%u 0x%p f=0x0", aItem.Name(), aName, aIndex,
                             &aItem)
          .get();
    }

    const nsIFrame* f = aItem.Frame();
    nsAutoString contentData;
#ifdef DEBUG_FRAME_DUMP
    f->GetFrameName(contentData);
#endif
    nsIContent* content = f->GetContent();
    if (content) {
      nsString tmp;
      if (content->GetID()) {
        content->GetID()->ToString(tmp);
        contentData.AppendLiteral(" id:");
        contentData.Append(tmp);
      }
      const nsAttrValue* classes =
          content->IsElement() ? content->AsElement()->GetClasses() : nullptr;
      if (classes) {
        classes->ToString(tmp);
        contentData.AppendLiteral(" class:");
        contentData.Append(tmp);
      }
    }
    return nsPrintfCString("%s %s#%u p=0x%p f=0x%p(%s) key=%" PRIu32,
                           aItem.Name(), aName, aIndex, &aItem, f,
                           NS_ConvertUTF16toUTF8(contentData).get(),
                           aItem.GetPerFrameKey())
        .get();
  }
};

void DisplayListBlueprint::processChildren(nsDisplayList* aList,
                                           const char* aName,
                                           unsigned& aIndex) {
  if (!aList) {
    return;
  }
  const uint32_t n = aList->Count();
  if (n == 0) {
    return;
  }
  mItems.reserve(n);
  for (nsDisplayItem* item = aList->GetBottom(); item;
       item = item->GetAbove()) {
    mItems.emplace_back(*item, aName, aIndex);
  }
  MOZ_ASSERT(mItems.size() == n);
}

bool DisplayItemBlueprintStack::Output(std::stringstream& aSs) const {
  const bool output = mPrevious ? mPrevious->Output(aSs) : false;
  if (mItem) {
    if (output) {
      aSs << " > ";
    }
    aSs << mItem->mIndexString;
    return true;
  }
  return output;
}

std::string DisplayListBlueprint::Find(
    const nsIFrame* aFrame, uint32_t aPerFrameKey,
    const DisplayItemBlueprintStack& aStack) const {
  for (const DisplayItemBlueprint& item : mItems) {
    if (item.mFrame == aFrame && item.mPerFrameKey == aPerFrameKey) {
      std::stringstream ss;
      if (aStack.Output(ss)) {
        ss << " > ";
      }
      ss << item.mDescription;
      return ss.str();
    }
    const DisplayItemBlueprintStack stack = {&aStack, &item};
    std::string s = item.mChildren.Find(aFrame, aPerFrameKey, stack);
    if (!s.empty()) {
      return s;
    }
  }
  return "";
}

bool DisplayListBlueprint::CompareList(
    const DisplayListBlueprint& aRoot, const DisplayListBlueprint& aOther,
    const DisplayListBlueprint& aOtherRoot, std::stringstream& aDiff,
    const DisplayItemBlueprintStack& aStack,
    const DisplayItemBlueprintStack& aStackOther) const {
  bool same = true;
  unsigned previousFoundIndex = 0;
  const DisplayItemBlueprint* previousFoundItemBefore = nullptr;
  const DisplayItemBlueprint* previousFoundItemAfter = nullptr;
  for (const DisplayItemBlueprint& itemBefore : mItems) {
    bool found = false;
    unsigned foundIndex = 0;
    for (const DisplayItemBlueprint& itemAfter : aOther.mItems) {
      if (itemBefore.CompareItem(itemAfter, aDiff)) {
        found = true;

        if (mVerifyOrder) {
          if (foundIndex < previousFoundIndex) {
            same = false;
            aDiff << "\n";
            if (aStack.Output(aDiff)) {
              aDiff << " > ";
            }
            aDiff << itemBefore.mDescription;
            aDiff << "\n * Corresponding item in unexpected order: ";
            if (aStackOther.Output(aDiff)) {
              aDiff << " > ";
            }
            aDiff << itemAfter.mDescription;
            aDiff << "\n * Was expected after: ";
            if (aStackOther.Output(aDiff)) {
              aDiff << " > ";
            }
            MOZ_ASSERT(previousFoundItemAfter);
            aDiff << previousFoundItemAfter->mDescription;
            aDiff << "\n   which corresponds to: ";
            if (aStack.Output(aDiff)) {
              aDiff << " > ";
            }
            MOZ_ASSERT(previousFoundItemBefore);
            aDiff << previousFoundItemBefore->mDescription;
          }
          previousFoundIndex = foundIndex;
          previousFoundItemBefore = &itemBefore;
          previousFoundItemAfter = &itemAfter;
        }

        const DisplayItemBlueprintStack stack = {&aStack, &itemBefore};
        const DisplayItemBlueprintStack stackOther = {&aStackOther, &itemAfter};
        if (!itemBefore.mChildren.CompareList(aRoot, itemAfter.mChildren,
                                              aOtherRoot, aDiff, stack,
                                              stackOther)) {
          same = false;
        }
        break;
      }
      ++foundIndex;
    }
    if (!found) {
      same = false;
      aDiff << "\n";
      if (aStack.Output(aDiff)) {
        aDiff << " > ";
      }
      aDiff << itemBefore.mDescription;
      aDiff << "\n * Cannot find corresponding item under ";
      if (!aStackOther.Output(aDiff)) {
        if (!aOtherRoot.mItems.empty()) {
          aDiff << aOtherRoot.mItems[0].mListName;
        } else {
          aDiff << "other root";
        }
      }
      std::string elsewhere =
          aOtherRoot.Find(itemBefore.mFrame, itemBefore.mPerFrameKey);
      if (!elsewhere.empty()) {
        aDiff << "\n * But found: " << elsewhere;
      }
    }
  }
  return same;
}

void DisplayListBlueprint::Dump(std::stringstream& aSs, unsigned aDepth) const {
  for (const DisplayItemBlueprint& item : mItems) {
    item.Dump(aSs, aDepth);
  }
}

void DisplayItemBlueprint::Dump(std::stringstream& aSs, unsigned aDepth) const {
  aSs << "\n" << mIndexStringFW << " ";
  for (unsigned i = 0; i < aDepth; ++i) {
    aSs << "  ";
  }
  aSs << mDescription;
  mChildren.Dump(aSs, aDepth + 1);
}

DisplayListChecker::DisplayListChecker() : mBlueprint(nullptr) {}

DisplayListChecker::DisplayListChecker(nsDisplayList* aList, const char* aName)
    : mBlueprint(MakeUnique<DisplayListBlueprint>(aList, aName)) {}

DisplayListChecker::~DisplayListChecker() = default;

void DisplayListChecker::Set(nsDisplayList* aList, const char* aName) {
  mBlueprint = MakeUnique<DisplayListBlueprint>(aList, aName);
}

// Compare this list with another one, output differences between the two
// into aDiff.
// Differences include: Display items from one tree for which a corresponding
// item (same frame and per-frame key) cannot be found under corresponding
// parent items.
// Returns true if trees are similar, false if different.
bool DisplayListChecker::CompareList(const DisplayListChecker& aOther,
                                     std::stringstream& aDiff) const {
  MOZ_ASSERT(mBlueprint);
  MOZ_ASSERT(aOther.mBlueprint);
  return mBlueprint->CompareList(*aOther.mBlueprint, aDiff);
}

void DisplayListChecker::Dump(std::stringstream& aSs) const {
  MOZ_ASSERT(mBlueprint);
  mBlueprint->Dump(aSs);
}

}  // namespace mozilla
