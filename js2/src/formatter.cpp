/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
* The Original Code is the JavaScript 2 Prototype.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.   Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation. All
* Rights Reserved.
*
* Contributor(s):
*
* Alternatively, the contents of this file may be used under the
* terms of the GNU Public License (the "GPL"), in which case the
* provisions of the GPL are applicable instead of those above.
* If you wish to allow use of your version of this file only
* under the terms of the GPL and not to allow others to use your
* version of this file under the NPL, indicate your decision by
* deleting the provisions above and replace them with the notice
* and other provisions required by the GPL.  If you do not delete
* the provisions above, a recipient may use your version of this
* file under either the NPL or the GPL.
*/

#include "algo.h"
#include "formatter.h"

namespace JS = JavaScript;


static const char controlCharNames[6] = {'b', 't', 'n', 'v', 'f', 'r'};

// Print the characters from begin to end, escaping them as necessary to make
// the resulting string be readable if placed between two quotes specified by
// quote (which should be either '\'' or '"').
void JS::escapeString(Formatter &f, const char16 *begin, const char16 *end, char16 quote)
{
    ASSERT(begin <= end);

    const char16 *chunk = begin;
    while (begin != end) {
        char16 ch = *begin++;
        CharInfo ci(ch);
        if (char16Value(ch) < 0x20 || isLineBreak(ci) || isFormat(ci) || ch == '\\' || ch == quote) {
            if (begin-1 != chunk)
                printString(f, chunk, begin-1);
            chunk = begin;

            f << '\\';
            switch (ch) {
              case 0x0008:
              case 0x0009:
              case 0x000A:
              case 0x000B:
              case 0x000C:
              case 0x000D:
                f << controlCharNames[ch - 0x0008];
                break;

              case '\'':
              case '"':
              case '\\':
                f << ch;
                break;

              case 0x0000:
                if (begin == end || char16Value(*begin) < '0' || char16Value(*begin) > '9') {
                    f << '0';
                    break;
                }
              default:
                if (char16Value(ch) <= 0xFF) {
                    f << 'x';
                    printHex(f, static_cast<uint32>(char16Value(ch)), 2);
                } else {
                    f << 'u';
                    printHex(f, static_cast<uint32>(char16Value(ch)), 4);
                }
            }
        }
    }
    if (begin != chunk)
        printString(f, chunk, begin);
}


// Print s as a quoted string using the given quotes (which should be
// either '\'' or '"').
void JS::quoteString(Formatter &f, const String &s, char16 quote)
{
    f << quote;
    const char16 *begin = s.data();
    escapeString(f, begin, begin + s.size(), quote);
    f << quote;
}


#ifdef XP_MAC_MPW
// Macintosh MPW replacements for the ANSI routines.  These translate LF's to
// CR's because the MPW libraries supplied by Metrowerks don't do that for some
// reason.
static void translateLFtoCR(char *begin, char *end)
{
    while (begin != end) {
        if (*begin == '\n')
            *begin = '\r';
        ++begin;
    }
}


size_t JS::printChars(FILE *file, const char *begin, const char *end)
{
    ASSERT(end >= begin);
    size_t n = toSize_t(end - begin);
    size_t extra = 0;
    char buffer[1024];

    while (n > sizeof buffer) {
        std::memcpy(buffer, begin, sizeof buffer);
        translateLFtoCR(buffer, buffer + sizeof buffer);
        extra += fwrite(buffer, 1, sizeof buffer, file);
        n -= sizeof buffer;
        begin += sizeof buffer;
    }
    std::memcpy(buffer, begin, n);
    translateLFtoCR(buffer, buffer + n);
    return extra + fwrite(buffer, 1, n, file);
}


int std::fputc(int c, FILE *file)
{
    char buffer = static_cast<char>(c);
    if (buffer == '\n')
        buffer = '\r';
    return static_cast<int>(fwrite(&buffer, 1, 1, file));
}


int std::fputs(const char *s, FILE *file)
{
    return static_cast<int>(JS::printChars(file, s, s + strlen(s)));
}


int std::fprintf(FILE* file, const char *format, ...)
{
    JS::Buffer<char, 1024> b;

    while (true) {
        va_list args;
        va_start(args, format);
        int n = vsnprintf(b.buffer, b.size, format, args);
        va_end(args);
        if (n >= 0 && n < b.size) {
            translateLFtoCR(b.buffer, b.buffer + n);
            return static_cast<int>(fwrite(b.buffer, 1, JS::toSize_t(n), file));
        }
        b.expand(b.size*2);
    }
}
#endif // XP_MAC_MPW


// Write ch.
void JS::Formatter::printChar8(char ch)
{
    printStr8(&ch, &ch + 1);
}


// Write ch.
void JS::Formatter::printChar16(char16 ch)
{
    printStr16(&ch, &ch + 1);
}


// Write the null-terminated string str.
void JS::Formatter::printZStr8(const char *str)
{
    printStr8(str, str + strlen(str));
}


// Write the String s.
void JS::Formatter::printString16(const String &s)
{
    const char16 *begin = s.data();
    printStr16(begin, begin + s.size());
}


// Write the printf format using the supplied args.
void JS::Formatter::printVFormat8(const char *format, va_list args)
{
    Buffer<char, 1024> b;

    while (true) {
        int n = vsnprintf(b.buffer, b.size, format, args);
        if (n >= 0 && static_cast<uint>(n) < b.size) {
            printStr8(b.buffer, b.buffer + n);
            return;
        }
        b.expand(b.size*2);
    }
}


// Write either "true" or "false".
JS::Formatter &JS::Formatter::operator<<(bool b)
{
    printZStr8(b ? "true" : "false");
    return *this;
}


// Write the printf format using the supplied args.
void JS::printFormat(Formatter &f, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    f.printVFormat8(format, args);
    va_end(args);
}


static const int printCharBufferSize = 64;

// Print ch count times.
void JS::printChar(Formatter &f, char ch, int count)
{
    char str[printCharBufferSize];

    while (count > 0) {
        int c = count;
        if (c > printCharBufferSize)
            c = printCharBufferSize;
        count -= c;
        STD::memset(str, ch, toSize_t(c));
        printString(f, str, str+c);
    }
}


// Print ch count times.
void JS::printChar(Formatter &f, char16 ch, int count)
{
    char16 str[printCharBufferSize];

    while (count > 0) {
        int c = count;
        if (c > printCharBufferSize)
            c = printCharBufferSize;
        count -= c;
        char16 *strEnd = str + c;
        std::fill(str, strEnd, ch);
        printString(f, str, strEnd);
    }
}


// Print i using the given formatting string, padding on the left with pad
// characters to use at least nDigits characters.
void JS::printNum(Formatter &f, uint32 i, int nDigits, char pad, const char *format)
{
    char str[20];
    int n = sprintf(str, format, i);
    if (n < nDigits)
        printChar(f, pad, nDigits - n);
    printString(f, str, str+n);
}


// Print p as a pointer.
void JS::printPtr(Formatter &f, void *p)
{
    char str[20];
    int n = sprintf(str, "%p", p);
    printString(f, str, str+n);
}


// printf formats for printing non-ASCII characters on an ASCII stream
#ifdef XP_MAC
static const char unprintableFormat[] = "\xC7%.4X\xC8"; // Use angle quotes
#elif defined _WIN32
static const char unprintableFormat[] = "\xAB%.4X\xBB"; // Use angle quotes
#else
static const char unprintableFormat[] = "<%.4X>";
#endif


static const uint16 defaultFilterRanges[] = {
    0x00, 0x09,     // Filter all control characters except \t and \n
    0x0B, 0x20,
    0x7F, 0x100,    // Filter all non-ASCII characters
    0, 0
};

JS::BitSet<256> JS::AsciiFileFormatter::defaultFilter(defaultFilterRanges);


// Construct an AsciiFileFormatter using the given file and filter f.
// If f is nil, use the default filter.
JS::AsciiFileFormatter::AsciiFileFormatter(FILE *file, BitSet<256> *f):
    file(file), filter(f ? *f : defaultFilter)
{
    filterEmpty = filter.none();
}


// Write ch, escaping non-ASCII characters.
void JS::AsciiFileFormatter::printChar8(char ch)
{
    if (filterChar(ch))
        fprintf(file, unprintableFormat, static_cast<uchar>(ch));
    else
        fputc(ch, file);
}


// Write ch, escaping non-ASCII characters.
void JS::AsciiFileFormatter::printChar16(char16 ch)
{
    if (filterChar(ch))
        fprintf(file, unprintableFormat, char16Value(ch));
    else
        fputc(static_cast<char>(ch), file);
}


// Write the null-terminated string str, escaping non-ASCII characters.
void JS::AsciiFileFormatter::printZStr8(const char *str)
{
    if (filterEmpty)
        fputs(str, file);
    else
        printStr8(str, str + strlen(str));
}


// Write the string between strBegin and strEnd, escaping non-ASCII characters.
void JS::AsciiFileFormatter::printStr8(const char *strBegin, const char *strEnd)
{
    if (filterEmpty)
        printChars(file, strBegin, strEnd);
    else {
        ASSERT(strEnd >= strBegin);
        const char *p = strBegin;
        while (strBegin != strEnd) {
            char ch = *strBegin;
            if (filterChar(ch)) {
                if (p != strBegin) {
                    printChars(file, p, strBegin);
                    p = strBegin;
                }
                fprintf(file, unprintableFormat, static_cast<uchar>(ch));
            }
            ++strBegin;
        }
        if (p != strBegin)
            printChars(file, p, strBegin);
    }
}


// Write the string between strBegin and strEnd, escaping non-ASCII characters.
void JS::AsciiFileFormatter::printStr16(const char16 *strBegin, const char16 *strEnd)
{
    char buffer[512];

    ASSERT(strEnd >= strBegin);
    char *q = buffer;
    while (strBegin != strEnd) {
        char16 ch = *strBegin++;
        if (filterChar(ch)) {
            if (q != buffer) {
                printChars(file, buffer, q);
                q = buffer;
            }
            fprintf(file, unprintableFormat, char16Value(ch));
        } else {
            *q++ = static_cast<char>(ch);
            if (q == buffer + sizeof buffer) {
                printChars(file, buffer, buffer + sizeof buffer);
                q = buffer;
            }
        }
    }
    if (q != buffer)
        printChars(file, buffer, q);
}


JS::AsciiFileFormatter JS::stdOut(stdout);
JS::AsciiFileFormatter JS::stdErr(stderr);


// Write ch.
void JS::StringFormatter::printChar8(char ch)
{
    s += ch;
}


// Write ch.
void JS::StringFormatter::printChar16(char16 ch)
{
    s += ch;
}


// Write the null-terminated string str.
void JS::StringFormatter::printZStr8(const char *str)
{
    s += str;
}


// Write the string between strBegin and strEnd.
void JS::StringFormatter::printStr8(const char *strBegin, const char *strEnd)
{
    appendChars(s, strBegin, strEnd);
}


// Write the string between strBegin and strEnd.
void JS::StringFormatter::printStr16(const char16 *strBegin, const char16 *strEnd)
{
    s.append(strBegin, strEnd);
}


// Write the String str.
void JS::StringFormatter::printString16(const String &str)
{
    s += str;
}



//
// Formatted Output
//

// See "Prettyprinting" by Derek Oppen in ACM Transactions on Programming
// Languages and Systems 2:4, October 1980, pages 477-482 for the algorithm.

// The default line width for pretty printing
uint32 JS::PrettyPrinter::defaultLineWidth = 20;


// Create a PrettyPrinter that outputs to Formatter f.  The PrettyPrinter
// breaks lines at optional breaks so as to try not to exceed lines of width
// lineWidth, although it may not always be able to do so.  Formatter f should
// be at the beginning of a line. Call end before destroying the Formatter;
// otherwise the last line may not be output to f.
JS::PrettyPrinter::PrettyPrinter(Formatter &f, uint32 lineWidth):
        lineWidth(v_min(lineWidth, static_cast<uint32>(unlimitedLineWidth))),
        outputFormatter(f),
        outputPos(0),
        lineNum(0),
        lastBreak(0),
        margin(0),
        nNestedBlocks(0),
        leftSerialPos(0),
        rightSerialPos(0),
        itemPool(20)
{
#ifdef DEBUG
    topRegion = 0;
#endif
}


// Destroy the PrettyPrinter.  Because it's a very bad idea for a destructor to
// throw exceptions, this destructor does not flush any buffered output.  Call
// end just before destroying the PrettyPrinter to do that.
JS::PrettyPrinter::~PrettyPrinter()
{
    ASSERT(!topRegion && !nNestedBlocks);
}


// Output either a line break (if sameLine is false) or length spaces (if
// sameLine is true). Also advance leftSerialPos by length.
//
// If this method throws an exception, it is guaranteed to already have updated
// all of the PrettyPrinter state; all that might be missing would be some
// output to outputFormatter.
void JS::PrettyPrinter::outputBreak(bool sameLine, uint32 length)
{
    leftSerialPos += length;

    if (sameLine) {
        outputPos += length;
        // Exceptions may be thrown below.
        printChar(outputFormatter, ' ', static_cast<int>(length));
    } else {
        lastBreak = ++lineNum;
        outputPos = margin;
        // Exceptions may be thrown below.
        outputFormatter << '\n';
        printChar(outputFormatter, ' ', static_cast<int>(margin));
    }
}


// Check to see whether (rightSerialPos+rightOffset)-leftSerialPos has gotten so large that we may pop items
// off the left end of activeItems because their totalLengths are known to be larger than the
// amount of space left on the current line.
// Return true if there are any items left on activeItems.
//
// If this method throws an exception, it leaves the PrettyPrinter in a consistent state, having
// atomically popped off one or more items from the left end of activeItems.
bool JS::PrettyPrinter::reduceLeftActiveItems(uint32 rightOffset)
{
    uint32 newRightSerialPos = rightSerialPos + rightOffset;
    while (activeItems) {
        Item *leftItem = &activeItems.front();
        if (itemStack && leftItem == itemStack.front()) {
            if (outputPos + newRightSerialPos - leftSerialPos > lineWidth) {
                itemStack.pop_front();
                leftItem->lengthKnown = true;
                leftItem->totalLength = infiniteLength;
            } else if (leftItem->lengthKnown)
                itemStack.pop_front();
        }

        if (!leftItem->lengthKnown)
            return true;

        activeItems.pop_front();
        try {
            uint32 length = leftItem->length;
            switch (leftItem->kind) {
              case Item::text:
                {
                    outputPos += length;
                    leftSerialPos += length;
                        // Exceptions may be thrown below.
                    char16 *textBegin;
                    char16 *textEnd;
                    do {
                        length -= itemText.pop_front(length, textBegin, textEnd);
                        printString(outputFormatter, textBegin, textEnd);
                    } while (length);
                }
                break;

              case Item::blockBegin:
              case Item::indentBlockBegin:
                {
                    BlockInfo *b = savedBlocks.advance_back();
                    b->margin = margin;
                    b->lastBreak = lastBreak;
                    b->fits = outputPos + leftItem->totalLength <= lineWidth;
                    if (leftItem->hasKind(Item::blockBegin))
                        margin = outputPos;
                    else
                        margin += length;
                }
                break;

              case Item::blockEnd:
                {
                    BlockInfo &b = savedBlocks.pop_back();
                    margin = b.margin;
                    lastBreak = b.lastBreak;
                }
                break;

              case Item::indent:
                margin += length;
                ASSERT(static_cast<int32>(margin) >= 0);
                break;

              case Item::linearBreak:
                // Exceptions may be thrown below, but only after updating the PrettyPrinter.
                outputBreak(savedBlocks.back().fits, length);
                break;

              case Item::fillBreak:
                // Exceptions may be thrown below, but only after updating the PrettyPrinter.
                outputBreak(lastBreak == lineNum && outputPos + leftItem->totalLength <= lineWidth, length);
                break;
            }
        } catch (...) {
            itemPool.destroy(leftItem);
            throw;
        }
        itemPool.destroy(leftItem);
    }
    return false;
}


// A break or end of input is about to be processed.  Check whether there are
// any complete blocks or clumps on the itemStack whose lengths we can now
// compute; if so, compute these and pop them off the itemStack.
// The current rightSerialPos must be the beginning of the break or end of input.
//
// This method can't throw exceptions.
void JS::PrettyPrinter::reduceRightActiveItems()
{
    uint32 nUnmatchedBlockEnds = 0;
    while (itemStack) {
        Item *rightItem = itemStack.pop_back();
        switch (rightItem->kind) {
          case Item::blockBegin:
          case Item::indentBlockBegin:
            if (!nUnmatchedBlockEnds) {
                itemStack.fast_push_back(rightItem);
                return;
            }
            rightItem->computeTotalLength(rightSerialPos);
            --nUnmatchedBlockEnds;
            break;

          case Item::blockEnd:
            ++nUnmatchedBlockEnds;
            break;

          case Item::linearBreak:
          case Item::fillBreak:
            rightItem->computeTotalLength(rightSerialPos);
            if (!nUnmatchedBlockEnds)
                // There can be at most one consecutive break posted on the itemStack.
                return;
            break;

          default:
            ASSERT(false);  // Other kinds can't be pushed onto the itemStack.
        }
    }
}


// Indent the beginning of every new line after this one by offset until the
// corresponding endIndent call.  Return an Item to pass to endIndent that will
// end this indentation. This method may throw an exception, in which case the
// PrettyPrinter is left unchanged.
JS::PrettyPrinter::Item &JS::PrettyPrinter::beginIndent(int32 offset)
{
    Item *unindent = new(itemPool) Item(Item::indent, static_cast<uint32>(-offset));
    if (activeItems) {
        try {
            activeItems.push_back(*new(itemPool) Item(Item::indent, static_cast<uint32>(offset)));
        } catch (...) {
            itemPool.destroy(unindent);
            throw;
        }
    } else {
        margin += offset;
        ASSERT(static_cast<int32>(margin) >= 0);
    }
    return *unindent;
}


// End an indent began by beginIndent.  i should be the result of a beginIndent.
// This method can't throw exceptions (it's called by the Indent destructor).
void JS::PrettyPrinter::endIndent(Item &i)
{
    if (activeItems)
        activeItems.push_back(i);
    else {
        margin += i.length;
        ASSERT(static_cast<int32>(margin) >= 0);
        itemPool.destroy(&i);
    }
}


// Begin a logical block.  If kind is Item::indentBlockBegin, offset is the
// indent to use for the second and subsequent lines of this block.
// Return an Item to pass to endBlock that will end this block.
// This method may throw an exception, in which case the PrettyPrinter is left
// unchanged.
JS::PrettyPrinter::Item &JS::PrettyPrinter::beginBlock(Item::Kind kind, int32 offset)
{
    uint32 newNNestedBlocks = nNestedBlocks + 1;
    savedBlocks.reserve(newNNestedBlocks);
    itemStack.reserve_back(1 + newNNestedBlocks);
    Item *endItem = new(itemPool) Item(Item::blockEnd);
    Item *beginItem;
    try {
        beginItem = new(itemPool) Item(kind, static_cast<uint32>(offset), rightSerialPos);
    } catch (...) {
        itemPool.destroy(endItem);
        throw;
    }
    // No state modifications before this point.
    // No exceptions after this point.
    activeItems.push_back(*beginItem);
    itemStack.fast_push_back(beginItem);
    nNestedBlocks = newNNestedBlocks;
    return *endItem;
}


// End a logical block began by beginBlock.  i should be the result of a
// beginBlock.
// This method can't throw exceptions (it's called by the Block destructor).
void JS::PrettyPrinter::endBlock(Item &i)
{
    activeItems.push_back(i);
    itemStack.fast_push_back(&i);
    --nNestedBlocks;
}


// Write a conditional line break.  This kind of a line break can only be
// emitted inside a block.
// A linear line break starts a new line if the containing block cannot be put
// all one one line; otherwise the line break is replaced by nSpaces spaces.
// Typically a block contains several linear breaks; either they all start new
// lines or none of them do.
// Moreover, if a block directly contains a required break then linear breaks
// become required breaks.
//
// A fill line break starts a new line if either the preceding clump or the
// following clump cannot be placed entirely on one line or if the following
// clump would not fit on the current line.  A clump is a consecutive sequence
// of strings and nested blocks delimited by either a break or the beginning or
// end of the currently enclosing block.
//
// If this method throws an exception, it leaves the PrettyPrinter in a
// consistent state.
void JS::PrettyPrinter::conditionalBreak(uint32 nSpaces, Item::Kind kind)
{
    ASSERT(nSpaces <= unlimitedLineWidth && nNestedBlocks);
    reduceRightActiveItems();
    itemStack.reserve_back(1 + nNestedBlocks);
    // Begin of exception-atomic stack update.  Only new(itemPool) can throw
    // an exception here, in which case nothing is updated.
    Item *i = new(itemPool) Item(kind, nSpaces, rightSerialPos);
    activeItems.push_back(*i);
    itemStack.fast_push_back(i);
    rightSerialPos += nSpaces;
    // End of exception-atomic stack update.
    reduceLeftActiveItems(0);
}


// Write the string between strBegin and strEnd.  Any embedded newlines ('\n'
// only) become required line breaks.
//
// If this method throws an exception, it may have partially formatted the
// string but leaves the PrettyPrinter in a consistent state.
void JS::PrettyPrinter::printStr8(const char *strBegin, const char *strEnd)
{
    while (strBegin != strEnd) {
        const char *sectionEnd = findValue(strBegin, strEnd, '\n');
        uint32 sectionLength = static_cast<uint32>(sectionEnd - strBegin);
        if (sectionLength) {
            if (reduceLeftActiveItems(sectionLength)) {
                itemText.reserve_back(sectionLength);
                Item &backItem = activeItems.back();
                // Begin of exception-atomic update.  Only new(itemPool) can throw an exception here,
                // in which case nothing is updated.
                if (backItem.hasKind(Item::text))
                    backItem.length += sectionLength;
                else
                    activeItems.push_back(*new(itemPool) Item(Item::text, sectionLength));
                rightSerialPos += sectionLength;
                itemText.fast_append(reinterpret_cast<const uchar *>(strBegin), reinterpret_cast<const uchar *>(sectionEnd));
                // End of exception-atomic update.
            } else {
                ASSERT(!itemStack && !activeItems && !itemText && leftSerialPos == rightSerialPos);
                outputPos += sectionLength;
                printString(outputFormatter, strBegin, sectionEnd);
            }
            strBegin = sectionEnd;
            if (strBegin == strEnd)
                break;
        }
        requiredBreak();
        ++strBegin;
    }
}


// Write the string between strBegin and strEnd.  Any embedded newlines ('\n'
// only) become required line breaks.
//
// If this method throws an exception, it may have partially formatted the
// string but leaves the PrettyPrinter in a consistent state.
void JS::PrettyPrinter::printStr16(const char16 *strBegin, const char16 *strEnd)
{
    while (strBegin != strEnd) {
        const char16 *sectionEnd = findValue(strBegin, strEnd, uni::lf);
        uint32 sectionLength = static_cast<uint32>(sectionEnd - strBegin);
        if (sectionLength) {
            if (reduceLeftActiveItems(sectionLength)) {
                itemText.reserve_back(sectionLength);
                Item &backItem = activeItems.back();
                // Begin of exception-atomic update.  Only new(itemPool) can throw an exception here,
                // in which case nothing is updated.
                if (backItem.hasKind(Item::text))
                    backItem.length += sectionLength;
                else
                    activeItems.push_back(*new(itemPool) Item(Item::text, sectionLength));
                rightSerialPos += sectionLength;
                itemText.fast_append(strBegin, sectionEnd);
                // End of exception-atomic update.
            } else {
                ASSERT(!itemStack && !activeItems && !itemText && leftSerialPos == rightSerialPos);
                outputPos += sectionLength;
                printString(outputFormatter, strBegin, sectionEnd);
            }
            strBegin = sectionEnd;
            if (strBegin == strEnd)
                break;
        }
        requiredBreak();
        ++strBegin;
    }
}


// Write a required line break.
//
// If this method throws an exception, it may have emitted partial output but
// leaves the PrettyPrinter in a consistent state.
void JS::PrettyPrinter::requiredBreak()
{
    reduceRightActiveItems();
    reduceLeftActiveItems(infiniteLength);
    ASSERT(!itemStack && !activeItems && !itemText && leftSerialPos == rightSerialPos);
    outputBreak(false, 0);
}


// If required is true, write a required line break; otherwise write a linear
// line break of the given width.
//
// If this method throws an exception, it may have emitted partial output but
// leaves the PrettyPrinter in a consistent state.
void JS::PrettyPrinter::linearBreak(uint32 nSpaces, bool required)
{
    if (required)
        requiredBreak();
    else
        linearBreak(nSpaces);
}


// Flush any saved output in the PrettyPrinter to the output.  Call this just
// before destroying the PrettyPrinter.  All Indent and Block objects must have
// been exited already.
//
// If this method throws an exception, it may have emitted partial output but
// leaves the PrettyPrinter in a consistent state.
void JS::PrettyPrinter::end()
{
    ASSERT(!topRegion);
    reduceRightActiveItems();
    reduceLeftActiveItems(infiniteLength);
    ASSERT(!savedBlocks && !itemStack && !activeItems && !itemText && rightSerialPos == leftSerialPos && !margin);
}
