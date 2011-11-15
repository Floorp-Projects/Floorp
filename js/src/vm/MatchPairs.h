/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Chris Leary <cdleary@mozilla.com>
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

#ifndef MatchPairs_h__
#define MatchPairs_h__

/*
 * RegExp match results are succinctly represented by pairs of integer
 * indices delimiting (start, limit] segments of the input string.
 *
 * The pair count for a given RegExp match is the capturing parentheses
 * count plus one for the "0 capturing paren" whole text match.
 */

namespace js {

struct MatchPair
{
    int start;
    int limit;

    MatchPair(int start, int limit) : start(start), limit(limit) {}

    size_t length() const {
        JS_ASSERT(!isUndefined());
        return limit - start;
    }

    bool isUndefined() const {
        return start == -1;
    }

    void check() const {
        JS_ASSERT(limit >= start);
        JS_ASSERT_IF(!isUndefined(), start >= 0);
    }
};

class MatchPairs
{
    size_t  pairCount_;
    int     buffer_[1];

    explicit MatchPairs(size_t pairCount) : pairCount_(pairCount) {
        initPairValues();
    }

    void initPairValues() {
        for (int *it = buffer_; it < buffer_ + 2 * pairCount_; ++it)
            *it = -1;
    }

    static size_t calculateSize(size_t backingPairCount) {
        return sizeof(MatchPairs) - sizeof(int) + sizeof(int) * backingPairCount;
    }

    int *buffer() { return buffer_; }

    friend class detail::RegExpPrivate;

  public:
    /*
     * |backingPairCount| is necessary because PCRE uses extra space
     * after the actual results in the buffer.
     */
    static MatchPairs *create(LifoAlloc &alloc, size_t pairCount, size_t backingPairCount);

    size_t pairCount() const { return pairCount_; }

    MatchPair pair(size_t i) {
        JS_ASSERT(i < pairCount());
        return MatchPair(buffer_[2 * i], buffer_[2 * i + 1]);
    }

    void displace(size_t amount) {
        if (!amount)
            return;

        for (int *it = buffer_; it < buffer_ + 2 * pairCount_; ++it)
            *it = (*it < 0) ? -1 : *it + amount;
    }

    inline void checkAgainst(size_t length);
};

} /* namespace js */

#endif
