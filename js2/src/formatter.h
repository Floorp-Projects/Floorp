/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef formatter_h___
#define formatter_h___

#include <cstdio>
#include <cstdarg>

#include "systemtypes.h"
#include "utilities.h"
#include "stlcfg.h"
#include "ds.h"
#include "strings.h"

namespace JavaScript
{
//
// Output
//

    // Print the characters between begin and end to the given file.  These
    // characters may include nulls.
    size_t printChars(FILE *file, const char *begin, const char *end);

#ifndef XP_MAC_MPW
    inline size_t printChars(FILE *file, const char *begin, const char *end) {
        ASSERT(end >= begin);
        return STD::fwrite(begin, 1, toSize_t(end - begin), file);
    }
#endif


    // A Formatter is an abstract base class representing a simplified output stream.
    // One can print text to a Formatter by using << and the various global
    // print... methods below. Formatters accept both char and char16 text and
    // convert as appropriate to their actual stream.
    class Formatter {
      protected:
        virtual void printChar8(char ch);
        virtual void printChar16(char16 ch);
        virtual void printZStr8(const char *str);
        virtual void printStr8(const char *strBegin, const char *strEnd) = 0;
        virtual void printStr16(const char16 *strBegin, const char16 *strEnd) = 0;
        virtual void printString16(const String &s);
        virtual void printVFormat8(const char *format, va_list args);
      public:

#ifdef __GNUC__ // Workaround for gcc pedantry.  No one should be calling delete on a raw Formatter.
        virtual ~Formatter() {}
#endif

        Formatter &operator<<(char ch) {printChar8(ch); return *this;}
        Formatter &operator<<(char16 ch) {printChar16(ch); return *this;}
        Formatter &operator<<(const char *str) {printZStr8(str); return *this;}
        Formatter &operator<<(const String &s) {printString16(s); return *this;}
        Formatter &operator<<(bool b);
        Formatter &operator<<(uint8 i) {printFormat(*this, "%u", i); return *this;}
        Formatter &operator<<(uint32 i) {printFormat(*this, "%u", i); return *this;}
        Formatter &operator<<(int32 i) {printFormat(*this, "%d", i); return *this;}

#ifndef _WIN32
        // Cause compile-time undefined YOU_TRIED_TO_PRINT_A_RAW_POINTER identifier errors for accidental printing of pointers.
        // The error occurs at the place where you try to instantiate this template; the compiler may or may not tell you where it is.
        template<class T> Formatter &operator<<(const T *s) {YOU_TRIED_TO_PRINT_A_RAW_POINTER(s); return *this;}
#endif

        friend void printString(Formatter &f, const char *strBegin, const char *strEnd) {f.printStr8(strBegin, strEnd);}
        friend void printString(Formatter &f, const char16 *strBegin, const char16 *strEnd) {f.printStr16(strBegin, strEnd);}
        friend void printFormat(Formatter &f, const char *format, ...);
    };

    void printNum(Formatter &f, uint32 i, int nDigits, char pad, const char *format);
    void printChar(Formatter &f, char ch, int count);
    void printChar(Formatter &f, char16 ch, int count);
    inline void printDec(Formatter &f, int32 i, int nDigits = 0, char pad = ' ') {printNum(f, (uint32)i, nDigits, pad, "%i");}
    inline void printDec(Formatter &f, uint32 i, int nDigits = 0, char pad = ' ') {printNum(f, i, nDigits, pad, "%u");}
    inline void printHex(Formatter &f, int32 i, int nDigits = 0, char pad = '0') {printNum(f, (uint32)i, nDigits, pad, "%X");}
    inline void printHex(Formatter &f, uint32 i, int nDigits = 0, char pad = '0') {printNum(f, i, nDigits, pad, "%X");}
    void printPtr(Formatter &f, void *p);


    // An AsciiFileFormatter is a Formatter that prints to a standard ASCII
    // file or stream. Characters with Unicode values of 256 or higher are
    // converted to escape sequences. Selected lower characters can also be
    // converted to escape sequences; these are specified by set bits in the
    // BitSet passed to the constructor.
    class AsciiFileFormatter: public Formatter {
        FILE *file;
        const BitSet<256> filter;   // Set of first 256 characters that are to be converted to escape sequences
        bool filterEmpty;           // True if filter passes all 256 characters
      public:
        static BitSet<256> defaultFilter;  // Default value of filter when not given in the constructor

        explicit AsciiFileFormatter(FILE *file, BitSet<256> *filter = 0);

      private:
        bool filterChar(char ch) {return filter[static_cast<uchar>(ch)];}
        bool filterChar(char16 ch) {
            return char16Value(ch) >= 0x100 || filter[char16Value(ch)];
        }

      protected:
        void printChar8(char ch);
        void printChar16(char16 ch);
        void printZStr8(const char *str);
        void printStr8(const char *strBegin, const char *strEnd);
        void printStr16(const char16 *strBegin, const char16 *strEnd);
    };

    extern AsciiFileFormatter stdOut;
    extern AsciiFileFormatter stdErr;


    // A StringFormatter is a Formatter that prints to a String.
    class StringFormatter: public Formatter {
        String s;

      public:
        const String& getString() { return s; }
        void clear() {JavaScript::clear(s);}
      protected:
        void printChar8(char ch);
        void printChar16(char16 ch);
        void printZStr8(const char *str);
        void printStr8(const char *strBegin, const char *strEnd);
        void printStr16(const char16 *strBegin, const char16 *strEnd);
        void printString16(const String &str);
    };


//
// Formatted Output
//

    class PrettyPrinter: public Formatter {
      public:
        STATIC_CONST(uint32, unlimitedLineWidth = 0x7FFFFFFF);
        class Region;
        class Indent;
        class Block;

      private:
        STATIC_CONST(uint32, infiniteLength = 0x80000000);
        const uint32 lineWidth;         // Current maximum desired line width

        struct BlockInfo {
            uint32 margin;              // Saved margin before this block's beginning
            uint32 lastBreak;           // Saved lastBreak before this block's beginning
            bool fits;                  // True if this entire block fits on one line
        };

        // Variables for the back end that prints to the destination
        Formatter &outputFormatter;     // Destination formatter on which the result should be printed
        uint32 outputPos;               // Number of characters printed on current output line
        uint32 lineNum;                 // Serial number of current line
        uint32 lastBreak;               // Number of line just after the last break that occurred in this block
        uint32 margin;                  // Current left margin in spaces
        ArrayBuffer<BlockInfo, 20> savedBlocks; // Stack of saved information about partially printed blocks

        // Variables for the front end that calculates block sizes
        struct Item: ListQueueEntry {
            enum Kind {text, blockBegin, indentBlockBegin, blockEnd, indent, linearBreak, fillBreak};

            const Kind kind;            // The kind of this text sequence
            bool lengthKnown;           // True if totalLength is known; always true for text, blockEnd, and indent Items
            uint32 length;              // Length of this text sequence, number of spaces for this break, or delta for indent or indentBlockBegin
            uint32 totalLength;         // Total length of this block (for blockBegin) or length of this break plus following clump (for breaks);
                                        // If lengthKnown is false, this is the serialPos of this Item instead of a length
            bool hasKind(Kind k) const {return kind == k;}

            explicit Item(Kind kind): kind(kind), lengthKnown(true), length(0) {}
            Item(Kind kind, uint32 length): kind(kind), lengthKnown(true), length(length) {}
            Item(Kind kind, uint32 length, uint32 beginSerialPos):
                    kind(kind), lengthKnown(false), length(length), totalLength(beginSerialPos) {}

            void computeTotalLength(uint32 endSerialPos) {
                ASSERT(!lengthKnown);
                lengthKnown = true;
                totalLength = endSerialPos - totalLength;
            }

        };

#ifdef DEBUG
        Region *topRegion;              // Most deeply nested Region
#endif
        uint32 nNestedBlocks;           // Number of nested Blocks

        uint32 leftSerialPos;           // The difference rightSerialPos-
        uint32 rightSerialPos;          // leftSerialPos is always the number of characters that would be output by
                                        // printing activeItems if they all fit on one line; only the difference
                                        // matters -- the absolute values are irrelevant and may wrap around 2^32.

        ArrayQueue<Item *, 20> itemStack; // Stack of enclosing nested Items whose lengths have not yet been determined;
                                        // itemStack always has room for at least nNestedBlocks extra entries so that end Items
                                        // may be added without throwing an exception.
        Pool<Item> itemPool;            // Pool from which to allocate activeItems
        ListQueue<Item> activeItems;    // Queue of items left to be printed
        ArrayQueue<char16, 256> itemText; // Text of text items in activeItems, in the same order as in activeItems

      public:
        static uint32 defaultLineWidth; // Default for lineWidth if not given to the constructor

        explicit PrettyPrinter(Formatter &f, uint32 lineWidth = defaultLineWidth);
      private:
        PrettyPrinter(const PrettyPrinter&);    // No copy constructor
        void operator=(const PrettyPrinter&);   // No assignment operator
      public:
        virtual ~PrettyPrinter();

      private:
        void outputBreak(bool sameLine, uint32 nSpaces);
        bool reduceLeftActiveItems(uint32 rightOffset);
        void reduceRightActiveItems();

        Item &beginIndent(int32 offset);
        void endIndent(Item &i);

        Item &beginBlock(Item::Kind kind, int32 offset);
        void endBlock(Item &i);

        void conditionalBreak(uint32 nSpaces, Item::Kind kind);

      protected:
        void printStr8(const char *strBegin, const char *strEnd);
        void printStr16(const char16 *strBegin, const char16 *strEnd);
      public:

        void requiredBreak();
        void linearBreak(uint32 nSpaces) {conditionalBreak(nSpaces, Item::linearBreak);}
        void linearBreak(uint32 nSpaces, bool required);
        void fillBreak(uint32 nSpaces) {conditionalBreak(nSpaces, Item::fillBreak);}

        void end();

        friend class Region;
        friend class Indent;
        friend class Block;

        class Region {
#ifdef DEBUG
            Region *next;  // Link to next most deeply nested Region
#endif
          protected:
            PrettyPrinter &pp;

            Region(PrettyPrinter &pp): pp(pp) {DEBUG_ONLY(next = pp.topRegion; pp.topRegion = this;);}
          private:
            Region(const Region&);          // No copy constructor
            void operator=(const Region&);  // No assignment operator
          protected:
#ifdef DEBUG
            ~Region() {pp.topRegion = next;}
#endif
        };

        // Use an Indent object to temporarily indent a PrettyPrinter by the
        // offset given to the Indent's constructor. The PrettyPrinter's margin
        // is set back to its original value when the Indent object is destroyed.
        // Using an Indent object is exception-safe; no matter how control
        // leaves an Indent scope, the indent is undone.
        // Scopes of Indent and Block objects must be properly nested.
        class Indent: public Region {
            Item &endItem;              // The Item returned by beginIndent
          public:
            Indent(PrettyPrinter &pp, int32 offset): Region(pp), endItem(pp.beginIndent(offset)) {}
            ~Indent() {pp.endIndent(endItem);}
        };

        // Use a Block object to temporarily enter a PrettyPrinter block.  If an
        // offset is provided, line breaks inside the block are indented by that
        // offset relative to the existing indent; otherwise, line breaks inside
        // the block are indented to the current output position.  The block
        // lasts until the Block object is destroyed.
        // Scopes of Indent and Block objects must be properly nested.
        class Block: public Region {
            Item &endItem;              // The Item returned by beginBlock
          public:
            explicit Block(PrettyPrinter &pp): Region(pp), endItem(pp.beginBlock(Item::blockBegin, 0)) {}
            Block(PrettyPrinter &pp, int32 offset): Region(pp), endItem(pp.beginBlock(Item::indentBlockBegin, offset)) {}
            ~Block() {pp.endBlock(endItem);}
        };
    };


    void escapeString(Formatter &f, const char16 *begin, const char16 *end, char16 quote);
    void quoteString(Formatter &f, const String &s, char16 quote);
}
#endif /* formatter_h___ */
