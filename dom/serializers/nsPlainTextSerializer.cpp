/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsIContentSerializer implementation that can be used with an
 * nsIDocumentEncoder to convert a DOM into plaintext in a nice way
 * (eg for copy/paste as plaintext).
 */

#include "nsPlainTextSerializer.h"

#include <limits>

#include "nsPrintfCString.h"
#include "nsDebug.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "mozilla/Casting.h"
#include "mozilla/EditorUtils.h"
#include "mozilla/dom/CharacterData.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLBRElement.h"
#include "mozilla/dom/Text.h"
#include "mozilla/intl/Segmenter.h"
#include "mozilla/intl/UnicodeProperties.h"
#include "nsUnicodeProperties.h"
#include "mozilla/Span.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_converter.h"
#include "nsComputedDOMStyle.h"

namespace mozilla {
class Encoding;
}

using namespace mozilla;
using namespace mozilla::dom;

#define PREF_STRUCTS "converter.html2txt.structs"
#define PREF_HEADER_STRATEGY "converter.html2txt.header_strategy"

static const int32_t kTabSize = 4;
static const int32_t kIndentSizeHeaders =
    2; /* Indention of h1, if
        mHeaderStrategy = kIndentIncreasedWithHeaderLevel
        or = kNumberHeadingsAndIndentSlightly. Indention of
        other headers is derived from that. */
static const int32_t kIndentIncrementHeaders =
    2; /* If mHeaderStrategy = kIndentIncreasedWithHeaderLevel,
   indent h(x+1) this many
   columns more than h(x) */
static const int32_t kIndentSizeList = kTabSize;
// Indention of non-first lines of ul and ol
static const int32_t kIndentSizeDD = kTabSize;  // Indention of <dd>
static const char16_t kNBSP = 160;
static const char16_t kSPACE = ' ';

static int32_t HeaderLevel(const nsAtom* aTag);
static int32_t GetUnicharWidth(char32_t ucs);
static int32_t GetUnicharStringWidth(Span<const char16_t> aString);

// Someday may want to make this non-const:
static const uint32_t TagStackSize = 500;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPlainTextSerializer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPlainTextSerializer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPlainTextSerializer)
  NS_INTERFACE_MAP_ENTRY(nsIContentSerializer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsPlainTextSerializer, mElement)

nsresult NS_NewPlainTextSerializer(nsIContentSerializer** aSerializer) {
  RefPtr<nsPlainTextSerializer> it = new nsPlainTextSerializer();
  it.forget(aSerializer);
  return NS_OK;
}

// @param aFlags As defined in nsIDocumentEncoder.idl.
static void DetermineLineBreak(const int32_t aFlags, nsAString& aLineBreak) {
  // Set the line break character:
  if ((aFlags & nsIDocumentEncoder::OutputCRLineBreak) &&
      (aFlags & nsIDocumentEncoder::OutputLFLineBreak)) {
    // Windows
    aLineBreak.AssignLiteral(u"\r\n");
  } else if (aFlags & nsIDocumentEncoder::OutputCRLineBreak) {
    // Mac
    aLineBreak.AssignLiteral(u"\r");
  } else if (aFlags & nsIDocumentEncoder::OutputLFLineBreak) {
    // Unix/DOM
    aLineBreak.AssignLiteral(u"\n");
  } else {
    // Platform/default
    aLineBreak.AssignLiteral(NS_ULINEBREAK);
  }
}

void nsPlainTextSerializer::CurrentLine::MaybeReplaceNbspsInContent(
    const int32_t aFlags) {
  if (!(aFlags & nsIDocumentEncoder::OutputPersistNBSP)) {
    // First, replace all nbsp characters with spaces,
    // which the unicode encoder won't do for us.
    mContent.ReplaceChar(kNBSP, kSPACE);
  }
}

void nsPlainTextSerializer::CurrentLine::ResetContentAndIndentationHeader() {
  mContent.Truncate();
  mIndentation.mHeader.Truncate();
}

int32_t nsPlainTextSerializer::CurrentLine::FindWrapIndexForContent(
    const uint32_t aWrapColumn, bool aUseLineBreaker) const {
  MOZ_ASSERT(!mContent.IsEmpty());

  const uint32_t prefixwidth = DeterminePrefixWidth();
  int32_t goodSpace = 0;

  if (aUseLineBreaker) {
    // We advance one line break point at a time from the beginning of the
    // mContent until we find a width less than or equal to wrap column.
    uint32_t width = 0;
    intl::LineBreakIteratorUtf16 lineBreakIter(mContent);
    while (const Maybe<uint32_t> nextGoodSpace = lineBreakIter.Next()) {
      width += GetUnicharStringWidth(Span<const char16_t>(
          mContent.get() + goodSpace, *nextGoodSpace - goodSpace));
      if (prefixwidth + width > aWrapColumn) {
        // The next break point makes the width exceeding the wrap column, so
        // goodSpace is what we want.
        break;
      }
      goodSpace = AssertedCast<int32_t>(*nextGoodSpace);
    }

    return goodSpace;
  }

  // In this case we don't want strings, especially CJK-ones, to be split. See
  // bug 333064 for more information. We break only at ASCII spaces.
  if (aWrapColumn >= prefixwidth) {
    // Search backward from the adjusted wrap column or from the text end.
    goodSpace =
        std::min<int32_t>(aWrapColumn - prefixwidth, mContent.Length() - 1);
    while (goodSpace >= 0) {
      if (nsCRT::IsAsciiSpace(mContent.CharAt(goodSpace))) {
        return goodSpace;
      }
      goodSpace--;
    }
  }

  // Search forward from the adjusted wrap column.
  goodSpace = (prefixwidth > aWrapColumn) ? 1 : aWrapColumn - prefixwidth;
  const int32_t contentLength = mContent.Length();
  while (goodSpace < contentLength &&
         !nsCRT::IsAsciiSpace(mContent.CharAt(goodSpace))) {
    goodSpace++;
  }

  return goodSpace;
}

nsPlainTextSerializer::OutputManager::OutputManager(const int32_t aFlags,
                                                    nsAString& aOutput)
    : mFlags{aFlags}, mOutput{aOutput}, mAtFirstColumn{true} {
  MOZ_ASSERT(aOutput.IsEmpty());

  DetermineLineBreak(mFlags, mLineBreak);
}

void nsPlainTextSerializer::OutputManager::Append(
    const CurrentLine& aCurrentLine,
    const StripTrailingWhitespaces aStripTrailingWhitespaces) {
  if (IsAtFirstColumn()) {
    nsAutoString quotesAndIndent;
    aCurrentLine.CreateQuotesAndIndent(quotesAndIndent);

    if ((aStripTrailingWhitespaces == StripTrailingWhitespaces::kMaybe)) {
      const bool stripTrailingSpaces = aCurrentLine.mContent.IsEmpty();
      if (stripTrailingSpaces) {
        quotesAndIndent.Trim(" ", false, true, false);
      }
    }

    Append(quotesAndIndent);
  }

  Append(aCurrentLine.mContent);
}

void nsPlainTextSerializer::OutputManager::Append(const nsAString& aString) {
  if (!aString.IsEmpty()) {
    mOutput.Append(aString);
    mAtFirstColumn = false;
  }
}

void nsPlainTextSerializer::OutputManager::AppendLineBreak() {
  mOutput.Append(mLineBreak);
  mAtFirstColumn = true;
}

uint32_t nsPlainTextSerializer::OutputManager::GetOutputLength() const {
  return mOutput.Length();
}

nsPlainTextSerializer::nsPlainTextSerializer()
    : mFloatingLines(-1),
      mLineBreakDue(false),
      kSpace(u" "_ns)  // Init of "constant"
{
  mHeadLevel = 0;
  mHasWrittenCiteBlockquote = false;
  mSpanLevel = 0;
  for (int32_t i = 0; i <= 6; i++) {
    mHeaderCounter[i] = 0;
  }

  // Flow
  mEmptyLines = 1;  // The start of the document is an "empty line" in itself,
  mInWhitespace = false;
  mPreFormattedMail = false;

  mPreformattedBlockBoundary = false;

  // initialize the tag stack to zero:
  // The stack only ever contains pointers to static atoms, so they don't
  // need refcounting.
  mTagStack = new const nsAtom*[TagStackSize];
  mTagStackIndex = 0;
  mIgnoreAboveIndex = (uint32_t)kNotFound;

  mULCount = 0;

  mIgnoredChildNodeLevel = 0;
}

nsPlainTextSerializer::~nsPlainTextSerializer() {
  delete[] mTagStack;
  NS_WARNING_ASSERTION(mHeadLevel == 0, "Wrong head level!");
}

nsPlainTextSerializer::Settings::HeaderStrategy
nsPlainTextSerializer::Settings::Convert(const int32_t aPrefHeaderStrategy) {
  HeaderStrategy result{HeaderStrategy::kIndentIncreasedWithHeaderLevel};

  switch (aPrefHeaderStrategy) {
    case 0: {
      result = HeaderStrategy::kNoIndentation;
      break;
    }
    case 1: {
      result = HeaderStrategy::kIndentIncreasedWithHeaderLevel;
      break;
    }
    case 2: {
      result = HeaderStrategy::kNumberHeadingsAndIndentSlightly;
      break;
    }
    default: {
      NS_WARNING(
          nsPrintfCString("Header strategy pref contains undefined value: %i",
                          aPrefHeaderStrategy)
              .get());
    }
  }

  return result;
}

const int32_t kDefaultHeaderStrategy = 1;

void nsPlainTextSerializer::Settings::Init(const int32_t aFlags,
                                           const uint32_t aWrapColumn) {
  mFlags = aFlags;

  if (mFlags & nsIDocumentEncoder::OutputFormatted) {
    // Get some prefs that controls how we do formatted output
    mStructs = Preferences::GetBool(PREF_STRUCTS, mStructs);

    int32_t headerStrategy =
        Preferences::GetInt(PREF_HEADER_STRATEGY, kDefaultHeaderStrategy);
    mHeaderStrategy = Convert(headerStrategy);
  }

  mWithRubyAnnotation = StaticPrefs::converter_html2txt_always_include_ruby() ||
                        (mFlags & nsIDocumentEncoder::OutputRubyAnnotation);

  // XXX We should let the caller decide whether to do this or not
  mFlags &= ~nsIDocumentEncoder::OutputNoFramesContent;

  mWrapColumn = aWrapColumn;
}

NS_IMETHODIMP
nsPlainTextSerializer::Init(const uint32_t aFlags, uint32_t aWrapColumn,
                            const Encoding* aEncoding, bool aIsCopying,
                            bool aIsWholeDocument,
                            bool* aNeedsPreformatScanning, nsAString& aOutput) {
#ifdef DEBUG
  // Check if the major control flags are set correctly.
  if (aFlags & nsIDocumentEncoder::OutputFormatFlowed) {
    NS_ASSERTION(aFlags & nsIDocumentEncoder::OutputFormatted,
                 "If you want format=flowed, you must combine it with "
                 "nsIDocumentEncoder::OutputFormatted");
  }

  if (aFlags & nsIDocumentEncoder::OutputFormatted) {
    NS_ASSERTION(
        !(aFlags & nsIDocumentEncoder::OutputPreformatted),
        "Can't do formatted and preformatted output at the same time!");
  }
#endif
  MOZ_ASSERT(!(aFlags & nsIDocumentEncoder::OutputFormatDelSp) ||
             (aFlags & nsIDocumentEncoder::OutputFormatFlowed));

  *aNeedsPreformatScanning = true;
  mSettings.Init(aFlags, aWrapColumn);
  mOutputManager.emplace(mSettings.GetFlags(), aOutput);

  mUseLineBreaker = mSettings.MayWrap() && mSettings.MayBreakLines();

  mLineBreakDue = false;
  mFloatingLines = -1;

  mPreformattedBlockBoundary = false;

  MOZ_ASSERT(mOLStack.IsEmpty());

  return NS_OK;
}

bool nsPlainTextSerializer::GetLastBool(const nsTArray<bool>& aStack) {
  uint32_t size = aStack.Length();
  if (size == 0) {
    return false;
  }
  return aStack.ElementAt(size - 1);
}

void nsPlainTextSerializer::SetLastBool(nsTArray<bool>& aStack, bool aValue) {
  uint32_t size = aStack.Length();
  if (size > 0) {
    aStack.ElementAt(size - 1) = aValue;
  } else {
    NS_ERROR("There is no \"Last\" value");
  }
}

void nsPlainTextSerializer::PushBool(nsTArray<bool>& aStack, bool aValue) {
  aStack.AppendElement(bool(aValue));
}

bool nsPlainTextSerializer::PopBool(nsTArray<bool>& aStack) {
  return aStack.Length() ? aStack.PopLastElement() : false;
}

bool nsPlainTextSerializer::IsIgnorableRubyAnnotation(
    const nsAtom* aTag) const {
  if (mSettings.GetWithRubyAnnotation()) {
    return false;
  }

  return aTag == nsGkAtoms::rp || aTag == nsGkAtoms::rt ||
         aTag == nsGkAtoms::rtc;
}

// Return true if aElement has 'display:none' or if we just don't know.
static bool IsDisplayNone(Element* aElement) {
  RefPtr<const ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aElement);
  return !computedStyle ||
         computedStyle->StyleDisplay()->mDisplay == StyleDisplay::None;
}

static bool IsIgnorableScriptOrStyle(Element* aElement) {
  return aElement->IsAnyOfHTMLElements(nsGkAtoms::script, nsGkAtoms::style) &&
         IsDisplayNone(aElement);
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendText(nsIContent* aText, int32_t aStartOffset,
                                  int32_t aEndOffset) {
  if (mIgnoreAboveIndex != (uint32_t)kNotFound) {
    return NS_OK;
  }

  NS_ASSERTION(aStartOffset >= 0, "Negative start offset for text fragment!");
  if (aStartOffset < 0) return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG(aText);

  nsresult rv = NS_OK;

  nsIContent* content = aText;
  const nsTextFragment* frag;
  if (!content || !(frag = content->GetText())) {
    return NS_ERROR_FAILURE;
  }

  int32_t fragLength = frag->GetLength();
  int32_t endoffset =
      (aEndOffset == -1) ? fragLength : std::min(aEndOffset, fragLength);
  NS_ASSERTION(aStartOffset <= endoffset,
               "A start offset is beyond the end of the text fragment!");

  int32_t length = endoffset - aStartOffset;
  if (length <= 0) {
    return NS_OK;
  }

  nsAutoString textstr;
  if (frag->Is2b()) {
    textstr.Assign(frag->Get2b() + aStartOffset, length);
  } else {
    // AssignASCII is for 7-bit character only, so don't use it
    const char* data = frag->Get1b();
    CopyASCIItoUTF16(Substring(data + aStartOffset, data + endoffset), textstr);
  }

  // Mask the text if the text node is in a password field.
  if (content->HasFlag(NS_MAYBE_MASKED)) {
    EditorUtils::MaskString(textstr, *content->AsText(), 0, aStartOffset);
  }

  // We have to split the string across newlines
  // to match parser behavior
  int32_t start = 0;
  int32_t offset = textstr.FindCharInSet(u"\n\r");
  while (offset != kNotFound) {
    if (offset > start) {
      // Pass in the line
      DoAddText(false, Substring(textstr, start, offset - start));
    }

    // Pass in a newline
    DoAddText();

    start = offset + 1;
    offset = textstr.FindCharInSet(u"\n\r", start);
  }

  // Consume the last bit of the string if there's any left
  if (start < length) {
    if (start) {
      DoAddText(false, Substring(textstr, start, length - start));
    } else {
      DoAddText(false, textstr);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendCDATASection(nsIContent* aCDATASection,
                                          int32_t aStartOffset,
                                          int32_t aEndOffset) {
  return AppendText(aCDATASection, aStartOffset, aEndOffset);
}

NS_IMETHODIMP
nsPlainTextSerializer::ScanElementForPreformat(Element* aElement) {
  mPreformatStack.push(IsElementPreformatted(aElement));
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::ForgetElementForPreformat(Element* aElement) {
  MOZ_RELEASE_ASSERT(!mPreformatStack.empty(),
                     "Tried to pop without previous push.");
  mPreformatStack.pop();
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendElementStart(Element* aElement,
                                          Element* aOriginalElement) {
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  nsresult rv;
  nsAtom* id = GetIdForContent(mElement);

  bool isContainer = !FragmentOrElement::IsHTMLVoid(id);

  if (isContainer) {
    rv = DoOpenContainer(id);
  } else {
    rv = DoAddLeaf(id);
  }

  mElement = nullptr;

  if (id == nsGkAtoms::head) {
    ++mHeadLevel;
  }

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendElementEnd(Element* aElement,
                                        Element* aOriginalElement) {
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  nsresult rv;
  nsAtom* id = GetIdForContent(mElement);

  bool isContainer = !FragmentOrElement::IsHTMLVoid(id);

  rv = NS_OK;
  if (isContainer) {
    rv = DoCloseContainer(id);
  }

  mElement = nullptr;

  if (id == nsGkAtoms::head) {
    NS_ASSERTION(mHeadLevel != 0, "mHeadLevel being decremented below 0");
    --mHeadLevel;
  }

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::FlushAndFinish() {
  MOZ_ASSERT(mOutputManager);

  mOutputManager->Flush(mCurrentLine);
  return Finish();
}

NS_IMETHODIMP
nsPlainTextSerializer::Finish() {
  mOutputManager.reset();

  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::GetOutputLength(uint32_t& aLength) const {
  MOZ_ASSERT(mOutputManager);

  aLength = mOutputManager->GetOutputLength();

  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendDocumentStart(Document* aDocument) {
  return NS_OK;
}

constexpr int32_t kOlStackDummyValue = 0;

nsresult nsPlainTextSerializer::DoOpenContainer(const nsAtom* aTag) {
  if (IsIgnorableRubyAnnotation(aTag)) {
    // Ignorable ruby annotation shouldn't be replaced by a placeholder
    // character, neither any of its descendants.
    mIgnoredChildNodeLevel++;
    return NS_OK;
  }
  if (IsIgnorableScriptOrStyle(mElement)) {
    mIgnoredChildNodeLevel++;
    return NS_OK;
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputForPlainTextClipboardCopy)) {
    if (mPreformattedBlockBoundary && DoOutput()) {
      // Should always end a line, but get no more whitespace
      if (mFloatingLines < 0) mFloatingLines = 0;
      mLineBreakDue = true;
    }
    mPreformattedBlockBoundary = false;
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputRaw)) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.

    return NS_OK;
  }

  if (mTagStackIndex < TagStackSize) {
    mTagStack[mTagStackIndex++] = aTag;
  }

  if (mIgnoreAboveIndex != (uint32_t)kNotFound) {
    return NS_OK;
  }

  // Reset this so that <blockquote type=cite> doesn't affect the whitespace
  // above random <pre>s below it.
  mHasWrittenCiteBlockquote =
      mHasWrittenCiteBlockquote && aTag == nsGkAtoms::pre;

  bool isInCiteBlockquote = false;

  // XXX special-case <blockquote type=cite> so that we don't add additional
  // newlines before the text.
  if (aTag == nsGkAtoms::blockquote) {
    nsAutoString value;
    nsresult rv = GetAttributeValue(nsGkAtoms::type, value);
    isInCiteBlockquote = NS_SUCCEEDED(rv) && value.EqualsIgnoreCase("cite");
  }

  if (mLineBreakDue && !isInCiteBlockquote) EnsureVerticalSpace(mFloatingLines);

  // Check if this tag's content that should not be output
  if ((aTag == nsGkAtoms::noscript &&
       !mSettings.HasFlag(nsIDocumentEncoder::OutputNoScriptContent)) ||
      ((aTag == nsGkAtoms::iframe || aTag == nsGkAtoms::noframes) &&
       !mSettings.HasFlag(nsIDocumentEncoder::OutputNoFramesContent))) {
    // Ignore everything that follows the current tag in
    // question until a matching end tag is encountered.
    mIgnoreAboveIndex = mTagStackIndex - 1;
    return NS_OK;
  }

  if (aTag == nsGkAtoms::body) {
    // Try to figure out here whether we have a
    // preformatted style attribute set by Thunderbird.
    //
    // Trigger on the presence of a "pre-wrap" in the
    // style attribute. That's a very simplistic way to do
    // it, but better than nothing.
    nsAutoString style;
    int32_t whitespace;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::style, style)) &&
        (kNotFound != (whitespace = style.Find(u"white-space:")))) {
      if (kNotFound != style.LowerCaseFindASCII("pre-wrap", whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormattedMail based on style pre-wrap\n");
#endif
        mPreFormattedMail = true;
      } else if (kNotFound != style.LowerCaseFindASCII("pre", whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormattedMail based on style pre\n");
#endif
        mPreFormattedMail = true;
      }
    } else {
      /* See comment at end of function. */
      mInWhitespace = true;
      mPreFormattedMail = false;
    }

    return NS_OK;
  }

  // Keep this in sync with DoCloseContainer!
  if (!DoOutput()) {
    return NS_OK;
  }

  if (aTag == nsGkAtoms::p)
    EnsureVerticalSpace(1);
  else if (aTag == nsGkAtoms::pre) {
    if (GetLastBool(mIsInCiteBlockquote))
      EnsureVerticalSpace(0);
    else if (mHasWrittenCiteBlockquote) {
      EnsureVerticalSpace(0);
      mHasWrittenCiteBlockquote = false;
    } else
      EnsureVerticalSpace(1);
  } else if (aTag == nsGkAtoms::tr) {
    PushBool(mHasWrittenCellsForRow, false);
  } else if (aTag == nsGkAtoms::td || aTag == nsGkAtoms::th) {
    // We must make sure that the content of two table cells get a
    // space between them.

    // To make the separation between cells most obvious and
    // importable, we use a TAB.
    if (mHasWrittenCellsForRow.IsEmpty()) {
      // We don't always see a <tr> (nor a <table>) before the <td> if we're
      // copying part of a table
      PushBool(mHasWrittenCellsForRow, true);  // will never be popped
    } else if (GetLastBool(mHasWrittenCellsForRow)) {
      // Bypass |Write| so that the TAB isn't compressed away.
      AddToLine(u"\t", 1);
      mInWhitespace = true;
    } else {
      SetLastBool(mHasWrittenCellsForRow, true);
    }
  } else if (aTag == nsGkAtoms::ul) {
    // Indent here to support nested lists, which aren't included in li :-(
    EnsureVerticalSpace(IsInOlOrUl() ? 0 : 1);
    // Must end the current line before we change indention
    mCurrentLine.mIndentation.mLength += kIndentSizeList;
    mULCount++;
  } else if (aTag == nsGkAtoms::ol) {
    EnsureVerticalSpace(IsInOlOrUl() ? 0 : 1);
    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
      // Must end the current line before we change indention
      nsAutoString startAttr;
      int32_t startVal = 1;
      if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::start, startAttr))) {
        nsresult rv = NS_OK;
        startVal = startAttr.ToInteger(&rv);
        if (NS_FAILED(rv)) {
          startVal = 1;
        }
      }
      mOLStack.AppendElement(startVal);
    } else {
      mOLStack.AppendElement(kOlStackDummyValue);
    }
    mCurrentLine.mIndentation.mLength += kIndentSizeList;  // see ul
  } else if (aTag == nsGkAtoms::li &&
             mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
    if (mTagStackIndex > 1 && IsInOL()) {
      if (!mOLStack.IsEmpty()) {
        nsAutoString valueAttr;
        if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::value, valueAttr))) {
          nsresult rv = NS_OK;
          int32_t valueAttrVal = valueAttr.ToInteger(&rv);
          if (NS_SUCCEEDED(rv)) {
            mOLStack.LastElement() = valueAttrVal;
          }
        }
        // This is what nsBulletFrame does for OLs:
        mCurrentLine.mIndentation.mHeader.AppendInt(mOLStack.LastElement(), 10);
        mOLStack.LastElement()++;
      } else {
        mCurrentLine.mIndentation.mHeader.Append(char16_t('#'));
      }

      mCurrentLine.mIndentation.mHeader.Append(char16_t('.'));

    } else {
      static const char bulletCharArray[] = "*o+#";
      uint32_t index = mULCount > 0 ? (mULCount - 1) : 3;
      char bulletChar = bulletCharArray[index % 4];
      mCurrentLine.mIndentation.mHeader.Append(char16_t(bulletChar));
    }

    mCurrentLine.mIndentation.mHeader.Append(char16_t(' '));
  } else if (aTag == nsGkAtoms::dl) {
    EnsureVerticalSpace(1);
  } else if (aTag == nsGkAtoms::dt) {
    EnsureVerticalSpace(0);
  } else if (aTag == nsGkAtoms::dd) {
    EnsureVerticalSpace(0);
    mCurrentLine.mIndentation.mLength += kIndentSizeDD;
  } else if (aTag == nsGkAtoms::span) {
    ++mSpanLevel;
  } else if (aTag == nsGkAtoms::blockquote) {
    // Push
    PushBool(mIsInCiteBlockquote, isInCiteBlockquote);
    if (isInCiteBlockquote) {
      EnsureVerticalSpace(0);
      mCurrentLine.mCiteQuoteLevel++;
    } else {
      EnsureVerticalSpace(1);
      mCurrentLine.mIndentation.mLength +=
          kTabSize;  // Check for some maximum value?
    }
  } else if (aTag == nsGkAtoms::q) {
    Write(u"\""_ns);
  }

  // Else make sure we'll separate block level tags,
  // even if we're about to leave, before doing any other formatting.
  else if (IsCssBlockLevelElement(mElement)) {
    EnsureVerticalSpace(0);
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
    OpenContainerForOutputFormatted(aTag);
  }
  return NS_OK;
}

void nsPlainTextSerializer::OpenContainerForOutputFormatted(
    const nsAtom* aTag) {
  const bool currentNodeIsConverted = IsCurrentNodeConverted();

  if (aTag == nsGkAtoms::h1 || aTag == nsGkAtoms::h2 || aTag == nsGkAtoms::h3 ||
      aTag == nsGkAtoms::h4 || aTag == nsGkAtoms::h5 || aTag == nsGkAtoms::h6) {
    EnsureVerticalSpace(2);
    if (mSettings.GetHeaderStrategy() ==
        Settings::HeaderStrategy::kNumberHeadingsAndIndentSlightly) {
      mCurrentLine.mIndentation.mLength += kIndentSizeHeaders;
      // Caching
      int32_t level = HeaderLevel(aTag);
      // Increase counter for current level
      mHeaderCounter[level]++;
      // Reset all lower levels
      int32_t i;

      for (i = level + 1; i <= 6; i++) {
        mHeaderCounter[i] = 0;
      }

      // Construct numbers
      nsAutoString leadup;
      for (i = 1; i <= level; i++) {
        leadup.AppendInt(mHeaderCounter[i]);
        leadup.Append(char16_t('.'));
      }
      leadup.Append(char16_t(' '));
      Write(leadup);
    } else if (mSettings.GetHeaderStrategy() ==
               Settings::HeaderStrategy::kIndentIncreasedWithHeaderLevel) {
      mCurrentLine.mIndentation.mLength += kIndentSizeHeaders;
      for (int32_t i = HeaderLevel(aTag); i > 1; i--) {
        // for h(x), run x-1 times
        mCurrentLine.mIndentation.mLength += kIndentIncrementHeaders;
      }
    }
  } else if (aTag == nsGkAtoms::sup && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"^"_ns);
  } else if (aTag == nsGkAtoms::sub && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"_"_ns);
  } else if (aTag == nsGkAtoms::code && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"|"_ns);
  } else if ((aTag == nsGkAtoms::strong || aTag == nsGkAtoms::b) &&
             mSettings.GetStructs() && !currentNodeIsConverted) {
    Write(u"*"_ns);
  } else if ((aTag == nsGkAtoms::em || aTag == nsGkAtoms::i) &&
             mSettings.GetStructs() && !currentNodeIsConverted) {
    Write(u"/"_ns);
  } else if (aTag == nsGkAtoms::u && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"_"_ns);
  }

  /* Container elements are always block elements, so we shouldn't
     output any whitespace immediately after the container tag even if
     there's extra whitespace there because the HTML is pretty-printed
     or something. To ensure that happens, tell the serializer we're
     already in whitespace so it won't output more. */
  mInWhitespace = true;
}

nsresult nsPlainTextSerializer::DoCloseContainer(const nsAtom* aTag) {
  if (IsIgnorableRubyAnnotation(aTag)) {
    mIgnoredChildNodeLevel--;
    return NS_OK;
  }
  if (IsIgnorableScriptOrStyle(mElement)) {
    mIgnoredChildNodeLevel--;
    return NS_OK;
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputForPlainTextClipboardCopy)) {
    if (DoOutput() && IsElementPreformatted() &&
        IsCssBlockLevelElement(mElement)) {
      // If we're closing a preformatted block element, output a line break
      // when we find a new container.
      mPreformattedBlockBoundary = true;
    }
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputRaw)) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.

    return NS_OK;
  }

  if (mTagStackIndex > 0) {
    --mTagStackIndex;
  }

  if (mTagStackIndex >= mIgnoreAboveIndex) {
    if (mTagStackIndex == mIgnoreAboveIndex) {
      // We're dealing with the close tag whose matching
      // open tag had set the mIgnoreAboveIndex value.
      // Reset mIgnoreAboveIndex before discarding this tag.
      mIgnoreAboveIndex = (uint32_t)kNotFound;
    }
    return NS_OK;
  }

  MOZ_ASSERT(mOutputManager);

  // End current line if we're ending a block level tag
  if ((aTag == nsGkAtoms::body) || (aTag == nsGkAtoms::html)) {
    // We want the output to end with a new line,
    // but in preformatted areas like text fields,
    // we can't emit newlines that weren't there.
    // So add the newline only in the case of formatted output.
    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
      EnsureVerticalSpace(0);
    } else {
      mOutputManager->Flush(mCurrentLine);
    }
    // We won't want to do anything with these in formatted mode either,
    // so just return now:
    return NS_OK;
  }

  // Keep this in sync with DoOpenContainer!
  if (!DoOutput()) {
    return NS_OK;
  }

  if (aTag == nsGkAtoms::tr) {
    PopBool(mHasWrittenCellsForRow);
    // Should always end a line, but get no more whitespace
    if (mFloatingLines < 0) mFloatingLines = 0;
    mLineBreakDue = true;
  } else if (((aTag == nsGkAtoms::li) || (aTag == nsGkAtoms::dt)) &&
             mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
    // Items that should always end a line, but get no more whitespace
    if (mFloatingLines < 0) mFloatingLines = 0;
    mLineBreakDue = true;
  } else if (aTag == nsGkAtoms::pre) {
    mFloatingLines = GetLastBool(mIsInCiteBlockquote) ? 0 : 1;
    mLineBreakDue = true;
  } else if (aTag == nsGkAtoms::ul) {
    mOutputManager->Flush(mCurrentLine);
    mCurrentLine.mIndentation.mLength -= kIndentSizeList;
    --mULCount;
    if (!IsInOlOrUl()) {
      mFloatingLines = 1;
      mLineBreakDue = true;
    }
  } else if (aTag == nsGkAtoms::ol) {
    mOutputManager->Flush(mCurrentLine);  // Doing this after decreasing
                                          // OLStackIndex would be wrong.
    mCurrentLine.mIndentation.mLength -= kIndentSizeList;
    MOZ_ASSERT(!mOLStack.IsEmpty(), "Wrong OLStack level!");
    mOLStack.RemoveLastElement();
    if (!IsInOlOrUl()) {
      mFloatingLines = 1;
      mLineBreakDue = true;
    }
  } else if (aTag == nsGkAtoms::dl) {
    mFloatingLines = 1;
    mLineBreakDue = true;
  } else if (aTag == nsGkAtoms::dd) {
    mOutputManager->Flush(mCurrentLine);
    mCurrentLine.mIndentation.mLength -= kIndentSizeDD;
  } else if (aTag == nsGkAtoms::span) {
    NS_ASSERTION(mSpanLevel, "Span level will be negative!");
    --mSpanLevel;
  } else if (aTag == nsGkAtoms::div) {
    if (mFloatingLines < 0) mFloatingLines = 0;
    mLineBreakDue = true;
  } else if (aTag == nsGkAtoms::blockquote) {
    mOutputManager->Flush(mCurrentLine);  // Is this needed?

    // Pop
    bool isInCiteBlockquote = PopBool(mIsInCiteBlockquote);

    if (isInCiteBlockquote) {
      NS_ASSERTION(mCurrentLine.mCiteQuoteLevel,
                   "CiteQuote level will be negative!");
      mCurrentLine.mCiteQuoteLevel--;
      mFloatingLines = 0;
      mHasWrittenCiteBlockquote = true;
    } else {
      mCurrentLine.mIndentation.mLength -= kTabSize;
      mFloatingLines = 1;
    }
    mLineBreakDue = true;
  } else if (aTag == nsGkAtoms::q) {
    Write(u"\""_ns);
  } else if (IsCssBlockLevelElement(mElement)) {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
      EnsureVerticalSpace(1);
    } else {
      if (mFloatingLines < 0) mFloatingLines = 0;
      mLineBreakDue = true;
    }
  }

  if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
    CloseContainerForOutputFormatted(aTag);
  }

  return NS_OK;
}

void nsPlainTextSerializer::CloseContainerForOutputFormatted(
    const nsAtom* aTag) {
  const bool currentNodeIsConverted = IsCurrentNodeConverted();

  if (aTag == nsGkAtoms::h1 || aTag == nsGkAtoms::h2 || aTag == nsGkAtoms::h3 ||
      aTag == nsGkAtoms::h4 || aTag == nsGkAtoms::h5 || aTag == nsGkAtoms::h6) {
    using HeaderStrategy = Settings::HeaderStrategy;
    if ((mSettings.GetHeaderStrategy() ==
         HeaderStrategy::kIndentIncreasedWithHeaderLevel) ||
        (mSettings.GetHeaderStrategy() ==
         HeaderStrategy::kNumberHeadingsAndIndentSlightly)) {
      mCurrentLine.mIndentation.mLength -= kIndentSizeHeaders;
    }
    if (mSettings.GetHeaderStrategy() ==
        HeaderStrategy::kIndentIncreasedWithHeaderLevel) {
      for (int32_t i = HeaderLevel(aTag); i > 1; i--) {
        // for h(x), run x-1 times
        mCurrentLine.mIndentation.mLength -= kIndentIncrementHeaders;
      }
    }
    EnsureVerticalSpace(1);
  } else if (aTag == nsGkAtoms::a && !currentNodeIsConverted) {
    nsAutoString url;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::href, url)) &&
        !url.IsEmpty()) {
      nsAutoString temp;
      temp.AssignLiteral(" <");
      temp += url;
      temp.Append(char16_t('>'));
      Write(temp);
    }
  } else if ((aTag == nsGkAtoms::sup || aTag == nsGkAtoms::sub) &&
             mSettings.GetStructs() && !currentNodeIsConverted) {
    Write(kSpace);
  } else if (aTag == nsGkAtoms::code && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"|"_ns);
  } else if ((aTag == nsGkAtoms::strong || aTag == nsGkAtoms::b) &&
             mSettings.GetStructs() && !currentNodeIsConverted) {
    Write(u"*"_ns);
  } else if ((aTag == nsGkAtoms::em || aTag == nsGkAtoms::i) &&
             mSettings.GetStructs() && !currentNodeIsConverted) {
    Write(u"/"_ns);
  } else if (aTag == nsGkAtoms::u && mSettings.GetStructs() &&
             !currentNodeIsConverted) {
    Write(u"_"_ns);
  }
}

bool nsPlainTextSerializer::MustSuppressLeaf() const {
  if (mIgnoredChildNodeLevel > 0) {
    return true;
  }

  if ((mTagStackIndex > 1 &&
       mTagStack[mTagStackIndex - 2] == nsGkAtoms::select) ||
      (mTagStackIndex > 0 &&
       mTagStack[mTagStackIndex - 1] == nsGkAtoms::select)) {
    // Don't output the contents of SELECT elements;
    // Might be nice, eventually, to output just the selected element.
    // Read more in bug 31994.
    return true;
  }

  return false;
}

void nsPlainTextSerializer::DoAddText() { DoAddText(true, u""_ns); }

void nsPlainTextSerializer::DoAddText(bool aIsLineBreak,
                                      const nsAString& aText) {
  // If we don't want any output, just return
  if (!DoOutput()) {
    return;
  }

  if (!aIsLineBreak) {
    // Make sure to reset this, since it's no longer true.
    mHasWrittenCiteBlockquote = false;
  }

  if (mLineBreakDue) EnsureVerticalSpace(mFloatingLines);

  if (MustSuppressLeaf()) {
    return;
  }

  if (aIsLineBreak) {
    // The only times we want to pass along whitespace from the original
    // html source are if we're forced into preformatted mode via flags,
    // or if we're prettyprinting and we're inside a <pre>.
    // Otherwise, either we're collapsing to minimal text, or we're
    // prettyprinting to mimic the html format, and in neither case
    // does the formatting of the html source help us.
    if (mSettings.HasFlag(nsIDocumentEncoder::OutputPreformatted) ||
        (mPreFormattedMail && !mSettings.GetWrapColumn()) ||
        IsElementPreformatted()) {
      EnsureVerticalSpace(mEmptyLines + 1);
    } else if (!mInWhitespace) {
      Write(kSpace);
      mInWhitespace = true;
    }
    return;
  }

  Write(aText);
}

void CreateLineOfDashes(nsAString& aResult, const uint32_t aWrapColumn) {
  MOZ_ASSERT(aResult.IsEmpty());

  const uint32_t width = (aWrapColumn > 0 ? aWrapColumn : 25);
  while (aResult.Length() < width) {
    aResult.Append(char16_t('-'));
  }
}

nsresult nsPlainTextSerializer::DoAddLeaf(const nsAtom* aTag) {
  mPreformattedBlockBoundary = false;

  if (!DoOutput()) {
    return NS_OK;
  }

  if (mLineBreakDue) EnsureVerticalSpace(mFloatingLines);

  if (MustSuppressLeaf()) {
    return NS_OK;
  }

  if (aTag == nsGkAtoms::br) {
    // Another egregious editor workaround, see bug 38194:
    // ignore the bogus br tags that the editor sticks here and there.
    // FYI: `brElement` may be `nullptr` if the element is <br> element
    //      of non-HTML element.
    // XXX Do we need to call `EnsureVerticalSpace()` when the <br> element
    //     is not an HTML element?
    HTMLBRElement* brElement = HTMLBRElement::FromNodeOrNull(mElement);
    if (!brElement || !brElement->IsPaddingForEmptyLastLine()) {
      EnsureVerticalSpace(mEmptyLines + 1);
    }
  } else if (aTag == nsGkAtoms::hr &&
             mSettings.HasFlag(nsIDocumentEncoder::OutputFormatted)) {
    EnsureVerticalSpace(0);

    // Make a line of dashes as wide as the wrap width
    // XXX honoring percentage would be nice
    nsAutoString line;
    CreateLineOfDashes(line, mSettings.GetWrapColumn());
    Write(line);

    EnsureVerticalSpace(0);
  } else if (aTag == nsGkAtoms::img) {
    /* Output (in decreasing order of preference)
       alt, title or nothing */
    // See <http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG>
    nsAutoString imageDescription;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::alt, imageDescription))) {
      // If the alt attribute has an empty value (|alt=""|), output nothing
    } else if (NS_SUCCEEDED(
                   GetAttributeValue(nsGkAtoms::title, imageDescription)) &&
               !imageDescription.IsEmpty()) {
      imageDescription = u" ["_ns + imageDescription + u"] "_ns;
    }

    Write(imageDescription);
  }

  return NS_OK;
}

/**
 * Adds as many newline as necessary to get |aNumberOfRows| empty lines
 *
 * aNumberOfRows = -1    :   Being in the middle of some line of text
 * aNumberOfRows =  0    :   Being at the start of a line
 * aNumberOfRows =  n>0  :   Having n empty lines before the current line.
 */
void nsPlainTextSerializer::EnsureVerticalSpace(const int32_t aNumberOfRows) {
  // If we have something in the indent we probably want to output
  // it and it's not included in the count for empty lines so we don't
  // realize that we should start a new line.
  if (aNumberOfRows >= 0 && !mCurrentLine.mIndentation.mHeader.IsEmpty()) {
    EndLine(false);
    mInWhitespace = true;
  }

  while (mEmptyLines < aNumberOfRows) {
    EndLine(false);
    mInWhitespace = true;
  }
  mLineBreakDue = false;
  mFloatingLines = -1;
}

void nsPlainTextSerializer::OutputManager::Flush(CurrentLine& aCurrentLine) {
  if (!aCurrentLine.mContent.IsEmpty()) {
    aCurrentLine.MaybeReplaceNbspsInContent(mFlags);

    Append(aCurrentLine, StripTrailingWhitespaces::kNo);

    aCurrentLine.ResetContentAndIndentationHeader();
  }
}

static bool IsSpaceStuffable(const char16_t* s) {
  return (s[0] == '>' || s[0] == ' ' || s[0] == kNBSP ||
          NS_strncmp(s, u"From ", 5) == 0);
}

void nsPlainTextSerializer::MaybeWrapAndOutputCompleteLines() {
  if (!mSettings.MayWrap()) {
    return;
  }

  const uint32_t prefixwidth = mCurrentLine.DeterminePrefixWidth();

  // Yes, wrap!
  // The "+4" is to avoid wrap lines that only would be a couple
  // of letters too long. We give this bonus only if the
  // wrapcolumn is more than 20.
  const uint32_t wrapColumn = mSettings.GetWrapColumn();
  uint32_t bonuswidth = (wrapColumn > 20) ? 4 : 0;

  while (!mCurrentLine.mContent.IsEmpty()) {
    // The width of the line as it will appear on the screen (approx.).
    const uint32_t currentLineContentWidth =
        GetUnicharStringWidth(mCurrentLine.mContent);
    if (currentLineContentWidth + prefixwidth <= wrapColumn + bonuswidth) {
      break;
    }

    const int32_t goodSpace =
        mCurrentLine.FindWrapIndexForContent(wrapColumn, mUseLineBreaker);

    const int32_t contentLength = mCurrentLine.mContent.Length();
    if ((goodSpace < contentLength) && (goodSpace > 0)) {
      // Found a place to break

      // -1 (trim a char at the break position)
      // only if the line break was a space.
      nsAutoString restOfContent;
      if (nsCRT::IsAsciiSpace(mCurrentLine.mContent.CharAt(goodSpace))) {
        mCurrentLine.mContent.Right(restOfContent,
                                    contentLength - goodSpace - 1);
      } else {
        mCurrentLine.mContent.Right(restOfContent, contentLength - goodSpace);
      }
      // if breaker was U+0020, it has to consider for delsp=yes support
      const bool breakBySpace = mCurrentLine.mContent.CharAt(goodSpace) == ' ';
      mCurrentLine.mContent.Truncate(goodSpace);
      EndLine(true, breakBySpace);
      mCurrentLine.mContent.Truncate();
      // Space stuff new line?
      if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatFlowed)) {
        if (!restOfContent.IsEmpty() && IsSpaceStuffable(restOfContent.get()) &&
            mCurrentLine.mCiteQuoteLevel ==
                0  // We space-stuff quoted lines anyway
        ) {
          // Space stuffing a la RFC 2646 (format=flowed).
          mCurrentLine.mContent.Append(char16_t(' '));
          // XXX doesn't seem to work correctly for ' '
        }
      }
      mCurrentLine.mContent.Append(restOfContent);
      mEmptyLines = -1;
    } else {
      // Nothing to do. Hopefully we get more data later
      // to use for a place to break line
      break;
    }
  }
}

/**
 * This function adds a piece of text to the current stored line. If we are
 * wrapping text and the stored line will become too long, a suitable
 * location to wrap will be found and the line that's complete will be
 * output.
 */
void nsPlainTextSerializer::AddToLine(const char16_t* aLineFragment,
                                      int32_t aLineFragmentLength) {
  if (mLineBreakDue) EnsureVerticalSpace(mFloatingLines);

  if (mCurrentLine.mContent.IsEmpty()) {
    if (0 == aLineFragmentLength) {
      return;
    }

    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatFlowed)) {
      if (IsSpaceStuffable(aLineFragment) &&
          mCurrentLine.mCiteQuoteLevel ==
              0  // We space-stuff quoted lines anyway
      ) {
        // Space stuffing a la RFC 2646 (format=flowed).
        mCurrentLine.mContent.Append(char16_t(' '));
      }
    }
    mEmptyLines = -1;
  }

  mCurrentLine.mContent.Append(aLineFragment, aLineFragmentLength);

  MaybeWrapAndOutputCompleteLines();
}

// The signature separator (RFC 2646).
const char kSignatureSeparator[] = "-- ";

// The OpenPGP dash-escaped signature separator in inline
// signed messages according to the OpenPGP standard (RFC 2440).
const char kDashEscapedSignatureSeparator[] = "- -- ";

static bool IsSignatureSeparator(const nsAString& aString) {
  return aString.EqualsLiteral(kSignatureSeparator) ||
         aString.EqualsLiteral(kDashEscapedSignatureSeparator);
}

/**
 * Outputs the contents of mCurrentLine.mContent, and resets line
 * specific variables. Also adds an indentation and prefix if there is one
 * specified. Strips ending spaces from the line if it isn't preformatted.
 */
void nsPlainTextSerializer::EndLine(bool aSoftLineBreak, bool aBreakBySpace) {
  if (aSoftLineBreak && mCurrentLine.mContent.IsEmpty()) {
    // No meaning
    return;
  }

  /* In non-preformatted mode, remove spaces from the end of the line for
   * format=flowed compatibility. Don't do this for these special cases:
   * "-- ", the signature separator (RFC 2646) shouldn't be touched and
   * "- -- ", the OpenPGP dash-escaped signature separator in inline
   * signed messages according to the OpenPGP standard (RFC 2440).
   */
  if (!mSettings.HasFlag(nsIDocumentEncoder::OutputPreformatted) &&
      (aSoftLineBreak || !IsSignatureSeparator(mCurrentLine.mContent))) {
    mCurrentLine.mContent.Trim(" ", false, true, false);
  }

  if (aSoftLineBreak &&
      mSettings.HasFlag(nsIDocumentEncoder::OutputFormatFlowed) &&
      (mCurrentLine.mIndentation.mLength == 0)) {
    // Add the soft part of the soft linebreak (RFC 2646 4.1)
    // We only do this when there is no indentation since format=flowed
    // lines and indentation doesn't work well together.

    // If breaker character is ASCII space with RFC 3676 support (delsp=yes),
    // add twice space.
    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatDelSp) &&
        aBreakBySpace) {
      mCurrentLine.mContent.AppendLiteral("  ");
    } else {
      mCurrentLine.mContent.Append(char16_t(' '));
    }
  }

  if (aSoftLineBreak) {
    mEmptyLines = 0;
  } else {
    // Hard break
    if (mCurrentLine.HasContentOrIndentationHeader()) {
      mEmptyLines = 0;
    } else {
      mEmptyLines++;
    }
  }

  MOZ_ASSERT(mOutputManager);

  mCurrentLine.MaybeReplaceNbspsInContent(mSettings.GetFlags());

  // If we don't have anything "real" to output we have to
  // make sure the indent doesn't end in a space since that
  // would trick a format=flowed-aware receiver.
  mOutputManager->Append(mCurrentLine,
                         OutputManager::StripTrailingWhitespaces::kMaybe);
  mOutputManager->AppendLineBreak();
  mCurrentLine.ResetContentAndIndentationHeader();
  mInWhitespace = true;
  mLineBreakDue = false;
  mFloatingLines = -1;
}

/**
 * Creates the calculated and stored indent and text in the indentation. That is
 * quote chars and numbers for numbered lists and such.
 */
void nsPlainTextSerializer::CurrentLine::CreateQuotesAndIndent(
    nsAString& aResult) const {
  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel > 0) {
    nsAutoString quotes;
    for (int i = 0; i < mCiteQuoteLevel; i++) {
      quotes.Append(char16_t('>'));
    }
    if (!mContent.IsEmpty()) {
      /* Better don't output a space here, if the line is empty,
         in case a receiving format=flowed-aware UA thinks, this were a flowed
         line, which it isn't - it's just empty. (Flowed lines may be joined
         with the following one, so the empty line may be lost completely.) */
      quotes.Append(char16_t(' '));
    }
    aResult = quotes;
  }

  // Indent if necessary
  int32_t indentwidth = mIndentation.mLength - mIndentation.mHeader.Length();
  if (indentwidth > 0 && HasContentOrIndentationHeader()
      // Don't make empty lines look flowed
  ) {
    nsAutoString spaces;
    for (int i = 0; i < indentwidth; ++i) spaces.Append(char16_t(' '));
    aResult += spaces;
  }

  if (!mIndentation.mHeader.IsEmpty()) {
    aResult += mIndentation.mHeader;
  }
}

static bool IsLineFeedCarriageReturnBlankOrTab(char16_t c) {
  return ('\n' == c || '\r' == c || ' ' == c || '\t' == c);
}

static void ReplaceVisiblyTrailingNbsps(nsAString& aString) {
  const int32_t totLen = aString.Length();
  for (int32_t i = totLen - 1; i >= 0; i--) {
    char16_t c = aString[i];
    if (IsLineFeedCarriageReturnBlankOrTab(c)) {
      continue;
    }
    if (kNBSP == c) {
      aString.Replace(i, 1, ' ');
    } else {
      break;
    }
  }
}

void nsPlainTextSerializer::ConvertToLinesAndOutput(const nsAString& aString) {
  const int32_t totLen = aString.Length();
  int32_t newline{0};

  // Put the mail quote "> " chars in, if appropriate.
  // Have to put it in before every line.
  int32_t bol = 0;
  while (bol < totLen) {
    bool outputLineBreak = false;
    bool spacesOnly = true;

    // Find one of '\n' or '\r' using iterators since nsAString
    // doesn't have the old FindCharInSet function.
    nsAString::const_iterator iter;
    aString.BeginReading(iter);
    nsAString::const_iterator done_searching;
    aString.EndReading(done_searching);
    iter.advance(bol);
    int32_t new_newline = bol;
    newline = kNotFound;
    while (iter != done_searching) {
      if ('\n' == *iter || '\r' == *iter) {
        newline = new_newline;
        break;
      }
      if (' ' != *iter) {
        spacesOnly = false;
      }
      ++new_newline;
      ++iter;
    }

    // Done searching
    nsAutoString stringpart;
    if (newline == kNotFound) {
      // No new lines.
      stringpart.Assign(Substring(aString, bol, totLen - bol));
      if (!stringpart.IsEmpty()) {
        char16_t lastchar = stringpart.Last();
        mInWhitespace = IsLineFeedCarriageReturnBlankOrTab(lastchar);
      }
      mEmptyLines = -1;
      bol = totLen;
    } else {
      // There is a newline
      stringpart.Assign(Substring(aString, bol, newline - bol));
      mInWhitespace = true;
      outputLineBreak = true;
      mEmptyLines = 0;
      bol = newline + 1;
      if ('\r' == *iter && bol < totLen && '\n' == *++iter) {
        // There was a CRLF in the input. This used to be illegal and
        // stripped by the parser. Apparently not anymore. Let's skip
        // over the LF.
        bol++;
      }
    }

    if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatFlowed)) {
      if ((outputLineBreak || !spacesOnly) &&  // bugs 261467,125928
          !IsQuotedLine(stringpart) && !IsSignatureSeparator(stringpart)) {
        stringpart.Trim(" ", false, true, true);
      }
      if (IsSpaceStuffable(stringpart.get()) && !IsQuotedLine(stringpart)) {
        mCurrentLine.mContent.Append(char16_t(' '));
      }
    }
    mCurrentLine.mContent.Append(stringpart);

    mCurrentLine.MaybeReplaceNbspsInContent(mSettings.GetFlags());

    mOutputManager->Append(mCurrentLine,
                           OutputManager::StripTrailingWhitespaces::kNo);
    if (outputLineBreak) {
      mOutputManager->AppendLineBreak();
    }

    mCurrentLine.ResetContentAndIndentationHeader();
  }

#ifdef DEBUG_wrapping
  printf("No wrapping: newline is %d, totLen is %d\n", newline, totLen);
#endif
}

/**
 * Write a string. This is the highlevel function to use to get text output.
 * By using AddToLine, Output, EndLine and other functions it handles quotation,
 * line wrapping, indentation, whitespace compression and other things.
 */
void nsPlainTextSerializer::Write(const nsAString& aStr) {
  // XXX Copy necessary to use nsString methods and gain
  // access to underlying buffer
  nsAutoString str(aStr);

#ifdef DEBUG_wrapping
  printf("Write(%s): wrap col = %d\n", NS_ConvertUTF16toUTF8(str).get(),
         mSettings.GetWrapColumn());
#endif

  const int32_t totLen = str.Length();

  // If the string is empty, do nothing:
  if (totLen <= 0) return;

  // For Flowed text change nbsp-ses to spaces at end of lines to allow them
  // to be cut off along with usual spaces if required. (bug #125928)
  if (mSettings.HasFlag(nsIDocumentEncoder::OutputFormatFlowed)) {
    ReplaceVisiblyTrailingNbsps(str);
  }

  // We have two major codepaths here. One that does preformatted text and one
  // that does normal formatted text. The one for preformatted text calls
  // Output directly while the other code path goes through AddToLine.
  if ((mPreFormattedMail && !mSettings.GetWrapColumn()) ||
      (IsElementPreformatted() && !mPreFormattedMail) ||
      (mSpanLevel > 0 && mEmptyLines >= 0 && IsQuotedLine(str))) {
    // No intelligent wrapping.

    // This mustn't be mixed with intelligent wrapping without clearing
    // the mCurrentLine.mContent buffer before!!!
    NS_ASSERTION(mCurrentLine.mContent.IsEmpty() ||
                     (IsElementPreformatted() && !mPreFormattedMail),
                 "Mixed wrapping data and nonwrapping data on the same line");
    MOZ_ASSERT(mOutputManager);

    if (!mCurrentLine.mContent.IsEmpty()) {
      mOutputManager->Flush(mCurrentLine);
    }

    ConvertToLinesAndOutput(str);
    return;
  }

  // Intelligent handling of text
  // If needed, strip out all "end of lines"
  // and multiple whitespace between words
  int32_t nextpos;
  const char16_t* offsetIntoBuffer = nullptr;

  int32_t bol = 0;
  while (bol < totLen) {  // Loop over lines
    // Find a place where we may have to do whitespace compression
    nextpos = str.FindCharInSet(u" \t\n\r", bol);
#ifdef DEBUG_wrapping
    nsAutoString remaining;
    str.Right(remaining, totLen - bol);
    foo = ToNewCString(remaining);
    // printf("Next line: bol = %d, newlinepos = %d, totLen = %d, "
    //        "string = '%s'\n", bol, nextpos, totLen, foo);
    free(foo);
#endif

    if (nextpos == kNotFound) {
      // The rest of the string
      offsetIntoBuffer = str.get() + bol;
      AddToLine(offsetIntoBuffer, totLen - bol);
      bol = totLen;
      mInWhitespace = false;
    } else {
      // There's still whitespace left in the string
      if (nextpos != 0 && (nextpos + 1) < totLen) {
        offsetIntoBuffer = str.get() + nextpos;
        // skip '\n' if it is between CJ chars
        if (offsetIntoBuffer[0] == '\n' && IS_CJ_CHAR(offsetIntoBuffer[-1]) &&
            IS_CJ_CHAR(offsetIntoBuffer[1])) {
          offsetIntoBuffer = str.get() + bol;
          AddToLine(offsetIntoBuffer, nextpos - bol);
          bol = nextpos + 1;
          continue;
        }
      }
      // If we're already in whitespace and not preformatted, just skip it:
      if (mInWhitespace && (nextpos == bol) && !mPreFormattedMail &&
          !mSettings.HasFlag(nsIDocumentEncoder::OutputPreformatted)) {
        // Skip whitespace
        bol++;
        continue;
      }

      if (nextpos == bol) {
        // Note that we are in whitespace.
        mInWhitespace = true;
        offsetIntoBuffer = str.get() + nextpos;
        AddToLine(offsetIntoBuffer, 1);
        bol++;
        continue;
      }

      mInWhitespace = true;

      offsetIntoBuffer = str.get() + bol;
      if (mPreFormattedMail ||
          mSettings.HasFlag(nsIDocumentEncoder::OutputPreformatted)) {
        // Preserve the real whitespace character
        nextpos++;
        AddToLine(offsetIntoBuffer, nextpos - bol);
        bol = nextpos;
      } else {
        // Replace the whitespace with a space
        AddToLine(offsetIntoBuffer, nextpos - bol);
        AddToLine(kSpace.get(), 1);
        bol = nextpos + 1;  // Let's eat the whitespace
      }
    }
  }  // Continue looping over the string
}

/**
 * Gets the value of an attribute in a string. If the function returns
 * NS_ERROR_NOT_AVAILABLE, there was none such attribute specified.
 */
nsresult nsPlainTextSerializer::GetAttributeValue(const nsAtom* aName,
                                                  nsString& aValueRet) const {
  if (mElement) {
    if (mElement->GetAttr(kNameSpaceID_None, aName, aValueRet)) {
      return NS_OK;
    }
  }

  return NS_ERROR_NOT_AVAILABLE;
}

/**
 * Returns true, if the element was inserted by Moz' TXT->HTML converter.
 * In this case, we should ignore it.
 */
bool nsPlainTextSerializer::IsCurrentNodeConverted() const {
  nsAutoString value;
  nsresult rv = GetAttributeValue(nsGkAtoms::_class, value);
  return (NS_SUCCEEDED(rv) && (value.EqualsIgnoreCase("moz-txt", 7) ||
                               value.EqualsIgnoreCase("\"moz-txt", 8)));
}

// static
nsAtom* nsPlainTextSerializer::GetIdForContent(nsIContent* aContent) {
  if (!aContent->IsHTMLElement()) {
    return nullptr;
  }

  nsAtom* localName = aContent->NodeInfo()->NameAtom();
  return localName->IsStatic() ? localName : nullptr;
}

bool nsPlainTextSerializer::IsElementPreformatted() const {
  return !mPreformatStack.empty() && mPreformatStack.top();
}

bool nsPlainTextSerializer::IsElementPreformatted(Element* aElement) {
  RefPtr<const ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aElement);
  if (computedStyle) {
    const nsStyleText* textStyle = computedStyle->StyleText();
    return textStyle->WhiteSpaceOrNewlineIsSignificant();
  }
  // Fall back to looking at the tag, in case there is no style information.
  return GetIdForContent(aElement) == nsGkAtoms::pre;
}

bool nsPlainTextSerializer::IsCssBlockLevelElement(Element* aElement) {
  RefPtr<const ComputedStyle> computedStyle =
      nsComputedDOMStyle::GetComputedStyleNoFlush(aElement);
  if (computedStyle) {
    const nsStyleDisplay* displayStyle = computedStyle->StyleDisplay();
    return displayStyle->IsBlockOutsideStyle();
  }
  // Fall back to looking at the tag, in case there is no style information.
  return nsContentUtils::IsHTMLBlockLevelElement(aElement);
}

/**
 * This method is required only to identify LI's inside OL.
 * Returns TRUE if we are inside an OL tag and FALSE otherwise.
 */
bool nsPlainTextSerializer::IsInOL() const {
  int32_t i = mTagStackIndex;
  while (--i >= 0) {
    if (mTagStack[i] == nsGkAtoms::ol) return true;
    if (mTagStack[i] == nsGkAtoms::ul) {
      // If a UL is reached first, LI belongs the UL nested in OL.
      return false;
    }
  }
  // We may reach here for orphan LI's.
  return false;
}

bool nsPlainTextSerializer::IsInOlOrUl() const {
  return (mULCount > 0) || !mOLStack.IsEmpty();
}

/*
  @return 0 = no header, 1 = h1, ..., 6 = h6
*/
int32_t HeaderLevel(const nsAtom* aTag) {
  if (aTag == nsGkAtoms::h1) {
    return 1;
  }
  if (aTag == nsGkAtoms::h2) {
    return 2;
  }
  if (aTag == nsGkAtoms::h3) {
    return 3;
  }
  if (aTag == nsGkAtoms::h4) {
    return 4;
  }
  if (aTag == nsGkAtoms::h5) {
    return 5;
  }
  if (aTag == nsGkAtoms::h6) {
    return 6;
  }
  return 0;
}

/* These functions define the column width of an ISO 10646 character
 * as follows:
 *
 *    - The null character (U+0000) has a column width of 0.
 *
 *    - Other C0/C1 control characters and DEL will lead to a return
 *      value of -1.
 *
 *    - Non-spacing and enclosing combining characters (general
 *      category code Mn or Me in the Unicode database) have a
 *      column width of 0.
 *
 *    - Spacing characters in the East Asian Wide (W) or East Asian
 *      FullWidth (F) category as defined in Unicode Technical
 *      Report #11 have a column width of 2.
 *
 *    - All remaining characters (including all printable
 *      ISO 8859-1 and WGL4 characters, Unicode control characters,
 *      etc.) have a column width of 1.
 */

int32_t GetUnicharWidth(char32_t aCh) {
  /* test for 8-bit control characters */
  if (aCh == 0) {
    return 0;
  }
  if (aCh < 32 || (aCh >= 0x7f && aCh < 0xa0)) {
    return -1;
  }

  /* The first combining char in Unicode is U+0300 */
  if (aCh < 0x0300) {
    return 1;
  }

  auto gc = unicode::GetGeneralCategory(aCh);
  if (gc == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK ||
      gc == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) {
    return 0;
  }

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  /* fast test for majority of non-wide scripts */
  if (aCh < 0x1100) {
    return 1;
  }

  return intl::UnicodeProperties::IsEastAsianWidthFW(aCh) ? 2 : 1;
}

int32_t GetUnicharStringWidth(Span<const char16_t> aString) {
  int32_t width = 0;
  for (auto iter = aString.begin(); iter != aString.end(); ++iter) {
    char32_t c = *iter;
    if (NS_IS_HIGH_SURROGATE(c) && (iter + 1) != aString.end() &&
        NS_IS_LOW_SURROGATE(*(iter + 1))) {
      c = SURROGATE_TO_UCS4(c, *++iter);
    }
    const int32_t w = GetUnicharWidth(c);
    // Taking 1 as the width of non-printable character, for bug 94475.
    width += (w < 0 ? 1 : w);
  }
  return width;
}
