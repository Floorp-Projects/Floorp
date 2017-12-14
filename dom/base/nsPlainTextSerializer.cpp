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
#include "nsIServiceManager.h"
#include "nsGkAtoms.h"
#include "nsNameSpaceManager.h"
#include "nsTextFragment.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Preferences.h"
#include "mozilla/BinarySearch.h"
#include "nsComputedDOMStyle.h"

namespace mozilla {
class Encoding;
}

using namespace mozilla;
using namespace mozilla::dom;

#define PREF_STRUCTS "converter.html2txt.structs"
#define PREF_HEADER_STRATEGY "converter.html2txt.header_strategy"
#define PREF_ALWAYS_INCLUDE_RUBY "converter.html2txt.always_include_ruby"

static const  int32_t kTabSize=4;
static const  int32_t kIndentSizeHeaders = 2;  /* Indention of h1, if
                                                mHeaderStrategy = 1 or = 2.
                                                Indention of other headers
                                                is derived from that.
                                                XXX center h1? */
static const  int32_t kIndentIncrementHeaders = 2;  /* If mHeaderStrategy = 1,
                                                indent h(x+1) this many
                                                columns more than h(x) */
static const  int32_t kIndentSizeList = kTabSize;
                               // Indention of non-first lines of ul and ol
static const  int32_t kIndentSizeDD = kTabSize;  // Indention of <dd>
static const  char16_t  kNBSP = 160;
static const  char16_t kSPACE = ' ';

static int32_t HeaderLevel(nsAtom* aTag);
static int32_t GetUnicharWidth(char16_t ucs);
static int32_t GetUnicharStringWidth(const char16_t* pwcs, int32_t n);

// Someday may want to make this non-const:
static const uint32_t TagStackSize = 500;
static const uint32_t OLStackSize = 100;

static bool gPreferenceInitialized = false;
static bool gAlwaysIncludeRuby = false;

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPlainTextSerializer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPlainTextSerializer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPlainTextSerializer)
  NS_INTERFACE_MAP_ENTRY(nsIContentSerializer)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(nsPlainTextSerializer,
                         mElement)

nsresult
NS_NewPlainTextSerializer(nsIContentSerializer** aSerializer)
{
  RefPtr<nsPlainTextSerializer> it = new nsPlainTextSerializer();
  it.forget(aSerializer);
  return NS_OK;
}

nsPlainTextSerializer::nsPlainTextSerializer()
  : kSpace(NS_LITERAL_STRING(" ")) // Init of "constant"
{

  mOutputString = nullptr;
  mHeadLevel = 0;
  mAtFirstColumn = true;
  mIndent = 0;
  mCiteQuoteLevel = 0;
  mStructs = true;       // will be read from prefs later
  mHeaderStrategy = 1 /*indent increasingly*/;   // ditto
  mHasWrittenCiteBlockquote = false;
  mSpanLevel = 0;
  for (int32_t i = 0; i <= 6; i++) {
    mHeaderCounter[i] = 0;
  }

  // Line breaker
  mWrapColumn = 72;     // XXX magic number, we expect someone to reset this
  mCurrentLineWidth = 0;

  // Flow
  mEmptyLines = 1; // The start of the document is an "empty line" in itself,
  mInWhitespace = false;
  mPreFormattedMail = false;
  mStartedOutput = false;

  mPreformattedBlockBoundary = false;
  mWithRubyAnnotation = false;  // will be read from pref and flag later

  // initialize the tag stack to zero:
  // The stack only ever contains pointers to static atoms, so they don't
  // need refcounting.
  mTagStack = new nsAtom*[TagStackSize];
  mTagStackIndex = 0;
  mIgnoreAboveIndex = (uint32_t)kNotFound;

  // initialize the OL stack, where numbers for ordered lists are kept
  mOLStack = new int32_t[OLStackSize];
  mOLStackIndex = 0;

  mULCount = 0;

  mIgnoredChildNodeLevel = 0;

  if (!gPreferenceInitialized) {
    Preferences::AddBoolVarCache(&gAlwaysIncludeRuby, PREF_ALWAYS_INCLUDE_RUBY,
                                 true);
    gPreferenceInitialized = true;
  }
}

nsPlainTextSerializer::~nsPlainTextSerializer()
{
  delete[] mTagStack;
  delete[] mOLStack;
  NS_WARNING_ASSERTION(mHeadLevel == 0, "Wrong head level!");
}

NS_IMETHODIMP
nsPlainTextSerializer::Init(uint32_t aFlags,
                            uint32_t aWrapColumn,
                            const Encoding* aEncoding,
                            bool aIsCopying,
                            bool aIsWholeDocument,
                            bool* aNeedsPreformatScanning)
{
#ifdef DEBUG
  // Check if the major control flags are set correctly.
  if (aFlags & nsIDocumentEncoder::OutputFormatFlowed) {
    NS_ASSERTION(aFlags & nsIDocumentEncoder::OutputFormatted,
                 "If you want format=flowed, you must combine it with "
                 "nsIDocumentEncoder::OutputFormatted");
  }

  if (aFlags & nsIDocumentEncoder::OutputFormatted) {
    NS_ASSERTION(!(aFlags & nsIDocumentEncoder::OutputPreformatted),
                 "Can't do formatted and preformatted output at the same time!");
  }
#endif

  *aNeedsPreformatScanning = true;
  mFlags = aFlags;
  mWrapColumn = aWrapColumn;

  // Only create a linebreaker if we will handle wrapping.
  if (MayWrap() && MayBreakLines()) {
    mLineBreaker = nsContentUtils::LineBreaker();
  }

  // Set the line break character:
  if ((mFlags & nsIDocumentEncoder::OutputCRLineBreak)
      && (mFlags & nsIDocumentEncoder::OutputLFLineBreak)) {
    // Windows
    mLineBreak.AssignLiteral("\r\n");
  }
  else if (mFlags & nsIDocumentEncoder::OutputCRLineBreak) {
    // Mac
    mLineBreak.Assign(char16_t('\r'));
  }
  else if (mFlags & nsIDocumentEncoder::OutputLFLineBreak) {
    // Unix/DOM
    mLineBreak.Assign(char16_t('\n'));
  }
  else {
    // Platform/default
    mLineBreak.AssignLiteral(NS_LINEBREAK);
  }

  mLineBreakDue = false;
  mFloatingLines = -1;

  mPreformattedBlockBoundary = false;

  if (mFlags & nsIDocumentEncoder::OutputFormatted) {
    // Get some prefs that controls how we do formatted output
    mStructs = Preferences::GetBool(PREF_STRUCTS, mStructs);

    mHeaderStrategy =
      Preferences::GetInt(PREF_HEADER_STRATEGY, mHeaderStrategy);
  }

  // The pref is default inited to false in libpref, but we use true
  // as fallback value because we don't want to affect behavior in
  // other places which use this serializer currently.
  mWithRubyAnnotation =
    gAlwaysIncludeRuby ||
    (mFlags & nsIDocumentEncoder::OutputRubyAnnotation);

  // XXX We should let the caller decide whether to do this or not
  mFlags &= ~nsIDocumentEncoder::OutputNoFramesContent;

  return NS_OK;
}

bool
nsPlainTextSerializer::GetLastBool(const nsTArray<bool>& aStack)
{
  uint32_t size = aStack.Length();
  if (size == 0) {
    return false;
  }
  return aStack.ElementAt(size-1);
}

void
nsPlainTextSerializer::SetLastBool(nsTArray<bool>& aStack, bool aValue)
{
  uint32_t size = aStack.Length();
  if (size > 0) {
    aStack.ElementAt(size-1) = aValue;
  }
  else {
    NS_ERROR("There is no \"Last\" value");
  }
}

void
nsPlainTextSerializer::PushBool(nsTArray<bool>& aStack, bool aValue)
{
    aStack.AppendElement(bool(aValue));
}

bool
nsPlainTextSerializer::PopBool(nsTArray<bool>& aStack)
{
  bool returnValue = false;
  uint32_t size = aStack.Length();
  if (size > 0) {
    returnValue = aStack.ElementAt(size-1);
    aStack.RemoveElementAt(size-1);
  }
  return returnValue;
}

bool
nsPlainTextSerializer::ShouldReplaceContainerWithPlaceholder(nsAtom* aTag)
{
  // If nsIDocumentEncoder::OutputNonTextContentAsPlaceholder is set,
  // non-textual container element should be serialized as placeholder
  // character and its child nodes should be ignored. See bug 895239.
  if (!(mFlags & nsIDocumentEncoder::OutputNonTextContentAsPlaceholder)) {
    return false;
  }

  return
    (aTag == nsGkAtoms::audio) ||
    (aTag == nsGkAtoms::canvas) ||
    (aTag == nsGkAtoms::iframe) ||
    (aTag == nsGkAtoms::meter) ||
    (aTag == nsGkAtoms::progress) ||
    (aTag == nsGkAtoms::object) ||
    (aTag == nsGkAtoms::svg) ||
    (aTag == nsGkAtoms::video);
}

bool
nsPlainTextSerializer::IsIgnorableRubyAnnotation(nsAtom* aTag)
{
  if (mWithRubyAnnotation) {
    return false;
  }

  return
    aTag == nsGkAtoms::rp ||
    aTag == nsGkAtoms::rt ||
    aTag == nsGkAtoms::rtc;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendText(nsIContent* aText,
                                  int32_t aStartOffset,
                                  int32_t aEndOffset,
                                  nsAString& aStr)
{
  if (mIgnoreAboveIndex != (uint32_t)kNotFound) {
    return NS_OK;
  }

  NS_ASSERTION(aStartOffset >= 0, "Negative start offset for text fragment!");
  if ( aStartOffset < 0 )
    return NS_ERROR_INVALID_ARG;

  NS_ENSURE_ARG(aText);

  nsresult rv = NS_OK;

  nsIContent* content = aText;
  const nsTextFragment* frag;
  if (!content || !(frag = content->GetText())) {
    return NS_ERROR_FAILURE;
  }

  int32_t fragLength = frag->GetLength();
  int32_t endoffset = (aEndOffset == -1) ? fragLength : std::min(aEndOffset, fragLength);
  NS_ASSERTION(aStartOffset <= endoffset, "A start offset is beyond the end of the text fragment!");

  int32_t length = endoffset - aStartOffset;
  if (length <= 0) {
    return NS_OK;
  }

  nsAutoString textstr;
  if (frag->Is2b()) {
    textstr.Assign(frag->Get2b() + aStartOffset, length);
  }
  else {
    // AssignASCII is for 7-bit character only, so don't use it
    const char *data = frag->Get1b();
    CopyASCIItoUTF16(Substring(data + aStartOffset, data + endoffset), textstr);
  }

  mOutputString = &aStr;

  // We have to split the string across newlines
  // to match parser behavior
  int32_t start = 0;
  int32_t offset = textstr.FindCharInSet("\n\r");
  while (offset != kNotFound) {

    if (offset>start) {
      // Pass in the line
      DoAddText(false,
                Substring(textstr, start, offset-start));
    }

    // Pass in a newline
    DoAddText(true, mLineBreak);

    start = offset+1;
    offset = textstr.FindCharInSet("\n\r", start);
  }

  // Consume the last bit of the string if there's any left
  if (start < length) {
    if (start) {
      DoAddText(false, Substring(textstr, start, length - start));
    }
    else {
      DoAddText(false, textstr);
    }
  }

  mOutputString = nullptr;

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendCDATASection(nsIContent* aCDATASection,
                                          int32_t aStartOffset,
                                          int32_t aEndOffset,
                                          nsAString& aStr)
{
  return AppendText(aCDATASection, aStartOffset, aEndOffset, aStr);
}

NS_IMETHODIMP
nsPlainTextSerializer::ScanElementForPreformat(Element* aElement)
{
  mPreformatStack.push(IsElementPreformatted(aElement));
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::ForgetElementForPreformat(Element* aElement)
{
  MOZ_RELEASE_ASSERT(!mPreformatStack.empty(), "Tried to pop without previous push.");
  mPreformatStack.pop();
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendElementStart(Element* aElement,
                                          Element* aOriginalElement,
                                          nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  nsresult rv;
  nsAtom* id = GetIdForContent(mElement);

  bool isContainer = !FragmentOrElement::IsHTMLVoid(id);

  mOutputString = &aStr;

  if (isContainer) {
    rv = DoOpenContainer(id);
  }
  else {
    rv = DoAddLeaf(id);
  }

  mElement = nullptr;
  mOutputString = nullptr;

  if (id == nsGkAtoms::head) {
    ++mHeadLevel;
  }

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendElementEnd(Element* aElement,
                                        nsAString& aStr)
{
  NS_ENSURE_ARG(aElement);

  mElement = aElement;

  nsresult rv;
  nsAtom* id = GetIdForContent(mElement);

  bool isContainer = !FragmentOrElement::IsHTMLVoid(id);

  mOutputString = &aStr;

  rv = NS_OK;
  if (isContainer) {
    rv = DoCloseContainer(id);
  }

  mElement = nullptr;
  mOutputString = nullptr;

  if (id == nsGkAtoms::head) {
    NS_ASSERTION(mHeadLevel != 0,
                 "mHeadLevel being decremented below 0");
    --mHeadLevel;
  }

  return rv;
}

NS_IMETHODIMP
nsPlainTextSerializer::Flush(nsAString& aStr)
{
  mOutputString = &aStr;
  FlushLine();
  mOutputString = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
nsPlainTextSerializer::AppendDocumentStart(nsIDocument *aDocument,
                                           nsAString& aStr)
{
  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoOpenContainer(nsAtom* aTag)
{
  // Check if we need output current node as placeholder character and ignore
  // child nodes.
  if (ShouldReplaceContainerWithPlaceholder(mElement->NodeInfo()->NameAtom())) {
    if (mIgnoredChildNodeLevel == 0) {
      // Serialize current node as placeholder character
      Write(NS_LITERAL_STRING(u"\xFFFC"));
    }
    // Ignore child nodes.
    mIgnoredChildNodeLevel++;
    return NS_OK;
  }
  if (IsIgnorableRubyAnnotation(aTag)) {
    // Ignorable ruby annotation shouldn't be replaced by a placeholder
    // character, neither any of its descendants.
    mIgnoredChildNodeLevel++;
    return NS_OK;
  }

  if (mFlags & nsIDocumentEncoder::OutputForPlainTextClipboardCopy) {
    if (mPreformattedBlockBoundary && DoOutput()) {
      // Should always end a line, but get no more whitespace
      if (mFloatingLines < 0)
        mFloatingLines = 0;
      mLineBreakDue = true;
    }
    mPreformattedBlockBoundary = false;
  }

  if (mFlags & nsIDocumentEncoder::OutputRaw) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.  I mean it!  Don't make me smack you!

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
  mHasWrittenCiteBlockquote = mHasWrittenCiteBlockquote &&
                              aTag == nsGkAtoms::pre;

  bool isInCiteBlockquote = false;

  // XXX special-case <blockquote type=cite> so that we don't add additional
  // newlines before the text.
  if (aTag == nsGkAtoms::blockquote) {
    nsAutoString value;
    nsresult rv = GetAttributeValue(nsGkAtoms::type, value);
    isInCiteBlockquote = NS_SUCCEEDED(rv) && value.EqualsIgnoreCase("cite");
  }

  if (mLineBreakDue && !isInCiteBlockquote)
    EnsureVerticalSpace(mFloatingLines);

  // Check if this tag's content that should not be output
  if ((aTag == nsGkAtoms::noscript &&
       !(mFlags & nsIDocumentEncoder::OutputNoScriptContent)) ||
      ((aTag == nsGkAtoms::iframe || aTag == nsGkAtoms::noframes) &&
       !(mFlags & nsIDocumentEncoder::OutputNoFramesContent))) {
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
    // Also set mWrapColumn to the value given there
    // (which arguably we should only do if told to do so).
    nsAutoString style;
    int32_t whitespace;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::style, style)) &&
       (kNotFound != (whitespace = style.Find("white-space:")))) {

      if (kNotFound != style.Find("pre-wrap", true, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormattedMail based on style pre-wrap\n");
#endif
        mPreFormattedMail = true;
        int32_t widthOffset = style.Find("width:");
        if (widthOffset >= 0) {
          // We have to search for the ch before the semicolon,
          // not for the semicolon itself, because nsString::ToInteger()
          // considers 'c' to be a valid numeric char (even if radix=10)
          // but then gets confused if it sees it next to the number
          // when the radix specified was 10, and returns an error code.
          int32_t semiOffset = style.Find("ch", false, widthOffset+6);
          int32_t length = (semiOffset > 0 ? semiOffset - widthOffset - 6
                            : style.Length() - widthOffset);
          nsAutoString widthstr;
          style.Mid(widthstr, widthOffset+6, length);
          nsresult err;
          int32_t col = widthstr.ToInteger(&err);

          if (NS_SUCCEEDED(err)) {
            mWrapColumn = (uint32_t)col;
#ifdef DEBUG_preformatted
            printf("Set wrap column to %d based on style\n", mWrapColumn);
#endif
          }
        }
      }
      else if (kNotFound != style.Find("pre", true, whitespace)) {
#ifdef DEBUG_preformatted
        printf("Set mPreFormattedMail based on style pre\n");
#endif
        mPreFormattedMail = true;
        mWrapColumn = 0;
      }
    }
    else {
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
    }
    else
      EnsureVerticalSpace(1);
  }
  else if (aTag == nsGkAtoms::tr) {
    PushBool(mHasWrittenCellsForRow, false);
  }
  else if (aTag == nsGkAtoms::td || aTag == nsGkAtoms::th) {
    // We must make sure that the content of two table cells get a
    // space between them.

    // To make the separation between cells most obvious and
    // importable, we use a TAB.
    if (GetLastBool(mHasWrittenCellsForRow)) {
      // Bypass |Write| so that the TAB isn't compressed away.
      AddToLine(u"\t", 1);
      mInWhitespace = true;
    }
    else if (mHasWrittenCellsForRow.IsEmpty()) {
      // We don't always see a <tr> (nor a <table>) before the <td> if we're
      // copying part of a table
      PushBool(mHasWrittenCellsForRow, true); // will never be popped
    }
    else {
      SetLastBool(mHasWrittenCellsForRow, true);
    }
  }
  else if (aTag == nsGkAtoms::ul) {
    // Indent here to support nested lists, which aren't included in li :-(
    EnsureVerticalSpace(mULCount + mOLStackIndex == 0 ? 1 : 0);
         // Must end the current line before we change indention
    mIndent += kIndentSizeList;
    mULCount++;
  }
  else if (aTag == nsGkAtoms::ol) {
    EnsureVerticalSpace(mULCount + mOLStackIndex == 0 ? 1 : 0);
    if (mFlags & nsIDocumentEncoder::OutputFormatted) {
      // Must end the current line before we change indention
      if (mOLStackIndex < OLStackSize) {
        nsAutoString startAttr;
        int32_t startVal = 1;
        if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::start, startAttr))) {
          nsresult rv = NS_OK;
          startVal = startAttr.ToInteger(&rv);
          if (NS_FAILED(rv))
            startVal = 1;
        }
        mOLStack[mOLStackIndex++] = startVal;
      }
    } else {
      mOLStackIndex++;
    }
    mIndent += kIndentSizeList;  // see ul
  }
  else if (aTag == nsGkAtoms::li &&
           (mFlags & nsIDocumentEncoder::OutputFormatted)) {
    if (mTagStackIndex > 1 && IsInOL()) {
      if (mOLStackIndex > 0) {
        nsAutoString valueAttr;
        if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::value, valueAttr))) {
          nsresult rv = NS_OK;
          int32_t valueAttrVal = valueAttr.ToInteger(&rv);
          if (NS_SUCCEEDED(rv))
            mOLStack[mOLStackIndex-1] = valueAttrVal;
        }
        // This is what nsBulletFrame does for OLs:
        mInIndentString.AppendInt(mOLStack[mOLStackIndex-1]++, 10);
      }
      else {
        mInIndentString.Append(char16_t('#'));
      }

      mInIndentString.Append(char16_t('.'));

    }
    else {
      static char bulletCharArray[] = "*o+#";
      uint32_t index = mULCount > 0 ? (mULCount - 1) : 3;
      char bulletChar = bulletCharArray[index % 4];
      mInIndentString.Append(char16_t(bulletChar));
    }

    mInIndentString.Append(char16_t(' '));
  }
  else if (aTag == nsGkAtoms::dl) {
    EnsureVerticalSpace(1);
  }
  else if (aTag == nsGkAtoms::dt) {
    EnsureVerticalSpace(0);
  }
  else if (aTag == nsGkAtoms::dd) {
    EnsureVerticalSpace(0);
    mIndent += kIndentSizeDD;
  }
  else if (aTag == nsGkAtoms::span) {
    ++mSpanLevel;
  }
  else if (aTag == nsGkAtoms::blockquote) {
    // Push
    PushBool(mIsInCiteBlockquote, isInCiteBlockquote);
    if (isInCiteBlockquote) {
      EnsureVerticalSpace(0);
      mCiteQuoteLevel++;
    }
    else {
      EnsureVerticalSpace(1);
      mIndent += kTabSize; // Check for some maximum value?
    }
  }
  else if (aTag == nsGkAtoms::q) {
    Write(NS_LITERAL_STRING("\""));
  }

  // Else make sure we'll separate block level tags,
  // even if we're about to leave, before doing any other formatting.
  else if (IsElementBlock(mElement)) {
    EnsureVerticalSpace(0);
  }

  //////////////////////////////////////////////////////////////
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }
  //////////////////////////////////////////////////////////////
  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  //////////////////////////////////////////////////////////////

  // Push on stack
  bool currentNodeIsConverted = IsCurrentNodeConverted();

  if (aTag == nsGkAtoms::h1 || aTag == nsGkAtoms::h2 ||
      aTag == nsGkAtoms::h3 || aTag == nsGkAtoms::h4 ||
      aTag == nsGkAtoms::h5 || aTag == nsGkAtoms::h6)
  {
    EnsureVerticalSpace(2);
    if (mHeaderStrategy == 2) {  // numbered
      mIndent += kIndentSizeHeaders;
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
    }
    else if (mHeaderStrategy == 1) { // indent increasingly
      mIndent += kIndentSizeHeaders;
      for (int32_t i = HeaderLevel(aTag); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent += kIndentIncrementHeaders;
      }
    }
  }
  else if (aTag == nsGkAtoms::a && !currentNodeIsConverted) {
    nsAutoString url;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::href, url))
        && !url.IsEmpty()) {
      mURL = url;
    }
  }
  else if (aTag == nsGkAtoms::sup && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("^"));
  }
  else if (aTag == nsGkAtoms::sub && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("_"));
  }
  else if (aTag == nsGkAtoms::code && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((aTag == nsGkAtoms::strong || aTag == nsGkAtoms::b)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((aTag == nsGkAtoms::em || aTag == nsGkAtoms::i)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (aTag == nsGkAtoms::u && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("_"));
  }

  /* Container elements are always block elements, so we shouldn't
     output any whitespace immediately after the container tag even if
     there's extra whitespace there because the HTML is pretty-printed
     or something. To ensure that happens, tell the serializer we're
     already in whitespace so it won't output more. */
  mInWhitespace = true;

  return NS_OK;
}

nsresult
nsPlainTextSerializer::DoCloseContainer(nsAtom* aTag)
{
  if (ShouldReplaceContainerWithPlaceholder(mElement->NodeInfo()->NameAtom())) {
    mIgnoredChildNodeLevel--;
    return NS_OK;
  }
  if (IsIgnorableRubyAnnotation(aTag)) {
    mIgnoredChildNodeLevel--;
    return NS_OK;
  }

  if (mFlags & nsIDocumentEncoder::OutputForPlainTextClipboardCopy) {
    if (DoOutput() && IsInPre() && IsElementBlock(mElement)) {
      // If we're closing a preformatted block element, output a line break
      // when we find a new container.
      mPreformattedBlockBoundary = true;
    }
  }

  if (mFlags & nsIDocumentEncoder::OutputRaw) {
    // Raw means raw.  Don't even think about doing anything fancy
    // here like indenting, adding line breaks or any other
    // characters such as list item bullets, quote characters
    // around <q>, etc.  I mean it!  Don't make me smack you!

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

  // End current line if we're ending a block level tag
  if ((aTag == nsGkAtoms::body) || (aTag == nsGkAtoms::html)) {
    // We want the output to end with a new line,
    // but in preformatted areas like text fields,
    // we can't emit newlines that weren't there.
    // So add the newline only in the case of formatted output.
    if (mFlags & nsIDocumentEncoder::OutputFormatted) {
      EnsureVerticalSpace(0);
    }
    else {
      FlushLine();
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
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = true;
  }
  else if (((aTag == nsGkAtoms::li) ||
            (aTag == nsGkAtoms::dt)) &&
           (mFlags & nsIDocumentEncoder::OutputFormatted)) {
    // Items that should always end a line, but get no more whitespace
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = true;
  }
  else if (aTag == nsGkAtoms::pre) {
    mFloatingLines = GetLastBool(mIsInCiteBlockquote) ? 0 : 1;
    mLineBreakDue = true;
  }
  else if (aTag == nsGkAtoms::ul) {
    FlushLine();
    mIndent -= kIndentSizeList;
    if (--mULCount + mOLStackIndex == 0) {
      mFloatingLines = 1;
      mLineBreakDue = true;
    }
  }
  else if (aTag == nsGkAtoms::ol) {
    FlushLine(); // Doing this after decreasing OLStackIndex would be wrong.
    mIndent -= kIndentSizeList;
    NS_ASSERTION(mOLStackIndex, "Wrong OLStack level!");
    mOLStackIndex--;
    if (mULCount + mOLStackIndex == 0) {
      mFloatingLines = 1;
      mLineBreakDue = true;
    }
  }
  else if (aTag == nsGkAtoms::dl) {
    mFloatingLines = 1;
    mLineBreakDue = true;
  }
  else if (aTag == nsGkAtoms::dd) {
    FlushLine();
    mIndent -= kIndentSizeDD;
  }
  else if (aTag == nsGkAtoms::span) {
    NS_ASSERTION(mSpanLevel, "Span level will be negative!");
    --mSpanLevel;
  }
  else if (aTag == nsGkAtoms::div) {
    if (mFloatingLines < 0)
      mFloatingLines = 0;
    mLineBreakDue = true;
  }
  else if (aTag == nsGkAtoms::blockquote) {
    FlushLine();    // Is this needed?

    // Pop
    bool isInCiteBlockquote = PopBool(mIsInCiteBlockquote);

    if (isInCiteBlockquote) {
      NS_ASSERTION(mCiteQuoteLevel, "CiteQuote level will be negative!");
      mCiteQuoteLevel--;
      mFloatingLines = 0;
      mHasWrittenCiteBlockquote = true;
    }
    else {
      mIndent -= kTabSize;
      mFloatingLines = 1;
    }
    mLineBreakDue = true;
  }
  else if (aTag == nsGkAtoms::q) {
    Write(NS_LITERAL_STRING("\""));
  }
  else if (IsElementBlock(mElement) && aTag != nsGkAtoms::script) {
    // All other blocks get 1 vertical space after them
    // in formatted mode, otherwise 0.
    // This is hard. Sometimes 0 is a better number, but
    // how to know?
    if (mFlags & nsIDocumentEncoder::OutputFormatted)
      EnsureVerticalSpace(1);
    else {
      if (mFloatingLines < 0)
        mFloatingLines = 0;
      mLineBreakDue = true;
    }
  }

  //////////////////////////////////////////////////////////////
  if (!(mFlags & nsIDocumentEncoder::OutputFormatted)) {
    return NS_OK;
  }
  //////////////////////////////////////////////////////////////
  // The rest of this routine is formatted output stuff,
  // which we should skip if we're not formatted:
  //////////////////////////////////////////////////////////////

  // Pop the currentConverted stack
  bool currentNodeIsConverted = IsCurrentNodeConverted();

  if (aTag == nsGkAtoms::h1 || aTag == nsGkAtoms::h2 ||
      aTag == nsGkAtoms::h3 || aTag == nsGkAtoms::h4 ||
      aTag == nsGkAtoms::h5 || aTag == nsGkAtoms::h6) {

    if (mHeaderStrategy) {  /*numbered or indent increasingly*/
      mIndent -= kIndentSizeHeaders;
    }
    if (mHeaderStrategy == 1 /*indent increasingly*/ ) {
      for (int32_t i = HeaderLevel(aTag); i > 1; i--) {
           // for h(x), run x-1 times
        mIndent -= kIndentIncrementHeaders;
      }
    }
    EnsureVerticalSpace(1);
  }
  else if (aTag == nsGkAtoms::a && !currentNodeIsConverted && !mURL.IsEmpty()) {
    nsAutoString temp;
    temp.AssignLiteral(" <");
    temp += mURL;
    temp.Append(char16_t('>'));
    Write(temp);
    mURL.Truncate();
  }
  else if ((aTag == nsGkAtoms::sup || aTag == nsGkAtoms::sub)
           && mStructs && !currentNodeIsConverted) {
    Write(kSpace);
  }
  else if (aTag == nsGkAtoms::code && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("|"));
  }
  else if ((aTag == nsGkAtoms::strong || aTag == nsGkAtoms::b)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("*"));
  }
  else if ((aTag == nsGkAtoms::em || aTag == nsGkAtoms::i)
           && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("/"));
  }
  else if (aTag == nsGkAtoms::u && mStructs && !currentNodeIsConverted) {
    Write(NS_LITERAL_STRING("_"));
  }

  return NS_OK;
}

bool
nsPlainTextSerializer::MustSuppressLeaf()
{
  if (mIgnoredChildNodeLevel > 0) {
    return true;
  }

  if ((mTagStackIndex > 1 &&
       mTagStack[mTagStackIndex-2] == nsGkAtoms::select) ||
      (mTagStackIndex > 0 &&
        mTagStack[mTagStackIndex-1] == nsGkAtoms::select)) {
    // Don't output the contents of SELECT elements;
    // Might be nice, eventually, to output just the selected element.
    // Read more in bug 31994.
    return true;
  }

  if (mTagStackIndex > 0 &&
      (mTagStack[mTagStackIndex-1] == nsGkAtoms::script ||
       mTagStack[mTagStackIndex-1] == nsGkAtoms::style)) {
    // Don't output the contents of <script> or <style> tags;
    return true;
  }

  return false;
}

void
nsPlainTextSerializer::DoAddText(bool aIsLineBreak, const nsAString& aText)
{
  // If we don't want any output, just return
  if (!DoOutput()) {
    return;
  }

  if (!aIsLineBreak) {
    // Make sure to reset this, since it's no longer true.
    mHasWrittenCiteBlockquote = false;
  }

  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

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
    if ((mFlags & nsIDocumentEncoder::OutputPreformatted) ||
        (mPreFormattedMail && !mWrapColumn) ||
        IsInPre()) {
      EnsureVerticalSpace(mEmptyLines+1);
    }
    else if (!mInWhitespace) {
      Write(kSpace);
      mInWhitespace = true;
    }
    return;
  }

  /* Check, if we are in a link (symbolized with mURL containing the URL)
     and the text is equal to the URL. In that case we don't want to output
     the URL twice so we scrap the text in mURL. */
  if (!mURL.IsEmpty() && mURL.Equals(aText)) {
    mURL.Truncate();
  }
  Write(aText);
}

nsresult
nsPlainTextSerializer::DoAddLeaf(nsAtom* aTag)
{
  mPreformattedBlockBoundary = false;

  // If we don't want any output, just return
  if (!DoOutput()) {
    return NS_OK;
  }

  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

  if (MustSuppressLeaf()) {
    return NS_OK;
  }

  if (aTag == nsGkAtoms::br) {
    // Another egregious editor workaround, see bug 38194:
    // ignore the bogus br tags that the editor sticks here and there.
    nsAutoString tagAttr;
    if (NS_FAILED(GetAttributeValue(nsGkAtoms::type, tagAttr))
        || !tagAttr.EqualsLiteral("_moz")) {
      EnsureVerticalSpace(mEmptyLines+1);
    }
  }
  else if (aTag == nsGkAtoms::hr &&
           (mFlags & nsIDocumentEncoder::OutputFormatted)) {
    EnsureVerticalSpace(0);

    // Make a line of dashes as wide as the wrap width
    // XXX honoring percentage would be nice
    nsAutoString line;
    uint32_t width = (mWrapColumn > 0 ? mWrapColumn : 25);
    while (line.Length() < width) {
      line.Append(char16_t('-'));
    }
    Write(line);

    EnsureVerticalSpace(0);
  }
  else if (mFlags & nsIDocumentEncoder::OutputNonTextContentAsPlaceholder) {
    Write(NS_LITERAL_STRING(u"\xFFFC"));
  }
  else if (aTag == nsGkAtoms::img) {
    /* Output (in decreasing order of preference)
       alt, title or nothing */
    // See <http://www.w3.org/TR/REC-html40/struct/objects.html#edef-IMG>
    nsAutoString imageDescription;
    if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::alt,
                                       imageDescription))) {
      // If the alt attribute has an empty value (|alt=""|), output nothing
    }
    else if (NS_SUCCEEDED(GetAttributeValue(nsGkAtoms::title,
                                            imageDescription))
             && !imageDescription.IsEmpty()) {
      imageDescription = NS_LITERAL_STRING(" [") +
                         imageDescription +
                         NS_LITERAL_STRING("] ");
    }

    Write(imageDescription);
  }

  return NS_OK;
}

/**
 * Adds as many newline as necessary to get |noOfRows| empty lines
 *
 * noOfRows = -1    :   Being in the middle of some line of text
 * noOfRows =  0    :   Being at the start of a line
 * noOfRows =  n>0  :   Having n empty lines before the current line.
 */
void
nsPlainTextSerializer::EnsureVerticalSpace(int32_t noOfRows)
{
  // If we have something in the indent we probably want to output
  // it and it's not included in the count for empty lines so we don't
  // realize that we should start a new line.
  if (noOfRows >= 0 && !mInIndentString.IsEmpty()) {
    EndLine(false);
    mInWhitespace = true;
  }

  while(mEmptyLines < noOfRows) {
    EndLine(false);
    mInWhitespace = true;
  }
  mLineBreakDue = false;
  mFloatingLines = -1;
}

/**
 * This empties the current line cache without adding a NEWLINE.
 * Should not be used if line wrapping is of importance since
 * this function destroys the cache information.
 *
 * It will also write indentation and quotes if we believe us to be
 * at the start of the line.
 */
void
nsPlainTextSerializer::FlushLine()
{
  if (!mCurrentLine.IsEmpty()) {
    if (mAtFirstColumn) {
      OutputQuotesAndIndent(); // XXX: Should we always do this? Bug?
    }

    Output(mCurrentLine);
    mAtFirstColumn = mAtFirstColumn && mCurrentLine.IsEmpty();
    mCurrentLine.Truncate();
    mCurrentLineWidth = 0;
  }
}

/**
 * Prints the text to output to our current output device (the string mOutputString).
 * The only logic here is to replace non breaking spaces with a normal space since
 * most (all?) receivers of the result won't understand the nbsp and even be
 * confused by it.
 */
void
nsPlainTextSerializer::Output(nsString& aString)
{
  if (!aString.IsEmpty()) {
    mStartedOutput = true;
  }

  if (!(mFlags & nsIDocumentEncoder::OutputPersistNBSP)) {
    // First, replace all nbsp characters with spaces,
    // which the unicode encoder won't do for us.
    aString.ReplaceChar(kNBSP, kSPACE);
  }
  mOutputString->Append(aString);
}

static bool
IsSpaceStuffable(const char16_t *s)
{
  if (s[0] == '>' || s[0] == ' ' || s[0] == kNBSP ||
      nsCRT::strncmp(s, u"From ", 5) == 0)
    return true;
  else
    return false;
}

/**
 * This function adds a piece of text to the current stored line. If we are
 * wrapping text and the stored line will become too long, a suitable
 * location to wrap will be found and the line that's complete will be
 * output.
 */
void
nsPlainTextSerializer::AddToLine(const char16_t * aLineFragment,
                                 int32_t aLineFragmentLength)
{
  uint32_t prefixwidth = (mCiteQuoteLevel > 0 ? mCiteQuoteLevel + 1:0)+mIndent;

  if (mLineBreakDue)
    EnsureVerticalSpace(mFloatingLines);

  int32_t linelength = mCurrentLine.Length();
  if (0 == linelength) {
    if (0 == aLineFragmentLength) {
      // Nothing at all. Are you kidding me?
      return;
    }

    if (mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
      if (IsSpaceStuffable(aLineFragment)
         && mCiteQuoteLevel == 0  // We space-stuff quoted lines anyway
         )
        {
          // Space stuffing a la RFC 2646 (format=flowed).
          mCurrentLine.Append(char16_t(' '));

          if (MayWrap()) {
            mCurrentLineWidth += GetUnicharWidth(' ');
#ifdef DEBUG_wrapping
            NS_ASSERTION(GetUnicharStringWidth(mCurrentLine.get(),
                                               mCurrentLine.Length()) ==
                         (int32_t)mCurrentLineWidth,
                         "mCurrentLineWidth and reality out of sync!");
#endif
          }
        }
    }
    mEmptyLines=-1;
  }

  mCurrentLine.Append(aLineFragment, aLineFragmentLength);
  if (MayWrap()) {
    mCurrentLineWidth += GetUnicharStringWidth(aLineFragment,
                                               aLineFragmentLength);
#ifdef DEBUG_wrapping
    NS_ASSERTION(GetUnicharstringWidth(mCurrentLine.get(),
                                       mCurrentLine.Length()) ==
                 (int32_t)mCurrentLineWidth,
                 "mCurrentLineWidth and reality out of sync!");
#endif
  }

  linelength = mCurrentLine.Length();

  //  Wrap?
  if (MayWrap())
  {
#ifdef DEBUG_wrapping
    NS_ASSERTION(GetUnicharstringWidth(mCurrentLine.get(),
                                  mCurrentLine.Length()) ==
                 (int32_t)mCurrentLineWidth,
                 "mCurrentLineWidth and reality out of sync!");
#endif
    // Yes, wrap!
    // The "+4" is to avoid wrap lines that only would be a couple
    // of letters too long. We give this bonus only if the
    // wrapcolumn is more than 20.
    uint32_t bonuswidth = (mWrapColumn > 20) ? 4 : 0;

    // XXX: Should calculate prefixwidth with GetUnicharStringWidth
    while(mCurrentLineWidth+prefixwidth > mWrapColumn+bonuswidth) {
      // We go from the end removing one letter at a time until
      // we have a reasonable width
      int32_t goodSpace = mCurrentLine.Length();
      uint32_t width = mCurrentLineWidth;
      while(goodSpace > 0 && (width+prefixwidth > mWrapColumn)) {
        goodSpace--;
        width -= GetUnicharWidth(mCurrentLine[goodSpace]);
      }

      goodSpace++;

      if (mLineBreaker) {
        goodSpace = mLineBreaker->Prev(mCurrentLine.get(),
                                    mCurrentLine.Length(), goodSpace);
        if (goodSpace != NS_LINEBREAKER_NEED_MORE_TEXT &&
            nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace-1))) {
          --goodSpace;    // adjust the position since line breaker returns a position next to space
        }
      }
      // fallback if the line breaker is unavailable or failed
      if (!mLineBreaker) {
        if (mCurrentLine.IsEmpty() || mWrapColumn < prefixwidth) {
          goodSpace = NS_LINEBREAKER_NEED_MORE_TEXT;
        } else {
          goodSpace = std::min(mWrapColumn - prefixwidth, mCurrentLine.Length() - 1);
          while (goodSpace >= 0 &&
                 !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
            goodSpace--;
          }
        }
      }

      nsAutoString restOfLine;
      if (goodSpace == NS_LINEBREAKER_NEED_MORE_TEXT) {
        // If we didn't find a good place to break, accept long line and
        // try to find another place to break
        goodSpace=(prefixwidth>mWrapColumn+1)?1:mWrapColumn-prefixwidth+1;
        if (mLineBreaker) {
          if ((uint32_t)goodSpace < mCurrentLine.Length())
            goodSpace = mLineBreaker->Next(mCurrentLine.get(),
                                           mCurrentLine.Length(), goodSpace);
          if (goodSpace == NS_LINEBREAKER_NEED_MORE_TEXT)
            goodSpace = mCurrentLine.Length();
        }
        // fallback if the line breaker is unavailable or failed
        if (!mLineBreaker) {
          goodSpace=(prefixwidth>mWrapColumn)?1:mWrapColumn-prefixwidth;
          while (goodSpace < linelength &&
                 !nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
            goodSpace++;
          }
        }
      }

      if ((goodSpace < linelength) && (goodSpace > 0)) {
        // Found a place to break

        // -1 (trim a char at the break position)
        // only if the line break was a space.
        if (nsCRT::IsAsciiSpace(mCurrentLine.CharAt(goodSpace))) {
          mCurrentLine.Right(restOfLine, linelength-goodSpace-1);
        }
        else {
          mCurrentLine.Right(restOfLine, linelength-goodSpace);
        }
        // if breaker was U+0020, it has to consider for delsp=yes support
        bool breakBySpace = mCurrentLine.CharAt(goodSpace) == ' ';
        mCurrentLine.Truncate(goodSpace);
        EndLine(true, breakBySpace);
        mCurrentLine.Truncate();
        // Space stuff new line?
        if (mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
          if (!restOfLine.IsEmpty() && IsSpaceStuffable(restOfLine.get())
              && mCiteQuoteLevel == 0  // We space-stuff quoted lines anyway
            )
          {
            // Space stuffing a la RFC 2646 (format=flowed).
            mCurrentLine.Append(char16_t(' '));
            //XXX doesn't seem to work correctly for ' '
          }
        }
        mCurrentLine.Append(restOfLine);
        mCurrentLineWidth = GetUnicharStringWidth(mCurrentLine.get(),
                                                  mCurrentLine.Length());
        linelength = mCurrentLine.Length();
        mEmptyLines = -1;
      }
      else {
        // Nothing to do. Hopefully we get more data later
        // to use for a place to break line
        break;
      }
    }
  }
  else {
    // No wrapping.
  }
}

/**
 * Outputs the contents of mCurrentLine, and resets line specific
 * variables. Also adds an indentation and prefix if there is
 * one specified. Strips ending spaces from the line if it isn't
 * preformatted.
 */
void
nsPlainTextSerializer::EndLine(bool aSoftlinebreak, bool aBreakBySpace)
{
  uint32_t currentlinelength = mCurrentLine.Length();

  if (aSoftlinebreak && 0 == currentlinelength) {
    // No meaning
    return;
  }

  /* In non-preformatted mode, remove spaces from the end of the line for
   * format=flowed compatibility. Don't do this for these special cases:
   * "-- ", the signature separator (RFC 2646) shouldn't be touched and
   * "- -- ", the OpenPGP dash-escaped signature separator in inline
   * signed messages according to the OpenPGP standard (RFC 2440).
   */
  if (!(mFlags & nsIDocumentEncoder::OutputPreformatted) &&
      !(mFlags & nsIDocumentEncoder::OutputDontRemoveLineEndingSpaces) &&
     (aSoftlinebreak ||
     !(mCurrentLine.EqualsLiteral("-- ") || mCurrentLine.EqualsLiteral("- -- ")))) {
    // Remove spaces from the end of the line.
    while(currentlinelength > 0 &&
          mCurrentLine[currentlinelength-1] == ' ') {
      --currentlinelength;
    }
    mCurrentLine.SetLength(currentlinelength);
  }

  if (aSoftlinebreak &&
     (mFlags & nsIDocumentEncoder::OutputFormatFlowed) &&
     (mIndent == 0)) {
    // Add the soft part of the soft linebreak (RFC 2646 4.1)
    // We only do this when there is no indentation since format=flowed
    // lines and indentation doesn't work well together.

    // If breaker character is ASCII space with RFC 3676 support (delsp=yes),
    // add twice space.
    if ((mFlags & nsIDocumentEncoder::OutputFormatDelSp) && aBreakBySpace)
      mCurrentLine.AppendLiteral("  ");
    else
      mCurrentLine.Append(char16_t(' '));
  }

  if (aSoftlinebreak) {
    mEmptyLines=0;
  }
  else {
    // Hard break
    if (!mCurrentLine.IsEmpty() || !mInIndentString.IsEmpty()) {
      mEmptyLines=-1;
    }

    mEmptyLines++;
  }

  if (mAtFirstColumn) {
    // If we don't have anything "real" to output we have to
    // make sure the indent doesn't end in a space since that
    // would trick a format=flowed-aware receiver.
    bool stripTrailingSpaces = mCurrentLine.IsEmpty();
    OutputQuotesAndIndent(stripTrailingSpaces);
  }

  mCurrentLine.Append(mLineBreak);
  Output(mCurrentLine);
  mCurrentLine.Truncate();
  mCurrentLineWidth = 0;
  mAtFirstColumn=true;
  mInWhitespace=true;
  mLineBreakDue = false;
  mFloatingLines = -1;
}


/**
 * Outputs the calculated and stored indent and text in the indentation. That is
 * quote chars and numbers for numbered lists and such. It will also reset any
 * stored text to put in the indentation after using it.
 */
void
nsPlainTextSerializer::OutputQuotesAndIndent(bool stripTrailingSpaces /* = false */)
{
  nsAutoString stringToOutput;

  // Put the mail quote "> " chars in, if appropriate:
  if (mCiteQuoteLevel > 0) {
    nsAutoString quotes;
    for(int i=0; i < mCiteQuoteLevel; i++) {
      quotes.Append(char16_t('>'));
    }
    if (!mCurrentLine.IsEmpty()) {
      /* Better don't output a space here, if the line is empty,
         in case a receiving f=f-aware UA thinks, this were a flowed line,
         which it isn't - it's just empty.
         (Flowed lines may be joined with the following one,
         so the empty line may be lost completely.) */
      quotes.Append(char16_t(' '));
    }
    stringToOutput = quotes;
    mAtFirstColumn = false;
  }

  // Indent if necessary
  int32_t indentwidth = mIndent - mInIndentString.Length();
  if (indentwidth > 0
      && (!mCurrentLine.IsEmpty() || !mInIndentString.IsEmpty())
      // Don't make empty lines look flowed
      ) {
    nsAutoString spaces;
    for (int i=0; i < indentwidth; ++i)
      spaces.Append(char16_t(' '));
    stringToOutput += spaces;
    mAtFirstColumn = false;
  }

  if (!mInIndentString.IsEmpty()) {
    stringToOutput += mInIndentString;
    mAtFirstColumn = false;
    mInIndentString.Truncate();
  }

  if (stripTrailingSpaces) {
    int32_t lineLength = stringToOutput.Length();
    while(lineLength > 0 &&
          ' ' == stringToOutput[lineLength-1]) {
      --lineLength;
    }
    stringToOutput.SetLength(lineLength);
  }

  if (!stringToOutput.IsEmpty()) {
    Output(stringToOutput);
  }

}

/**
 * Write a string. This is the highlevel function to use to get text output.
 * By using AddToLine, Output, EndLine and other functions it handles quotation,
 * line wrapping, indentation, whitespace compression and other things.
 */
void
nsPlainTextSerializer::Write(const nsAString& aStr)
{
  // XXX Copy necessary to use nsString methods and gain
  // access to underlying buffer
  nsAutoString str(aStr);

#ifdef DEBUG_wrapping
  printf("Write(%s): wrap col = %d\n",
         NS_ConvertUTF16toUTF8(str).get(), mWrapColumn);
#endif

  int32_t bol = 0;
  int32_t newline;

  int32_t totLen = str.Length();

  // If the string is empty, do nothing:
  if (totLen <= 0) return;

  // For Flowed text change nbsp-ses to spaces at end of lines to allow them
  // to be cut off along with usual spaces if required. (bug #125928)
  if (mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
    for (int32_t i = totLen-1; i >= 0; i--) {
      char16_t c = str[i];
      if ('\n' == c || '\r' == c || ' ' == c || '\t' == c)
        continue;
      if (kNBSP == c)
        str.Replace(i, 1, ' ');
      else
        break;
    }
  }

  // We have two major codepaths here. One that does preformatted text and one
  // that does normal formatted text. The one for preformatted text calls
  // Output directly while the other code path goes through AddToLine.
  if ((mPreFormattedMail && !mWrapColumn) || (IsInPre() && !mPreFormattedMail)
      || (mSpanLevel > 0 && mEmptyLines >= 0 && IsQuotedLine(str))) {
    // No intelligent wrapping.

    // This mustn't be mixed with intelligent wrapping without clearing
    // the mCurrentLine buffer before!!!
    NS_ASSERTION(mCurrentLine.IsEmpty() || (IsInPre() && !mPreFormattedMail),
                 "Mixed wrapping data and nonwrapping data on the same line");
    if (!mCurrentLine.IsEmpty()) {
      FlushLine();
    }

    // Put the mail quote "> " chars in, if appropriate.
    // Have to put it in before every line.
    while(bol<totLen) {
      bool outputQuotes = mAtFirstColumn;
      bool atFirstColumn = mAtFirstColumn;
      bool outputLineBreak = false;
      bool spacesOnly = true;

      // Find one of '\n' or '\r' using iterators since nsAString
      // doesn't have the old FindCharInSet function.
      nsAString::const_iterator iter;           str.BeginReading(iter);
      nsAString::const_iterator done_searching; str.EndReading(done_searching);
      iter.advance(bol);
      int32_t new_newline = bol;
      newline = kNotFound;
      while(iter != done_searching) {
        if ('\n' == *iter || '\r' == *iter) {
          newline = new_newline;
          break;
        }
        if (' ' != *iter)
          spacesOnly = false;
        ++new_newline;
        ++iter;
      }

      // Done searching
      nsAutoString stringpart;
      if (newline == kNotFound) {
        // No new lines.
        stringpart.Assign(Substring(str, bol, totLen - bol));
        if (!stringpart.IsEmpty()) {
          char16_t lastchar = stringpart[stringpart.Length()-1];
          if ((lastchar == '\t') || (lastchar == ' ') ||
             (lastchar == '\r') ||(lastchar == '\n')) {
            mInWhitespace = true;
          }
          else {
            mInWhitespace = false;
          }
        }
        mEmptyLines=-1;
        atFirstColumn = mAtFirstColumn && (totLen-bol)==0;
        bol = totLen;
      }
      else {
        // There is a newline
        stringpart.Assign(Substring(str, bol, newline-bol));
        mInWhitespace = true;
        outputLineBreak = true;
        mEmptyLines=0;
        atFirstColumn = true;
        bol = newline+1;
        if ('\r' == *iter && bol < totLen && '\n' == *++iter) {
          // There was a CRLF in the input. This used to be illegal and
          // stripped by the parser. Apparently not anymore. Let's skip
          // over the LF.
          bol++;
        }
      }

      mCurrentLine.Truncate();
      if (mFlags & nsIDocumentEncoder::OutputFormatFlowed) {
        if ((outputLineBreak || !spacesOnly) && // bugs 261467,125928
            !IsQuotedLine(stringpart) &&
            !stringpart.EqualsLiteral("-- ") &&
            !stringpart.EqualsLiteral("- -- "))
          stringpart.Trim(" ", false, true, true);
        if (IsSpaceStuffable(stringpart.get()) && !IsQuotedLine(stringpart))
          mCurrentLine.Append(char16_t(' '));
      }
      mCurrentLine.Append(stringpart);

      if (outputQuotes) {
        // Note: this call messes with mAtFirstColumn
        OutputQuotesAndIndent();
      }

      Output(mCurrentLine);
      if (outputLineBreak) {
        Output(mLineBreak);
      }
      mAtFirstColumn = atFirstColumn;
    }

    // Reset mCurrentLine.
    mCurrentLine.Truncate();

#ifdef DEBUG_wrapping
    printf("No wrapping: newline is %d, totLen is %d\n",
           newline, totLen);
#endif
    return;
  }

  // Intelligent handling of text
  // If needed, strip out all "end of lines"
  // and multiple whitespace between words
  int32_t nextpos;
  const char16_t * offsetIntoBuffer = nullptr;

  while (bol < totLen) {    // Loop over lines
    // Find a place where we may have to do whitespace compression
    nextpos = str.FindCharInSet(" \t\n\r", bol);
#ifdef DEBUG_wrapping
    nsAutoString remaining;
    str.Right(remaining, totLen - bol);
    foo = ToNewCString(remaining);
    //    printf("Next line: bol = %d, newlinepos = %d, totLen = %d, string = '%s'\n",
    //           bol, nextpos, totLen, foo);
    free(foo);
#endif

    if (nextpos == kNotFound) {
      // The rest of the string
      offsetIntoBuffer = str.get() + bol;
      AddToLine(offsetIntoBuffer, totLen-bol);
      bol=totLen;
      mInWhitespace=false;
    }
    else {
      // There's still whitespace left in the string
      if (nextpos != 0 && (nextpos + 1) < totLen) {
        offsetIntoBuffer = str.get() + nextpos;
        // skip '\n' if it is between CJ chars
        if (offsetIntoBuffer[0] == '\n' && IS_CJ_CHAR(offsetIntoBuffer[-1]) && IS_CJ_CHAR(offsetIntoBuffer[1])) {
          offsetIntoBuffer = str.get() + bol;
          AddToLine(offsetIntoBuffer, nextpos-bol);
          bol = nextpos + 1;
          continue;
        }
      }
      // If we're already in whitespace and not preformatted, just skip it:
      if (mInWhitespace && (nextpos == bol) && !mPreFormattedMail &&
          !(mFlags & nsIDocumentEncoder::OutputPreformatted)) {
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
      if (mPreFormattedMail || (mFlags & nsIDocumentEncoder::OutputPreformatted)) {
        // Preserve the real whitespace character
        nextpos++;
        AddToLine(offsetIntoBuffer, nextpos-bol);
        bol = nextpos;
      }
      else {
        // Replace the whitespace with a space
        AddToLine(offsetIntoBuffer, nextpos-bol);
        AddToLine(kSpace.get(),1);
        bol = nextpos + 1; // Let's eat the whitespace
      }
    }
  } // Continue looping over the string
}


/**
 * Gets the value of an attribute in a string. If the function returns
 * NS_ERROR_NOT_AVAILABLE, there was none such attribute specified.
 */
nsresult
nsPlainTextSerializer::GetAttributeValue(nsAtom* aName,
                                         nsString& aValueRet)
{
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
bool
nsPlainTextSerializer::IsCurrentNodeConverted()
{
  nsAutoString value;
  nsresult rv = GetAttributeValue(nsGkAtoms::_class, value);
  return (NS_SUCCEEDED(rv) &&
          (value.EqualsIgnoreCase("moz-txt", 7) ||
           value.EqualsIgnoreCase("\"moz-txt", 8)));
}


// static
nsAtom*
nsPlainTextSerializer::GetIdForContent(nsIContent* aContent)
{
  if (!aContent->IsHTMLElement()) {
    return nullptr;
  }

  nsAtom* localName = aContent->NodeInfo()->NameAtom();
  return localName->IsStaticAtom() ? localName : nullptr;
}

bool
nsPlainTextSerializer::IsInPre()
{
  return !mPreformatStack.empty() && mPreformatStack.top();
}

bool
nsPlainTextSerializer::IsElementPreformatted(Element* aElement)
{
  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextNoFlush(aElement, nullptr, nullptr);
  if (styleContext) {
    const nsStyleText* textStyle = styleContext->StyleText();
    return textStyle->WhiteSpaceOrNewlineIsSignificant();
  }
  // Fall back to looking at the tag, in case there is no style information.
  return GetIdForContent(aElement) == nsGkAtoms::pre;
}

bool
nsPlainTextSerializer::IsElementBlock(Element* aElement)
{
  RefPtr<nsStyleContext> styleContext =
    nsComputedDOMStyle::GetStyleContextNoFlush(aElement, nullptr, nullptr);
  if (styleContext) {
    const nsStyleDisplay* displayStyle = styleContext->StyleDisplay();
    return displayStyle->IsBlockOutsideStyle();
  }
  // Fall back to looking at the tag, in case there is no style information.
  return nsContentUtils::IsHTMLBlock(aElement);
}

/**
 * This method is required only to identify LI's inside OL.
 * Returns TRUE if we are inside an OL tag and FALSE otherwise.
 */
bool
nsPlainTextSerializer::IsInOL()
{
  int32_t i = mTagStackIndex;
  while(--i >= 0) {
    if (mTagStack[i] == nsGkAtoms::ol)
      return true;
    if (mTagStack[i] == nsGkAtoms::ul) {
      // If a UL is reached first, LI belongs the UL nested in OL.
      return false;
    }
  }
  // We may reach here for orphan LI's.
  return false;
}

/*
  @return 0 = no header, 1 = h1, ..., 6 = h6
*/
int32_t HeaderLevel(nsAtom* aTag)
{
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


/*
 * This is an implementation of GetUnicharWidth() and
 * GetUnicharStringWidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2000-02-08 -- public domain
 *
 * Minor alterations to fit Mozilla's data types by Daniel Bratell
 */

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
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

namespace {

struct interval
{
  uint16_t first;
  uint16_t last;
};

struct CombiningComparator
{
  const char16_t mUcs;
  explicit CombiningComparator(char16_t aUcs) : mUcs(aUcs) {}
  int operator()(const interval& combining) const {
    if (mUcs > combining.last)
      return 1;
    if (mUcs < combining.first)
      return -1;

    MOZ_ASSERT(combining.first <= mUcs);
    MOZ_ASSERT(mUcs <= combining.last);
    return 0;
  }
};

} // namespace

int32_t GetUnicharWidth(char16_t ucs)
{
  /* sorted list of non-overlapping intervals of non-spacing characters */
  static const interval combining[] = {
    { 0x0300, 0x034E }, { 0x0360, 0x0362 }, { 0x0483, 0x0486 },
    { 0x0488, 0x0489 }, { 0x0591, 0x05A1 }, { 0x05A3, 0x05B9 },
    { 0x05BB, 0x05BD }, { 0x05BF, 0x05BF }, { 0x05C1, 0x05C2 },
    { 0x05C4, 0x05C4 }, { 0x064B, 0x0655 }, { 0x0670, 0x0670 },
    { 0x06D6, 0x06E4 }, { 0x06E7, 0x06E8 }, { 0x06EA, 0x06ED },
    { 0x0711, 0x0711 }, { 0x0730, 0x074A }, { 0x07A6, 0x07B0 },
    { 0x0901, 0x0902 }, { 0x093C, 0x093C }, { 0x0941, 0x0948 },
    { 0x094D, 0x094D }, { 0x0951, 0x0954 }, { 0x0962, 0x0963 },
    { 0x0981, 0x0981 }, { 0x09BC, 0x09BC }, { 0x09C1, 0x09C4 },
    { 0x09CD, 0x09CD }, { 0x09E2, 0x09E3 }, { 0x0A02, 0x0A02 },
    { 0x0A3C, 0x0A3C }, { 0x0A41, 0x0A42 }, { 0x0A47, 0x0A48 },
    { 0x0A4B, 0x0A4D }, { 0x0A70, 0x0A71 }, { 0x0A81, 0x0A82 },
    { 0x0ABC, 0x0ABC }, { 0x0AC1, 0x0AC5 }, { 0x0AC7, 0x0AC8 },
    { 0x0ACD, 0x0ACD }, { 0x0B01, 0x0B01 }, { 0x0B3C, 0x0B3C },
    { 0x0B3F, 0x0B3F }, { 0x0B41, 0x0B43 }, { 0x0B4D, 0x0B4D },
    { 0x0B56, 0x0B56 }, { 0x0B82, 0x0B82 }, { 0x0BC0, 0x0BC0 },
    { 0x0BCD, 0x0BCD }, { 0x0C3E, 0x0C40 }, { 0x0C46, 0x0C48 },
    { 0x0C4A, 0x0C4D }, { 0x0C55, 0x0C56 }, { 0x0CBF, 0x0CBF },
    { 0x0CC6, 0x0CC6 }, { 0x0CCC, 0x0CCD }, { 0x0D41, 0x0D43 },
    { 0x0D4D, 0x0D4D }, { 0x0DCA, 0x0DCA }, { 0x0DD2, 0x0DD4 },
    { 0x0DD6, 0x0DD6 }, { 0x0E31, 0x0E31 }, { 0x0E34, 0x0E3A },
    { 0x0E47, 0x0E4E }, { 0x0EB1, 0x0EB1 }, { 0x0EB4, 0x0EB9 },
    { 0x0EBB, 0x0EBC }, { 0x0EC8, 0x0ECD }, { 0x0F18, 0x0F19 },
    { 0x0F35, 0x0F35 }, { 0x0F37, 0x0F37 }, { 0x0F39, 0x0F39 },
    { 0x0F71, 0x0F7E }, { 0x0F80, 0x0F84 }, { 0x0F86, 0x0F87 },
    { 0x0F90, 0x0F97 }, { 0x0F99, 0x0FBC }, { 0x0FC6, 0x0FC6 },
    { 0x102D, 0x1030 }, { 0x1032, 0x1032 }, { 0x1036, 0x1037 },
    { 0x1039, 0x1039 }, { 0x1058, 0x1059 }, { 0x17B7, 0x17BD },
    { 0x17C6, 0x17C6 }, { 0x17C9, 0x17D3 }, { 0x18A9, 0x18A9 },
    { 0x20D0, 0x20E3 }, { 0x302A, 0x302F }, { 0x3099, 0x309A },
    { 0xFB1E, 0xFB1E }, { 0xFE20, 0xFE23 }
  };

  /* test for 8-bit control characters */
  if (ucs == 0)
    return 0;
  if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0))
    return -1;

  /* first quick check for Latin-1 etc. characters */
  if (ucs < combining[0].first)
    return 1;

  /* binary search in table of non-spacing characters */
  size_t idx;
  if (BinarySearchIf(combining, 0, ArrayLength(combining),
                     CombiningComparator(ucs), &idx)) {
    return 0;
  }

  /* if we arrive here, ucs is not a combining or C0/C1 control character */

  /* fast test for majority of non-wide scripts */
  if (ucs < 0x1100)
    return 1;

  return 1 +
    ((ucs >= 0x1100 && ucs <= 0x115f) || /* Hangul Jamo */
     (ucs >= 0x2e80 && ucs <= 0xa4cf && (ucs & ~0x0011) != 0x300a &&
      ucs != 0x303f) ||                  /* CJK ... Yi */
     (ucs >= 0xac00 && ucs <= 0xd7a3) || /* Hangul Syllables */
     (ucs >= 0xf900 && ucs <= 0xfaff) || /* CJK Compatibility Ideographs */
     (ucs >= 0xfe30 && ucs <= 0xfe6f) || /* CJK Compatibility Forms */
     (ucs >= 0xff00 && ucs <= 0xff5f) || /* Fullwidth Forms */
     (ucs >= 0xffe0 && ucs <= 0xffe6));
}


int32_t GetUnicharStringWidth(const char16_t* pwcs, int32_t n)
{
  int32_t w, width = 0;

  for (;*pwcs && n-- > 0; pwcs++)
    if ((w = GetUnicharWidth(*pwcs)) < 0)
      ++width; // Taking 1 as the width of non-printable character, for bug# 94475.
    else
      width += w;

  return width;
}
